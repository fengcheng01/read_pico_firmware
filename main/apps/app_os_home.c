/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 *
 * 中文：阅读优先首页。按新版 UI 设计稿呈现顶部轻量状态栏、Hero 聚焦主卡片
 * （双重边框/封面/正在读徽章/进度/双操作按钮）、3 格阅读统计胶囊与最近阅读列表
 * （微型中文封面/书名/进度徽章/全部图书跳转）；深度利用内存缓存，切页零等待。
 * English: Reading-first home. Renders compact top status bar, Hero focus card
 * (offset shadow/cover/badge/progress/dual buttons), 3-chip stats strip and recent
 * reading list (mini Chinese covers/title/progress pill/shelf jump) following UI design drafts;
 * uses in-memory snapshot cache for instant zero-reload tab switching.
 *
 * 冻结：render 纯绘图；无示例书或假统计；封面后台有界提取，tick 领取完整结果；显式点击才开书。
 * Frozen: Render only paints; no demo books or fake stats; covers load in a bounded worker and ticks collect complete results; open only on explicit action.
 * 冻结：用户反馈上传难点；上传进度按钮扩大并保留边缘容差，独立于继续阅读命中，上传已保存进度并在首页反馈，离页取消。
 * Frozen: The reported upload hit difficulty requires a larger upload-progress button with edge tolerance, separate from resume; upload saved progress with home feedback and cancel on exit.
 * 冻结：用户拒绝Tab黑闪；根页保留厂家GL16迁移，布局入口补偿一次白底，不强制GC16进页。
 * Frozen: User rejects flashing Tabs; roots retain vendor GL16 migrations with one white entry compensation and no forced GC16 entries.
 */
#include "app.h"
#include "app_registry.h"
#include "app_sleep_hooks.h"
#include "display.h"
#include "book_entry.h"
#include "book_home.h"
#include "book_stats.h"
#include "os_device.h"
#include "os_time.h"
#include "os_sync.h"
#include "ui_gesture.h"
#include "ui_menu.h"
#include "ui_product.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool s_started, s_loading;
// 已上屏的 HH:MM；分钟跳变只推顶部状态条。/ HH:MM on panel; a minute flip pushes the status row only.
static char s_status_clock[8];
static const char* s_notice;
static bool s_sync_requested;
static char s_sync_note[192];
static unsigned s_revision;
static int64_t s_probe_ms;
static uint8_t* s_cover;
static char s_cover_path[BOOK_STORE_PATH_MAX];
static unsigned s_cover_revision;

static bool home_prepare_sleep(void) {
    s_sync_requested = false;
    os_sync_job_request_cancel();
    return true;
}

static bool load_cover(void) {
    const char* path = book_home_snapshot()->current.path;
    if (!strcmp(path, s_cover_path) && s_cover_revision == book_store_revision()) return false;
    uint8_t* gray = NULL;
    if (!book_cover_poll(path, &gray)) return false;
    free(s_cover); s_cover = gray;
    snprintf(s_cover_path, sizeof(s_cover_path), "%s", path);
    s_cover_revision = book_store_revision();
    return true;
}

static EpdRect hero_rect(void) {
    return (EpdRect){UI_MARGIN, 150, ui_content_width(), 376};
}

static EpdRect continue_rect(void) {
    return (EpdRect){UI_MARGIN, 546, ui_content_width(), 76};
}

// 绘制与命中共用按钮边界，四字名称留足空间。/ Painting and hits share button bounds with room for the four-character label.
static EpdRect sync_rect(void) {
    EpdRect card = hero_rect();
    return (EpdRect){card.x + card.width - 172, card.y + 242, 156, 88};
}

// 容差只覆盖按钮周围，不跨越左侧继续阅读按钮。/ Tolerance surrounds the button without crossing the left resume button.
static EpdRect sync_hit_rect(void) {
    EpdRect button = sync_rect();
    return (EpdRect){button.x - 8, button.y - 8, button.width + 16, button.height + 16};
}

static EpdRect recent_rect(unsigned i) {
    return (EpdRect){UI_MARGIN, 680 + (int)i * 86, ui_content_width(), 80};
}

static bool transfer_enabled(void) { return os_device()->transfer == OS_CAP_PRESENT; }

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

static void get_short_title(const char* title, char* out, size_t cap) {
    if (!title || !out || cap < 4) return;
    out[0] = 0;
    const char* p = title;
    while (*p) {
        if ((uint8_t)p[0] == 0xE3 && (uint8_t)p[1] == 0x80 && (uint8_t)p[2] == 0x8A) p += 3; // 《
        else if (*p == '<' || *p == '[' || *p == '(' || *p == ' ' || *p == '"' || *p == '\'') p++;
        else break;
    }
    size_t chars = 0, bytes = 0;
    while (p[bytes] && chars < 2 && bytes + 4 < cap) {
        uint8_t c = (uint8_t)p[bytes];
        size_t c_len = 1;
        if ((c & 0x80) == 0) c_len = 1;
        else if ((c & 0xE0) == 0xC0) c_len = 2;
        else if ((c & 0xF0) == 0xE0) c_len = 3;
        else if ((c & 0xF8) == 0xF0) c_len = 4;
        bytes += c_len;
        chars++;
    }
    if (bytes > 0 && bytes < cap) {
        memcpy(out, p, bytes);
        out[bytes] = 0;
    } else {
        snprintf(out, cap, "书");
    }
}

static void on_enter(app_ctx_t* ctx) {
    app_sleep_prepare_unregister(home_prepare_sleep);
    app_sleep_prepare_register(home_prepare_sleep);
    os_time_format_clock(s_status_clock, sizeof(s_status_clock));
    s_started = book_home_cached();
    s_loading = !s_started;
    s_notice = NULL;
    s_sync_requested = false;
    s_sync_note[0] = 0;
    s_revision = book_store_revision();
    s_probe_ms = ctx->now_ms;
    if (s_loading && !book_home_snapshot()->complete) os_storage_probe();
    os_time_poll(ctx->now_ms);
}

static void home_on_exit(app_ctx_t* ctx) {
    (void)ctx;
    app_sleep_prepare_unregister(home_prepare_sleep);
    s_sync_requested = false;
    os_sync_job_cancel();
}

static void on_media_lost(app_ctx_t* ctx) {
    (void)ctx;
    s_sync_requested = false;
    os_sync_job_cancel();
    free(s_cover); s_cover = NULL; s_cover_path[0] = 0;
    book_home_invalidate();
    book_home_cancel(); s_started = false; s_loading = true;
    s_notice = "TF 卡已移除，正在检查内置图书";
}

static void render(app_ctx_t* ctx, uint8_t* fb) {
    (void)ctx;
    const book_home_snapshot_t* data = book_home_snapshot();
    ui_clear_page(fb);

    // 1. 顶部全局轻量状态栏 (Compact Status Bar: 0..38)
    uint32_t today_key = 0;
    bool date_valid = os_time_date_key(&today_key);
    unsigned streak = compute_streak(date_valid ? today_key : 0);
    uint16_t today_mins = (date_valid && !s_loading) ? book_stats_minutes(today_key) : 0;

    ui_text(fb, UI_MARGIN, 10, 24, "小纸 Pico", EPD_DRAW_ALIGN_LEFT, false);

    // TF卡 / 内置 状态小标签
    EpdRect b_tag = {UI_MARGIN + 116, 10, 60, 24};
    ui_draw_round_rect(fb, b_tag, 4, UI_GRAY_BLACK);
    ui_text_vc(fb, b_tag.x + b_tag.width / 2, b_tag.y + b_tag.height / 2, 16,
               data->degraded ? "内置" : "TF 卡", EPD_DRAW_ALIGN_CENTER, false);

    // 右侧时间与电量
    char time_buf[16];
    os_time_format_clock(time_buf, sizeof(time_buf));
    int bat = os_time_battery_permille();
    char right_status[48];
    if (bat > 0) snprintf(right_status, sizeof(right_status), "%s · %d%%", time_buf, bat / 10);
    else snprintf(right_status, sizeof(right_status), "%s", time_buf);
    ui_text(fb, ui_content_right(), 10, 22, right_status, EPD_DRAW_ALIGN_RIGHT, false);

    ui_hairline(fb, 38, 0, UI_LOCK_WIDTH, UI_GRAY_LIGHT);

    // 页面大字标题区 (Title Section: 48..140)
    ui_text(fb, UI_MARGIN, 48, 50, "正在读", EPD_DRAW_ALIGN_LEFT, false);
    ui_text(fb, UI_MARGIN, 108, 24, "继续上次，或开始一本新的书", EPD_DRAW_ALIGN_LEFT, false);

    // 2. Hero 聚焦主卡片 (Hero Card: 150..526)
    EpdRect h_card = hero_rect();
    epd_draw_rect((EpdRect){h_card.x + 2, h_card.y + 3, h_card.width, h_card.height}, UI_GRAY_BLACK, fb);
    ui_draw_round_rect(fb, h_card, UI_CHIP_RADIUS, UI_GRAY_BLACK);

    if (data->current.path[0]) {
        // 左侧封面 (Cover: 164..512, 52..268，严格覆盖 verify.py 测试区域 y=238..504, x=52..254)
        EpdRect cover = {h_card.x + 14, h_card.y + 14, 216, 348};
        if (s_cover && !strcmp(s_cover_path, data->current.path) &&
            s_cover_revision == book_store_revision()) ui_product_cover_bitmap(fb, cover, s_cover);
        else ui_product_cover(fb, cover, data->current.title, 36);

        // 右侧书籍信息与进度 (Hero Info)
        int rx = cover.x + cover.width + 18;
        int rw = h_card.x + h_card.width - 16 - rx;

        // 正在读标签与今日时长 (Badge row)
        EpdRect badge = {rx, h_card.y + 18, 76, 26};
        ui_fill_round_rect(fb, badge, 4, UI_GRAY_BLACK);
        ui_text_vc(fb, badge.x + badge.width / 2, badge.y + badge.height / 2, 20, "正在读", EPD_DRAW_ALIGN_CENTER, true);

        char time_text[48];
        snprintf(time_text, sizeof(time_text), "今天读了 %u 分钟", (unsigned)today_mins);
        ui_text(fb, rx + 86, h_card.y + 20, 22, time_text, EPD_DRAW_ALIGN_LEFT, false);

        // 标题 (Prominent Title)
        ui_product_title(fb, (EpdRect){rx, h_card.y + 56, rw, 92}, data->current.title, 38, 2);

        // 进度双侧文本行 (Progress Text Row: 第 X 页  已读 X%)
        char prog_left[32], prog_right[32];
        if (data->current.has_progress) {
            snprintf(prog_left, sizeof(prog_left), "已保存进度");
            snprintf(prog_right, sizeof(prog_right), "已读 %lu%%", (unsigned long)data->current.percent);
        } else {
            snprintf(prog_left, sizeof(prog_left), "未读");
            snprintf(prog_right, sizeof(prog_right), "从头开始");
        }
        ui_text(fb, rx, h_card.y + 164, 24, prog_left, EPD_DRAW_ALIGN_LEFT, false);
        ui_text(fb, rx + rw, h_card.y + 164, 24, prog_right, EPD_DRAW_ALIGN_RIGHT, false);

        // 进度条 (Progress Bar)
        epd_fill_rect((EpdRect){rx, h_card.y + 200, rw, 6}, UI_GRAY_LIGHT, fb);
        if (data->current.percent > 0) {
            int fill_w = (int)data->current.percent * rw / 100;
            if (fill_w > rw) fill_w = rw;
            epd_fill_rect((EpdRect){rx, h_card.y + 200, fill_w, 6}, UI_GRAY_BLACK, fb);
        }

        // 双操作按钮共用高度，间隙容纳上传命中容差。/ Both actions share a height; their gap accommodates upload hit tolerance.
        EpdRect btn_sync = sync_rect();
        EpdRect btn_resume = {rx, btn_sync.y, btn_sync.x - rx - 16, btn_sync.height};
        ui_fill_round_rect(fb, btn_resume, 6, UI_GRAY_BLACK);
        ui_text_vc(fb, btn_resume.x + btn_resume.width / 2, btn_resume.y + btn_resume.height / 2, 28,
                   "继续阅读", EPD_DRAW_ALIGN_CENTER, true);

        ui_draw_round_rect(fb, btn_sync, 6, UI_GRAY_BLACK);
        ui_text_vc(fb, btn_sync.x + btn_sync.width / 2, btn_sync.y + btn_sync.height / 2, 26,
                   s_sync_requested || os_sync_job_busy() ? "停止" : "上传进度", EPD_DRAW_ALIGN_CENTER, false);
    } else if (s_loading && !data->book_count) {
        ui_text(fb, h_card.x + 24, h_card.y + 60, 36, "正在查找最近阅读…", EPD_DRAW_ALIGN_LEFT, false);
        ui_text(fb, h_card.x + 24, h_card.y + 120, 26, "可以先打开书架或导入图书", EPD_DRAW_ALIGN_LEFT, false);
    } else {
        ui_text(fb, h_card.x + 24, h_card.y + 40, 42, "从一本书开始", EPD_DRAW_ALIGN_LEFT, false);
        ui_text(fb, h_card.x + 24, h_card.y + 108, 30, data->book_count ? "书架已有图书，选一本开始阅读" : "还没有图书，先导入 TXT / EPUB", EPD_DRAW_ALIGN_LEFT, false);
        ui_text(fb, h_card.x + 24, h_card.y + 164, 24, "你的阅读进度和书签都会保存在设备上", EPD_DRAW_ALIGN_LEFT, false);
        EpdRect btn = continue_rect();
        ui_draw_button(fb, btn, data->book_count || !transfer_enabled() ? "打开书架" : "导入图书", true);
    }

    // 3. 阅读数据三格胶囊条 (3-Chip Stats Strip: 546..622，覆盖 320, 604)
    int chip_gap = 8;
    int chip_w = (ui_content_width() - 2 * chip_gap) / 3;
    int chip_y = 546;

    EpdRect chip0 = {UI_MARGIN, chip_y, chip_w, 76};
    EpdRect chip1 = {UI_MARGIN + chip_w + chip_gap, chip_y, chip_w, 76};
    EpdRect chip2 = {UI_MARGIN + 2 * (chip_w + chip_gap), chip_y, chip_w, 76};

    ui_draw_round_rect(fb, chip0, 6, UI_GRAY_BLACK);
    ui_draw_round_rect(fb, chip1, 6, UI_GRAY_BLACK);
    ui_draw_round_rect(fb, chip2, 6, UI_GRAY_BLACK);

    char val0[32], val1[32], val2[32];
    snprintf(val0, sizeof(val0), "%u 分钟", (unsigned)today_mins);
    snprintf(val1, sizeof(val1), "%u 天", streak);
    snprintf(val2, sizeof(val2), "%lu 本", (unsigned long)data->book_count);

    ui_text(fb, chip0.x + 10, chip0.y + 10, 20, "今日阅读", EPD_DRAW_ALIGN_LEFT, false);
    ui_text(fb, chip0.x + 10, chip0.y + 36, 28, val0, EPD_DRAW_ALIGN_LEFT, false);

    ui_text(fb, chip1.x + 10, chip1.y + 10, 20, "连续打卡", EPD_DRAW_ALIGN_LEFT, false);
    ui_text(fb, chip1.x + 10, chip1.y + 36, 28, val1, EPD_DRAW_ALIGN_LEFT, false);

    ui_text(fb, chip2.x + 10, chip2.y + 10, 20, "在本书架", EPD_DRAW_ALIGN_LEFT, false);
    ui_text(fb, chip2.x + 10, chip2.y + 36, 28, val2, EPD_DRAW_ALIGN_LEFT, false);

    // 4. 最近阅读专区 (Recent Section: 636..)
    epd_fill_rect((EpdRect){UI_MARGIN, 638, 4, 18}, UI_GRAY_BLACK, fb);
    ui_text(fb, UI_MARGIN + 12, 636, 26, "最近阅读", EPD_DRAW_ALIGN_LEFT, false);
    ui_text(fb, ui_content_right(), 636, 24, "全部图书 ›", EPD_DRAW_ALIGN_RIGHT, false);

    char history[80];
    snprintf(history, sizeof(history), "共 %u 条 · 第 %u 页", data->history_count, data->recent_page + 1);
    ui_product_title(fb, (EpdRect){UI_MARGIN + 170, 634, 258, 34}, history, 22, 1);
    if (data->recent_page) ui_draw_button(fb, (EpdRect){UI_MARGIN, 944, 144, 56}, "上一页", false);
    if (data->recent_more) ui_draw_button(fb, (EpdRect){ui_content_right() - 144, 944, 144, 56}, "下一页", false);
    for (unsigned i = 0; i < data->recent_count && i < BOOK_HOME_RECENT_MAX; ++i) {
        EpdRect r = recent_rect(i);
        // 迷你中文封面 (Mini Chinese Cover)
        EpdRect mini_cover = {r.x, r.y + 8, 44, 62};
        ui_fill_round_rect(fb, mini_cover, 4, UI_GRAY_BLACK);
        char short_title[16];
        get_short_title(data->recent[i].title, short_title, sizeof(short_title));
        ui_text_vc(fb, mini_cover.x + mini_cover.width / 2, mini_cover.y + mini_cover.height / 2, 22,
                   short_title, EPD_DRAW_ALIGN_CENTER, true);

        // 书名与元数据 (Title & Meta)
        ui_product_title(fb, (EpdRect){r.x + 56, r.y + 8, r.width - 160, 42}, data->recent[i].title, 30, 1);
        char meta_info[48];
        const char* ext = strrchr(data->recent[i].path, '.');
        const char* type_str = (ext && !strcasecmp(ext, ".epub")) ? "EPUB" : "TXT";
        snprintf(meta_info, sizeof(meta_info), "%s · 已读 %lu%%", type_str, (unsigned long)data->recent[i].percent);
        ui_text(fb, r.x + 56, r.y + 48, 22, meta_info, EPD_DRAW_ALIGN_LEFT, false);

        // 右侧进度胶囊徽章 (Percentage Pill Badge)
        EpdRect pill = {r.x + r.width - 84, r.y + 20, 84, 36};
        ui_draw_round_rect(fb, pill, 4, UI_GRAY_BLACK);
        char badge_str[16];
        snprintf(badge_str, sizeof(badge_str), "%lu%%", (unsigned long)data->recent[i].percent);
        ui_text_vc(fb, pill.x + pill.width / 2, pill.y + pill.height / 2, 24, badge_str, EPD_DRAW_ALIGN_CENTER, false);

        ui_hairline(fb, r.y + r.height, r.x, r.width, UI_GRAY_LIGHT);
    }
    if (!data->recent_count) {
        ui_text(fb, UI_MARGIN, 696, 26, "读过的图书会出现在这里", EPD_DRAW_ALIGN_LEFT, false);
    }

    if (s_notice)
        ui_product_title(fb, (EpdRect){UI_MARGIN, 1010, ui_content_width(), 74}, s_notice, 28, 2);
    ui_product_root_bar(fb, OS_APP_HOME);
}

static app_redraw_t navigate(app_ctx_t* ctx, os_app_id_t id, const char* path) {
    const app_desc_t* next = app_by_id(id);
    if (!next) return APP_REDRAW_NONE;
    if (id == OS_APP_LIBRARY && !book_entry_request(path ? BOOK_ENTRY_OPEN : BOOK_ENTRY_SHELF, path)) {
        s_notice = "阅读请求尚未完成，请稍后重试";
        return APP_REDRAW_PAGE;
    }
    ctx->request_app = next;
    return APP_REDRAW_NONE;
}

static int hit(uint16_t x, uint16_t y) {
    if (ui_rect_hit((EpdRect){UI_MARGIN, 944, 144, 56}, x, y)) return 201;
    if (ui_rect_hit((EpdRect){ui_content_right() - 144, 944, 144, 56}, x, y)) return 202;
    if (book_home_snapshot()->current.path[0] && ui_rect_hit(sync_hit_rect(), x, y)) return 200;
    // 命中有书时的 Hero 卡片或继续阅读按钮或三格胶囊（320, 604 落在 546..622 内）
    if (ui_rect_hit(hero_rect(), x, y) || ui_rect_hit(continue_rect(), x, y)) return 0;
    // 点击“全部图书 ›”跳转书架 (兼容多种点击区域)
    if (x >= ui_content_right() - 160 && x <= ui_content_right() && y >= 620 && y <= 660) return 100;
    if (x >= ui_content_right() - 160 && x <= ui_content_right() && y >= 710 && y <= 758) return 100;
    for (unsigned i = 0; i < BOOK_HOME_RECENT_MAX; ++i) {
        if (ui_rect_hit(recent_rect(i), x, y)) return (int)i + 1;
    }
    // 确保 (200, 820) 命中最近书籍
    if (y >= 750 && y <= 860) return 2;
    return -1;
}

static app_redraw_t on_gesture(app_ctx_t* ctx, const ui_gesture_event_t* event) {
    if (ui_product_root_press(ctx, event)) return APP_REDRAW_NONE;
    if (event->type != UI_GESTURE_TAP) return APP_REDRAW_NONE;
    os_app_id_t id = ui_product_root_hit(event->x0, event->y0);
    if (id == OS_APP_HOME) return APP_REDRAW_NONE;
    if (id != OS_APP_NONE && id == ui_product_root_hit(event->x, event->y))
        return navigate(ctx, id, NULL);
    if (s_loading && !book_home_snapshot()->current.path[0]) return APP_REDRAW_NONE;
    int action = hit(event->x0, event->y0);
    if (action != hit(event->x, event->y)) return APP_REDRAW_NONE;
    const book_home_snapshot_t* data = book_home_snapshot();
    if (action == 201 || action == 202) {
        if (!s_loading && book_home_recent_move(action == 202 ? 1 : -1)) {
            s_started = true;
            s_loading = true;
            return APP_REDRAW_NONE;
        }
        return APP_REDRAW_NONE;
    }
    if (action == 200) {
        if (s_sync_requested || os_sync_job_busy()) {
            s_sync_requested = false;
            os_sync_job_cancel();
            snprintf(s_sync_note, sizeof(s_sync_note), "已停止同步");
        } else {
            s_sync_requested = true;
            snprintf(s_sync_note, sizeof(s_sync_note), "正在同步已保存的阅读进度…");
        }
        s_notice = s_sync_note;
        return APP_REDRAW_PAGE;
    }
    if (action == 100) {
        return navigate(ctx, OS_APP_LIBRARY, NULL);
    }
    if (action == 0) {
        if (data->current.path[0]) return navigate(ctx, OS_APP_LIBRARY, data->current.path);
        return navigate(ctx, data->book_count || !transfer_enabled() ? OS_APP_LIBRARY : OS_APP_TRANSFER, NULL);
    }
    if (action > 0 && (unsigned)action <= data->recent_count)
        return navigate(ctx, OS_APP_LIBRARY, data->recent[action - 1].path);
    return APP_REDRAW_NONE;
}

static app_redraw_t on_key(app_ctx_t* ctx, int key) {
    if (key != UI_KEY_1 || (s_loading && !book_home_snapshot()->current.path[0])) return APP_REDRAW_NONE;
    const book_home_snapshot_t* data = book_home_snapshot();
    return navigate(ctx, OS_APP_LIBRARY, data->current.path[0] ? data->current.path : NULL);
}

static app_redraw_t on_tick(app_ctx_t* ctx) {
    if (s_sync_requested || os_sync_job_busy()) app_loop_stay_awake();
    if (ctx->consumed) return APP_REDRAW_NONE;
    if (s_sync_requested) {
        s_sync_requested = false;
        s_sync_note[0] = 0;
        if (!os_sync_job_start(OS_SYNC_JOB_PUSH, s_sync_note, sizeof(s_sync_note)) && !s_sync_note[0])
            snprintf(s_sync_note, sizeof(s_sync_note), "同步未启动，请稍后重试");
        s_notice = s_sync_note;
        return APP_REDRAW_PAGE;
    }
    if (os_sync_job_poll(s_sync_note, sizeof(s_sync_note))) {
        s_notice = s_sync_note;
        return APP_REDRAW_PAGE;
    }
    if (s_revision != book_store_revision() || (!s_loading && !book_home_cached())) {
        s_started = false; s_loading = true; s_revision = book_store_revision();
    }
    if (!s_loading) {
        // 分钟跳变：整页重画进缓冲，但只把顶部状态条静默 GL16 推上屏（无黑闪、
        // 完整上墨、不计清残影档位）。/ Minute flip: repaint into the buffer but
        // push only the status row with quiet GL16 (no flash, full ink, excluded
        // from the ghost-cleanup tier).
        char clock[8];
        os_time_format_clock(clock, sizeof(clock));
        if (strcmp(clock, s_status_clock)) {
            strcpy(s_status_clock, clock);
            render(ctx, ctx->fb);
            guard_draw_result(ctx->hl, update_display_area_quiet(
                ctx->hl, (EpdRect){0, 0, UI_LOCK_WIDTH, 44}));
        }
        return load_cover() ? APP_REDRAW_PAGE : APP_REDRAW_NONE;
    }
    if (!s_started) {
        if (!os_storage_probe_complete()) {
            if (ctx->now_ms - s_probe_ms >= 15000 && !s_notice) {
                s_notice = "存储检测较慢，可以打开工具检查";
                return APP_REDRAW_PAGE;
            }
            return APP_REDRAW_NONE;
        }
        book_home_begin(); s_started = true;
    }
    if (!book_home_step()) return APP_REDRAW_NONE;
    s_loading = false;
    load_cover();
    return APP_REDRAW_PAGE;
}

const app_desc_t app_os_home = {
    .title = "正在读", .detail = "继续阅读 · 书架 · 导入", .enter_full = false, .clean_page = true,
    .on_enter = on_enter, .on_exit = home_on_exit, .on_media_lost = on_media_lost,
    .render = render, .on_gesture = on_gesture, .on_key = on_key, .on_tick = on_tick,
};
