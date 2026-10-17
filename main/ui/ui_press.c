/*
 * SPDX-FileCopyrightText: 2026 mindreset
 *
 * 中文：按下反馈的推屏实现。DU 局推与定稿调度见头文件；本文件不画任何控件。
 * English: Presentation side of press feedback. See the header for the DU
 * pushes and settle scheduling; this file paints no controls itself.
 */

#include "ui_press.h"

#include "display.h"
#include "e0470_epaper_waveform.h"

void ui_press_reset(ui_press_t* p) {
    if (!p) return;
    p->id = -1;
    p->du = 0;
    p->du_ms = 0;
    p->du_area = (EpdRect){0};
}

bool ui_press_active(const ui_press_t* p) {
    return p && p->id >= 0;
}

// 一次 DU 局推并入定稿记账。/ One DU push folded into the settle bookkeeping.
static void push_du(app_ctx_t* ctx, ui_press_t* p, EpdRect rect) {
    guard_draw_result(ctx->hl, update_display_area_with(
        ctx->hl, &E0470_FOLLOW_WAVEFORM, MODE_DU, rect));
    p->du_area = p->du ? ui_rect_union(p->du_area, rect) : rect;
    ++p->du;
    p->du_ms = ctx->now_ms;
}

app_redraw_t ui_press_begin(
    app_ctx_t* ctx, ui_press_t* p, int id, EpdRect rect, ui_press_repaint_t repaint
) {
    p->id = id;
    p->rect = rect;
    if (repaint) repaint(ctx, ctx->fb);
    push_du(ctx, p, rect);
    return APP_REDRAW_DONE;
}

app_redraw_t ui_press_restore(app_ctx_t* ctx, ui_press_t* p, ui_press_repaint_t repaint) {
    if (!ui_press_active(p)) return APP_REDRAW_NONE;
    EpdRect rect = p->rect;
    p->id = -1;
    return ui_press_rewind(ctx, p, rect, repaint);
}

void ui_press_forget(ui_press_t* p) {
    if (p) p->id = -1;
}

app_redraw_t ui_press_rewind(
    app_ctx_t* ctx, ui_press_t* p, EpdRect rect, ui_press_repaint_t repaint
) {
    if (repaint) repaint(ctx, ctx->fb);
    push_du(ctx, p, rect);
    return APP_REDRAW_DONE;
}

void ui_press_settle(app_ctx_t* ctx, ui_press_t* p, int64_t now_ms) {
    if (!p || !p->du) return;
    if (p->du < UI_SETTLE_DU_MAX && now_ms - p->du_ms < UI_SETTLE_IDLE_MS) return;
    guard_draw_result(ctx->hl, update_display_area_with(
        ctx->hl, &E0470_WAVEFORM, MODE_GL16, p->du_area));
    p->du = 0;
    p->du_area = (EpdRect){0};
}
