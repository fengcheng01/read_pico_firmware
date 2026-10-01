/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
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
#include <stdio.h>
#define TOOL_ROWS 7
static EpdRect row_rect(int i) { return (EpdRect){UI_MARGIN, 216 + i * 118, ui_content_width(), 108}; }
static EpdRect back_rect(void) { return (EpdRect){470, 68, 174, 68}; }
static int leaves(void) { return (app_diagnostic_count() + TOOL_ROWS - 1) / TOOL_ROWS; }
static void render(app_ctx_t* ctx, uint8_t* fb) {
    ui_clear_page(fb);
    char subtitle[64]; snprintf(subtitle, sizeof(subtitle), "开发与维护 · %d / %d", ctx->leaf + 1, leaves());
    ui_product_header(fb, "诊断", subtitle);
    ui_draw_button(fb, back_rect(), "返回设置", false);
    for (int i = 0; i < TOOL_ROWS; ++i) {
        const app_desc_t* app = app_diagnostic_at(ctx->leaf * TOOL_ROWS + i);
        if (!app) break;
        EpdRect r = row_rect(i);
        ui_product_title(fb, (EpdRect){r.x, r.y + 8, r.width, 48}, app->title, 36, 1);
        ui_product_title(fb, (EpdRect){r.x, r.y + 60, r.width, 34}, app->detail, 26, 1);
        ui_hairline(fb, r.y + r.height, r.x, r.width, UI_GRAY_LIGHT);
    }
    ui_draw_button(fb, ui_bar_rect(0, 2), "上一页", ctx->leaf > 0);
    ui_draw_button(fb, ui_bar_rect(1, 2), "下一页", ctx->leaf + 1 < leaves());
    ui_draw_menu_handle(fb, false);
}
static app_redraw_t gesture(app_ctx_t* ctx, const ui_gesture_event_t* ev) {
    if (ev->type != UI_GESTURE_TAP) return APP_REDRAW_NONE;
    if (ui_rect_hit(back_rect(), ev->x0, ev->y0) && ui_rect_hit(back_rect(), ev->x, ev->y)) {
        ctx->request_app = app_by_id(OS_APP_SETTINGS); return APP_REDRAW_NONE;
    }
    int nav = ui_bar_hit(ev->x0, ev->y0, 2);
    if (nav >= 0 && nav == ui_bar_hit(ev->x, ev->y, 2)) {
        int next = ctx->leaf + (nav ? 1 : -1);
        if (next >= 0 && next < leaves()) ctx->leaf = next;
        return APP_REDRAW_PAGE;
    }
    for (int i = 0; i < TOOL_ROWS; ++i) {
        EpdRect r = row_rect(i);
        if (ui_rect_hit(r, ev->x0, ev->y0) && ui_rect_hit(r, ev->x, ev->y)) {
            ctx->request_app = app_diagnostic_at(ctx->leaf * TOOL_ROWS + i); break;
        }
    }
    return APP_REDRAW_NONE;
}
static app_redraw_t key(app_ctx_t* ctx, int key) {
    if (key == UI_KEY_1) { ctx->request_app = app_by_id(OS_APP_SETTINGS); }
    return APP_REDRAW_NONE;
}
const app_desc_t app_os_tools = {
    .title = "进阶与诊断", .detail = "开发与维护入口", .enter_full = true,
    .render = render, .on_gesture = gesture, .on_key = key,
};
