/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 *
 * 全屏菜单与底栏把手的绘制和命中。条目来自 app 注册表。
 *
 * Draw and hit-test for the full-screen menu and the bar handle. Items
 * come from the app registry.
 * 冻结：全局菜单仅四个产品根入口，诊断经设置访问；返回真实目录索引。
 * Frozen: Four product roots only; diagnostics live under Settings; return actual catalog indices.
 */

#include "ui_menu.h"

#include <stdio.h>

#include "app_registry.h"
#include "ui_kit.h"

#define UI_MENU_LIST_TOP UI_CONTENT_TOP
#define UI_MENU_ROW_H \
    ((UI_CONTENT_BOTTOM - UI_MENU_LIST_TOP) / 4)

int ui_menu_leaf_count(void) {
    const int n = app_product_count();
    if (n <= 0) return 1;
    return (n + UI_MENU_ITEMS_PER_PAGE - 1) / UI_MENU_ITEMS_PER_PAGE;
}

static int leaf_range(int leaf, int* out_count) {
    const int first = leaf * UI_MENU_ITEMS_PER_PAGE;
    int count = app_product_count() - first;
    if (count > UI_MENU_ITEMS_PER_PAGE) count = UI_MENU_ITEMS_PER_PAGE;
    if (count < 0) count = 0;
    *out_count = count;
    return first;
}

int ui_menu_leaf_for_app(const app_desc_t* app) {
    const int index = app_index_of(app);
    if (index < 0 || index >= app_product_count()) return 0;
    return index / UI_MENU_ITEMS_PER_PAGE;
}

static int clamp_leaf(int leaf) {
    const int leaves = ui_menu_leaf_count();
    if (leaf < 0) return 0;
    if (leaf >= leaves) return leaves - 1;
    return leaf;
}

static EpdRect menu_btn_rect(void) {
    return (EpdRect){
        .x = epd_rotated_display_width() - UI_BAR_MARGIN - UI_MENU_BTN_SIZE,
        .y = UI_BAR_TOP,
        .width = UI_MENU_BTN_SIZE,
        .height = UI_MENU_BTN_SIZE,
    };
}

static EpdRect menu_nav_rect(bool next) {
    return ui_bar_rect(next ? 1 : 0, 2);
}

void ui_draw_menu_handle(uint8_t* framebuffer, bool menu_open) {
    EpdRect btn = menu_btn_rect();
    ui_clear_rect_fast(framebuffer, btn);
    ui_text_vc(framebuffer, btn.x + btn.width / 2, btn.y + btn.height / 2,
               44, menu_open ? "×" : "≡", EPD_DRAW_ALIGN_CENTER, false);
}

bool ui_menu_row_rect(int leaf, int row, EpdRect* out) {
    int count;
    if (!out || leaf < 0 || leaf >= ui_menu_leaf_count()) return false;
    leaf_range(leaf, &count);
    if (row < 0 || row >= count) return false;
    *out = (EpdRect){ UI_MARGIN, UI_MENU_LIST_TOP + row * UI_MENU_ROW_H,
        ui_content_width(), UI_MENU_ROW_H - UI_GAP };
    return true;
}

void ui_draw_menu_row_pressed(uint8_t* framebuffer, const app_desc_t* current,
                              int leaf, int row, bool pressed) {
    int count;
    const int first = leaf_range(leaf, &count);
    char line[80];
    const app_desc_t* item = app_at(first + row);
    if (item == NULL) return;
    EpdRect rect;
    if (!ui_menu_row_rect(leaf, row, &rect)) return;
    ui_clear_rect_fast(framebuffer, rect);
    if (pressed) ui_draw_pressed_round_rect(framebuffer, rect, UI_BTN_RADIUS);
    else ui_draw_choice_round_rect(framebuffer, rect, UI_BTN_RADIUS, item == current);

    const int badge_cx = rect.x + 46;
    const int text_x = rect.x + 96;
    const int text_top = rect.y + (rect.height - (36 + 8 + UI_PX_CAPTION)) / 2;
    snprintf(line, sizeof(line), "%d", first + row + 1);
    ui_text_vc(
        framebuffer, badge_cx, rect.y + rect.height / 2, UI_PX_LABEL, line,
        EPD_DRAW_ALIGN_CENTER, false
    );
    ui_text(
        framebuffer, text_x, text_top, 36, item->title,
        EPD_DRAW_ALIGN_LEFT, false
    );
    ui_text(
        framebuffer, text_x, text_top + 44, UI_PX_CAPTION, item->detail,
        EPD_DRAW_ALIGN_LEFT, false
    );

}

void ui_draw_menu_page(uint8_t* framebuffer, const app_desc_t* current, int leaf) {
    leaf = clamp_leaf(leaf);
    int count = 0;
    leaf_range(leaf, &count);
    const int leaves = ui_menu_leaf_count();

    ui_clear_page(framebuffer);
    ui_draw_header(framebuffer, "导航", "阅读优先 · 诊断在设置中");

    for (int row = 0; row < count; row++) {
        ui_draw_menu_row_pressed(framebuffer, current, leaf, row, false);
    }

    if (leaves > 1) {
        ui_draw_button(framebuffer, menu_nav_rect(false), "上一页 Prev", leaf > 0);
        ui_draw_button(framebuffer, menu_nav_rect(true), "下一页 Next", leaf + 1 < leaves);
    }
    ui_draw_menu_handle(framebuffer, true);
}

bool ui_menu_handle_hit_test(uint16_t x, uint16_t y) {
    return ui_rect_hit(menu_btn_rect(), x, y);
}

int ui_menu_hit_test(uint16_t x, uint16_t y, int leaf) {
    leaf = clamp_leaf(leaf);

    if (ui_rect_hit(menu_nav_rect(false), x, y)) {
        return leaf > 0 ? UI_MENU_HIT_PREV : UI_MENU_HIT_NONE;
    }
    if (ui_rect_hit(menu_nav_rect(true), x, y)) {
        return leaf + 1 < ui_menu_leaf_count() ? UI_MENU_HIT_NEXT : UI_MENU_HIT_NONE;
    }

    const int width = epd_rotated_display_width();
    if ((int)x < UI_MARGIN || (int)x >= width - UI_MARGIN) {
        return UI_MENU_HIT_NONE;
    }
    if ((int)y < UI_MENU_LIST_TOP) return UI_MENU_HIT_NONE;

    int row = ((int)y - UI_MENU_LIST_TOP) / UI_MENU_ROW_H;
    if (row < 0 || row >= UI_MENU_ITEMS_PER_PAGE) return UI_MENU_HIT_NONE;
    int row_top = UI_MENU_LIST_TOP + row * UI_MENU_ROW_H;
    if ((int)y >= row_top + UI_MENU_ROW_H - UI_GAP) return UI_MENU_HIT_NONE;

    int count = 0;
    const int first = leaf_range(leaf, &count);
    if (row >= count) return UI_MENU_HIT_NONE;
    return first + row;
}

// 按 UI_KEY_PITCH 均分，三个按键的中心（80/240/400）正好落在各段中点，
// 手指偏一点也还在同一段里。
// Split by UI_KEY_PITCH so the three key centers (80/240/400) sit at mid-segment;
// a slight finger miss still lands in the same segment.
int ui_key_hit_test(uint16_t x, uint16_t y) {
    if (y < UI_KEY_AREA_TOP) return -1;
    int index = (int)x / UI_KEY_PITCH;
    if (index >= UI_KEY_COUNT) return -1;
    return index;
}
