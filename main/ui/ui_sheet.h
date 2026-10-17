/*
 * SPDX-FileCopyrightText: 2026 mindreset
 *
 * 中文：页面共用的底部面板：危险动作的确认面板与多档设置的选择面板。
 * 绘制与命中共用几何；面板盖住底栏上缘、不遮菜单把手；面板上方轻点视为取消。
 * English: Shared bottom sheets for pages: a confirm sheet for dangerous
 * actions and a choice sheet for multi-tier settings. Drawing and hit testing
 * share geometry; the sheet stops above the bottom bar without covering the
 * menu handle, and a tap above the sheet counts as cancel.
 *
 * 冻结：本组件不执行动作、不持有页面状态；确认键固定右侧实底，取消键固定左侧。
 * Frozen: No actions or page state here; Confirm stays the solid right key and Cancel the left one.
 */

#pragma once

#include <stdint.h>

#include "ui_kit.h"

#ifdef __cplusplus
extern "C" {
#endif

/// 面板上方轻点：调用方按取消处理。/ Tap above the sheet: callers treat it as cancel.
#define UI_SHEET_HIT_NONE (-1)
#define UI_SHEET_HIT_OUTSIDE (-2)

/// 确认面板整块矩形。/ The whole confirm-sheet rect.
EpdRect ui_sheet_confirm_rect(void);
/// 确认面板两键：0 取消（左），1 确认（右，实底）。/ Sheet keys: 0 Cancel (left), 1 Confirm (right, solid).
EpdRect ui_sheet_confirm_button(int i);
/// pressed 取 0/1 画按下态，<0 无。message 自动按内容宽折两行。
/// / pressed paints 0/1 as held, <0 none. message wraps to two lines by measured width.
void ui_draw_confirm_sheet(
    uint8_t* fb, const char* title, const char* message,
    const char* confirm_label, int pressed
);
/// 0 取消、1 确认、OUTSIDE 面板上方、NONE 其余。/ 0 cancel, 1 confirm, OUTSIDE above the sheet, NONE elsewhere.
int ui_confirm_sheet_hit(uint16_t x, uint16_t y);

/// 选择面板：count 个选项每行四枚。/ Choice sheet: four option chips per row.
EpdRect ui_sheet_choice_rect(int count);
/// 第 index 枚选项的矩形；count 决定面板行数。/ Chip rect for index; count fixes the panel's row count.
EpdRect ui_sheet_choice_chip(int index, int count);
/// current 画选中内圈，pressed 画按下灰底（取 0..count-1，<0 无）。
/// / current draws the selected inset ring; pressed paints the gray bed (0..count-1, <0 none).
void ui_draw_choice_sheet(
    uint8_t* fb, const char* title, const char* const* labels,
    int count, int current, int pressed
);
/// 0..count-1 选项、OUTSIDE 面板上方、NONE 其余。/ 0..count-1 options, OUTSIDE above the sheet, NONE elsewhere.
int ui_choice_sheet_hit(uint16_t x, uint16_t y, int count);

#ifdef __cplusplus
}
#endif
