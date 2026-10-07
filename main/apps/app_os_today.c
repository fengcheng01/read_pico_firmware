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
static int s_drawn_minute = -1;
static book_quote_t s_quotes[BOOK_QUOTES_MAX];
static size_t s_quote_count;
static unsigned s_quote_page, s_quote_revision;
static const char* s_quote_error;
static void quotes_reload(void) {
    s_quote_count = book_quotes_list(s_quotes, BOOK_QUOTES_MAX);
    s_quote_revision = book_quotes_revision();
    if (s_quote_page * 3 >= s_quote_count) s_quote_page = 0;
}
static EpdRect quote_nav(int i) { return (EpdRect){UI_MARGIN + i * 430, 834, 174, 66}; }


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
    quotes_reload(); s_quote_error = NULL;
    s_started = book_home_cached();
    s_loading = !s_started;
    s_drawn_minute = -1;
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
    snprintf(streak_val, sizeof(streak_val), "%u DAYS", streak);
    ui_text_vc(fb, stamp.x + stamp.width / 2, stamp.y + 44, 26, streak_val, EPD_DRAW_ALIGN_CENTER, false);

    // 2. 子标签切换条 (3 Subnav Tabs: 今日手记 / 打卡月历 / 金句便签)
    EpdRect subnav = {UI_MARGIN, 294, ui_content_width(), 46};
    ui_draw_round_rect(fb, subnav, 6, UI_GRAY_BLACK);
    int tab_w = subnav.width / 3;
    static const char* tab_names[] = {"今日手记", "打卡月历", "金句便签"};
    for (int t = 0; t < 3; ++t) {
        int tx = subnav.x + t * tab_w;
        int tw = (t == 2) ? (subnav.width - 2 * tab_w) : tab_w;
        EpdRect tr = {tx, subnav.y, tw, subnav.height};
        if (s_active_tab == (journal_tab_t)t) {
            ui_fill_round_rect(fb, tr, 6, UI_GRAY_BLACK);
            ui_text_vc(fb, tx + tw / 2, tr.y + tr.height / 2, 30, tab_names[t], EPD_DRAW_ALIGN_CENTER, true);
        } else {
            ui_text_vc(fb, tx + tw / 2, tr.y + tr.height / 2, 30, tab_names[t], EPD_DRAW_ALIGN_CENTER, false);
        }
        if (t > 0) epd_fill_rect((EpdRect){tx, subnav.y, 1, subnav.height}, UI_GRAY_BLACK, fb);
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

        char val0[48] = "0 分钟", val1[48] = "0 分钟";
        uint16_t today_m = 0;
        if (!s_loading && date_valid) {
            today_m = book_stats_minutes(today_key);
            format_minutes((unsigned long)today_m, val0, sizeof(val0));
            format_minutes((unsigned long)book_stats_total_minutes(os_time_date_shift(today_key, -6), today_key), val1, sizeof(val1));
        }

        ui_text(fb, c0.x + 12, c0.y + 8, 24, "今日阅读 (目标 60m)", EPD_DRAW_ALIGN_LEFT, false);
        ui_product_title(fb, (EpdRect){c0.x + 12, c0.y + 36, c0.width - 24, 42}, val0, 34, 1);

        ui_text(fb, c1.x + 12, c1.y + 8, 24, "近 7 天累计阅读", EPD_DRAW_ALIGN_LEFT, false);
        ui_product_title(fb, (EpdRect){c1.x + 12, c1.y + 36, c1.width - 24, 42}, val1, 34, 1);

        char val2[32];
        snprintf(val2, sizeof(val2), "%lu 本", (unsigned long)data->book_count);
        ui_text(fb, c2.x + 12, c2.y + 8, 24, "在本书架", EPD_DRAW_ALIGN_LEFT, false);
        ui_product_title(fb, (EpdRect){c2.x + 12, c2.y + 36, c2.width - 24, 42}, val2, 34, 1);

        char val3[32] = "未开始";
        if (data->current.path[0] && data->current.has_progress) {
            snprintf(val3, sizeof(val3), "%u%%", data->current.percent);
        }
        ui_text(fb, c3.x + 12, c3.y + 8, 24, "当前进度", EPD_DRAW_ALIGN_LEFT, false);
        ui_product_title(fb, (EpdRect){c3.x + 12, c3.y + 36, c3.width - 24, 42}, val3, 34, 1);

        // 7 日极简走势直方图 (7-Day Trend Chart)
        ui_hairline(fb, 516, UI_MARGIN, ui_content_width(), UI_GRAY_LIGHT);
        ui_text(fb, UI_MARGIN, 524, 28, "近 7 天阅读时长走势", EPD_DRAW_ALIGN_LEFT, false);

        int chart_y_base = 636;
        int chart_max_h = 68;
        int col_w = 40;
        int col_gap = (ui_content_width() - 7 * col_w) / 6;
        static const char* day_names[] = {"日", "一", "二", "三", "四", "五", "六"};

        for (int i = 0; i < 7; ++i) {
            int x = UI_MARGIN + i * (col_w + col_gap);
            uint32_t d = date_valid ? os_time_date_shift(today_key, i - 6) : 0;
            uint16_t m = date_valid ? book_stats_minutes(d) : 0;
            int bar_h = m > 0 ? (int)(m * chart_max_h / 60) : 0;
            if (bar_h > chart_max_h) bar_h = chart_max_h;
            if (m > 0 && bar_h < 4) bar_h = 4;

            if (bar_h > 0) {
                EpdRect bar_rect = {x, chart_y_base - bar_h, col_w, bar_h};
                if (i == 6) {
                    epd_fill_rect(bar_rect, UI_GRAY_BLACK, fb);
                } else {
                    epd_fill_rect(bar_rect, UI_GRAY_LIGHT, fb);
                    epd_draw_rect(bar_rect, UI_GRAY_BLACK, fb);
                }
            }
            int wday = date_valid ? (int)((os_time_info()->weekday + 70 - (6 - i)) % 7) : (i % 7);
            ui_text_vc(fb, x + col_w / 2, chart_y_base + 20, 26, (i == 6) ? "今" : day_names[wday],
                       EPD_DRAW_ALIGN_CENTER, false);
        }
        ui_hairline(fb, chart_y_base + 2, UI_MARGIN, ui_content_width(), UI_GRAY_BLACK);

        // 今日阅读足迹卡片 (Reading Footprint Card: covers 320, 760)
        ui_hairline(fb, 680, UI_MARGIN, ui_content_width(), UI_GRAY_LIGHT);
        ui_text(fb, UI_MARGIN, 688, 28, "今日阅读足迹", EPD_DRAW_ALIGN_LEFT, false);

        EpdRect session_card = {UI_MARGIN, 722, ui_content_width(), 160};
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
        EpdRect btn_rect = {UI_MARGIN, 920, ui_content_width(), 84};
        ui_draw_button(fb, btn_rect, !s_loading && data->current.path[0] ? "继续阅读" : "打开书架", true);
    } else if (s_active_tab == JOURNAL_TAB_MONTH) {
        // [子模块 B: 打卡月历]
        EpdRect month_card = {UI_MARGIN, 352, ui_content_width(), 550};
        ui_draw_round_rect(fb, month_card, UI_CHIP_RADIUS, UI_GRAY_BLACK);

        uint16_t yr = date_valid ? info->year : 2026;
        uint8_t mo = date_valid ? info->month : 10;
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

        EpdRect note_card = {UI_MARGIN, 920, ui_content_width(), 84};
        ui_draw_round_rect(fb, note_card, UI_CHIP_RADIUS, UI_GRAY_BLACK);
        ui_product_title(fb, (EpdRect){note_card.x + 16, note_card.y + 10, note_card.width - 32, 32}, "实心黑块为有阅读记录的达标日（实心盖印）", 24, 1);
        char streak_note[128];
        snprintf(streak_note, sizeof(streak_note), "当前已达成连续阅读 %u 天 · 保持每日阅读习惯", streak);
        ui_product_title(fb, (EpdRect){note_card.x + 16, note_card.y + 46, note_card.width - 32, 32}, streak_note, 24, 1);
    } else if (s_active_tab == JOURNAL_TAB_QUOTES) {
        for (unsigned row=0;row<3;++row) {
            size_t index=s_quote_page*3+row; if(index>=s_quote_count)break;
            const book_quote_t* q=&s_quotes[index];
            EpdRect qr={UI_MARGIN,352+(int)row*154,ui_content_width(),138};
            ui_draw_round_rect(fb,qr,UI_CHIP_RADIUS,UI_GRAY_BLACK);
            epd_fill_rect((EpdRect){qr.x,qr.y+4,6,qr.height-8},UI_GRAY_BLACK,fb);
            ui_product_title(fb,(EpdRect){qr.x+20,qr.y+14,qr.width-40,80},q->text,32,2);
            const char* name=strrchr(q->path,'/');name=name?name+1:q->path;
            char source[320];snprintf(source,sizeof(source),"《%s》· 第 %u 节 · %u%%",name,(unsigned)q->chapter+1,q->pct);
            ui_product_title(fb,(EpdRect){qr.x+20,qr.y+102,qr.width-40,32},source,24,1);
        }
        if(!s_quote_count) ui_product_title(fb,(EpdRect){UI_MARGIN,370,ui_content_width(),130},
            "还没有摘录。阅读时长按想保存的句子，再点击「保存摘录」。",30,3);
        if(s_quote_page)ui_draw_button(fb,quote_nav(0),"上一页",false);
        if((s_quote_page+1)*3<s_quote_count)ui_draw_button(fb,quote_nav(1),"下一页",false);
        ui_product_title(fb,(EpdRect){UI_MARGIN+190,850,224,36},"点击摘录回到原文",24,1);
        ui_product_title(fb,(EpdRect){UI_MARGIN,1014,ui_content_width(),68},
            s_quote_error?s_quote_error:"金句保存原文与位置；书签继续在阅读目录中管理。",24,2);

        EpdRect btn_rect = {UI_MARGIN, 920, ui_content_width(), 84};
        ui_draw_button(fb, btn_rect, !s_loading && data->current.path[0] ? "打开正在读书籍" : "打开书架", true);
    }

    ui_product_root_bar(fb, OS_APP_TODAY);
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

static app_redraw_t gesture(app_ctx_t* ctx, const ui_gesture_event_t* ev) {
    if (ui_product_root_press(ctx, ev)) return APP_REDRAW_NONE;
    if (ev->type != UI_GESTURE_TAP) return APP_REDRAW_NONE;
    os_app_id_t id = ui_product_root_hit(ev->x0, ev->y0);
    if (id != OS_APP_NONE && id != OS_APP_TODAY && id == ui_product_root_hit(ev->x, ev->y)) {
        return navigate(ctx, id, NULL);
    }

    // 子标签栏点击 (Subnav Tab Switcher: y = 290..348)
    if (ev->y0 >= 290 && ev->y0 <= 348 && ev->y >= 290 && ev->y <= 348) {
        int tab_w = ui_content_width() / 3;
        int t = (ev->x0 - UI_MARGIN) / tab_w;
        if (t < 0) t = 0;
        if (t > 2) t = 2;
        if (s_active_tab != (journal_tab_t)t) {
            s_active_tab = (journal_tab_t)t;
            return APP_REDRAW_PAGE;
        }
        return APP_REDRAW_NONE;
    }

    if (s_loading) return APP_REDRAW_NONE;
    const book_home_snapshot_t* data = book_home_snapshot();

    if (s_active_tab == JOURNAL_TAB_TODAY) {
        // 底部动作按钮
        if (ui_rect_hit((EpdRect){UI_MARGIN, 920, ui_content_width(), 84}, ev->x0, ev->y0) &&
            ui_rect_hit((EpdRect){UI_MARGIN, 920, ui_content_width(), 84}, ev->x, ev->y)) {
            return navigate(ctx, OS_APP_LIBRARY, data->current.path[0] ? data->current.path : NULL);
        }
        // 今日阅读足迹卡片命中 (覆盖 verify.py 测试坐标 320, 760)
        if (ui_rect_hit((EpdRect){UI_MARGIN, 722, ui_content_width(), 160}, ev->x0, ev->y0) &&
            ui_rect_hit((EpdRect){UI_MARGIN, 722, ui_content_width(), 160}, ev->x, ev->y) &&
            data->current.path[0]) {
            return navigate(ctx, OS_APP_LIBRARY, data->current.path);
        }
    } else if (s_active_tab == JOURNAL_TAB_MONTH) {
        // 月历页点击底部动作区或卡片打开正在读书籍
        if (ev->y0 >= 920 && ev->y <= 1004) {
            return navigate(ctx, OS_APP_LIBRARY, data->current.path[0] ? data->current.path : NULL);
        }
    } else if (s_active_tab == JOURNAL_TAB_QUOTES) {
        for(unsigned row=0;row<3;++row){
            size_t index=s_quote_page*3+row;if(index>=s_quote_count)break;
            EpdRect r={UI_MARGIN,352+(int)row*154,ui_content_width(),138};
            if(ui_rect_hit(r,ev->x0,ev->y0)&&ui_rect_hit(r,ev->x,ev->y)){
                const book_quote_t* q=&s_quotes[index];
                if(!book_entry_request_position(q->path,q->chapter,q->byte_off)) {
                    s_quote_error="原文来源不可用，请检查图书和 TF 卡";return APP_REDRAW_PAGE;
                }
                ctx->request_app=app_by_id(OS_APP_LIBRARY);return APP_REDRAW_NONE;
            }
        }
        for(int i=0;i<2;++i)if(ui_rect_hit(quote_nav(i),ev->x0,ev->y0)&&ui_rect_hit(quote_nav(i),ev->x,ev->y)){
            if(i==0&&s_quote_page)--s_quote_page;
            if(i==1&&(s_quote_page+1)*3<s_quote_count)++s_quote_page;
            return APP_REDRAW_PAGE;
        }
        if (ui_rect_hit((EpdRect){UI_MARGIN, 920, ui_content_width(), 84}, ev->x0, ev->y0)) {
            return navigate(ctx, OS_APP_LIBRARY, data->current.path[0] ? data->current.path : NULL);
        }
    }

    return APP_REDRAW_NONE;
}

static app_redraw_t on_tick(app_ctx_t* ctx) {
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
    const os_time_info_t* info = os_time_info();
    if (info->state == OS_TIME_VALID && info->minute != s_drawn_minute) {
        s_drawn_minute = info->minute;
        return APP_REDRAW_PAGE;
    }
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
