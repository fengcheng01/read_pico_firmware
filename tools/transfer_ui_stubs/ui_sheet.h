/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * 中文：配网测试的确认面板替身；几何镜像 main/ui/ui_sheet.c，改动需同步。
 * English: Confirm-sheet stand-in for provisioning tests; geometry mirrors main/ui/ui_sheet.c, keep them in sync.
 * 冻结：仅供测试。/ Frozen: Tests only.
 */
#pragma once
#include "transfer_ui_test_env.h"

#define UI_SHEET_HIT_NONE (-1)
#define UI_SHEET_HIT_OUTSIDE (-2)

// 镜像：面板 {24, 836, 636, 248}，两键 y992 高76、宽276。/ Mirror: panel {24,836,636,248}; keys at y992, h76, w276.
static inline EpdRect ui_sheet_confirm_rect(void) {
    return (EpdRect){24, 836, 636, 248};
}
static inline EpdRect ui_sheet_confirm_button(int i) {
    return (EpdRect){60 + i * 288, 992, 276, 76};
}
static inline void ui_draw_confirm_sheet(uint8_t* f, const char* t, const char* m,
                                          const char* c, int p) {
    (void)f; (void)t; (void)m; (void)c; (void)p;
}
static inline int ui_confirm_sheet_hit(uint16_t x, uint16_t y) {
    for (int i = 0; i < 2; ++i)
        if (ui_rect_hit(ui_sheet_confirm_button(i), x, y)) return i;
    if (!ui_rect_hit(ui_sheet_confirm_rect(), x, y))
        return y < 836 ? UI_SHEET_HIT_OUTSIDE : UI_SHEET_HIT_NONE;
    return UI_SHEET_HIT_NONE;
}
