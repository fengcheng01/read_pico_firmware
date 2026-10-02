/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 *
 * 中文：今日根页。时钟来自 PMU RTC 与用户时区，未校时显式可见；阅读摘要
 * 来自真实进度与本地阅读统计；用户批准以续读入口替代未实现待办区。
 * English: Today root. The clock comes from the PMU RTC with the user timezone
 * and stays visibly uncalibrated; the reading summary uses real progress and
 * local reading stats; the approved continue-reading action replaces the unimplemented todo area.
 *
 * 冻结：不伪造时间、任务或阅读数字；用户反馈残影后，分钟变化使用灰阶页刷新；render 只绘图。
 * Frozen: Never invent time, tasks or reading numbers; hardware feedback changes minute updates to grayscale pages; render only paints.
 */
#include "app.h"
#include "app_registry.h"
#include "book_entry.h"
#include "book_home.h"
#include "book_stats.h"
#include "os_device.h"
#include "os_time.h"
#include "ui_gesture.h"
#include "ui_product.h"
#include <stdio.h>

static bool s_started, s_loading;
static unsigned s_revision;
static int64_t s_probe_ms;
static int s_drawn_minute = -1;
static EpdRect s_area;

// 时钟带包含完整未校时说明，分钟变化交给灰阶页刷新。/ The clock band includes the full uncalibrated notice; minute changes use grayscale page presentation.
static EpdRect clock_rect(void) { return (EpdRect){UI_MARGIN, 200, ui_content_width(), 296}; }
static EpdRect reading_row_rect(int i) { return (EpdRect){UI_MARGIN, 604 + i * 64, ui_content_width(), 58}; }

static void paint_clock(uint8_t* fb) {
    ui_clear_rect_fast(fb, clock_rect());
    char clock[16], date[48];
    os_time_format_clock(clock, sizeof(clock));
    os_time_format_date(date, sizeof(date));
    ui_text(fb, UI_MARGIN, 214, 132, clock, EPD_DRAW_ALIGN_LEFT, false);
    ui_text(fb, UI_MARGIN, 414, 40, date, EPD_DRAW_ALIGN_LEFT, false);
    int permille = os_time_battery_permille();
    if (permille > 0) {
        char battery[20];
        snprintf(battery, sizeof(battery), "电量 %d%%", permille / 10);
        ui_text(fb, ui_content_right(), 420, 34, battery, EPD_DRAW_ALIGN_RIGHT, false);
    }
    if (os_time_info()->state != OS_TIME_VALID)
        ui_text(fb, UI_MARGIN, 462, 30, "传书连接已有 WiFi 后自动校准", EPD_DRAW_ALIGN_LEFT, false);
    s_drawn_minute = os_time_info()->state == OS_TIME_VALID ? os_time_info()->minute : -1;
}

static void format_minutes(unsigned long minutes, char* out, size_t cap) {
    if (minutes >= 60) snprintf(out, cap, "%lu 小时 %lu 分", minutes / 60, minutes % 60);
    else snprintf(out, cap, "%lu 分钟", minutes);
}

static void on_enter(app_ctx_t* ctx) {
    book_home_cancel();
    s_started = book_home_cached(); s_loading = !s_started; s_drawn_minute = -1;
    s_revision = book_store_revision(); s_probe_ms = ctx->now_ms;
    if (s_loading) os_storage_probe();
    os_time_poll(ctx->now_ms);
}
static void today_on_exit(app_ctx_t* ctx) { (void)ctx; book_home_cancel(); }
static void on_media_lost(app_ctx_t* ctx) {
    (void)ctx;
    book_home_invalidate();
    book_home_cancel(); s_started = false; s_loading = true;
}
static void render(app_ctx_t* ctx, uint8_t* fb) {
    (void)ctx;
    const book_home_snapshot_t* data = book_home_snapshot();
    ui_clear_page(fb);
    ui_product_header(fb, "今日", "时间与阅读");
    paint_clock(fb);
    ui_hairline(fb, 496, UI_MARGIN, ui_content_width(), UI_GRAY_BLACK);
    ui_text(fb, UI_MARGIN, 524, 44, "阅读", EPD_DRAW_ALIGN_LEFT, false);
    if (!s_loading) {
        uint32_t date = 0;
        bool date_valid = os_time_date_key(&date);
        char value[48];
        if (date_valid) format_minutes((unsigned long)book_stats_minutes(date), value, sizeof(value));
        else snprintf(value, sizeof(value), "时间未校时");
        ui_text(fb, UI_MARGIN, 614, 36, "今日阅读", EPD_DRAW_ALIGN_LEFT, false);
        ui_text(fb, ui_content_right(), 616, 38, value, EPD_DRAW_ALIGN_RIGHT, false);
        ui_hairline(fb, 662, UI_MARGIN, ui_content_width(), UI_GRAY_LIGHT);
        format_minutes((unsigned long)book_stats_total_minutes(date_valid ? os_time_date_shift(date, -6) : 0, date_valid ? date : 0),
                       value, sizeof(value));
        ui_text(fb, UI_MARGIN, 678, 36, "近 7 日", EPD_DRAW_ALIGN_LEFT, false);
        ui_text(fb, ui_content_right(), 680, 38, value, EPD_DRAW_ALIGN_RIGHT, false);
        ui_hairline(fb, 726, UI_MARGIN, ui_content_width(), UI_GRAY_LIGHT);
        if (data->current.path[0]) {
            char pct[16];
            snprintf(pct, sizeof(pct), "%u%%", data->current.percent);
            ui_product_title(fb, (EpdRect){UI_MARGIN, 744, ui_content_width() - 120, 52},
                             data->current.title, 40, 1);
            ui_text_vc(fb, ui_content_right(), 772, 36, data->current.has_progress ? pct : "未读", EPD_DRAW_ALIGN_RIGHT, false);
        } else {
            ui_text(fb, UI_MARGIN, 744, 36, data->book_count ? "书架已有图书，打开一本开始" : "还没有图书，先到设置导入", EPD_DRAW_ALIGN_LEFT, false);
        }
        ui_hairline(fb, 806, UI_MARGIN, ui_content_width(), UI_GRAY_LIGHT);
        char count[32];
        snprintf(count, sizeof(count), "%lu 本", (unsigned long)data->book_count);
        ui_text(fb, UI_MARGIN, 822, 36, "书架", EPD_DRAW_ALIGN_LEFT, false);
        ui_text(fb, ui_content_right(), 824, 38, count, EPD_DRAW_ALIGN_RIGHT, false);
    } else {
        ui_text(fb, UI_MARGIN, 614, 36, "正在查找最近阅读…", EPD_DRAW_ALIGN_LEFT, false);
    }
    ui_hairline(fb, 892, UI_MARGIN, ui_content_width(), UI_GRAY_BLACK);
    ui_draw_button(fb, (EpdRect){UI_MARGIN, 930, ui_content_width(), 88},
                   !s_loading && data->current.path[0] ? "继续阅读" : "打开书架", true);
    ui_product_root_bar(fb, OS_APP_TODAY);
}
static app_redraw_t navigate(app_ctx_t* ctx, os_app_id_t id, const char* path) {
    const app_desc_t* next = app_by_id(id);
    if (!next) return APP_REDRAW_NONE;
    if (id == OS_APP_LIBRARY && !book_entry_request(path ? BOOK_ENTRY_OPEN : BOOK_ENTRY_SHELF, path)) return APP_REDRAW_NONE;
    ctx->request_app = next;
    return APP_REDRAW_NONE;
}
static app_redraw_t gesture(app_ctx_t* ctx, const ui_gesture_event_t* ev) {
    if (ev->type != UI_GESTURE_TAP) return APP_REDRAW_NONE;
    os_app_id_t id = ui_product_root_hit(ev->x0, ev->y0);
    if (id != OS_APP_NONE && id != OS_APP_TODAY && id == ui_product_root_hit(ev->x, ev->y))
        return navigate(ctx, id, NULL);
    if (s_loading) return APP_REDRAW_NONE;
    const book_home_snapshot_t* data = book_home_snapshot();
    if (ui_rect_hit((EpdRect){UI_MARGIN, 930, ui_content_width(), 88}, ev->x0, ev->y0) &&
        ui_rect_hit((EpdRect){UI_MARGIN, 930, ui_content_width(), 88}, ev->x, ev->y))
        return navigate(ctx, OS_APP_LIBRARY, data->current.path[0] ? data->current.path : NULL);
    if (ui_rect_hit(reading_row_rect(2), ev->x0, ev->y0) && ui_rect_hit(reading_row_rect(2), ev->x, ev->y) &&
        data->current.path[0])
        return navigate(ctx, OS_APP_LIBRARY, data->current.path);
    if (ui_rect_hit(reading_row_rect(3), ev->x0, ev->y0) && ui_rect_hit(reading_row_rect(3), ev->x, ev->y))
        return navigate(ctx, OS_APP_LIBRARY, NULL);
    return APP_REDRAW_NONE;
}
static app_redraw_t on_tick(app_ctx_t* ctx) {
    if (ctx->consumed) return APP_REDRAW_NONE;
    os_time_poll(ctx->now_ms);
    if (s_revision != book_store_revision() || (!s_loading && !book_home_cached())) {
        book_home_cancel(); s_started = false; s_loading = true; s_revision = book_store_revision();
    }
    if (s_loading) {
        if (!s_started) {
            if (!os_storage_probe_complete()) return APP_REDRAW_NONE;
            book_home_begin(); s_started = true;
        }
        if (!book_home_step()) return APP_REDRAW_NONE;
        s_loading = false;
        return APP_REDRAW_PAGE;
    }
    const os_time_info_t* info = os_time_info();
    if (info->state == OS_TIME_VALID && info->minute != s_drawn_minute) {
        // 分钟变化保留字形灰阶，统一出口周期清理。/ Preserve glyph grayscale on minute changes; the shared path counts cleanup intervals.
        paint_clock(ctx->fb);
        s_area = clock_rect();
        return APP_REDRAW_PAGE;
    }
    return APP_REDRAW_NONE;
}
static EpdRect area_hint(app_ctx_t* ctx) { (void)ctx; return s_area; }
const app_desc_t app_os_today = {
    .title = "今日", .detail = "时钟 · 阅读 · 待办", .enter_full = false,
    .render = render, .on_gesture = gesture, .on_tick = on_tick,
    .on_enter = on_enter, .on_exit = today_on_exit, .on_media_lost = on_media_lost,
    .area_hint = area_hint,
};
