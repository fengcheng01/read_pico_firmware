/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 *
 * 中文：设置下的阅读子页。默认字号、正文字体与实验晃动都是持久设置；
 * 字体列表委托原字体页，本页不重复实现选择器。
 * English: Reading subpage under Settings. Default size, body font and the
 * experimental shake are persisted settings; the font list delegates to the
 * existing picker instead of duplicating it here.
 *
 * 冻结：只写既有设置项，不越权改字体文件；render 只绘图；预览文本不冒充正文。
 * Frozen: Write only existing settings, never font files; render only paints;
 * the preview line is not presented as book prose.
 */
#include "app.h"
#include "app_registry.h"
#include "os_catalog.h"
#include "settings.h"
#include "ttf_font.h"
#include "ui_gesture.h"
#include "ui_menu.h"
#include "ui_product.h"
#include <stdio.h>
#include <string.h>

// 与 settings.h 的 36..72/步长 4 契约一致；页面不引用 app_book 私有宏。
// Matches the settings.h contract of 36..72 in steps of 4; pages never share app_book's private macros.
#define READING_PX_MIN 36
#define READING_PX_MAX 72

static EpdRect back_rect(void) { return (EpdRect){470, 68, 174, 68}; }
static EpdRect step_rect(int i) { return (EpdRect){352 + i * 154, 266, 138, 88}; }
static EpdRect font_list_rect(void) { return (EpdRect){UI_MARGIN, 690, 320, 84}; }
static EpdRect footer_row_rect(int i) { return (EpdRect){UI_MARGIN, 872 + i * 66, ui_content_width(), 58}; }

static void render(app_ctx_t* ctx, uint8_t* fb) {
    (void)ctx;
    ui_clear_page(fb);
    ui_product_header(fb, "阅读", "默认字号与字体");
    ui_draw_button(fb, back_rect(), "返回设置", false);
    ui_text(fb, UI_MARGIN, 214, 30, "默认字号（新打开的图书）", EPD_DRAW_ALIGN_LEFT, false);
    int px = app_settings_book_px();
    char value[16];
    snprintf(value, sizeof(value), "%d px", px);
    ui_text(fb, UI_MARGIN, 282, 56, value, EPD_DRAW_ALIGN_LEFT, false);
    ui_draw_button(fb, step_rect(0), "− 4", px > READING_PX_MIN);
    ui_draw_button(fb, step_rect(1), "+ 4", px < READING_PX_MAX);
    ui_text(fb, UI_MARGIN, 396, px, "落霞与孤鹜齐飞", EPD_DRAW_ALIGN_LEFT, false);
    ui_hairline(fb, 508, UI_MARGIN, ui_content_width(), UI_GRAY_BLACK);
    ui_text(fb, UI_MARGIN, 534, 30, "正文字体", EPD_DRAW_ALIGN_LEFT, false);
    const char* path = ttf_font_path();
    char name[96];
    if (ttf_font_is_builtin() || !path[0]) snprintf(name, sizeof(name), "内置字库 · 中文子集（可能缺字）");
    else {
        const char* base = strrchr(path, '/');
        snprintf(name, sizeof(name), "%s", base ? base + 1 : path);
    }
    ui_product_title(fb, (EpdRect){UI_MARGIN, 586, ui_content_width(), 44}, name, 32, 1);
    ui_text(fb, UI_MARGIN, 640, 24, "完整中文显示需要 TF 卡上的 TTF 字库", EPD_DRAW_ALIGN_LEFT, false);
    ui_draw_button(fb, font_list_rect(), "打开字体列表", true);
    ui_hairline(fb, 812, UI_MARGIN, ui_content_width(), UI_GRAY_BLACK);
    ui_text(fb, UI_MARGIN, 836, 30, "阅读状态栏（页脚）", EPD_DRAW_ALIGN_LEFT, false);
    static const char* labels[] = {"时钟", "电量", "进度条"};
    static bool (*getters[])(void) = {app_settings_footer_clock, app_settings_footer_battery, app_settings_footer_bar};
    for (int i = 0; i < 3; ++i) {
        EpdRect r = footer_row_rect(i);
        ui_text_vc(fb, r.x, r.y + r.height / 2, 30, labels[i], EPD_DRAW_ALIGN_LEFT, false);
        ui_draw_choice_round_rect(fb, (EpdRect){ui_content_right() - 52, r.y + r.height / 2 - 26, 52, 52}, 8, getters[i]());
        ui_hairline(fb, r.y + r.height, r.x, r.width, UI_GRAY_LIGHT);
    }
    ui_text(fb, UI_MARGIN, 1062, 22, "状态栏在正文页脚左侧显示时钟与电量；进度条可整体关闭。", EPD_DRAW_ALIGN_LEFT, false);
    ui_draw_menu_handle(fb, false);
}

static app_redraw_t gesture(app_ctx_t* ctx, const ui_gesture_event_t* ev) {
    if (ev->type != UI_GESTURE_TAP) return APP_REDRAW_NONE;
    if (ui_rect_hit(back_rect(), ev->x0, ev->y0) && ui_rect_hit(back_rect(), ev->x, ev->y)) {
        ctx->request_app = app_by_id(OS_APP_SETTINGS);
        return APP_REDRAW_NONE;
    }
    int px = app_settings_book_px();
    if (ui_rect_hit(step_rect(0), ev->x0, ev->y0) && ui_rect_hit(step_rect(0), ev->x, ev->y) && px > READING_PX_MIN) {
        app_settings_set_book_px((uint8_t)(px - 4));
        return APP_REDRAW_PAGE;
    }
    if (ui_rect_hit(step_rect(1), ev->x0, ev->y0) && ui_rect_hit(step_rect(1), ev->x, ev->y) && px < READING_PX_MAX) {
        app_settings_set_book_px((uint8_t)(px + 4));
        return APP_REDRAW_PAGE;
    }
    if (ui_rect_hit(font_list_rect(), ev->x0, ev->y0) && ui_rect_hit(font_list_rect(), ev->x, ev->y)) {
        ctx->request_app = app_by_id(OS_APP_FONTS);
        return APP_REDRAW_NONE;
    }
    for (int i = 0; i < 3; ++i) {
        EpdRect r = footer_row_rect(i);
        if (ui_rect_hit(r, ev->x0, ev->y0) && ui_rect_hit(r, ev->x, ev->y)) {
            if (i == 0) app_settings_set_footer_clock(!app_settings_footer_clock());
            else if (i == 1) app_settings_set_footer_battery(!app_settings_footer_battery());
            else app_settings_set_footer_bar(!app_settings_footer_bar());
            return APP_REDRAW_PAGE;
        }
    }
    return APP_REDRAW_NONE;
}

static app_redraw_t key(app_ctx_t* ctx, int key) {
    if (key == UI_KEY_1) ctx->request_app = app_by_id(OS_APP_SETTINGS);
    return APP_REDRAW_NONE;
}

const app_desc_t app_os_reading = {
    .title = "阅读与字体", .detail = "字号 · 字体 · 实验", .enter_full = true,
    .render = render, .on_gesture = gesture, .on_key = key,
};
