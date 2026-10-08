/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 *
 * 墨水屏刷新、HV 轨空闲超时、pclk 回退。
 *
 * EPD present, HV-rail idle timeout, and pclk fallback.
 */

#pragma once

#include <stdint.h>

#include "epd_highlevel.h"
#include "epdiy.h"
#include "read_pico_epd_timing.h"

#ifdef __cplusplus
extern "C" {
#endif

// LCD 像素时钟：只决定有效像素段占一行的多少，剩下的由行结束段补足——行周期被锁在
// 波形标定的帧周期上，所以调 pclk 不会让刷新变快，只影响 DMA 的供数余量。实机反馈后开机
// 采用 12MHz，配合预填队列减少网络/字库竞争导致的欠载。18MHz 留给诊断实验。
// LCD pixel clock: it only sets how much of the line the active pixels occupy;
// the line-end pad fills the rest. The line period is locked to the waveform
// frame time, so pclk does not make refresh faster — it only changes DMA slack.
// Hardware feedback selects 12 MHz at boot with queue prefill to reduce
// underruns under network/font contention; 18 MHz remains a diagnostic option.
#define DISPLAY_PCLK_DEFAULT_MHZ 12
// 出现供数不足（EPD_DRAW_EMPTY_LINE_QUEUE）时退回这个确定安全的频率。
// Fall back to this known-safe clock on underrun (EPD_DRAW_EMPTY_LINE_QUEUE).
#define DISPLAY_PCLK_SAFE_MHZ READ_PICO_EPD_PCLK_MIN_MHZ
// 工程页上还能继续往上试。行消隐与 CKV 宽度会跟着频率重解。
// The lab page can still step higher. Line blanking and CKV width re-solve with the clock.
#define DISPLAY_PCLK_MIN_MHZ READ_PICO_EPD_PCLK_MIN_MHZ
#define DISPLAY_PCLK_MAX_MHZ READ_PICO_EPD_PCLK_MAX_MHZ
#define DISPLAY_PCLK_STEP_MHZ 1

void rails_keepalive(void);
void rails_idle_check(int64_t now_ms);

/// 大量文件I/O期间增加扫描预填，调用方离开时恢复。/ Increase scan prefill during bulk file I/O; caller restores on exit.
void display_set_bulk_io(bool active);

enum EpdDrawError update_display_mode(EpdiyHighlevelState* hl, enum EpdDrawMode mode);
enum EpdDrawError update_display_from_white(EpdiyHighlevelState* hl);
enum EpdDrawError update_display_from_white_with(
    EpdiyHighlevelState* hl, const EpdWaveform* waveform, enum EpdDrawMode mode
);
/// 下一次导航整页用厂家GC16清理一次；成功消费，失败保留，不用于按钮或普通翻页。
/// Arm one vendor GC16 cleaning at the next whole-page navigation; consume on success, retain on failure, never arm for controls or ordinary turns.
void display_request_navigation_settle(void);
/// 把前缓冲铺白再 GC16 全刷，物理屏回到白底。
/// Paint the front buffer white and GC16 the panel back to white.
enum EpdDrawError update_display_white(EpdiyHighlevelState* hl);
enum EpdDrawError update_display_full(EpdiyHighlevelState* hl);
enum EpdDrawError update_display_with(
    EpdiyHighlevelState* hl, const EpdWaveform* waveform, enum EpdDrawMode mode
);
/// 标准正文：日间厂家GL16不计周期；夜间保持，与直刷共计成功翻页，按gc_every清理（0关）。
/// Standard body: vendor GL16 by day without counting; night holds and shares successful-turn gc_every cleaning with direct (0 disables).
enum EpdDrawError update_display_text_turn(EpdiyHighlevelState* hl, bool white_on_black);
/// 厂家黑白DU直刷，实际二值目标和真实旧灰参考；日间不计周期，夜间与标准共计成功翻页。
/// Vendor black/white DU with actual binary targets and real prior grays; day never counts, night shares successful turns with standard.
enum EpdDrawError update_display_text_direct(EpdiyHighlevelState* hl, bool white_on_black);
/// 灰阶图还在屏上时置位：菜单盖上来或离页先刷白，避免从中间灰差分。
/// Set while a gray image is still on panel: wipe to white before the menu or leave so the next update is not a mid-gray differential.
void display_hold_white_exit(bool hold);
bool display_take_white_exit(void);
enum EpdDrawError update_display_area_with(
    EpdiyHighlevelState* hl, const EpdWaveform* waveform, enum EpdDrawMode mode,
    EpdRect area
);
/// 静默局推：产品GL16全像素推一个区域，不进清残影档位。时钟字带专用。
/// Quiet band push: product GL16 full-pixel area update outside the cleanup tier. For clock digit bands.
enum EpdDrawError update_display_area_quiet(EpdiyHighlevelState* hl, EpdRect area);

/// 当前像素时钟。/ Current pixel clock.
int display_pclk_mhz(void);
/// 欠载退回安全频率并重画；其它失败标记未知基准，下次先清白重画。
/// Underrun drops the clock and redraws; other failures mark an unknown baseline for a clean retry.
void guard_draw_result(EpdiyHighlevelState* hl, enum EpdDrawError result);

#ifdef __cplusplus
}
#endif

/// 仅冷启动：物理清屏铺白并归白软件基准；不用于普通切页。
/// Cold boot only: physically clear the panel white and white both software baselines; never for regular transitions.
enum EpdDrawError display_boot_white(EpdiyHighlevelState* hl);
