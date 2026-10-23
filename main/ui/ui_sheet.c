/*
 * SPDX-FileCopyrightText: 2026 mindreset
 *
 * 中文：底部面板的绘制与命中。面板底边固定在底栏上缘之上，菜单把手不被遮住；
 * 说明文字按实测宽度折两行，超出省略。
 * English: Bottom-sheet drawing and hit testing. The sheet bottom stays above
 * the bar so the menu handle is never covered; explanatory text wraps to two
 * measured lines with ellipsis beyond.
 */

#include "ui_sheet.h"

#include <string.h>

#include "ttf_font.h"

// 面板底边：底栏上缘留 12px，把手照常可用。/ Sheet bottom: 12px above the bar; the handle stays usable.
#define UI_SHEET_BOTTOM (UI_BAR_TOP - 12)
#define UI_SHEET_SIDE 24

// 按内容宽折两行；第二行仍放不下时省略。/ Wrap to two measured lines; ellipsize past the second.
static void wrap_two_lines(const char* text, int px, int width, char line[2][128]) {
    line[0][0] = line[1][0] = 0;
    int which = 0;
    const char* at = text;
    while (*at && which < 2) {
        char chunk[128];
        size_t used = 0;
        while (at[used]) {
            unsigned char first = (unsigned char)at[used];
            size_t n = first < 0x80 ? 1 : first < 0xE0 ? 2 : first < 0xF0 ? 3 : 4;
            size_t remain = strlen(at + used);
            if (n > remain) n = 1;
            if (used + n >= sizeof(chunk)) break;
            memcpy(chunk + used, at + used, n);
            chunk[used + n] = 0;
            if (ttf_text_width_px(px, chunk) > width && used) break;
            used += n;
        }
        if (!used) break;
        memcpy(line[which], chunk, used);
        line[which][used] = 0;
        ++which;
        at += used;
    }
    if (*at && which == 2) {
        // 第二行尾部换省略号：按整字符从尾回退，凑满三字节再替换，绝不切半个字符。
        // Trade the second line's tail for an ellipsis: retreat whole characters until
        // three bytes free up, never cutting a character in half.
        size_t cut = strlen(line[1]);
        bool placed = false;
        while (cut > 0 && !placed) {
            size_t k = cut;
            while (k > 0 && ((unsigned char)line[1][k - 1] & 0xC0) == 0x80) --k;
            if (k > 0) --k;
            if (cut - k >= 3) {
                memcpy(line[1] + k, "…", 3);
                line[1][k + 3] = 0;
                placed = true;
            } else if (k == 0) break;
            cut = k;
        }
        if (!placed) snprintf(line[1], 128, "…");
    }
}

EpdRect ui_sheet_confirm_rect(void) {
    return (EpdRect){UI_SHEET_SIDE, UI_SHEET_BOTTOM - 248,
                     UI_LOCK_WIDTH - 2 * UI_SHEET_SIDE, 248};
}

EpdRect ui_sheet_confirm_button(int i) {
    EpdRect panel = ui_sheet_confirm_rect();
    int width = (panel.width - 2 * 36 - 12) / 2;
    return (EpdRect){panel.x + 36 + i * (width + 12), panel.y + 156, width, 76};
}

void ui_draw_confirm_sheet(
    uint8_t* fb, const char* title, const char* message,
    const char* confirm_label, int pressed
) {
    EpdRect panel = ui_sheet_confirm_rect();
    ui_clear_rect_fast(fb, panel);
    ui_draw_round_rect(fb, panel, UI_BTN_RADIUS, UI_GRAY_BLACK);
    ui_text(fb, panel.x + 36, panel.y + 22, 34, title, EPD_DRAW_ALIGN_LEFT, false);
    char lines[2][128];
    wrap_two_lines(message, 26, panel.width - 2 * 36, lines);
    for (int i = 0; i < 2; ++i)
        if (lines[i][0]) ui_text(fb, panel.x + 36, panel.y + 76 + i * 38, 26, lines[i], EPD_DRAW_ALIGN_LEFT, false);
    for (int i = 0; i < 2; ++i) {
        EpdRect button = ui_sheet_confirm_button(i);
        if (pressed == i) ui_draw_pressed_round_rect(fb, button, UI_BTN_RADIUS);
        if (i) {
            ui_fill_round_rect(fb, button, UI_BTN_RADIUS, UI_GRAY_BLACK);
            ui_text_vc(fb, button.x + button.width / 2, button.y + button.height / 2,
                       UI_PX_BTN, confirm_label, EPD_DRAW_ALIGN_CENTER, true);
        } else {
            ui_draw_round_rect(fb, button, UI_BTN_RADIUS, UI_GRAY_BLACK);
            ui_text_vc(fb, button.x + button.width / 2, button.y + button.height / 2,
                       UI_PX_BTN, "取消", EPD_DRAW_ALIGN_CENTER, false);
        }
    }
}

int ui_confirm_sheet_hit(uint16_t x, uint16_t y) {
    for (int i = 0; i < 2; ++i)
        if (ui_rect_hit(ui_sheet_confirm_button(i), x, y)) return i;
    if (!ui_rect_hit(ui_sheet_confirm_rect(), x, y)) {
        return y < ui_sheet_confirm_rect().y ? UI_SHEET_HIT_OUTSIDE : UI_SHEET_HIT_NONE;
    }
    return UI_SHEET_HIT_NONE;
}

EpdRect ui_sheet_choice_rect(int count) {
    const int rows = (count + 3) / 4;
    const int height = 96 + rows * 82;
    return (EpdRect){UI_SHEET_SIDE, UI_SHEET_BOTTOM - height,
                     UI_LOCK_WIDTH - 2 * UI_SHEET_SIDE, height};
}

EpdRect ui_sheet_choice_chip(int index, int count) {
    EpdRect panel = ui_sheet_choice_rect(count);
    int width = (panel.width - 2 * 36 - 3 * 12) / 4;
    return (EpdRect){panel.x + 36 + (index % 4) * (width + 12),
                     panel.y + 78 + (index / 4) * 82, width, 72};
}

void ui_draw_choice_sheet(
    uint8_t* fb, const char* title, const char* const* labels,
    int count, int current, int pressed
) {
    EpdRect panel = ui_sheet_choice_rect(count);
    ui_clear_rect_fast(fb, panel);
    ui_draw_round_rect(fb, panel, UI_BTN_RADIUS, UI_GRAY_BLACK);
    ui_text(fb, panel.x + 36, panel.y + 20, 30, title, EPD_DRAW_ALIGN_LEFT, false);
    for (int i = 0; i < count; ++i) {
        EpdRect chip = ui_sheet_choice_chip(i, count);
        if (pressed == i) ui_draw_pressed_round_rect(fb, chip, UI_CHIP_RADIUS);
        else if (current == i) ui_draw_selected_round_rect(fb, chip, UI_CHIP_RADIUS);
        else ui_draw_round_rect(fb, chip, UI_CHIP_RADIUS, UI_GRAY_BLACK);
        ui_text_vc(fb, chip.x + chip.width / 2, chip.y + chip.height / 2,
                   26, labels[i], EPD_DRAW_ALIGN_CENTER, false);
    }
}

int ui_choice_sheet_hit(uint16_t x, uint16_t y, int count) {
    for (int i = 0; i < count; ++i)
        if (ui_rect_hit(ui_sheet_choice_chip(i, count), x, y)) return i;
    if (!ui_rect_hit(ui_sheet_choice_rect(count), x, y)) {
        return y < ui_sheet_choice_rect(count).y ? UI_SHEET_HIT_OUTSIDE : UI_SHEET_HIT_NONE;
    }
    return UI_SHEET_HIT_NONE;
}
