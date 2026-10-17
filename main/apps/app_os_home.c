/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 *
 * 中文：阅读优先首页。显示真实书封与进度、独立宽主按钮、单行统计和最近阅读；
 * 存储摘要与封面缓存跨页面复用，空态和加载态保留书架入口。
 * English: Reading-first home with real covers/progress, a separate wide primary
 * action, one stats row and recent reads. Snapshots/covers survive page changes;
 * empty/loading states retain access to the shelf.
 *
 * 冻结：render 纯绘图；无示例书或假统计；封面后台有界提取，tick 领取完整结果；显式点击才开书。
 * Frozen: Render only paints; no demo books or fake stats; covers load in a bounded worker and ticks collect complete results; open only on explicit action.
 * 冻结：用户反馈上传难点；上传进度按钮扩大并保留边缘容差，独立于继续阅读命中，上传已保存进度并在首页反馈，离页取消。
 * Frozen: The reported upload hit difficulty requires a larger upload-progress button with edge tolerance, separate from resume; upload saved progress with home feedback and cancel on exit.
 * 冻结修订：用户批准第一批 UI 改进；旧空态按钮被统计栏覆盖且续读过窄，现主按钮独占一行，统计只显示一次；上传保持 176×88 与 8px 容差。
 * Frozen revision: The user approves batch-one UI changes: overlapping empty actions and narrow resume require a separate full-width action and nonduplicated stats; upload retains 176×88 and 8px tolerance.
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
#include "ui_press.h"
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
static int64_t s_cover_retry_ms;

static bool home_prepare_sleep(void) {
    s_sync_requested = false;
    os_sync_job_request_cancel();
    return true;
}

static bool load_cover(int64_t now_ms) {
    const char* path = book_home_snapshot()->current.path;
    bool same = !strcmp(path, s_cover_path) && s_cover_revision == book_store_revision();
    if (same && (s_cover || now_ms < s_cover_retry_ms)) return false;
    uint8_t* gray = NULL;
    if (!book_cover_poll(path, &gray)) return false;
    free(s_cover); s_cover = gray;
    snprintf(s_cover_path, sizeof(s_cover_path), "%s", path);
    s_cover_revision = book_store_revision();
    s_cover_retry_ms = now_ms + 3000;
    return gray != NULL;
}

static EpdRect hero_rect(void) {
    return (EpdRect){UI_MARGIN, 150, ui_content_width(), 348};
}

static EpdRect continue_rect(void) {
    return (EpdRect){UI_MARGIN, 520, ui_content_width(), 96};
}

// 绘制与命中共用按钮边界，四字名称留足空间；比旧 156px 更宽，降低误触继续阅读的概率。
// Painting and hits share button bounds with room for the four-character label; wider than the old 156px to miss the resume button less.
static EpdRect sync_rect(void) {
    EpdRect card = hero_rect();
    return (EpdRect){card.x + card.width - 192, card.y + 242, 176, 88};
}

// 容差只覆盖上传按钮周围，不跨越下方主按钮。/ Upload tolerance stays clear of the primary action below.
static EpdRect sync_hit_rect(void) {
    EpdRect button = sync_rect();
    return (EpdRect){button.x - 8, button.y - 8, button.width + 16, button.height + 16};
}

// 有记录与空态共用主按钮槽位。/ Resume and empty states share the primary-action slot.
static EpdRect resume_rect(void) {
    return continue_rect();
}

static EpdRect recent_rect(unsigned i) {
    return (EpdRect){UI_MARGIN, 748 + (int)i * 86, ui_content_width(), 80};
}

static EpdRect all_books_rect(void) { return (EpdRect){ui_content_right() - 168, 688, 168, 56}; }
static EpdRect recent_nav_rect(bool next) { return (EpdRect){next ? ui_content_right() - 144 : UI_MARGIN, 1012, 144, 72}; }

static bool transfer_enabled(void) { return os_device()->transfer == OS_CAP_PRESENT; }

static ui_press_t s_press;
static bool pressed_id(int id) { return ui_press_active(&s_press) && s_press.id == id; }

// 仅按钮与最近阅读行有按下态；书封与统计保持原样。
// Only buttons and recent rows get press feedback; covers and stats stay unchanged.
static int decor_at(uint16_t x, uint16_t y, EpdRect* rect) {
    const book_home_snapshot_t* data = book_home_snapshot();
    if (data->recent_page && ui_rect_hit(recent_nav_rect(false), x, y)) {
        if (rect) *rect = recent_nav_rect(false);
        return 201;
    }
    if (data->recent_more && ui_rect_hit(recent_nav_rect(true), x, y)) {
        if (rect) *rect = recent_nav_rect(true);
        return 202;
    }
    if (data->current.path[0] && ui_rect_hit(sync_hit_rect(), x, y)) {
        if (rect) *rect = sync_rect();
        return 200;
    }
    if (data->current.path[0] && ui_rect_hit(resume_rect(), x, y)) {
        if (rect) *rect = resume_rect();
        return 210;
    }
    if (!data->current.path[0] && ui_rect_hit(continue_rect(), x, y)) {
        if (rect) *rect = continue_rect();
        return 0;
    }
    if (ui_rect_hit(all_books_rect(), x, y)) {
        if (rect) *rect = all_books_rect();
        return 100;
    }
    for (unsigned i = 0; i < BOOK_HOME_RECENT_MAX && i < data->recent_count; ++i)
        if (ui_rect_hit(recent_rect(i), x, y)) {
            if (rect) *rect = recent_rect(i);
            return 10 + (int)i;
        }
    return -1;
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
    s_cover_retry_ms = 0;
    s_sync_note[0] = 0;
    ui_press_reset(&s_press);
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

    /* ---- 页眉 / Header ---- */
    ui_text(fb, UI_MARGIN, 10, 24, "小纸 Pico", EPD_DRAW_ALIGN_LEFT, false);
    EpdRect tag = {UI_MARGIN + 116, 10, 60, 24};
    ui_draw_round_rect(fb, tag, 4, UI_GRAY_BLACK);
    ui_text_vc(fb, tag.x + tag.width / 2, tag.y + tag.height / 2, 16,
               s_loading ? "检测" : data->degraded ? "内置" : "TF 卡", EPD_DRAW_ALIGN_CENTER, false);
    char clock[16], status[48];
    os_time_format_clock(clock, sizeof(clock));
    int battery = os_time_battery_permille();
    if (battery > 0) snprintf(status, sizeof(status), "%s · %d%%", clock, battery / 10);
    else snprintf(status, sizeof(status), "%s", clock);
    ui_text(fb, ui_content_right(), 10, 22, status, EPD_DRAW_ALIGN_RIGHT, false);
    ui_hairline(fb, 38, 0, UI_LOCK_WIDTH, UI_GRAY_LIGHT);
    ui_text(fb, UI_MARGIN, 48, 50, "正在读", EPD_DRAW_ALIGN_LEFT, false);
    ui_text(fb, UI_MARGIN, 108, 24, "继续上次，或开始一本新的书", EPD_DRAW_ALIGN_LEFT, false);

    /* ---- 当前图书与主操作 / Current book and primary action ---- */
    EpdRect hero = hero_rect();
    const char* primary;
    int primary_id;
    if (data->current.path[0]) {
        EpdRect cover = {hero.x + 14, hero.y + 14, 216, 320};
        if (s_cover && !strcmp(s_cover_path, data->current.path) &&
            s_cover_revision == book_store_revision()) ui_product_cover_bitmap(fb, cover, s_cover);
        else ui_product_cover(fb, cover, data->current.title, 36);
        int x = cover.x + cover.width + 18;
        int width = ui_content_right() - 16 - x;
        ui_product_title(fb, (EpdRect){x, 170, width, 124}, data->current.title, 44, 2);
        char progress[32];
        snprintf(progress, sizeof(progress), "已读 %lu%%", (unsigned long)data->current.percent);
        ui_text(fb, x, 324, 24, data->current.has_progress ? "已保存进度" : "未读", EPD_DRAW_ALIGN_LEFT, false);
        ui_text(fb, x + width, 324, 24, progress, EPD_DRAW_ALIGN_RIGHT, false);
        epd_fill_rect((EpdRect){x, 360, width, 6}, UI_GRAY_LIGHT, fb);
        unsigned percent = data->current.percent > 100 ? 100 : data->current.percent;
        if (percent) epd_fill_rect((EpdRect){x, 360, (int)percent * width / 100, 6}, UI_GRAY_BLACK, fb);
        EpdRect sync = sync_rect();
        if (pressed_id(200)) ui_draw_pressed_round_rect(fb, sync, 6);
        else ui_draw_round_rect(fb, sync, 6, UI_GRAY_BLACK);
        ui_text_vc(fb, sync.x + sync.width / 2, sync.y + sync.height / 2, 26,
                   s_sync_requested || os_sync_job_busy() ? "停止" : "上传进度", EPD_DRAW_ALIGN_CENTER, false);
        primary = "继续阅读";
        primary_id = 210;
    } else {
        ui_text(fb, hero.x + 24, hero.y + 40, 42,
                s_loading ? "正在查找最近阅读…" : "从一本书开始", EPD_DRAW_ALIGN_LEFT, false);
        ui_product_title(fb, (EpdRect){hero.x + 24, hero.y + 108, hero.width - 48, 84},
                         s_loading ? "可以先打开书架，或从菜单导入图书" : data->book_count
                             ? "书架已有图书，选一本开始阅读" : "还没有图书，先导入 TXT / EPUB", 30, 2);
        ui_product_title(fb, (EpdRect){hero.x + 24, hero.y + 220, hero.width - 48, 74},
                         "你的阅读进度和书签都会保存在设备上", 26, 2);
        primary = s_loading || data->book_count || !transfer_enabled() ? "打开书架" : "导入图书";
        primary_id = 0;
    }
    EpdRect button = continue_rect();
    bool down = pressed_id(primary_id);
    if (down) ui_draw_pressed_round_rect(fb, button, 6);
    else ui_fill_round_rect(fb, button, 6, UI_GRAY_BLACK);
    ui_text_vc(fb, button.x + button.width / 2, button.y + button.height / 2, 36,
               primary, EPD_DRAW_ALIGN_CENTER, !down);

    /* ---- 统计或操作反馈 / Stats or action feedback ---- */
    EpdRect summary = {UI_MARGIN, 628, ui_content_width(), 64};
    if (s_notice) ui_product_title(fb, summary, s_notice, 24, 2);
    else {
        char stats[128];
        uint32_t day = 0;
        if (s_loading) snprintf(stats, sizeof(stats), "正在统计图书与阅读记录…");
        else if (!os_time_date_key(&day)) snprintf(stats, sizeof(stats), "今日 — · 连续 — · %lu 本图书", (unsigned long)data->book_count);
        else snprintf(stats, sizeof(stats), "今日 %u 分钟 · 连续 %u 天 · %lu 本图书",
                      (unsigned)book_stats_minutes(day), compute_streak(day), (unsigned long)data->book_count);
        ui_product_title(fb, summary, stats, 26, 1);
    }

    /* ---- 最近阅读 / Recent reads ---- */
    ui_text(fb, UI_MARGIN, 704, 28, "最近阅读", EPD_DRAW_ALIGN_LEFT, false);
    char history[64];
    if (data->history_count) {
        snprintf(history, sizeof(history), "%u 条 · %u 页", data->history_count, data->recent_page + 1);
        ui_product_title(fb, (EpdRect){UI_MARGIN + 142, 708, 256, 30}, history, 22, 1);
    }
    EpdRect all = all_books_rect();
    if (pressed_id(100)) ui_draw_pressed_round_rect(fb, all, UI_CHIP_RADIUS);
    ui_text_vc(fb, all.x + all.width / 2, all.y + all.height / 2, 24,
               "全部图书 ›", EPD_DRAW_ALIGN_CENTER, false);
    for (unsigned i = 0; i < data->recent_count && i < BOOK_HOME_RECENT_MAX; ++i) {
        EpdRect row = recent_rect(i);
        if (pressed_id(10 + (int)i)) ui_draw_pressed_round_rect(fb, row, UI_CHIP_RADIUS);
        EpdRect cover = {row.x, row.y + 8, 44, 62};
        ui_fill_round_rect(fb, cover, 4, UI_GRAY_BLACK);
        char short_title[16];
        get_short_title(data->recent[i].title, short_title, sizeof(short_title));
        ui_text_vc(fb, cover.x + cover.width / 2, cover.y + cover.height / 2, 22,
                   short_title, EPD_DRAW_ALIGN_CENTER, true);
        ui_product_title(fb, (EpdRect){row.x + 56, row.y + 8, row.width - 160, 42}, data->recent[i].title, 30, 1);
        const char* extension = strrchr(data->recent[i].path, '.');
        ui_text(fb, row.x + 56, row.y + 48, 22,
                extension && !strcasecmp(extension, ".epub") ? "EPUB" : "TXT", EPD_DRAW_ALIGN_LEFT, false);
        char progress[16];
        snprintf(progress, sizeof(progress), "%lu%%", (unsigned long)data->recent[i].percent);
        ui_text_vc(fb, ui_content_right(), row.y + row.height / 2, 28, progress, EPD_DRAW_ALIGN_RIGHT, false);
        ui_hairline(fb, row.y + row.height, row.x, row.width, UI_GRAY_LIGHT);
    }
    if (!data->recent_count) ui_text(fb, UI_MARGIN, 764, 26,
                                   s_loading ? "正在查找阅读记录…" : "读过的图书会出现在这里", EPD_DRAW_ALIGN_LEFT, false);
    for (int next = 0; next < 2; ++next) {
        if (!(next ? data->recent_more : data->recent_page)) continue;
        EpdRect nav = recent_nav_rect(next != 0);
        if (pressed_id(201 + next)) ui_draw_pressed_round_rect(fb, nav, UI_BTN_RADIUS);
        ui_draw_button(fb, nav, next ? "下一页" : "上一页", false);
    }
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
    const book_home_snapshot_t* data = book_home_snapshot();
    if (data->recent_page && ui_rect_hit(recent_nav_rect(false), x, y)) return 201;
    if (data->recent_more && ui_rect_hit(recent_nav_rect(true), x, y)) return 202;
    if (data->current.path[0] && ui_rect_hit(sync_hit_rect(), x, y)) return 200;
    // 上传容差先命中；统计不命中，主按钮在各状态下独占槽位。
    // Upload tolerance wins first; stats stay inert and the primary slot is separate in every state.
    if (data->current.path[0] && ui_rect_hit(hero_rect(), x, y)) return 0;
    if (ui_rect_hit(continue_rect(), x, y)) return 0;
    if (ui_rect_hit(all_books_rect(), x, y)) return 100;
    for (unsigned i = 0; i < data->recent_count && i < BOOK_HOME_RECENT_MAX; ++i) {
        if (ui_rect_hit(recent_rect(i), x, y)) return (int)i + 1;
    }
    return -1;
}

static app_redraw_t on_gesture(app_ctx_t* ctx, const ui_gesture_event_t* event) {
    if (ui_product_root_press(ctx, event)) return APP_REDRAW_NONE;
    EpdRect rect = {0};
    int decor = decor_at(event->x0, event->y0, &rect);
    if (event->type == UI_GESTURE_PRESS)
        return decor >= 0 ? ui_press_begin(ctx, &s_press, decor, rect, render) : APP_REDRAW_NONE;
    if (event->type != UI_GESTURE_TAP) return ui_press_restore(ctx, &s_press, render);
    int held = s_press.id;
    EpdRect held_rect = s_press.rect;
    ui_press_forget(&s_press);
    os_app_id_t id = ui_product_root_hit(event->x0, event->y0);
    if (id == OS_APP_HOME) return APP_REDRAW_NONE;
    if (id != OS_APP_NONE && id == ui_product_root_hit(event->x, event->y))
        return navigate(ctx, id, NULL);
    int action = hit(event->x0, event->y0);
    if (action != hit(event->x, event->y)) {
        if (held >= 0) return ui_press_rewind(ctx, &s_press, held_rect, render);
        return APP_REDRAW_NONE;
    }
    const book_home_snapshot_t* data = book_home_snapshot();
    // 加载期间只允许明确书架入口，不打开尚未完成的最近记录。
    // During loading allow explicit shelf entries, not incomplete recent records.
    if (s_loading && !data->current.path[0] && action != 0 && action != 100) {
        if (held >= 0) return ui_press_rewind(ctx, &s_press, held_rect, render);
        return APP_REDRAW_NONE;
    }
    if (action == 201 || action == 202) {
        if (!s_loading && book_home_recent_move(action == 202 ? 1 : -1)) {
            s_started = true;
            s_loading = true;
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
        return navigate(ctx, s_loading || data->book_count || !transfer_enabled() ? OS_APP_LIBRARY : OS_APP_TRANSFER, NULL);
    }
    if (action > 0 && (unsigned)action <= data->recent_count)
        return navigate(ctx, OS_APP_LIBRARY, data->recent[action - 1].path);
    if (held >= 0) return ui_press_rewind(ctx, &s_press, held_rect, render);
    return APP_REDRAW_NONE;
}

static app_redraw_t on_key(app_ctx_t* ctx, int key) {
    if (key != UI_KEY_1 || (s_loading && !book_home_snapshot()->current.path[0])) return APP_REDRAW_NONE;
    const book_home_snapshot_t* data = book_home_snapshot();
    return navigate(ctx, OS_APP_LIBRARY, data->current.path[0] ? data->current.path : NULL);
}

static app_redraw_t on_tick(app_ctx_t* ctx) {
    ui_press_settle(ctx, &s_press, ctx->now_ms);
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
        return load_cover(ctx->now_ms) ? APP_REDRAW_PAGE : APP_REDRAW_NONE;
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
    load_cover(ctx->now_ms);
    return APP_REDRAW_PAGE;
}

const app_desc_t app_os_home = {
    .title = "正在读", .detail = "继续阅读 · 书架 · 导入", .enter_full = false, .clean_page = true,
    .on_enter = on_enter, .on_exit = home_on_exit, .on_media_lost = on_media_lost,
    .render = render, .on_gesture = on_gesture, .on_key = on_key, .on_tick = on_tick,
};
