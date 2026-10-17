/*
 * SPDX-FileCopyrightText: 2026 mindreset
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
#include "ui_press.h"
#include <stdio.h>
#include <string.h>

// 与 settings.h 的 36..72/步长 4 契约一致；页面不引用 app_book 私有宏。
// Matches the settings.h contract of 36..72 in steps of 4; pages never share app_book's private macros.
#define READING_PX_MIN 36
#define READING_PX_MAX 72

static EpdRect back_rect(void) { return ui_product_back_rect(); }
static EpdRect step_rect(int i) { return (EpdRect){352 + i * 154, 266, 138, 88}; }
static EpdRect font_list_rect(void) { return (EpdRect){UI_MARGIN, 690, 320, 84}; }
// 四行开关收紧行距：标题 830、行 862 起步进 54，给底部提示留出一行。
// Four toggle rows with tighter steps: title at 830, rows from 862 in 54-px steps, leaving one hint line.
static EpdRect footer_row_rect(int i) { return (EpdRect){UI_MARGIN, 862 + i * 54, ui_content_width(), 50}; }

static ui_press_t s_press;
// 到界等拒绝提示；下一次成功动作清空。/ Rejection notes such as size limits; the next successful action clears them.
static char s_hint[64];

static void render(app_ctx_t* ctx, uint8_t* fb) {
    (void)ctx;
    ui_clear_page(fb);
    ui_product_header(fb, "阅读", "默认字号与字体");
    if (ui_press_active(&s_press) && s_press.id == 100) ui_draw_pressed_round_rect(fb, back_rect(), UI_BTN_RADIUS);
    ui_draw_button(fb, back_rect(), "返回设置", false);
    ui_text(fb, UI_MARGIN, 214, 30, "默认字号（新打开的图书）", EPD_DRAW_ALIGN_LEFT, false);
    int px = app_settings_book_px();
    char value[16];
    snprintf(value, sizeof(value), "%d px", px);
    ui_text(fb, UI_MARGIN, 282, 56, value, EPD_DRAW_ALIGN_LEFT, false);
    for (int i = 0; i < 2; ++i)
        if (ui_press_active(&s_press) && s_press.id == i)
            ui_draw_pressed_round_rect(fb, step_rect(i), UI_BTN_RADIUS);
    ui_draw_button(fb, step_rect(0), "− 4", px > READING_PX_MIN);
    ui_draw_button(fb, step_rect(1), "+ 4", px < READING_PX_MAX);
    ui_text(fb, UI_MARGIN, 396, px, "落霞与孤鹜齐飞", EPD_DRAW_ALIGN_LEFT, false);
    ui_hairline(fb, 508, UI_MARGIN, ui_content_width(), UI_GRAY_BLACK);
    ui_text(fb, UI_MARGIN, 534, 30, "正文字体", EPD_DRAW_ALIGN_LEFT, false);
    const char* path = ttf_font_path();
    char name[96];
    if (ttf_font_is_builtin() || !path[0]) snprintf(name, sizeof(name), "内置中文 · 6763 个常用汉字");
    else {
        const char* base = strrchr(path, '/');
        snprintf(name, sizeof(name), "%s", base ? base + 1 : path);
    }
    ui_product_title(fb, (EpdRect){UI_MARGIN, 586, ui_content_width(), 44}, name, 32, 1);
    ui_text(fb, UI_MARGIN, 640, 28, "生僻字或更多字体可从 TF 卡添加", EPD_DRAW_ALIGN_LEFT, false);
    if (ui_press_active(&s_press) && s_press.id == 2) ui_draw_pressed_round_rect(fb, font_list_rect(), UI_BTN_RADIUS);
    ui_draw_button(fb, font_list_rect(), "打开字体列表", true);
    ui_hairline(fb, 812, UI_MARGIN, ui_content_width(), UI_GRAY_BLACK);
    ui_text(fb, UI_MARGIN, 830, 30, "阅读状态栏（页脚）", EPD_DRAW_ALIGN_LEFT, false);
    static const char* labels[] = {"时钟", "电量", "阅读百分比", "剩余时间"};
    static bool (*getters[])(void) = {app_settings_footer_clock, app_settings_footer_battery, app_settings_footer_bar, app_settings_footer_time};
    for (int i = 0; i < 4; ++i) {
        EpdRect r = footer_row_rect(i);
        if (ui_press_active(&s_press) && s_press.id == 3 + i) ui_draw_pressed_round_rect(fb, r, UI_CHIP_RADIUS);
        ui_text_vc(fb, r.x, r.y + r.height / 2, 30, labels[i], EPD_DRAW_ALIGN_LEFT, false);
        ui_draw_choice_round_rect(fb, (EpdRect){ui_content_right() - 52, r.y + r.height / 2 - 26, 52, 52}, 8, getters[i]());
        ui_hairline(fb, r.y + r.height, r.x, r.width, UI_GRAY_LIGHT);
    }
    // 提示行复用页脚说明位置：拒绝时说明原因，平时是原说明文案。
    // The hint reuses the footer caption row: rejections explain themselves, otherwise the standing caption stays.
    ui_text(fb, UI_MARGIN, 1074, 22, s_hint[0] ? s_hint : "时钟、电量、百分比与剩余时间按需显示。", EPD_DRAW_ALIGN_LEFT, false);
    ui_draw_menu_handle(fb, false);
}

// 命中当前画出的控件；rect 可为 NULL。/ Hit drawn controls; rect may be NULL.
static int control_at(uint16_t x, uint16_t y, EpdRect* rect) {
    if (ui_rect_hit(back_rect(), x, y)) { if (rect) *rect = back_rect(); return 100; }
    if (ui_rect_hit(step_rect(0), x, y)) { if (rect) *rect = step_rect(0); return 0; }
    if (ui_rect_hit(step_rect(1), x, y)) { if (rect) *rect = step_rect(1); return 1; }
    if (ui_rect_hit(font_list_rect(), x, y)) { if (rect) *rect = font_list_rect(); return 2; }
    for (int i = 0; i < 4; ++i) if (ui_rect_hit(footer_row_rect(i), x, y)) {
        if (rect) *rect = footer_row_rect(i);
        return 3 + i;
    }
    return -1;
}

// 到界的步进键不回应按下，轻点时给提示。/ Steppers at bounds never light up; taps explain the limit.
static bool stepper_blocked(int id) {
    int px = app_settings_book_px();
    return (id == 0 && px <= READING_PX_MIN) || (id == 1 && px >= READING_PX_MAX);
}

static app_redraw_t gesture(app_ctx_t* ctx, const ui_gesture_event_t* ev) {
    EpdRect rect = {0};
    int id = control_at(ev->x0, ev->y0, &rect);
    if (ev->type == UI_GESTURE_PRESS)
        return id >= 0 && !stepper_blocked(id) ? ui_press_begin(ctx, &s_press, id, rect, render) : APP_REDRAW_NONE;
    if (ev->type != UI_GESTURE_TAP) return ui_press_restore(ctx, &s_press, render);
    int held = s_press.id;
    EpdRect held_rect = s_press.rect;
    ui_press_forget(&s_press);
    app_redraw_t result = APP_REDRAW_NONE;
    if (id >= 0 && id == control_at(ev->x, ev->y, NULL)) {
        if (stepper_blocked(id)) {
            snprintf(s_hint, sizeof(s_hint), "字号已到 %s（%d–%d px）", id == 0 ? "下限" : "上限", READING_PX_MIN, READING_PX_MAX);
            result = APP_REDRAW_PAGE;
        } else switch (id) {
            case 100:
                ctx->request_app = app_by_id(OS_APP_SETTINGS);
                return APP_REDRAW_NONE;
            case 0:
            case 1: {
                int px = app_settings_book_px();
                app_settings_set_book_px((uint8_t)(px + (id ? 4 : -4)));
                s_hint[0] = 0;
                result = APP_REDRAW_PAGE;
                break;
            }
            case 2:
                ctx->request_app = app_by_id(OS_APP_FONTS);
                return APP_REDRAW_NONE;
            default:
                if (id == 3) app_settings_set_footer_clock(!app_settings_footer_clock());
                else if (id == 4) app_settings_set_footer_battery(!app_settings_footer_battery());
                else if (id == 5) app_settings_set_footer_bar(!app_settings_footer_bar());
                else app_settings_set_footer_time(!app_settings_footer_time());
                s_hint[0] = 0;
                result = APP_REDRAW_PAGE;
                break;
        }
    }
    if (result == APP_REDRAW_NONE && held >= 0 && !ctx->request_app && !ctx->request_menu && !ctx->request_return)
        return ui_press_rewind(ctx, &s_press, held_rect, render);
    return result;
}

static void on_enter(app_ctx_t* ctx) { (void)ctx; ui_press_reset(&s_press); s_hint[0] = 0; }

static app_redraw_t on_tick(app_ctx_t* ctx) {
    ui_press_settle(ctx, &s_press, ctx->now_ms);
    return APP_REDRAW_NONE;
}

static app_redraw_t key(app_ctx_t* ctx, int key) {
    if (key == UI_KEY_1) ctx->request_app = app_by_id(OS_APP_SETTINGS);
    return APP_REDRAW_NONE;
}

const app_desc_t app_os_reading = {
    .title = "阅读与字体", .detail = "字号 · 字体 · 实验", .enter_full = false,
    .render = render, .on_gesture = gesture, .on_key = key,
    .on_enter = on_enter, .on_tick = on_tick,
};
