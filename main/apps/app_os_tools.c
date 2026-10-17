/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * 中文：从唯一注册表读取诊断页面，保留原测试入口但不放进产品根菜单。
 * English: Read diagnostic pages from the single registry; retain tests outside the product root menu.
 * 冻结：不执行硬件诊断；用户点击后才交给目标页面；render 只绘图。
 * Frozen: Run no hardware diagnostics here; delegate only on explicit selection; render only paints.
 */
#include "app.h"
#include "app_registry.h"
#include "ui_product.h"
#include "ui_menu.h"
#include "ui_gesture.h"
#include "ui_press.h"
#include <stdio.h>
#define TOOL_ROWS 7
static EpdRect row_rect(int i) { return (EpdRect){UI_MARGIN, 216 + i * 118, ui_content_width(), 108}; }
static EpdRect back_rect(void) { return ui_product_back_rect(); }
static ui_press_t s_press;
static int leaves(void) { return (app_diagnostic_count() + TOOL_ROWS - 1) / TOOL_ROWS; }
static void render(app_ctx_t* ctx, uint8_t* fb) {
    ui_clear_page(fb);
    char subtitle[64]; snprintf(subtitle, sizeof(subtitle), "开发与维护 · %d / %d", ctx->leaf + 1, leaves());
    ui_product_header(fb, "进阶与诊断", subtitle);
    if (ui_press_active(&s_press) && s_press.id == 100) ui_draw_pressed_round_rect(fb, back_rect(), UI_BTN_RADIUS);
    ui_draw_button(fb, back_rect(), "返回设置", false);
    for (int i = 0; i < TOOL_ROWS; ++i) {
        const app_desc_t* app = app_diagnostic_at(ctx->leaf * TOOL_ROWS + i);
        if (!app) break;
        EpdRect r = row_rect(i);
        if (ui_press_active(&s_press) && s_press.id == i) ui_draw_pressed_round_rect(fb, r, UI_CHIP_RADIUS);
        ui_product_title(fb, (EpdRect){r.x, r.y + 8, r.width, 48}, app->title, 36, 1);
        ui_product_title(fb, (EpdRect){r.x, r.y + 60, r.width, 34}, app->detail, 26, 1);
        ui_hairline(fb, r.y + r.height, r.x, r.width, UI_GRAY_LIGHT);
    }
    if (ui_press_active(&s_press) && s_press.id == 7) ui_draw_pressed_round_rect(fb, ui_bar_rect(0, 2), UI_BTN_RADIUS);
    ui_draw_button(fb, ui_bar_rect(0, 2), "上一页", ctx->leaf > 0);
    if (ui_press_active(&s_press) && s_press.id == 8) ui_draw_pressed_round_rect(fb, ui_bar_rect(1, 2), UI_BTN_RADIUS);
    ui_draw_button(fb, ui_bar_rect(1, 2), "下一页", ctx->leaf + 1 < leaves());
    ui_draw_menu_handle(fb, false);
}
// 命中实际画出的行与翻页键；越界方向不回应按下。/ Hit drawn rows and pagers; out-of-range directions never light up.
static int control_at(app_ctx_t* ctx, uint16_t x, uint16_t y, EpdRect* rect) {
    if (ui_rect_hit(back_rect(), x, y)) { if (rect) *rect = back_rect(); return 100; }
    int nav = ui_bar_hit(x, y, 2);
    if (nav == 0 && ctx->leaf > 0) { if (rect) *rect = ui_bar_rect(0, 2); return 7; }
    if (nav == 1 && ctx->leaf + 1 < leaves()) { if (rect) *rect = ui_bar_rect(1, 2); return 8; }
    for (int i = 0; i < TOOL_ROWS; ++i) {
        if (!app_diagnostic_at(ctx->leaf * TOOL_ROWS + i)) break;
        if (ui_rect_hit(row_rect(i), x, y)) { if (rect) *rect = row_rect(i); return i; }
    }
    return -1;
}
static app_redraw_t gesture(app_ctx_t* ctx, const ui_gesture_event_t* ev) {
    EpdRect rect = {0};
    int id = control_at(ctx, ev->x0, ev->y0, &rect);
    if (ev->type == UI_GESTURE_PRESS)
        return id >= 0 ? ui_press_begin(ctx, &s_press, id, rect, render) : APP_REDRAW_NONE;
    if (ev->type != UI_GESTURE_TAP) return ui_press_restore(ctx, &s_press, render);
    int held = s_press.id;
    EpdRect held_rect = s_press.rect;
    ui_press_forget(&s_press);
    app_redraw_t result = APP_REDRAW_NONE;
    if (id >= 0 && id == control_at(ctx, ev->x, ev->y, NULL)) {
        if (id == 100) {
            ctx->request_app = app_by_id(OS_APP_SETTINGS); return APP_REDRAW_NONE;
        }
        if (id >= 7) {
            ctx->leaf += id == 8 ? 1 : -1;
            result = APP_REDRAW_PAGE;
        } else {
            ctx->request_app = app_diagnostic_at(ctx->leaf * TOOL_ROWS + id);
            return APP_REDRAW_NONE;
        }
    }
    if (result == APP_REDRAW_NONE && held >= 0 && !ctx->request_app && !ctx->request_menu && !ctx->request_return)
        return ui_press_rewind(ctx, &s_press, held_rect, render);
    return result;
}
static void on_enter(app_ctx_t* ctx) { (void)ctx; ui_press_reset(&s_press); }
static app_redraw_t on_tick(app_ctx_t* ctx) {
    ui_press_settle(ctx, &s_press, ctx->now_ms);
    return APP_REDRAW_NONE;
}
static app_redraw_t key(app_ctx_t* ctx, int key) {
    if (key == UI_KEY_1) { ctx->request_app = app_by_id(OS_APP_SETTINGS); }
    return APP_REDRAW_NONE;
}
const app_desc_t app_os_tools = {
    .title = "进阶与诊断", .detail = "开发与维护入口", .enter_full = false,
    .render = render, .on_gesture = gesture, .on_key = key, .on_enter = on_enter, .on_tick = on_tick,
};
