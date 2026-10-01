/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 *
 * 中文：设置下的睡眠子页。选择锁屏后的电源模式（浅睡/深睡/关机）、浅睡
 * 拿起唤醒、锁屏样式（静态/时钟/日历/黄历）与锁屏密码入口；全部写入既有
 * NVS 设置，原演示睡眠页在诊断目录。
 * English: Sleep subpage under Settings. Picks the post-lock power mode
 * (light/deep/off), light-sleep pickup wake, the lock style (static/clock/
 * calendar/almanac) and the lock-PIN entry; everything writes the existing
 * NVS settings while the original demo page stays under diagnostics.
 *
 * 冻结：本页只改设置，不直接执行睡眠或关机；选择立即保存；render 只绘图；
 * 时钟/日历锁屏在未校时时自动回退静态图，不画假时间。
 * Frozen: This page only changes settings and never sleeps or powers off
 * directly; selections save at once; render only paints; clock/calendar faces
 * fall back to the static image while uncalibrated and never fake a time.
 */
#include "app.h"
#include "app_registry.h"
#include "os_catalog.h"
#include "settings.h"
#include "ui_gesture.h"
#include "ui_menu.h"
#include "ui_product.h"

static EpdRect back_rect(void) { return (EpdRect){470, 68, 174, 68}; }
static EpdRect mode_rect(int i) { return (EpdRect){UI_MARGIN, 240 + i * 108, ui_content_width(), 96}; }
static EpdRect pickup_rect(void) { return (EpdRect){UI_MARGIN, 636, ui_content_width(), 96}; }
static EpdRect style_rect(int i) { return ui_row_rect(i, 4, 810, 84); }
static EpdRect pin_rect(void) { return (EpdRect){UI_MARGIN, 936, ui_content_width(), 96}; }

static const struct {
    const char* title;
    const char* detail;
    app_sleep_mode_t mode;
} modes[] = {
    {"浅睡", "保留当前页面；按键或拿起唤醒，耗电较高", APP_SLEEP_LIGHT},
    {"深睡", "主控断电省电；短按电源键重新开机", APP_SLEEP_DEEP},
    {"关机", "锁屏即关机；长按电源键开机", APP_SLEEP_OFF},
};
static const char* style_names[] = {"静态图", "时钟", "日历", "黄历"};

static void render(app_ctx_t* ctx, uint8_t* fb) {
    (void)ctx;
    ui_clear_page(fb);
    ui_product_header(fb, "睡眠", "锁屏与休眠");
    ui_draw_button(fb, back_rect(), "返回设置", false);
    ui_text(fb, UI_MARGIN, 200, 30, "电源键短按锁屏后进入以下模式", EPD_DRAW_ALIGN_LEFT, false);
    app_sleep_mode_t current = app_settings_sleep_mode();
    for (unsigned i = 0; i < sizeof(modes) / sizeof(modes[0]); ++i) {
        EpdRect r = mode_rect(i);
        bool on = modes[i].mode == current;
        if (on) ui_draw_selected_round_rect(fb, r, UI_BTN_RADIUS);
        else ui_draw_round_rect(fb, r, UI_BTN_RADIUS, UI_GRAY_BLACK);
        ui_text(fb, r.x + 24, r.y + 10, 32, modes[i].title, EPD_DRAW_ALIGN_LEFT, false);
        ui_product_title(fb, (EpdRect){r.x + 24, r.y + 54, r.width - 140, 30}, modes[i].detail, 22, 1);
        ui_draw_choice_round_rect(fb, (EpdRect){r.x + r.width - 76, r.y + 22, 52, 52}, 8, on);
    }
    ui_hairline(fb, 572, UI_MARGIN, ui_content_width(), UI_GRAY_BLACK);
    ui_text(fb, UI_MARGIN, 596, 30, "唤醒", EPD_DRAW_ALIGN_LEFT, false);
    ui_text(fb, UI_MARGIN, 642, 32, "拿起唤醒（仅浅睡）", EPD_DRAW_ALIGN_LEFT, false);
    ui_product_title(fb, (EpdRect){UI_MARGIN, 690, ui_content_width() - 100, 30},
                     "浅睡中拿起设备直接回到原页", 22, 1);
    ui_draw_choice_round_rect(fb, (EpdRect){ui_content_right() - 60, 648, 52, 52}, 8, app_settings_pickup_wake());
    ui_hairline(fb, 744, UI_MARGIN, ui_content_width(), UI_GRAY_BLACK);
    ui_text(fb, UI_MARGIN, 768, 30, "锁屏样式", EPD_DRAW_ALIGN_LEFT, false);
    uint8_t style = app_settings_lock_style();
    for (int i = 0; i < 4; ++i) {
        EpdRect r = style_rect(i);
        if (style == (uint8_t)i) ui_draw_selected_round_rect(fb, r, UI_BTN_RADIUS);
        else ui_draw_round_rect(fb, r, UI_BTN_RADIUS, UI_GRAY_BLACK);
        ui_text_vc(fb, r.x + r.width / 2, r.y + r.height / 2, 26, style_names[i], EPD_DRAW_ALIGN_CENTER, false);
    }
    ui_hairline(fb, 906, UI_MARGIN, ui_content_width(), UI_GRAY_BLACK);
    char pin[8];
    bool armed = app_settings_lock_pin(pin, sizeof(pin));
    ui_text(fb, UI_MARGIN, 942, 32, "锁屏密码", EPD_DRAW_ALIGN_LEFT, false);
    ui_product_title(fb, (EpdRect){UI_MARGIN, 988, ui_content_width() - 140, 30},
                     armed ? "开机需输入密码；可修改或清除" : "设置后开机需输入密码", 22, 1);
    ui_text_vc(fb, ui_content_right(), 970, 30, armed ? "已设置 ›" : "未设置 ›", EPD_DRAW_ALIGN_RIGHT, false);
    ui_hairline(fb, 1046, UI_MARGIN, ui_content_width(), UI_GRAY_BLACK);
    uint8_t idle = app_settings_idle_lock_min();
    ui_text_vc(fb, UI_MARGIN, 1072, 28, "空闲锁屏", EPD_DRAW_ALIGN_LEFT, false);
    ui_text_vc(fb, ui_content_right(), 1072, 26,
               idle == 0 ? "关" : idle == 5 ? "5 分钟" : idle == 10 ? "10 分钟" : "30 分钟",
               EPD_DRAW_ALIGN_RIGHT, false);
    ui_draw_menu_handle(fb, false);
}

static app_redraw_t gesture(app_ctx_t* ctx, const ui_gesture_event_t* ev) {
    if (ev->type != UI_GESTURE_TAP) return APP_REDRAW_NONE;
    if (ui_rect_hit(back_rect(), ev->x0, ev->y0) && ui_rect_hit(back_rect(), ev->x, ev->y)) {
        ctx->request_app = app_by_id(OS_APP_SETTINGS);
        return APP_REDRAW_NONE;
    }
    for (unsigned i = 0; i < sizeof(modes) / sizeof(modes[0]); ++i) {
        EpdRect r = mode_rect(i);
        if (ui_rect_hit(r, ev->x0, ev->y0) && ui_rect_hit(r, ev->x, ev->y)) {
            if (modes[i].mode != app_settings_sleep_mode()) {
                app_settings_set_sleep_mode(modes[i].mode);
                return APP_REDRAW_PAGE;
            }
            return APP_REDRAW_NONE;
        }
    }
    if (ui_rect_hit(pickup_rect(), ev->x0, ev->y0) && ui_rect_hit(pickup_rect(), ev->x, ev->y)) {
        app_settings_set_pickup_wake(!app_settings_pickup_wake());
        return APP_REDRAW_PAGE;
    }
    for (int i = 0; i < 4; ++i) {
        EpdRect r = style_rect(i);
        if (ui_rect_hit(r, ev->x0, ev->y0) && ui_rect_hit(r, ev->x, ev->y)) {
            if (app_settings_lock_style() != (uint8_t)i) {
                app_settings_set_lock_style((uint8_t)i);
                return APP_REDRAW_PAGE;
            }
            return APP_REDRAW_NONE;
        }
    }
    if (ui_rect_hit((EpdRect){UI_MARGIN, 1050, ui_content_width(), 46}, ev->x0, ev->y0) &&
        ui_rect_hit((EpdRect){UI_MARGIN, 1050, ui_content_width(), 46}, ev->x, ev->y)) {
        static const uint8_t steps[] = {0, 5, 10, 30};
        uint8_t idle = app_settings_idle_lock_min();
        unsigned at = 0;
        for (unsigned i = 0; i < sizeof(steps) / sizeof(steps[0]); ++i) if (steps[i] == idle) at = i;
        app_settings_set_idle_lock_min(steps[(at + 1) % (sizeof(steps) / sizeof(steps[0]))]);
        return APP_REDRAW_PAGE;
    }
    if (ui_rect_hit(pin_rect(), ev->x0, ev->y0) && ui_rect_hit(pin_rect(), ev->x, ev->y)) {
        const app_desc_t* pin_page = app_by_id(OS_APP_PIN);
        if (pin_page) ctx->request_app = pin_page;
        return APP_REDRAW_NONE;
    }
    return APP_REDRAW_NONE;
}

static app_redraw_t key(app_ctx_t* ctx, int key) {
    if (key == UI_KEY_1) ctx->request_app = app_by_id(OS_APP_SETTINGS);
    return APP_REDRAW_NONE;
}

const app_desc_t app_os_sleep = {
    .title = "睡眠与锁屏", .detail = "模式 · 唤醒 · 样式 · 密码", .enter_full = true,
    .render = render, .on_gesture = gesture, .on_key = key,
};
