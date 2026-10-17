/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 * 中文：宿主布局测试的字体契约。/ English: Font contract for host layout tests.
 * 冻结：仅供测试。/ Frozen: Tests only.
 */
#pragma once
#include "epdiy.h"
int ttf_text_width_px(int px, const char* text);
int ttf_ascender_px(int px);
// 标题假粗体的宿主空实现：只保证链接。/ Host no-ops for the heading faux bold: link-only.
int ttf_get_stroke(void);
void ttf_set_stroke(int steps);
void ttf_draw_text_px(uint8_t* fb, int x, int y, int px, const char* text, enum EpdFontFlags align, uint8_t fg, uint8_t bg);
