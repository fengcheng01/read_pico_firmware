/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 *
 * 中文：设置下的时间子页。展示当前时间与校时状态，调整本地时区；自动校时
 * 依赖已有 WiFi 传书会话，本页不联网。
 * English: Time subpage under Settings. Shows the clock, calibration state and
 * adjusts the local timezone; auto-sync rides on STA transfer sessions and this
 * page never goes online.
 *
 * 冻结：不写 PMU 之外的任何电源域；未校时不给手动改日期的假精确；render 只绘图。
 * Frozen: No power domain beyond the PMU TIME_SYNC path; uncalibrated time offers
 * no fake manual precision; render only paints.
 */
#include "app.h"
#include "app_registry.h"
#include "os_catalog.h"
#include "os_time.h"
#include "ui_gesture.h"
#include "ui_menu.h"
#include "ui_product.h"
#include <stdio.h>

static EpdRect back_rect(void) { return ui_product_back_rect(); }
static EpdRect clock_rect(void) { return (EpdRect){UI_MARGIN, 200, ui_content_width(), 176}; }
static EpdRect tz_button_rect(int i) { return ui_row_rect(i, 2, 700, UI_BTN_H); }
static EpdRect sync_rect(void) { return (EpdRect){UI_MARGIN, 812, ui_content_width(), 72}; }

static int s_drawn_minute = -1;
static EpdRect s_area;

static void paint_clock(uint8_t* fb) {
    ui_clear_rect_fast(fb, clock_rect());
    char clock[16], date[48];
    os_time_format_clock(clock, sizeof(clock));
    os_time_format_date(date, sizeof(date));
    ui_text(fb, UI_MARGIN, 208, 84, clock, EPD_DRAW_ALIGN_LEFT, false);
    ui_text(fb, UI_MARGIN, 322, 30, date, EPD_DRAW_ALIGN_LEFT, false);
    s_drawn_minute = os_time_info()->state == OS_TIME_VALID ? os_time_info()->minute : -1;
}

static void render(app_ctx_t* ctx, uint8_t* fb) {
    (void)ctx;
    ui_clear_page(fb);
    ui_product_header(fb, "时间", "校时与时区");
    ui_draw_button(fb, back_rect(), "返回设置", false);
    paint_clock(fb);
    const os_time_info_t* info = os_time_info();
    ui_text(fb, UI_MARGIN, 408, 26, info->state == OS_TIME_VALID ? "已校时 · 走时由电源管理芯片维持" : "时间未校时 · 传书连接已有 WiFi 后自动校准",
            EPD_DRAW_ALIGN_LEFT, false);
    ui_hairline(fb, 462, UI_MARGIN, ui_content_width(), UI_GRAY_BLACK);
    ui_text(fb, UI_MARGIN, 492, 44, "时区", EPD_DRAW_ALIGN_LEFT, false);
    char tz[24];
    os_time_format_tz(info->tz_qh, tz, sizeof(tz));
    ui_text(fb, ui_content_right(), 500, 36, tz, EPD_DRAW_ALIGN_RIGHT, false);
    ui_product_title(fb, (EpdRect){UI_MARGIN, 560, ui_content_width(), 68},
                     "按 15 分钟步进调整，立即生效并保存。", 26, 2);
    ui_draw_button(fb, tz_button_rect(0), "− 一刻", true);
    ui_draw_button(fb, tz_button_rect(1), "+ 一刻", true);
    ui_hairline(fb, 800, UI_MARGIN, ui_content_width(), UI_GRAY_BLACK);
    ui_text_vc(fb, UI_MARGIN, 848, 32, "联网对时", EPD_DRAW_ALIGN_LEFT, false);
    ui_text_vc(fb, ui_content_right(), 848, 28, os_time_recently_synced() ? "已校准 ›" : "去传书页 ›", EPD_DRAW_ALIGN_RIGHT, false);
    ui_product_title(fb, (EpdRect){UI_MARGIN, 892, ui_content_width(), 96},
                     "在传书页连接已有 WiFi 后自动网络校时并写入硬件时钟，走时由电源管理芯片维持。",
                     24, 3);
    ui_draw_menu_handle(fb, false);
}

static app_redraw_t gesture(app_ctx_t* ctx, const ui_gesture_event_t* ev) {
    if (ev->type != UI_GESTURE_TAP) return APP_REDRAW_NONE;
    if (ui_rect_hit(back_rect(), ev->x0, ev->y0) && ui_rect_hit(back_rect(), ev->x, ev->y)) {
        ctx->request_app = app_by_id(OS_APP_SETTINGS);
        return APP_REDRAW_NONE;
    }
    if (ui_rect_hit(sync_rect(), ev->x0, ev->y0) && ui_rect_hit(sync_rect(), ev->x, ev->y)) {
        const app_desc_t* transfer = app_by_id(OS_APP_TRANSFER);
        if (transfer) ctx->request_app = transfer;
        return APP_REDRAW_NONE;
    }
    for (int i = 0; i < 2; ++i) {
        EpdRect r = tz_button_rect(i);
        if (ui_rect_hit(r, ev->x0, ev->y0) && ui_rect_hit(r, ev->x, ev->y)) {
            int16_t next = os_time_info()->tz_qh + (i ? 1 : -1);
            if (os_time_tz_valid(next)) {
                os_time_set_tz(next);
                return APP_REDRAW_PAGE;
            }
            return APP_REDRAW_NONE;
        }
    }
    return APP_REDRAW_NONE;
}

static app_redraw_t key(app_ctx_t* ctx, int key) {
    if (key == UI_KEY_1) ctx->request_app = app_by_id(OS_APP_SETTINGS);
    return APP_REDRAW_NONE;
}

static void on_enter(app_ctx_t* ctx) {
    // 进页先取一次时间，首帧就有时钟而不是未校时占位。/ Read time on entry so the first frame shows a clock, not the placeholder.
    os_time_poll(ctx->now_ms);
}

static app_redraw_t on_tick(app_ctx_t* ctx) {
    if (ctx->consumed) return APP_REDRAW_NONE;
    os_time_poll(ctx->now_ms);
    const os_time_info_t* info = os_time_info();
    if (info->state == OS_TIME_VALID && info->minute != s_drawn_minute) {
        paint_clock(ctx->fb);
        s_area = clock_rect();
        return APP_REDRAW_AREA;
    }
    return APP_REDRAW_NONE;
}

static EpdRect area_hint(app_ctx_t* ctx) { (void)ctx; return s_area; }

const app_desc_t app_os_time = {
    .title = "时间与时区", .detail = "校时 · 时区", .enter_full = true,
    .render = render, .on_gesture = gesture, .on_key = key,
    .on_enter = on_enter, .on_tick = on_tick, .area_hint = area_hint,
};
