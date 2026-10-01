/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 * 中文：基于现有绘图库的产品排版，文字缓冲固定大小。
 * English: Product layout atop the existing drawing kit, using fixed text buffers.
 */
#include "ui_product.h"
#include "ttf_font.h"
#include "app_registry.h"
#include "ui_menu.h"
#include "book_entry.h"
#include <stdio.h>
#include <string.h>

static size_t character_bytes(const char* p) {
    unsigned char c = (unsigned char)*p;
    size_t n = c < 0x80 ? 1 : (c & 0xe0) == 0xc0 ? 2 : (c & 0xf0) == 0xe0 ? 3 : (c & 0xf8) == 0xf0 ? 4 : 1;
    for (size_t i = 1; i < n; ++i) if (!p[i] || ((unsigned char)p[i] & 0xc0) != 0x80) return 1;
    return n;
}
void ui_product_title(uint8_t* fb, EpdRect rect, const char* title, int px, int max_lines) {
    if (!title || px <= 0 || rect.width <= 0) return;
    const char* p = title;
    for (int row = 0; row < max_lines && *p && (row + 1) * (px + 8) <= rect.height; ++row) {
        char line[260] = {0};
        size_t used = 0;
        bool last = row + 1 == max_lines || (row + 2) * (px + 8) > rect.height;
        int reserve = last ? ttf_text_width_px(px, "…") : 0;
        while (*p) {
            size_t n = character_bytes(p);
            if (used + n + 4 >= sizeof(line)) break;
            memcpy(line + used, p, n); line[used + n] = 0;
            if (ttf_text_width_px(px, line) > rect.width - reserve) { line[used] = 0; break; }
            used += n; p += n;
        }
        if (!used) break;
        if (last && *p) memcpy(line + used, "…", sizeof("…"));
        ui_text(fb, rect.x, rect.y + row * (px + 8), px, line, EPD_DRAW_ALIGN_LEFT, false);
    }
}
static bool available(os_app_id_t id, bool transfer_enabled) {
    return app_by_id(id) && (id != OS_APP_TRANSFER || transfer_enabled);
}
void ui_product_home_bar(uint8_t* fb, bool transfer_enabled) {
    const os_app_id_t ids[] = {OS_APP_LIBRARY, OS_APP_TRANSFER};
    const char* labels[] = {"书架", "导入图书"};
    for (int i = 0; i < 2; ++i)
        ui_draw_button(fb, ui_bar_rect(i, 2), labels[i], available(ids[i], transfer_enabled));
    ui_draw_menu_handle(fb, false);
}
os_app_id_t ui_product_home_bar_hit(uint16_t x, uint16_t y, bool transfer_enabled) {
    int hit = ui_bar_hit(x, y, 2);
    os_app_id_t id = hit == 0 ? OS_APP_LIBRARY : hit == 1 ? OS_APP_TRANSFER : OS_APP_NONE;
    return available(id, transfer_enabled) ? id : OS_APP_NONE;
}

void ui_product_header(uint8_t* fb, const char* title, const char* detail) {
    ui_text(fb, UI_MARGIN, 24, 24, "小纸 Pico  /  READ PICO", EPD_DRAW_ALIGN_LEFT, false);
    ui_text(fb, UI_MARGIN, 70, 56, title, EPD_DRAW_ALIGN_LEFT, false);
    ui_product_title(fb, (EpdRect){UI_MARGIN, 140, ui_content_width(), 34}, detail, 26, 1);
}
EpdRect ui_product_back_rect(void) { return (EpdRect){470, 60, 174, 80}; }
void ui_product_back(uint8_t* fb, const char* label) { ui_draw_button(fb, ui_product_back_rect(), label, false); }
static const os_app_id_t roots[] = {OS_APP_HOME, OS_APP_LIBRARY, OS_APP_TODAY, OS_APP_SETTINGS};
static const char* root_labels[] = {"正在读", "书架", "今日", "设置"};
void ui_product_root_bar(uint8_t* fb, os_app_id_t active) {
    ui_clear_rect_fast(fb, (EpdRect){0, UI_BAR_TOP, UI_LOCK_WIDTH, UI_BAR_H});
    ui_hairline(fb, UI_BAR_TOP, UI_MARGIN, ui_content_width(), UI_GRAY_BLACK);
    for (int i = 0; i < 4; ++i) {
        EpdRect r = ui_bar_rect(i, 4);
        ui_text_vc(fb, r.x + r.width / 2, r.y + 44, 28, root_labels[i], EPD_DRAW_ALIGN_CENTER, false);
        if (active == roots[i]) epd_fill_rect((EpdRect){r.x + 12, r.y + 78, r.width - 24, 4}, UI_GRAY_BLACK, fb);
    }
    ui_draw_menu_handle(fb, false);
}
os_app_id_t ui_product_root_hit(uint16_t x, uint16_t y) {
    int i = ui_bar_hit(x, y, 4);
    return i >= 0 && i < 4 && app_by_id(roots[i]) ? roots[i] : OS_APP_NONE;
}
bool ui_product_navigate(app_ctx_t* ctx, os_app_id_t id) {
    const app_desc_t* next = app_by_id(id);
    if (!next) return false;
    if (id == OS_APP_LIBRARY && !book_entry_request(BOOK_ENTRY_SHELF, NULL)) return false;
    ctx->request_app = next;
    return true;
}
void ui_product_cover(uint8_t* fb, EpdRect r, const char* title, int px) {
    epd_fill_rect(r, UI_GRAY_WHITE, fb);
    epd_draw_rect(r, UI_GRAY_BLACK, fb);
    epd_fill_rect((EpdRect){r.x + 8, r.y + 8, 5, r.height - 16}, UI_GRAY_BLACK, fb);
    ui_product_title(fb, (EpdRect){r.x + 24, r.y + 30, r.width - 40, r.height - 74}, title, px, 5);
    ui_hairline(fb, r.y + r.height - 40, r.x + 24, r.width - 40, UI_GRAY_BLACK);
    ui_text(fb, r.x + 24, r.y + r.height - 30, 18, "READ PICO", EPD_DRAW_ALIGN_LEFT, false);
}
EpdRect ui_product_shelf_rect(int row) { return (EpdRect){UI_MARGIN, 308 + row * 228, ui_content_width(), 208}; }
EpdRect ui_product_shelf_nav_rect(int index) { return ui_row_rect(index, 3, 1006, 80); }
void ui_product_shelf_card(uint8_t* fb, EpdRect r, const char* title, const char* meta,
                           unsigned percent, bool has_progress, bool pressed, const uint8_t* cover) {
    ui_clear_rect_fast(fb, r);
    if (pressed) epd_fill_rect(r, 0xE0, fb);
    EpdRect slot = {r.x, r.y + 6, BOOK_COVER_W, BOOK_COVER_H};
    if (cover) {
        // 位图封面：白底描边，逐像素灰度与章节插图同路径。/ Bitmap cover: white ground, bordered, drawn like chapter illustrations.
        epd_fill_rect(slot, UI_GRAY_WHITE, fb);
        for (int y = 0; y < BOOK_COVER_H; ++y)
            for (int x = 0; x < BOOK_COVER_W; ++x)
                epd_draw_pixel(slot.x + x, slot.y + y, cover[(size_t)y * BOOK_COVER_W + x], fb);
        epd_draw_rect(slot, UI_GRAY_BLACK, fb);
    } else ui_product_cover(fb, slot, title, 28);
    int x = r.x + 174, width = r.width - 174;
    ui_product_title(fb, (EpdRect){x, r.y + 16, width, 132}, title, 36, 3);
    ui_product_title(fb, (EpdRect){x, r.y + 154, width, 34}, meta, 26, 1);
    if (has_progress) {
        epd_fill_rect((EpdRect){x, r.y + 194, width, 3}, UI_GRAY_LIGHT, fb);
        epd_fill_rect((EpdRect){x, r.y + 194, width * (int)(percent > 100 ? 100 : percent) / 100, 3}, UI_GRAY_BLACK, fb);
    }
}
EpdRect ui_product_tool_rect(int index) {
    return ui_grid_rect(index % 3, 3, index / 3, UI_BAR_TOP - 2 * (UI_BTN_H + UI_GAP), UI_BTN_H);
}
static void reader_tools(uint8_t* fb, const char* title, int px, bool sizes, int pressed) {
    EpdRect panel = {UI_MARGIN - 16, 782, ui_content_width() + 32, UI_BAR_TOP - 782};
    ui_clear_rect_fast(fb, panel);
    ui_hairline(fb, 782, panel.x, panel.width, UI_GRAY_BLACK);
    ui_product_title(fb, (EpdRect){UI_MARGIN, 810, ui_content_width() - 180, 46}, title, 34, 1);
    char size[32]; snprintf(size, sizeof(size), "%d px", px);
    ui_text(fb, ui_content_right(), 816, 30, size, EPD_DRAW_ALIGN_RIGHT, false);
    ui_text(fb, UI_MARGIN, 864, 24, "阅读工具 · 中键长按打开导航", EPD_DRAW_ALIGN_LEFT, false);
    const char* labels[] = {"目录 / 书签", "添加书签", "字号", "更多设置", "清除残影", "书架"};
    const char* size_labels[] = {"返回工具", "字号 −", "字号 +", "正文字体", "更多设置", "书架"};
    for (int i = 0; i < 6; ++i) {
        EpdRect r = ui_product_tool_rect(i);
        if (pressed == 200 + i) ui_draw_pressed_round_rect(fb, r, 4);
        else ui_draw_round_rect(fb, r, 4, UI_GRAY_BLACK);
        ui_text_vc(fb, r.x + r.width / 2, r.y + r.height / 2, 32, sizes ? size_labels[i] : labels[i], EPD_DRAW_ALIGN_CENTER, false);
    }
}
void ui_product_reader_tools(uint8_t* fb, const char* title, int px, bool night, int pressed) {
    (void)night; reader_tools(fb, title, px, false, pressed);
}
void ui_product_reader_sizes(uint8_t* fb, const char* title, int px, int pressed) {
    reader_tools(fb, title, px, true, pressed);
}
EpdRect ui_product_reader_body(bool missing_glyphs) {
    int top = missing_glyphs ? 88 : 24;
    return (EpdRect){UI_MARGIN, top, ui_content_width(), UI_BAR_TOP - 8 - top};
}
void ui_product_reader_chrome(uint8_t* fb, const char* title, unsigned page, unsigned pages,
                             unsigned percent, bool missing_glyphs, const char* status, bool bar) {
    if (missing_glyphs) {
        ui_text(fb, UI_MARGIN, 24, 26, "本章有缺字 · 点此选择完整字体", EPD_DRAW_ALIGN_LEFT, false);
        ui_hairline(fb, 76, UI_MARGIN, ui_content_width(), UI_GRAY_LIGHT);
    }
    EpdRect track = ui_bar_rect(0, 1);
    if (bar) {
        epd_fill_rect((EpdRect){track.x, track.y + 16, track.width, 4}, UI_GRAY_LIGHT, fb);
        epd_fill_rect((EpdRect){track.x, track.y + 16, track.width * (int)(percent > 100 ? 100 : percent) / 100, 4}, UI_GRAY_BLACK, fb);
    }
    // 页脚左侧可先放状态（时钟/电量），标题让位；右侧保持页码与百分比。
    // The footer left may open with the status cluster (clock/battery); the right keeps pages and percent.
    int left_x = track.x, left_w = track.width - 260;
    if (status && status[0]) {
        ui_text(fb, track.x, track.y + 36, 24, status, EPD_DRAW_ALIGN_LEFT, false);
        left_x = track.x + ttf_text_width_px(24, status) + 20;
        left_w = track.x + track.width - 260 - left_x;
    }
    if (left_w > 40) ui_product_title(fb, (EpdRect){left_x, track.y + 36, left_w, 32}, title, 24, 1);
    char label[48];
    if (pages) snprintf(label, sizeof(label), "%u/%u · %u%%", page, pages, percent);
    else snprintf(label, sizeof(label), "%u/… · %u%%", page, percent);
    ui_text(fb, track.x + track.width, track.y + 36, 24, label, EPD_DRAW_ALIGN_RIGHT, false);
    ui_draw_menu_handle(fb, false);
}

/* ---- 锁屏密码键盘 / Lock PIN keypad ---- */
// 1..9 三行、0/清空/退格一行；绘制与命中共用几何。/ 1..9 in three rows plus 0/clear/backspace; drawing and hits share geometry.
static EpdRect lock_key_rect(int index) {
    if (index <= 8) return ui_grid_rect(index % 3, 3, index / 3, 372, 104);
    if (index == 9) return ui_grid_rect(1, 3, 3, 828, 104);
    if (index == 10) return ui_grid_rect(0, 3, 3, 828, 104);
    return ui_grid_rect(2, 3, 3, 828, 104);
}
void ui_product_lock_keypad(uint8_t* fb, const char* title, const char* message, unsigned digits, bool back) {
    ui_clear_page(fb);
    ui_text(fb, UI_LOCK_WIDTH / 2, 128, 44, title, EPD_DRAW_ALIGN_CENTER, false);
    for (int i = 0; i < 4; ++i) {
        EpdRect dot = {(i - 1) * 100 + 174, 224, 36, 36};
        if ((unsigned)i < digits) ui_fill_round_rect(fb, dot, 8, UI_GRAY_BLACK);
        else ui_draw_round_rect(fb, dot, 8, UI_GRAY_BLACK);
    }
    for (int digit = 1; digit <= 9; ++digit) {
        EpdRect r = lock_key_rect(digit - 1);
        ui_draw_round_rect(fb, r, UI_BTN_RADIUS, UI_GRAY_BLACK);
        char label[2] = {(char)('0' + digit), 0};
        ui_text_vc(fb, r.x + r.width / 2, r.y + r.height / 2, 44, label, EPD_DRAW_ALIGN_CENTER, false);
    }
    EpdRect zero = lock_key_rect(9);
    ui_draw_round_rect(fb, zero, UI_BTN_RADIUS, UI_GRAY_BLACK);
    ui_text_vc(fb, zero.x + zero.width / 2, zero.y + zero.height / 2, 44, "0", EPD_DRAW_ALIGN_CENTER, false);
    ui_draw_button(fb, lock_key_rect(10), "清空", true);
    ui_draw_button(fb, lock_key_rect(11), "退格", true);
    if (message && message[0])
        ui_text(fb, UI_LOCK_WIDTH / 2, 986, 30, message, EPD_DRAW_ALIGN_CENTER, false);
    else
        ui_text(fb, UI_LOCK_WIDTH / 2, 986, 26, "4 位数字 · 输完自动校验", EPD_DRAW_ALIGN_CENTER, false);
    if (back) ui_draw_button(fb, (EpdRect){470, 68, 174, 68}, "返回", false);
}
int ui_product_lock_keypad_hit(uint16_t x, uint16_t y) {
    for (int digit = 1; digit <= 9; ++digit)
        if (ui_rect_hit(lock_key_rect(digit - 1), x, y)) return digit;
    if (ui_rect_hit(lock_key_rect(9), x, y)) return 0;
    if (ui_rect_hit(lock_key_rect(10), x, y)) return 10;
    if (ui_rect_hit(lock_key_rect(11), x, y)) return 11;
    return -1;
}
