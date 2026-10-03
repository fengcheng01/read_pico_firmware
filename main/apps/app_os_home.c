/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 * 中文：阅读优先首页，展示真实已保存进度并通过类型化入口续读。
 * English: Reading-first home, showing real saved progress and resuming via typed entry requests.
 * 冻结：render 纯绘图；无示例书或假统计；封面只在 tick 有界提取；显式点击才开书。
 * Frozen: Render only paints; no demo books or fake stats; covers load with bounded work in ticks; open only on explicit action.
 */
#include "app.h"
#include "app_registry.h"
#include "book_entry.h"
#include "book_home.h"
#include "os_device.h"
#include "ttf_font.h"
#include "ui_gesture.h"
#include "ui_menu.h"
#include "ui_product.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static bool s_started, s_loading;
static const char* s_notice;
static unsigned s_revision;
static int64_t s_probe_ms;
static uint8_t* s_cover;
static char s_cover_path[BOOK_STORE_PATH_MAX];
static unsigned s_cover_revision;
static bool load_cover(void) {
    const char* path = book_home_snapshot()->current.path;
    if (!strcmp(path, s_cover_path) && s_cover_revision == book_store_revision()) return false;
    free(s_cover); s_cover = NULL;
    snprintf(s_cover_path, sizeof(s_cover_path), "%s", path);
    s_cover_revision = book_store_revision();
    if (*path) (void)book_cover_load(path, &s_cover);
    return true;
}

static EpdRect continue_rect(void) {
    return (EpdRect){UI_MARGIN, 560, ui_content_width(), 88};
}
static EpdRect recent_rect(unsigned i) {
    return (EpdRect){UI_MARGIN, 772 + (int)i * 96, ui_content_width(), 90};
}
static bool transfer_enabled(void) { return os_device()->transfer == OS_CAP_PRESENT; }
static void on_enter(app_ctx_t* ctx) {
    book_home_cancel();
    s_started = book_home_cached(); s_loading = !s_started; s_notice = NULL;
    s_revision = book_store_revision(); s_probe_ms = ctx->now_ms;
    if (s_loading) os_storage_probe();

}
static void home_on_exit(app_ctx_t* ctx) { (void)ctx; book_home_cancel(); }
static void on_media_lost(app_ctx_t* ctx) {
    (void)ctx;
    free(s_cover); s_cover = NULL; s_cover_path[0] = 0;
    book_home_invalidate();
    book_home_cancel(); s_started = false; s_loading = true;
    s_notice = "TF 卡已移除，正在检查内置图书";
}
static void render(app_ctx_t* ctx, uint8_t* fb) {
    (void)ctx;
    const book_home_snapshot_t* data = book_home_snapshot();
    ui_clear_page(fb);
    ui_product_header(fb, "正在读", "继续上次，或开始一本新的书");
    if (s_loading) {
        ui_text(fb, UI_MARGIN, 240, 46, "正在查找最近阅读…", EPD_DRAW_ALIGN_LEFT, false);
        ui_text(fb, UI_MARGIN, 312, 34, "可以先打开书架或导入图书", EPD_DRAW_ALIGN_LEFT, false);
    } else if (data->current.path[0]) {
        EpdRect cover = {UI_MARGIN, 216, 224, 310};
        if (s_cover) ui_product_cover_bitmap(fb, cover, s_cover);
        else ui_product_cover(fb, cover, data->current.title, 40);
        int x = cover.x + cover.width + 38;
        ui_text(fb, x, 234, 32, "上次阅读", EPD_DRAW_ALIGN_LEFT, false);
        ui_product_title(fb, (EpdRect){x, 292, ui_content_right() - x, 180}, data->current.title, 48, 3);
        char progress[64];
        if (data->current.has_progress) snprintf(progress, sizeof(progress), "已读 %lu%%", (unsigned long)data->current.percent);
        else snprintf(progress, sizeof(progress), "从头开始");
        ui_text(fb, x, 468, 36, progress, EPD_DRAW_ALIGN_LEFT, false);
        epd_fill_rect((EpdRect){x, 518, ui_content_right() - x, 4}, UI_GRAY_LIGHT, fb);
        epd_fill_rect((EpdRect){x, 518, (ui_content_right() - x) * data->current.percent / 100, 4}, UI_GRAY_BLACK, fb);
        EpdRect button = continue_rect();
        ui_fill_round_rect(fb, button, UI_BTN_RADIUS, UI_GRAY_BLACK);
        ui_text_vc(fb, button.x + button.width / 2, button.y + button.height / 2, 40,
                   "继续阅读", EPD_DRAW_ALIGN_CENTER, true);
    } else {
        ui_text(fb, UI_MARGIN, 224, 48, "从一本书开始", EPD_DRAW_ALIGN_LEFT, false);
        ui_text(fb, UI_MARGIN, 316, 38, data->book_count ? "书架已有图书，选一本开始阅读" : "还没有图书，先导入 TXT / EPUB", EPD_DRAW_ALIGN_LEFT, false);
        ui_text(fb, UI_MARGIN, 380, 34, "你的阅读进度会保存在设备上", EPD_DRAW_ALIGN_LEFT, false);
        ui_draw_button(fb, continue_rect(), data->book_count || !transfer_enabled() ? "打开书架" : "导入图书", true);
    }
    const char* notice = s_notice ? s_notice : !s_loading && data->degraded ? "部分存储不可用，请到设置检查" :
                         "点击继续阅读，恢复已保存的位置";
    ui_product_title(fb, (EpdRect){UI_MARGIN, 676, ui_content_width(), 44}, notice, 32, 1);
    ui_hairline(fb, 736, UI_MARGIN, ui_content_width(), UI_GRAY_BLACK);
    char heading[80];
    snprintf(heading, sizeof(heading), "最近阅读%s", s_loading ? "" : " · 已保存记录");
    ui_text(fb, UI_MARGIN, 750, 32, heading, EPD_DRAW_ALIGN_LEFT, false);
    if (!s_loading) {
        for (unsigned i = 0; i < data->recent_count; ++i) {
            EpdRect r = recent_rect(i);
            // 标题与元数据分列，长文件名不覆盖进度。/ Separate title and metadata so long names cannot cover progress.
            ui_product_title(fb, (EpdRect){r.x, r.y + 22, r.width - 106, 54}, data->recent[i].title, 42, 1);
            char pct[12]; snprintf(pct, sizeof(pct), "%lu%%", (unsigned long)data->recent[i].percent);
            ui_text_vc(fb, r.x + r.width, r.y + 48, 34, pct, EPD_DRAW_ALIGN_RIGHT, false);
            ui_hairline(fb, r.y + 88, r.x, r.width, UI_GRAY_LIGHT);
        }
        if (!data->recent_count) ui_text(fb, UI_MARGIN, 836, 36, "读过的图书会出现在这里", EPD_DRAW_ALIGN_LEFT, false);
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
    if (ui_rect_hit(continue_rect(), x, y) || ui_rect_hit((EpdRect){UI_MARGIN, 216, ui_content_width(), 310}, x, y)) return 0;
    for (unsigned i = 0; i < BOOK_HOME_RECENT_MAX; ++i) if (ui_rect_hit(recent_rect(i), x, y)) return (int)i + 1;
    return -1;
}
static app_redraw_t on_gesture(app_ctx_t* ctx, const ui_gesture_event_t* event) {
    if (event->type != UI_GESTURE_TAP) return APP_REDRAW_NONE;
    os_app_id_t id = ui_product_root_hit(event->x0, event->y0);
    if (id == OS_APP_HOME) return APP_REDRAW_NONE;
    if (id != OS_APP_NONE && id == ui_product_root_hit(event->x, event->y))
        return navigate(ctx, id, NULL);
    if (s_loading) return APP_REDRAW_NONE;
    int action = hit(event->x0, event->y0);
    if (action != hit(event->x, event->y)) return APP_REDRAW_NONE;
    const book_home_snapshot_t* data = book_home_snapshot();
    if (action == 0) {
        if (data->current.path[0]) return navigate(ctx, OS_APP_LIBRARY, data->current.path);
        return navigate(ctx, data->book_count || !transfer_enabled() ? OS_APP_LIBRARY : OS_APP_TRANSFER, NULL);
    }
    if (action > 0 && (unsigned)action <= data->recent_count)
        return navigate(ctx, OS_APP_LIBRARY, data->recent[action - 1].path);
    return APP_REDRAW_NONE;
}
static app_redraw_t on_key(app_ctx_t* ctx, int key) {
    if (key != UI_KEY_1 || s_loading) return APP_REDRAW_NONE;
    const book_home_snapshot_t* data = book_home_snapshot();
    return navigate(ctx, OS_APP_LIBRARY, data->current.path[0] ? data->current.path : NULL);
}
static app_redraw_t on_tick(app_ctx_t* ctx) {
    if (ctx->consumed) return APP_REDRAW_NONE;
    if (s_revision != book_store_revision() || (!s_loading && !book_home_cached())) {
        book_home_cancel(); s_started = false; s_loading = true; s_revision = book_store_revision();
    }
    if (!s_loading) return load_cover() ? APP_REDRAW_PAGE : APP_REDRAW_NONE;
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
    .title = "正在读", .detail = "继续阅读 · 书架 · 导入", .enter_full = true,
    .on_enter = on_enter, .on_exit = home_on_exit, .on_media_lost = on_media_lost,
    .render = render, .on_gesture = on_gesture, .on_key = on_key, .on_tick = on_tick,
};
