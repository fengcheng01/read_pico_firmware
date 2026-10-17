/*
 * SPDX-FileCopyrightText: 2026 mindreset
 *
 * 中文：字体选择器，六行分页列表与当前字体预览；测速留给开发工具。
 * English: Font picker with six-row pages and a current-font preview; profiling belongs in developer tools.
 *
 * 冻结：用户要求优先展示列表；字体切换使用灰阶直刷，不执行测速或强制黑闪。
 * Frozen: Prioritize the list as requested; use grayscale updates on selection, without profiling or forced flashes.
 */
#include <stdio.h>
#include <string.h>
#include "app.h"
#include "read_pico_sd.h"
#include "settings.h"
#include "ttf_font.h"
#include "ui_product.h"
#include "ui_menu.h"
#include "ui_press.h"

#define FONT_ROWS 6
static int s_count;
static char s_message[96];
static ui_press_t s_press;
static EpdRect item_rect(int row) { return (EpdRect){UI_MARGIN, 230 + row * 100, ui_content_width(), 92}; }
static EpdRect nav_rect(int i) { return ui_row_rect(i, 2, 852, 76); }
static int pages(void) { return (s_count + 1 + FONT_ROWS - 1) / FONT_ROWS; }
static bool selected(int index) {
    const ttf_font_item_t* item = index ? ttf_font_item(index - 1) : NULL;
    return index ? item && !strcmp(item->path, ttf_font_path()) : ttf_font_is_builtin();
}
static bool pressed_id(int id) { return ui_press_active(&s_press) && s_press.id == id; }
static void render(app_ctx_t* ctx, uint8_t* fb) {
    ui_clear_page(fb);
    char detail[96];
    snprintf(detail, sizeof(detail), "%d 款字体 · 第 %d/%d 页", s_count + 1, ctx->leaf + 1, pages());
    ui_product_header(fb, "选择字体", detail);
    if (pressed_id(100)) ui_draw_pressed_round_rect(fb, ui_product_back_rect(), UI_BTN_RADIUS);
    ui_product_back(fb, "返回");
    for (int row = 0; row < FONT_ROWS; ++row) {
        int index = ctx->leaf * FONT_ROWS + row;
        if (index > s_count) break;
        const ttf_font_item_t* item = index ? ttf_font_item(index - 1) : NULL;
        EpdRect r = item_rect(row);
        bool active = selected(index);
        if (pressed_id(10 + row)) ui_draw_pressed_round_rect(fb, r, UI_BTN_RADIUS);
        ui_draw_choice_round_rect(fb, r, UI_BTN_RADIUS, active);
        ui_product_title(fb, (EpdRect){r.x + 16, r.y + 10, r.width - 120, 54},
                         item ? item->name : "内置中文", 40, 1);
        ui_text(fb, r.x + 16, r.y + 60, 26, item ? "TF 卡字体" : "常用中文 · 无需内存卡", EPD_DRAW_ALIGN_LEFT, false);
        if (active) ui_text_vc(fb, r.x + r.width - 16, r.y + 46, 30, "已选", EPD_DRAW_ALIGN_RIGHT, false);
    }
    if (pressed_id(0)) ui_draw_pressed_round_rect(fb, nav_rect(0), UI_BTN_RADIUS);
    ui_draw_button(fb, nav_rect(0), "上一页", ctx->leaf > 0);
    if (pressed_id(1)) ui_draw_pressed_round_rect(fb, nav_rect(1), UI_BTN_RADIUS);
    ui_draw_button(fb, nav_rect(1), "下一页", ctx->leaf + 1 < pages());
    ui_text(fb, UI_MARGIN, 960, 38, "清风明月，山川与远方。", EPD_DRAW_ALIGN_LEFT, false);
    ui_product_title(fb, (EpdRect){UI_MARGIN, 1020, ui_content_width(), 42},
                     s_message[0] ? s_message : ttf_font_display_name(), 28, 1);
    if (pressed_id(101)) ui_draw_pressed_round_rect(fb, ui_bar_rect(0, 1), UI_BTN_RADIUS);
    ui_draw_button(fb, ui_bar_rect(0, 1), "返回", false);
    ui_draw_menu_handle(fb, false);
}
static void on_enter(app_ctx_t* ctx) {
    s_count = ttf_font_scan(); s_message[0] = 0; ctx->leaf = 0;
    ui_press_reset(&s_press);
    for (int i = 1; i <= s_count; ++i) if (selected(i)) ctx->leaf = i / FONT_ROWS;
}
static app_redraw_t pick(int index) {
    if (selected(index)) return APP_REDRAW_NONE;
    const ttf_font_item_t* item = index ? ttf_font_item(index - 1) : NULL;
    if (index && !item) return APP_REDRAW_NONE;
    char path[TTF_FONT_PATH_MAX];
    snprintf(path, sizeof(path), "%s", item ? item->path : "");
    esp_err_t err = item ? ttf_font_open(path) : ttf_font_open_builtin();
    if (err == ESP_OK) { app_settings_set_font_path(path); s_message[0] = 0; }
    else snprintf(s_message, sizeof(s_message), "字体读取失败，请检查文件与 TF 卡");
    return APP_REDRAW_PAGE;
}
// 命中当前画出的控件；越界翻页键不回应按下。/ Hit drawn controls; out-of-range pagers never light up.
static int control_at(app_ctx_t* ctx, uint16_t x, uint16_t y, EpdRect* rect) {
    if (ui_rect_hit(ui_product_back_rect(), x, y)) { if (rect) *rect = ui_product_back_rect(); return 100; }
    if (ctx->leaf > 0 && ui_rect_hit(nav_rect(0), x, y)) { if (rect) *rect = nav_rect(0); return 0; }
    if (ctx->leaf + 1 < pages() && ui_rect_hit(nav_rect(1), x, y)) { if (rect) *rect = nav_rect(1); return 1; }
    for (int row = 0; row < FONT_ROWS; ++row) {
        int index = ctx->leaf * FONT_ROWS + row;
        if (index > s_count) break;
        if (ui_rect_hit(item_rect(row), x, y)) { if (rect) *rect = item_rect(row); return 10 + row; }
    }
    if (ui_rect_hit(ui_bar_rect(0, 1), x, y)) { if (rect) *rect = ui_bar_rect(0, 1); return 101; }
    return -1;
}
// 按下即铺灰底；滑离恢复，抬起命中同一目标才生效（与锁屏键盘同规则）。
// Press paints the bed at once; sliding off restores and only a release over the same target commits (matching the lock keypad).
static app_redraw_t on_touch(app_ctx_t* ctx, const cst836u_touch_t* touch) {
    if (!touch || !touch->touched) return APP_REDRAW_NONE;
    EpdRect rect = {0};
    int id = control_at(ctx, touch->x, touch->y, &rect);
    return id >= 0 ? ui_press_begin(ctx, &s_press, id, rect, render) : APP_REDRAW_NONE;
}
static app_redraw_t on_tick(app_ctx_t* ctx) {
    if (!ui_press_active(&s_press)) {
        ui_press_settle(ctx, &s_press, ctx->now_ms);
        return APP_REDRAW_NONE;
    }
    const cst836u_touch_t* touch = ctx->touch;
    uint16_t x = touch ? touch->x : 0, y = touch ? touch->y : 0;
    if (touch && touch->touched) {
        if (control_at(ctx, x, y, NULL) != s_press.id) return ui_press_restore(ctx, &s_press, render);
        return APP_REDRAW_NONE;
    }
    int held = s_press.id;
    EpdRect held_rect = s_press.rect;
    ui_press_forget(&s_press);
    if (control_at(ctx, x, y, NULL) != held) return ui_press_rewind(ctx, &s_press, held_rect, render);
    if (held == 100 || held == 101) {
        ctx->request_return = true;
        return APP_REDRAW_NONE;
    }
    if (held < 2) {
        ctx->leaf += held ? 1 : -1;
        return APP_REDRAW_PAGE;
    }
    app_redraw_t result = pick(ctx->leaf * FONT_ROWS + (held - 10));
    if (result == APP_REDRAW_NONE) return ui_press_rewind(ctx, &s_press, held_rect, render);
    return result;
}
// 卡失效：重扫列表；正在用的卡上字体由全局字体回退处理。
// / Media loss: rescan the list; the in-use card font falls back through the global font swap.
static void on_media_lost(app_ctx_t* ctx) {
    s_count = ttf_font_scan();
    s_message[0] = 0;
    ctx->leaf = 0;
    for (int i = 1; i <= s_count; ++i) if (selected(i)) ctx->leaf = i / FONT_ROWS;
}
static app_redraw_t on_key(app_ctx_t* ctx, int key) {
    if (key == UI_KEY_1) ctx->request_return = true;
    return APP_REDRAW_NONE;
}
const app_desc_t app_font_pick = {
    .title = "选择字体", .detail = "字体列表 · 正文预览", .enter_full = false,
    .render = render, .on_enter = on_enter, .on_touch = on_touch, .on_key = on_key, .on_tick = on_tick,
    .on_media_lost = on_media_lost,
};
