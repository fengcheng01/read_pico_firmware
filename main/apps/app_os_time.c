/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 *
 * 中文：设置下的时间子页。展示当前时间与校时状态，调整本地时区；自动校时
 * 使用已保存 WiFi 按需对时，等待期间可返回，成功或离页即释放会话。
 * English: Time subpage under Settings. Shows the clock, calibration state and
 * adjusts the local timezone; on-demand saved-WiFi sync stays on this page and
 * releases its session on success, failure, exit or sleep.
 *
 * 冻结：用户要求直接联网对时；只启动 STA 网络，不启动传书服务；不写 PMU 之外的任何电源域；未校时不给手动改日期的假精确；render 只绘图。
 * Frozen: User-requested direct sync starts STA without the transfer server. No power domain beyond the PMU TIME_SYNC path; uncalibrated time offers
 * no fake manual precision; render only paints.
 */
#include "app.h"
#include "app_registry.h"
#include "app_loop.h"
#include "app_sleep_hooks.h"
#include "read_pico_transfer.h"
#include "os_catalog.h"
#include "os_time.h"
#include "settings.h"
#include "ui_gesture.h"
#include "ui_menu.h"
#include "ui_product.h"
#include "settings.h"
#include <stdio.h>
#include <string.h>

static EpdRect back_rect(void) { return ui_product_back_rect(); }
static EpdRect clock_rect(void) { return (EpdRect){UI_MARGIN, 200, ui_content_width(), 176}; }
static EpdRect tz_button_rect(int i) { return ui_row_rect(i, 2, 700, UI_BTN_H); }
static EpdRect sync_rect(void) { return (EpdRect){UI_MARGIN, 812, ui_content_width(), 72}; }
static EpdRect auto_rect(void) { return (EpdRect){UI_MARGIN, 1004, ui_content_width(), 80}; }

static int s_drawn_minute = -1;
static EpdRect s_area;
static bool s_sync_requested, s_sync_active, s_network_owned, s_sync_claimed;
static int64_t s_sync_started, s_service_ms;
static const char* s_sync_note;

static void stop_sync(void) {
    s_sync_requested = s_sync_active = false;
    if (s_network_owned) {
        os_time_network(false);
        read_pico_transfer_stop();
    }
    s_network_owned = false;
    if (s_sync_claimed) read_pico_transfer_release_sync();
    s_sync_claimed = false;
}
static bool prepare_sleep(void) {
    stop_sync();
    s_sync_note = "对时已停止，点击重试";
    return true;
}
static void time_on_exit(app_ctx_t* ctx) {
    (void)ctx;
    stop_sync();
    app_sleep_prepare_unregister(prepare_sleep);
}

static void start_sync(app_ctx_t* ctx) {
    s_sync_requested = false;
    char ssid[33];
    bool configured = false;
    // ssid 必须传真实缓冲：传 NULL 会直接 INVALID_ARG，已保存的 WiFi 也被误判为未配置。
    // A real ssid buffer is mandatory: NULL returns INVALID_ARG and misreports saved WiFi as unset.
    if (read_pico_transfer_get_saved_wifi(ssid, &configured) != ESP_OK || !configured) {
        s_sync_note = "请先配置 WiFi，再点击对时";
        return;
    }
    if (!read_pico_transfer_claim_sync()) { s_sync_note = "网络正忙，请稍后重试"; return; }
    s_sync_claimed = true;
    read_pico_transfer_status_t status;
    read_pico_transfer_get_status(&status);
    if (status.state != READ_PICO_TRANSFER_STOPPED) {
        s_sync_note = "网络正忙，请稍后重试";
        stop_sync(); return;
    }
    os_time_network(false);
    if (read_pico_transfer_start_saved_network() != ESP_OK) {
        s_sync_note = "连接失败，点击重试";
        stop_sync(); return;
    }
    s_network_owned = s_sync_active = true;
    s_sync_started = s_service_ms = ctx->now_ms;
    s_sync_note = "正在连接 WiFi…";
}


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
    char clock_status[128];
    os_time_clock_status(clock_status, sizeof(clock_status));
    ui_product_title(fb, (EpdRect){UI_MARGIN, 396, ui_content_width(), 64},
        info->state == OS_TIME_VALID ? clock_status : "时间未校时 · 点击联网对时", 24, 2);
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
    ui_text_vc(fb, ui_content_right(), 848, 28, s_sync_active || s_sync_requested ? "停止" : "立即对时 ›", EPD_DRAW_ALIGN_RIGHT, false);
    ui_product_title(fb, (EpdRect){UI_MARGIN, 892, ui_content_width(), 96},
                     s_sync_note ? s_sync_note : "使用已保存 WiFi 对时并写入硬件时钟，完成后自动断开。可随时返回设置。",
                     24, 3);
    ui_draw_button(fb, auto_rect(), app_settings_clock_auto() ? "锁屏自动校时：开启" : "锁屏自动校时：关闭", true);
    ui_draw_menu_handle(fb, false);
}

static app_redraw_t gesture(app_ctx_t* ctx, const ui_gesture_event_t* ev) {
    if (ev->type != UI_GESTURE_TAP) return APP_REDRAW_NONE;
    if (ui_rect_hit(auto_rect(), ev->x0, ev->y0) && ui_rect_hit(auto_rect(), ev->x, ev->y)) {
        app_settings_set_clock_auto(!app_settings_clock_auto());
        s_sync_note = NULL;
        return APP_REDRAW_PAGE;
    }
    if (ui_rect_hit(back_rect(), ev->x0, ev->y0) && ui_rect_hit(back_rect(), ev->x, ev->y)) {
        ctx->request_app = app_by_id(OS_APP_SETTINGS);
        return APP_REDRAW_NONE;
    }
    if (ui_rect_hit(sync_rect(), ev->x0, ev->y0) && ui_rect_hit(sync_rect(), ev->x, ev->y)) {
        if (s_sync_active || s_sync_requested) {
            stop_sync(); s_sync_note = "对时已停止，点击重试";
        } else {
            s_sync_requested = true;
            s_sync_note = "正在连接 WiFi…";
        }
        return APP_REDRAW_PAGE;
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
    stop_sync();
    s_sync_note = NULL;
    app_sleep_prepare_register(prepare_sleep);
    // 进页先取一次时间，首帧就有时钟而不是未校时占位。/ Read time on entry so the first frame shows a clock, not the placeholder.
    os_time_poll(ctx->now_ms);
}

static app_redraw_t on_tick(app_ctx_t* ctx) {
    if (s_sync_active || s_sync_requested) app_loop_stay_awake();
    if (ctx->consumed) return APP_REDRAW_NONE;
    if (s_sync_requested) { start_sync(ctx); return APP_REDRAW_PAGE; }
    if (s_sync_active) {
        if (ctx->now_ms - s_service_ms >= 500) {
            s_service_ms = ctx->now_ms;
            read_pico_transfer_service_poll();
        }
        read_pico_transfer_status_t status;
        read_pico_transfer_get_status(&status);
        os_time_network(status.mode == READ_PICO_TRANSFER_MODE_STA && status.network_ready);
        const char* note = status.network_ready ? "正在校准时间…" : "正在连接 WiFi…";
        if (os_time_recently_synced()) {
            s_sync_note = app_settings_clock_auto() ?
                "已校准 · WiFi 已断开。锁屏继续学习走时，完成后每六小时维护。" :
                "已校准 · WiFi 已断开。自动校时已关闭，需再次手动校时学习走时。";
            stop_sync(); return APP_REDRAW_PAGE;
        }
        if (status.state == READ_PICO_TRANSFER_ERROR || status.state == READ_PICO_TRANSFER_STOPPED ||
            ctx->now_ms - s_sync_started >= 30000) {
            s_sync_note = "对时失败，请检查网络后重试";
            stop_sync(); return APP_REDRAW_PAGE;
        }
        if (!s_sync_note || strcmp(note, s_sync_note)) {
            s_sync_note = note; return APP_REDRAW_PAGE;
        }
    }
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
    .title = "时间与时区", .detail = "校时 · 时区", .enter_full = false,
    .render = render, .on_gesture = gesture, .on_key = key,
    .on_enter = on_enter, .on_exit = time_on_exit, .on_tick = on_tick, .area_hint = area_hint,
};
