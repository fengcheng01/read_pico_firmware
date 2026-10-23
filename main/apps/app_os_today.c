/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 *
 * 中文：手帐根页。按 UI 设计稿呈现复古印章日期头、三项子模式导航
 * （今日手记 / 打卡月历 / 金句便签）；底层深度打通 PMU RTC、os_lunar 农历、
 * NVS 统计与真实正文摘录；纯净纸面无多余杂点。
 * English: Journal root page. Renders vintage date stamp header and 3-mode subnav
 * (Today's Notes / Monthly Calendar / Quotes & Notes) per UI design drafts;
 * backed by PMU RTC, os_lunar, NVS stats and actual body excerpts; pure paper ground.
 *
 * 冻结：不伪造时间、任务或阅读数字；真实反映打卡天数与分钟；render 只绘图。
 * Frozen: Never invent time, tasks or reading numbers; reflect real streak and
 * minutes from actual storage; render only paints.
 * 第三批修订：图表标明真实日期和分钟，未校时或加载不冒充零；最近阅读不冒充今日足迹，不设虚构阅读目标。
 * Batch-three revision: Charts label real dates/minutes, invalid time/loading never masquerade as zero; recent reading is not a dated footprint and no goal is invented.
 * 用户修订：月历说明不响应开书；提供可取消、保存失败保原值的每日目标，不改历史阅读记录。
 * User revision: Calendar notes never open books; offer cancellable daily goals with failure-safe saves, retaining historical reading records.
 * 冻结：用户拒绝Tab黑闪；根页保留厂家GL16迁移，布局入口补偿一次白底，不强制GC16进页。
 * Frozen: User rejects flashing Tabs; roots retain vendor GL16 migrations with one white entry compensation and no forced GC16 entries.
 */
#include "app.h"
#include "app_registry.h"
#include "book_entry.h"
#include "book_quotes.h"
#include "book_home.h"
#include "book_stats.h"
#include "os_device.h"
#include "os_lunar.h"
#include "os_time.h"
#include "ui_gesture.h"
#include "ui_product.h"
#include "ui_press.h"
#include "settings.h"
#include <stdio.h>
#include <string.h>

typedef enum {
    JOURNAL_TAB_TODAY = 0,
    JOURNAL_TAB_MONTH = 1,
    JOURNAL_TAB_QUOTES = 2,
} journal_tab_t;

static journal_tab_t s_active_tab = JOURNAL_TAB_TODAY;
static bool s_started, s_loading;
static unsigned s_revision;
static int64_t s_probe_ms;
// 已上屏的日期键：只在跨日（统计与月历真正变化）时重绘，分钟跳变不再整页闪。
// / The drawn date key: repaint only when the day flips (when stats and the calendar truly change); minute flips no longer flash the page.
static uint32_t s_drawn_day;
static ui_press_t s_press;
static bool pressed_id(int id) { return ui_press_active(&s_press) && s_press.id == id; }
static book_quote_t s_quotes[BOOK_QUOTES_MAX];
static size_t s_quote_count;
static unsigned s_quote_page, s_quote_revision;
static const char* s_quote_error;
static bool s_goal_open, s_goal_error;
static unsigned s_goal_candidate;
static const unsigned s_goal_presets[] = {0, 15, 30, 45, 60, 90};
static EpdRect goal_entry_rect(void) { return (EpdRect){UI_MARGIN, 1018, ui_content_width(), 64}; }
static EpdRect goal_control_rect(int id) {
    if (id >= 100 && id < 106) return ui_grid_rect((id - 100) % 3, 3, (id - 100) / 3, 550, 64);
    if (id == 106 || id == 107) return (EpdRect){id == 106 ? UI_MARGIN + 16 : ui_content_right() - 140, 730, 124, 70};
    return ui_row_rect(id - 108, 2, 910, 72);
}
static void draw_goal(uint8_t* fb) {
    EpdRect panel = {UI_MARGIN - 12, 314, ui_content_width() + 24, 692};
    ui_fill_round_rect(fb, panel, UI_BTN_RADIUS, UI_GRAY_WHITE);
    ui_draw_round_rect(fb, panel, UI_BTN_RADIUS, UI_GRAY_BLACK);
    ui_text(fb, UI_MARGIN + 16, 340, 36, "每日阅读目标", EPD_DRAW_ALIGN_LEFT, false);
    char value[40];
    if (s_goal_candidate) snprintf(value, sizeof(value), "%u 分钟", s_goal_candidate);
    else snprintf(value, sizeof(value), "未设置");
    ui_text_vc(fb, UI_LOCK_WIDTH / 2, 460, 60, value, EPD_DRAW_ALIGN_CENTER, false);
    for (int i = 0; i < 6; ++i) {
        char label[24];
        if (s_goal_presets[i]) snprintf(label, sizeof(label), "%u 分钟", s_goal_presets[i]);
        else snprintf(label, sizeof(label), "不设置");
        ui_draw_button(fb, goal_control_rect(100 + i), label, s_goal_candidate == s_goal_presets[i]);
    }
    ui_draw_button(fb, goal_control_rect(106), "−", false);
    ui_draw_button(fb, goal_control_rect(107), "+", false);
    ui_text_vc(fb, UI_LOCK_WIDTH / 2, 765, 24, "每次 5 分钟", EPD_DRAW_ALIGN_CENTER, false);
    ui_product_title(fb, (EpdRect){UI_MARGIN + 16, 824, ui_content_width() - 32, 70},
        s_goal_error ? "保存失败，原目标未改变。请重试。" : "目标用于今日完成情况；月历仍记录有阅读的日期。", 24, 2);
    ui_draw_button(fb, goal_control_rect(108), "取消", false);
    ui_draw_button(fb, goal_control_rect(109), "保存目标", true);
}
static void quotes_reload(void) {
    s_quote_count = book_quotes_list(s_quotes, BOOK_QUOTES_MAX);
    s_quote_revision = book_quotes_revision();
    if (s_quote_page * 3 >= s_quote_count) s_quote_page = 0;
}
static EpdRect quote_nav(int i) { return (EpdRect){UI_MARGIN + i * 430, 834, 174, 66}; }
// 底部动作按钮与三个子页共用几何。/ The bottom action button is shared by all three tabs.
static EpdRect action_rect(void) { return (EpdRect){UI_MARGIN, 920, ui_content_width(), 84}; }
// 仅最近阅读卡开书；月历说明是纯信息。/ Only the recent-reading card opens a book; calendar notes are informational.
static EpdRect footprint_rect(void) { return (EpdRect){UI_MARGIN, 722, ui_content_width(), 160}; }
// 子标签条三等分。/ The subnav strip split in three.
static EpdRect tab_rect(int t) {
    int width = ui_content_width();
    int tab_w = width / 3;
    int tw = (t == 2) ? (width - 2 * tab_w) : tab_w;
    return (EpdRect){UI_MARGIN + t * tab_w, 294, tw, 46};
}


static void format_minutes(unsigned long minutes, char* out, size_t cap) {
    if (minutes >= 60) snprintf(out, cap, "%lu 小时 %lu 分", minutes / 60, minutes % 60);
    else snprintf(out, cap, "%lu 分钟", minutes);
}

static unsigned compute_streak(uint32_t today) {
    if (!today) return 0;
    unsigned streak = 0;
    for (int i = 0; i < 30; ++i) {
        uint32_t d = os_time_date_shift(today, -i);
        if (book_stats_minutes(d) > 0) streak++;
        else if (i > 0) break;
    }
    return streak;
}

static void on_enter(app_ctx_t* ctx) {
    s_goal_open = s_goal_error = false;
    quotes_reload(); s_quote_error = NULL;
    s_started = book_home_cached();
    s_loading = !s_started;
    uint32_t day = 0;
    s_drawn_day = os_time_date_key(&day) ? day : 0;
    ui_press_reset(&s_press);
    s_revision = book_store_revision();
    s_probe_ms = ctx->now_ms;
    if (s_loading && !book_home_snapshot()->complete) os_storage_probe();
    os_time_poll(ctx->now_ms);
}

static void today_on_exit(app_ctx_t* ctx) {
    (void)ctx;
}
static void on_media_lost(app_ctx_t* ctx) {
    (void)ctx;
    book_home_invalidate();
    book_home_cancel();
    s_started = false;
    s_loading = true;
}

static void render(app_ctx_t* ctx, uint8_t* fb) {
    (void)ctx;
    const book_home_snapshot_t* data = book_home_snapshot();
    ui_clear_page(fb);
    ui_product_header(fb, "手帐", "阅读足迹 · 统计 · 打卡");
    ui_draw_button(fb,ui_product_back_rect(),"阅读记录",false);

    const os_time_info_t* info = os_time_info();
    uint32_t today_key = 0;
    bool date_valid = os_time_date_key(&today_key);
    unsigned streak = compute_streak(date_valid ? today_key : 0);

    // 1. 顶部复古印章日期头卡片 (Vintage Date Stamp Header Card)
    EpdRect top_card = {UI_MARGIN, 196, ui_content_width(), 86};
    ui_draw_round_rect(fb, top_card, UI_CHIP_RADIUS, UI_GRAY_BLACK);

    ui_text(fb, top_card.x + 16, top_card.y + 12, 36, "阅读手帐", EPD_DRAW_ALIGN_LEFT, false);

    char subtitle[96];
    if (date_valid && info->year >= 1900 && info->year <= 2100) {
        os_lunar_date_t lunar;
        if (os_lunar_from_solar(info->year, info->month, info->day, &lunar)) {
            char ganzhi[8];
            os_lunar_year_ganzhi(lunar.year, ganzhi);
            snprintf(subtitle, sizeof(subtitle), "%04u.%02u.%02u · %s年%s%s · 宜深读",
                     info->year, info->month, info->day,
                     ganzhi, os_lunar_month_name(lunar.month, lunar.leap), os_lunar_day_name(lunar.day));
        } else {
            snprintf(subtitle, sizeof(subtitle), "%04u.%02u.%02u · 宜深读", info->year, info->month, info->day);
        }
    } else {
        snprintf(subtitle, sizeof(subtitle), "时间未校时 · 连接传书或WiFi可同步");
    }
    ui_product_title(fb, (EpdRect){top_card.x + 16, top_card.y + 50, top_card.width - 188, 32}, subtitle, 24, 1);

    // 右侧打卡印章 (READING STREAK Stamp Seal)
    EpdRect stamp = {top_card.x + top_card.width - 156, top_card.y + 12, 140, 62};
    ui_draw_round_rect(fb, stamp, 6, UI_GRAY_BLACK);
    ui_text_vc(fb, stamp.x + stamp.width / 2, stamp.y + 18, 18, "READING STREAK", EPD_DRAW_ALIGN_CENTER, false);
    char streak_val[24];
    if (date_valid) snprintf(streak_val, sizeof(streak_val), "%u DAYS", streak);
    else snprintf(streak_val, sizeof(streak_val), "— DAYS");
    ui_text_vc(fb, stamp.x + stamp.width / 2, stamp.y + 44, 26, streak_val, EPD_DRAW_ALIGN_CENTER, false);

    // 2. 子标签切换条 (3 Subnav Tabs: 今日概览 / 打卡月历 / 金句便签)
    EpdRect subnav = {UI_MARGIN, 294, ui_content_width(), 46};
    ui_draw_round_rect(fb, subnav, 6, UI_GRAY_BLACK);
    static const char* tab_names[] = {"今日概览", "打卡月历", "金句便签"};
    for (int t = 0; t < 3; ++t) {
        EpdRect tr = tab_rect(t);
        if (pressed_id(t)) ui_draw_pressed_round_rect(fb, tr, 6);
        if (s_active_tab == (journal_tab_t)t) {
            ui_fill_round_rect(fb, tr, 6, UI_GRAY_BLACK);
            ui_text_vc(fb, tr.x + tr.width / 2, tr.y + tr.height / 2, 30, tab_names[t], EPD_DRAW_ALIGN_CENTER, true);
        } else {
            ui_text_vc(fb, tr.x + tr.width / 2, tr.y + tr.height / 2, 30, tab_names[t], EPD_DRAW_ALIGN_CENTER, false);
        }
        if (t > 0) epd_fill_rect((EpdRect){tr.x, subnav.y, 1, subnav.height}, UI_GRAY_BLACK, fb);
    }

    // 3. 子页面内容展示 (Sub-view Content)
    if (s_active_tab == JOURNAL_TAB_TODAY) {
        // [子模块 A: 今日手记]
        // 4 格数据大盘 (Stats Matrix)
        int cell_w = (ui_content_width() - 14) / 2;
        int cell_h = 76;
        int grid_y = 352;
        EpdRect c0 = {UI_MARGIN, grid_y, cell_w, cell_h};
        EpdRect c1 = {UI_MARGIN + cell_w + 14, grid_y, cell_w, cell_h};
        EpdRect c2 = {UI_MARGIN, grid_y + cell_h + 10, cell_w, cell_h};
        EpdRect c3 = {UI_MARGIN + cell_w + 14, grid_y + cell_h + 10, cell_w, cell_h};

        ui_draw_round_rect(fb, c0, UI_CHIP_RADIUS, UI_GRAY_BLACK);
        ui_draw_round_rect(fb, c1, UI_CHIP_RADIUS, UI_GRAY_BLACK);
        ui_draw_round_rect(fb, c2, UI_CHIP_RADIUS, UI_GRAY_BLACK);
        ui_draw_round_rect(fb, c3, UI_CHIP_RADIUS, UI_GRAY_BLACK);

        char val0[48] = "未校时", val1[48] = "未校时";
        if (s_loading && date_valid) { strcpy(val0, "加载中…"); strcpy(val1, "加载中…"); }
        uint16_t today_m = 0;
        if (!s_loading && date_valid) {
            today_m = book_stats_minutes(today_key);
            format_minutes((unsigned long)today_m, val0, sizeof(val0));
            format_minutes((unsigned long)book_stats_total_minutes(os_time_date_shift(today_key, -6), today_key), val1, sizeof(val1));
        }

        ui_text(fb, c0.x + 12, c0.y + 8, 24, "今日阅读", EPD_DRAW_ALIGN_LEFT, false);
        ui_product_title(fb, (EpdRect){c0.x + 12, c0.y + 36, c0.width - 24, 42}, val0, 34, 1);

        ui_text(fb, c1.x + 12, c1.y + 8, 24, "近 7 天累计阅读", EPD_DRAW_ALIGN_LEFT, false);
        ui_product_title(fb, (EpdRect){c1.x + 12, c1.y + 36, c1.width - 24, 42}, val1, 34, 1);

        char val2[32] = "未校时";
        if (s_loading && date_valid) snprintf(val2, sizeof(val2), "加载中…");
        else if (date_valid) {
            unsigned active_days = 0;
            for (int d = 0; d < BOOK_STATS_KEEP_DAYS; ++d)
                if (book_stats_minutes(os_time_date_shift(today_key, -d))) ++active_days;
            snprintf(val2, sizeof(val2), "%u 天", active_days);
        }
        ui_text(fb, c2.x + 12, c2.y + 8, 24, "近 30 天有阅读记录", EPD_DRAW_ALIGN_LEFT, false);
        ui_product_title(fb, (EpdRect){c2.x + 12, c2.y + 36, c2.width - 24, 42}, val2, 34, 1);
        char val3[32]; snprintf(val3, sizeof(val3), "%u 条", (unsigned)s_quote_count);
        ui_text(fb, c3.x + 12, c3.y + 8, 24, "已保存摘录", EPD_DRAW_ALIGN_LEFT, false);
        ui_product_title(fb, (EpdRect){c3.x + 12, c3.y + 36, c3.width - 24, 42}, val3, 34, 1);

        // 七日柱状图以本窗口最大分钟数缩放，每柱标出实际分钟与日期。
        // Scale seven-day bars to the window maximum, labeling each actual minute count and date.
        ui_hairline(fb, 516, UI_MARGIN, ui_content_width(), UI_GRAY_LIGHT);
        ui_text(fb, UI_MARGIN, 524, 26, "近 7 天阅读 · 分钟", EPD_DRAW_ALIGN_LEFT, false);
        if (date_valid && !s_loading) {
            uint16_t minutes[7]; unsigned maximum = 0;
            for (int i = 0; i < 7; ++i) {
                minutes[i] = book_stats_minutes(os_time_date_shift(today_key, i - 6));
                if (minutes[i] > maximum) maximum = minutes[i];
            }
            char scale[40]; snprintf(scale, sizeof(scale), "最高 %u 分", maximum);
            ui_text(fb, ui_content_right(), 528, 20, scale, EPD_DRAW_ALIGN_RIGHT, false);
            int step = ui_content_width() / 7, base = 640;
            for (int i = 0; i < 7; ++i) {
                int x = UI_MARGIN + i * step + (step - 36) / 2;
                int height = maximum ? (int)((unsigned)minutes[i] * 52 / maximum) : 0;
                if (minutes[i] && height < 3) height = 3;
                if (height) epd_fill_rect((EpdRect){x, base - height, 36, height}, i == 6 ? UI_GRAY_BLACK : UI_GRAY_LIGHT, fb);
                char value[12]; snprintf(value, sizeof(value), "%u", minutes[i]);
                ui_text(fb, x + 18, base - height - 26, 20, value, EPD_DRAW_ALIGN_CENTER, false);
                uint32_t date = os_time_date_shift(today_key, i - 6);
                char day[24]; snprintf(day, sizeof(day), "%u/%u%s", (unsigned)(date / 100 % 100), (unsigned)(date % 100), i == 6 ? "今" : "");
                ui_text(fb, x + 18, 650, 20, day, EPD_DRAW_ALIGN_CENTER, false);
            }
            ui_hairline(fb, base + 2, UI_MARGIN, ui_content_width(), UI_GRAY_BLACK);
        } else ui_product_title(fb, (EpdRect){UI_MARGIN, 580, ui_content_width(), 80},
                    date_valid ? "统计加载中…" : "时间未校时，暂不归日统计", 28, 2);

        // 最近书目沿用持久阅读顺序，不推断今日阅读书目。/ Recent books follow persisted order without inferring today's titles.
        ui_hairline(fb, 680, UI_MARGIN, ui_content_width(), UI_GRAY_LIGHT);
        ui_text(fb, UI_MARGIN, 688, 28, "最近阅读 · 点击继续", EPD_DRAW_ALIGN_LEFT, false);

        EpdRect session_card = footprint_rect();
        if (pressed_id(4)) ui_draw_pressed_round_rect(fb, session_card, UI_CHIP_RADIUS);
        ui_draw_round_rect(fb, session_card, UI_CHIP_RADIUS, UI_GRAY_BLACK);

        if (!s_loading && data->current.path[0]) {
            epd_fill_rect((EpdRect){session_card.x + 18, session_card.y + 24, 10, 10}, UI_GRAY_BLACK, fb);
            ui_product_title(fb, (EpdRect){session_card.x + 36, session_card.y + 14, session_card.width - 54, 90},
                             data->current.title, 36, 2);
            char prog_line[64];
            if (data->current.has_progress) {
                snprintf(prog_line, sizeof(prog_line), "已读进度 %u%% · 点击恢复阅读", data->current.percent);
            } else {
                snprintf(prog_line, sizeof(prog_line), "未开始 · 点击打开第一页");
            }
            ui_text(fb, session_card.x + 36, session_card.y + 112, 26, prog_line, EPD_DRAW_ALIGN_LEFT, false);
            int pw = session_card.width - 54;
            epd_fill_rect((EpdRect){session_card.x + 36, session_card.y + 148, pw, 4}, UI_GRAY_LIGHT, fb);
            int fill_w = data->current.percent * pw / 100;
            if (fill_w > pw) fill_w = pw;
            if (fill_w > 0) epd_fill_rect((EpdRect){session_card.x + 36, session_card.y + 148, fill_w, 4}, UI_GRAY_BLACK, fb);
        } else {
            ui_text(fb, session_card.x + 18, session_card.y + 40, 28,
                    s_loading ? "正在加载阅读足迹…" : "还没有阅读记录", EPD_DRAW_ALIGN_LEFT, false);
            ui_text(fb, session_card.x + 18, session_card.y + 92, 26,
                    s_loading ? "请稍候" : "打开书架，开始你的第一本手帐阅读", EPD_DRAW_ALIGN_LEFT, false);
        }

        // 底部动作按钮 (Action Button)
        if (pressed_id(3)) ui_draw_pressed_round_rect(fb, action_rect(), UI_BTN_RADIUS);
        ui_draw_button(fb, action_rect(), !s_loading && data->current.path[0] ? "继续阅读" : "打开书架", true);
    } else if (s_active_tab == JOURNAL_TAB_MONTH) {
        // [子模块 B: 打卡月历]
        EpdRect month_card = {UI_MARGIN, 352, ui_content_width(), 550};
        ui_draw_round_rect(fb, month_card, UI_CHIP_RADIUS, UI_GRAY_BLACK);

        if (!date_valid) {
            ui_text(fb, UI_MARGIN + 24, 396, 36, "时间未校时", EPD_DRAW_ALIGN_LEFT, false);
            ui_product_title(fb, (EpdRect){UI_MARGIN + 24, 468, ui_content_width() - 48, 160},
                             "连接已有 WiFi 完成校时后，再查看真实日期与阅读月历。", 30, 3);
            ui_product_title(fb, (EpdRect){UI_MARGIN, 932, ui_content_width(), 64}, "当前不显示日期和打卡记录", 26, 1);
        } else {
        uint16_t yr = info->year;
        uint8_t mo = info->month;
        uint8_t today_d = date_valid ? info->day : 0;

        static const uint8_t k_days_in_month[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
        uint8_t dim = (mo >= 1 && mo <= 12) ? k_days_in_month[mo - 1] : 30;
        if (mo == 2 && ((yr % 4 == 0 && yr % 100 != 0) || (yr % 400 == 0))) dim = 29;

        int wday_1st = date_valid ? ((int)info->weekday + 700 - (int)(info->day - 1)) % 7 : 4;

        unsigned stamped_days = 0;
        if (date_valid) {
            for (uint8_t d = 1; d <= dim; ++d) {
                uint32_t d_key = (uint32_t)yr * 10000 + (uint32_t)mo * 100 + d;
                if (book_stats_minutes(d_key) > 0) stamped_days++;
            }
        }

        char title_buf[64], meta_buf[32];
        snprintf(title_buf, sizeof(title_buf), "%u 年 %u 月打卡日历", yr, mo);
        snprintf(meta_buf, sizeof(meta_buf), "已打卡 %u/%u 天", stamped_days, dim);
        ui_text(fb, month_card.x + 16, month_card.y + 16, 32, title_buf, EPD_DRAW_ALIGN_LEFT, false);
        ui_text(fb, month_card.x + month_card.width - 16, month_card.y + 56, 26, meta_buf, EPD_DRAW_ALIGN_RIGHT, false);
        ui_hairline(fb, month_card.y + 94, month_card.x + 14, month_card.width - 28, UI_GRAY_LIGHT);

        static const char* k_wds[] = {"日", "一", "二", "三", "四", "五", "六"};
        int grid_left = month_card.x + 14;
        int grid_w = month_card.width - 28;
        int cell_w = grid_w / 7;
        int cell_h = 60;
        int grid_top = month_card.y + 106;

        for (int w = 0; w < 7; ++w) {
            ui_text_vc(fb, grid_left + w * cell_w + cell_w / 2, grid_top + 16, 28, k_wds[w], EPD_DRAW_ALIGN_CENTER, false);
        }
        grid_top += 36;

        for (uint8_t d = 1; d <= dim; ++d) {
            int slot = (int)(d - 1 + wday_1st);
            int col = slot % 7;
            int row = slot / 7;
            int cx = grid_left + col * cell_w;
            int cy = grid_top + row * (cell_h + 4);
            EpdRect cell_r = {cx + 2, cy, cell_w - 4, cell_h};

            uint32_t d_key = (uint32_t)yr * 10000 + (uint32_t)mo * 100 + d;
            bool stamped = date_valid && (book_stats_minutes(d_key) > 0);
            bool is_today = (d == today_d);

            char d_num[4];
            snprintf(d_num, sizeof(d_num), "%u", d);

            if (stamped) {
                ui_fill_round_rect(fb, cell_r, 4, UI_GRAY_BLACK);
                ui_text_vc(fb, cell_r.x + cell_r.width / 2, cell_r.y + cell_r.height / 2, 32, d_num, EPD_DRAW_ALIGN_CENTER, true);
            } else {
                ui_draw_round_rect(fb, cell_r, 4, UI_GRAY_LIGHT);
                ui_text_vc(fb, cell_r.x + cell_r.width / 2, cell_r.y + cell_r.height / 2, 32, d_num, EPD_DRAW_ALIGN_CENTER, false);
            }
            if (is_today) {
                epd_draw_rect((EpdRect){cell_r.x - 1, cell_r.y - 1, cell_r.width + 2, cell_r.height + 2}, UI_GRAY_BLACK, fb);
            }
        }

        // 说明只绘图，不响应点击或按下态。/ The legend paints only, without taps or a press bed.
        ui_draw_round_rect(fb, action_rect(), UI_CHIP_RADIUS, UI_GRAY_BLACK);
        ui_product_title(fb, (EpdRect){action_rect().x + 16, action_rect().y + 10, action_rect().width - 32, 32}, "实心：有阅读分钟的日期；外框：今天", 24, 1);
        char streak_note[128];
        unsigned goal = app_settings_daily_goal();
        if (goal) snprintf(streak_note, sizeof(streak_note), "连续有阅读记录 %u 天 · 目标 %u 分钟", streak, goal);
        else snprintf(streak_note, sizeof(streak_note), "连续有阅读记录 %u 天 · 目标未设置", streak);
        ui_product_title(fb, (EpdRect){action_rect().x + 16, action_rect().y + 46, action_rect().width - 32, 32}, streak_note, 24, 1);
        }
    } else if (s_active_tab == JOURNAL_TAB_QUOTES) {
        for (unsigned row=0;row<3;++row) {
            size_t index=s_quote_page*3+row; if(index>=s_quote_count)break;
            const book_quote_t* q=&s_quotes[index];
            EpdRect qr={UI_MARGIN,352+(int)row*154,ui_content_width(),138};
            if(pressed_id(10 + (int)row)) ui_draw_pressed_round_rect(fb, qr, UI_CHIP_RADIUS);
            ui_draw_round_rect(fb,qr,UI_CHIP_RADIUS,UI_GRAY_BLACK);
            epd_fill_rect((EpdRect){qr.x,qr.y+4,6,qr.height-8},UI_GRAY_BLACK,fb);
            ui_product_title(fb,(EpdRect){qr.x+20,qr.y+14,qr.width-40,80},q->text,32,2);
            const char* name=strrchr(q->path,'/');name=name?name+1:q->path;
            // 有章节名快照显示章节，旧记录回退“第 N 节”。/ Prefer the saved chapter title; legacy records fall back to the ordinal.
            char source[320];
            if(q->chapter_title[0]) snprintf(source,sizeof(source),"《%s》· %s · %u%%",name,q->chapter_title,q->pct);
            else snprintf(source,sizeof(source),"《%s》· 第 %u 节 · %u%%",name,(unsigned)q->chapter+1,q->pct);
            ui_product_title(fb,(EpdRect){qr.x+20,qr.y+102,qr.width-40,32},source,24,1);
        }
        if(!s_quote_count) ui_product_title(fb,(EpdRect){UI_MARGIN,370,ui_content_width(),130},
            "还没有摘录。阅读时长按想保存的句子，再点击「保存摘录」。",30,3);
        // 说明放在卡片列表与翻页行之间的空带：共用按钮(920)与每日目标行(1018)不再与文字重叠。
        // The note sits in the band between the card list and the nav row, clear of the shared button (920) and the goal row (1018).
        ui_product_title(fb,(EpdRect){UI_MARGIN,804,ui_content_width(),30},
            s_quote_error?s_quote_error:"金句保存原文与位置；书签继续在阅读目录中管理。",24,1);
        if(s_quote_page){ if(pressed_id(6)) ui_draw_pressed_round_rect(fb, quote_nav(0), UI_BTN_RADIUS); ui_draw_button(fb,quote_nav(0),"上一页",false); }
        if((s_quote_page+1)*3<s_quote_count){ if(pressed_id(7)) ui_draw_pressed_round_rect(fb, quote_nav(1), UI_BTN_RADIUS); ui_draw_button(fb,quote_nav(1),"下一页",false); }
        ui_product_title(fb,(EpdRect){UI_MARGIN+190,850,224,36},"点击摘录回到原文",24,1);

        if (pressed_id(3)) ui_draw_pressed_round_rect(fb, action_rect(), UI_BTN_RADIUS);
        ui_draw_button(fb, action_rect(), !s_loading && data->current.path[0] ? "打开正在读书籍" : "打开书架", true);
    }

    EpdRect goal_row = goal_entry_rect();
    if (pressed_id(8)) ui_draw_pressed_round_rect(fb, goal_row, UI_BTN_RADIUS);
    ui_draw_round_rect(fb, goal_row, UI_BTN_RADIUS, UI_GRAY_BLACK);
    char goal_note[128]; unsigned goal = app_settings_daily_goal();
    if (!goal) snprintf(goal_note, sizeof(goal_note), "每日目标：未设置 · 点击设置 ›");
    else if (!date_valid || s_loading) snprintf(goal_note, sizeof(goal_note), "每日目标 %u 分钟 · %s ›", goal, date_valid ? "加载中" : "未校时");
    else {
        unsigned actual = book_stats_minutes(today_key);
        snprintf(goal_note, sizeof(goal_note), "每日目标 %u 分钟 · 今日 %u/%u%s ›", goal, actual, goal, actual >= goal ? " 已达标" : "");
    }
    ui_product_title(fb, (EpdRect){goal_row.x + 16, goal_row.y + 14, goal_row.width - 32, 40}, goal_note, 26, 1);
    ui_product_root_bar(fb, OS_APP_TODAY);
    if (s_goal_open) draw_goal(fb);
}

static app_redraw_t navigate(app_ctx_t* ctx, os_app_id_t id, const char* path) {
    const app_desc_t* next = app_by_id(id);
    if (!next) return APP_REDRAW_NONE;
    if (id == OS_APP_LIBRARY && !book_entry_request(path ? BOOK_ENTRY_OPEN : BOOK_ENTRY_SHELF, path)) {
        return APP_REDRAW_NONE;
    }
    ctx->request_app = next;
    return APP_REDRAW_NONE;
}

// 仅命中实际动作；月历说明不借用开书触区。/ Hit actual actions only; calendar notes never borrow book-opening targets.
static int control_at(uint16_t x, uint16_t y, EpdRect* rect) {
    if (s_goal_open) {
        for (int i = 100; i < 110; ++i) if (ui_rect_hit(goal_control_rect(i), x, y)) {
            if (rect) *rect = goal_control_rect(i);
            return i;
        }
        return -1;
    }
    if (ui_rect_hit(ui_product_back_rect(),x,y)) { if(rect)*rect=ui_product_back_rect();return 9; }
    if (ui_rect_hit(goal_entry_rect(), x, y)) { if (rect) *rect = goal_entry_rect(); return 8; }
    for (int t = 0; t < 3; ++t)
        if (ui_rect_hit(tab_rect(t), x, y)) { if (rect) *rect = tab_rect(t); return t; }
    if (s_active_tab == JOURNAL_TAB_TODAY) {
        if (ui_rect_hit(footprint_rect(), x, y)) { if (rect) *rect = footprint_rect(); return 4; }
        if (ui_rect_hit(action_rect(), x, y)) { if (rect) *rect = action_rect(); return 3; }
    } else if (s_active_tab == JOURNAL_TAB_QUOTES) {
        for (unsigned row = 0; row < 3 && s_quote_page * 3 + row < s_quote_count; ++row) {
            EpdRect r = {UI_MARGIN, 352 + (int)row * 154, ui_content_width(), 138};
            if (ui_rect_hit(r, x, y)) { if (rect) *rect = r; return 10 + (int)row; }
        }
        if (s_quote_page && ui_rect_hit(quote_nav(0), x, y)) { if (rect) *rect = quote_nav(0); return 6; }
        if ((s_quote_page + 1) * 3 < s_quote_count && ui_rect_hit(quote_nav(1), x, y)) {
            if (rect) *rect = quote_nav(1);
            return 7;
        }
        if (ui_rect_hit(action_rect(), x, y)) { if (rect) *rect = action_rect(); return 3; }
    }
    return -1;
}

static app_redraw_t gesture(app_ctx_t* ctx, const ui_gesture_event_t* ev) {
    if (!s_goal_open && ui_product_root_press(ctx, ev)) return APP_REDRAW_NONE;
    EpdRect rect = {0};
    int id = control_at(ev->x0, ev->y0, &rect);
    if (ev->type == UI_GESTURE_PRESS)
        return id >= 0 ? ui_press_begin(ctx, &s_press, id, rect, render) : APP_REDRAW_NONE;
    if (ev->type != UI_GESTURE_TAP) return ui_press_restore(ctx, &s_press, render);
    int held = s_press.id;
    EpdRect held_rect = s_press.rect;
    ui_press_forget(&s_press);
    app_redraw_t result = APP_REDRAW_NONE;
    os_app_id_t root = ui_product_root_hit(ev->x0, ev->y0);
    if (!s_goal_open && root != OS_APP_NONE && root != OS_APP_TODAY && root == ui_product_root_hit(ev->x, ev->y))
        return navigate(ctx, root, NULL);
    const book_home_snapshot_t* data = book_home_snapshot();

    if (id >= 0 && id == control_at(ev->x, ev->y, NULL)) {
        if (id >= 100) {
            s_goal_error = false;
            if (id < 106) s_goal_candidate = s_goal_presets[id - 100];
            else if (id == 106 && s_goal_candidate >= 5) s_goal_candidate -= 5;
            else if (id == 107 && s_goal_candidate < 360) s_goal_candidate += 5;
            else if (id == 108) s_goal_open = false;
            else if (id == 109) {
                if (app_settings_set_daily_goal(s_goal_candidate)) s_goal_open = false;
                else s_goal_error = true;
            }
            result = APP_REDRAW_PAGE;
        } else if (id == 9) {
            ctx->request_app=app_by_id(OS_APP_HISTORY);
        } else if (id == 8) {
            s_goal_candidate = app_settings_daily_goal(); s_goal_open = true; s_goal_error = false;
            result = APP_REDRAW_PAGE;
        } else if (id < 3) {
            if (s_active_tab != (journal_tab_t)id) {
                s_active_tab = (journal_tab_t)id;
                result = APP_REDRAW_PAGE;
            }
        } else if (!s_loading) {
            if (id == 3) {
                result = navigate(ctx, OS_APP_LIBRARY, data->current.path[0] ? data->current.path : NULL);
            } else if (id == 4 && data->current.path[0]) {
                result = navigate(ctx, OS_APP_LIBRARY, data->current.path);
            } else if (id >= 10) {
                const book_quote_t* q = &s_quotes[s_quote_page * 3 + (id - 10)];
                if (!book_entry_request_position(q->path, q->chapter, q->byte_off)) {
                    s_quote_error = "原文来源不可用，请检查图书和 TF 卡";
                    result = APP_REDRAW_PAGE;
                } else ctx->request_app = app_by_id(OS_APP_LIBRARY);
            } else if (id == 6 && s_quote_page) {
                --s_quote_page;
                result = APP_REDRAW_PAGE;
            } else if (id == 7 && (s_quote_page + 1) * 3 < s_quote_count) {
                ++s_quote_page;
                result = APP_REDRAW_PAGE;
            }
        }
    }
    if (result == APP_REDRAW_NONE && held >= 0 && !ctx->request_app && !ctx->request_menu && !ctx->request_return)
        return ui_press_rewind(ctx, &s_press, held_rect, render);
    return result;
}

static app_redraw_t on_tick(app_ctx_t* ctx) {
    ui_press_settle(ctx, &s_press, ctx->now_ms);
    if (ctx->consumed) return APP_REDRAW_NONE;
    if (s_quote_revision != book_quotes_revision()) { quotes_reload(); return APP_REDRAW_PAGE; }
    if (s_revision != book_store_revision() || (!s_loading && !book_home_cached())) {
        s_revision = book_store_revision();
        s_started = false;
        s_loading = true;
    }
    if (s_loading) {
        if (!s_started) {
            if (!os_storage_probe_complete()) return APP_REDRAW_NONE;
            book_home_begin();
            s_started = true;
        }
        if (!book_home_step()) return APP_REDRAW_NONE;
        s_loading = false;
        return APP_REDRAW_PAGE;
    }
    os_time_poll(ctx->now_ms);
    uint32_t day = 0;
    (void)os_time_date_key(&day);
    if (day != s_drawn_day) { s_drawn_day = day; return APP_REDRAW_PAGE; }
    return APP_REDRAW_NONE;
}

const app_desc_t app_os_today = {
    .title = "手帐",
    .detail = "阅读足迹 · 统计 · 打卡",
    .enter_full = false, .clean_page = true,
    .render = render,
    .on_gesture = gesture,
    .on_tick = on_tick,
    .on_enter = on_enter,
    .on_exit = today_on_exit,
    .on_media_lost = on_media_lost,
};
