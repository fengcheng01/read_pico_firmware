/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 *
 * 中文：错相揭页引擎。一次差分、多拍启动条带、每像素完整相位序列；应用只依赖本公开接口。
 * English: Staggered page-turn engine. One difference pass, bands launched across ticks, every pixel keeps its full phase sequence; apps depend on this public API only.
 *
 * 冻结：不跳过或截短任何像素的相位；扫描慢于节拍时只补等、不压缩脉冲宽度；失败不回写基准帧，由调用方恢复。
 * Frozen: Never skip or truncate any pixel's phases; pad software waits when scans run slower than the tick, never shortening pulses; a failed scan keeps the old baseline for caller recovery.
 */

#pragma once

#include "epd_highlevel.h"
#include "epdiy.h"

#ifdef __cplusplus
extern "C" {
#endif

/// 逻辑屏幕上的揭页方向；库内按当前旋转映射到 framebuffer。/ Logical direction mapped through the current rotation.
typedef enum {
    E0470_TURN_LTR = 0,
    E0470_TURN_RTL = 1,
    E0470_TURN_TTB = 2,
    E0470_TURN_BTT = 3,
} e0470_turn_dir_t;

/// 主页水波纹的默认节拍：16 带 37 相约 52 拍。/ Default pacing for the home ripple: 16 bands / 37 phases take ~52 ticks.
#define E0470_TURN_DEFAULT_TICK_US 12000
/// 阅读快档只缩短软件补等，不改变单次扫描或每像素相位。/ The reader fast tick shortens software padding only, never a scan or a pixel's phases.
#define E0470_TURN_FAST_TICK_US 14000

const char* e0470_turn_dir_name(e0470_turn_dir_t dir);

/// 每拍目标间隔；实际扫描更慢时不截短。/ Target tick interval; a slower scan is never truncated.
void e0470_page_turn_set_tick_us(int us);
int e0470_page_turn_tick_us(void);
/// 释放按需分配的相位表，离开阅读时调用。/ Release the lazily allocated phase tables when leaving the reader.
void e0470_page_turn_release(void);

/// 默认入口：默认表的 GL16、16 带、一拍一带紧凑启动。/ Default entry: default-table GL16, 16 bands, one band per tick.
/// `area` 是逻辑坐标；无可用相位时返回 `EPD_DRAW_NO_PHASES_AVAILABLE`，不刷屏。
/// / `area` uses logical coordinates; missing phases return without scanning.
enum EpdDrawError e0470_page_turn(
    EpdiyHighlevelState* hl, EpdRect area, e0470_turn_dir_t dir
);

/// 显式灰阶序列与 2..32 条带，条带等间隔错开启动。/ Explicit gray sequence and 2..32 bands with spaced launches.
enum EpdDrawError e0470_page_turn_with_waveform(
    EpdiyHighlevelState* hl, EpdRect area, e0470_turn_dir_t dir,
    const EpdWaveform* waveform, unsigned bands
);

/// 显式序列的一拍一带入口；保留波形与指定条带数，16 带 37 相共 52 拍。
/// / Compact explicit sequence retaining the waveform and band count; 16 bands / 37 phases take 52 ticks.
enum EpdDrawError e0470_page_turn_with_waveform_compact(
    EpdiyHighlevelState* hl, EpdRect area, e0470_turn_dir_t dir,
    const EpdWaveform* waveform, unsigned bands
);

#ifdef __cplusplus
}
#endif
