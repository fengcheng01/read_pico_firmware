/*
 * SPDX-FileCopyrightText: 2026 mindreset
 *
 * 中文：页面共用的按下反馈组件。页面在 render 里画按下态、在命中函数里给控件
 * 编号，本组件只负责记账与推屏：按下/恢复用跟随 DU 局推，连续局推按阈值或
 * 闲置时间用区域 GL16 定稿。控件几何与动作语义都留在页面。
 * English: Shared press-feedback helper for pages. Pages paint the pressed
 * state in render and number controls in their own hit functions; this module
 * only tracks state and presents: press/restore push locally with follow DU,
 * and repeated pushes settle by an area GL16 after a count or idle threshold.
 * Control geometry and action semantics stay in the page.
 *
 * 冻结：本组件不持有控件几何、不执行动作；DU 用跟手表，定稿用 E0470_WAVEFORM 区域 GL16。
 * Frozen: No control geometry or actions here; DU uses the follow table and settling uses an E0470_WAVEFORM area GL16.
 */

#pragma once

#include "app.h"
#include "ui_kit.h"

#ifdef __cplusplus
extern "C" {
#endif

/// 页面整页重画回调，就是各页的 render。/ Full-page repaint callback; each page's render.
typedef void (*ui_press_repaint_t)(app_ctx_t* ctx, uint8_t* fb);

typedef struct {
    /// 当前按下的控件编号；-1 无。/ Pressed control id; -1 none.
    int id;
    /// 按下控件的逻辑矩形。/ Logical rect of the pressed control.
    EpdRect rect;
    /// 自上次定稿以来的 DU 局推次数与时间。/ DU pushes and time since the last settle.
    unsigned du;
    int64_t du_ms;
    /// 定稿候选：DU 区域并集；du 为 0 时无效。/ Settle candidate: union of DU areas; invalid when du is 0.
    EpdRect du_area;
} ui_press_t;

/// 清到无按下态；进页、离页和整页重画前调用。/ Clear to no press; call on enter, exit and before repaints.
void ui_press_reset(ui_press_t* p);
bool ui_press_active(const ui_press_t* p);

/// 按下：记录目标，整页重画（render 读 p->id 画按下态）后 DU 推该矩形。
/// / Press: record the target, repaint (render reads p->id for the bed), then DU-push the rect.
app_redraw_t ui_press_begin(
    app_ctx_t* ctx, ui_press_t* p, int id, EpdRect rect, ui_press_repaint_t repaint
);
/// 取消/滑出：清目标，整页重画常态后 DU 推原矩形；无按下态返回 NONE。
/// / Cancel or slide-off: clear the target, repaint normal, then DU-push the old rect; NONE without a press.
app_redraw_t ui_press_restore(app_ctx_t* ctx, ui_press_t* p, ui_press_repaint_t repaint);
/// 只清目标不推屏；动作即将整页重画时用，定稿记账保留。
/// / Clear the target without pushing; for repaints that follow, keeping settle bookkeeping.
void ui_press_forget(ui_press_t* p);
/// 抬起没有触发任何呈现时的恢复出口：整页重画常态后 DU 推原矩形。
/// / Restore path when a release produced no presentation: repaint normal, then DU-push the held rect.
app_redraw_t ui_press_rewind(
    app_ctx_t* ctx, ui_press_t* p, EpdRect rect, ui_press_repaint_t repaint
);
/// on_tick 调用：DU 次数或闲置超限时对并集区域做一次 GL16 定稿。
/// / Call from on_tick: settle the union area with GL16 after the count or idle threshold.
void ui_press_settle(app_ctx_t* ctx, ui_press_t* p, int64_t now_ms);

#ifdef __cplusplus
}
#endif
