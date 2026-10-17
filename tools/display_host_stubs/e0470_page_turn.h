/* SPDX-License-Identifier: Apache-2.0
 * 错相揭页引擎替身：宿主显示测试不跑真实条带调度。/ Staggered page-turn shim: host display tests never run the real band scheduler.
 */
#pragma once
#include "epdiy.h"
typedef enum { E0470_TURN_LTR = 0, E0470_TURN_RTL = 1, E0470_TURN_TTB = 2, E0470_TURN_BTT = 3 } e0470_turn_dir_t;
#define E0470_TURN_DEFAULT_TICK_US 12000
#define E0470_TURN_FAST_TICK_US 14000
void e0470_page_turn_set_tick_us(int us);
int e0470_page_turn_tick_us(void);
void e0470_page_turn_release(void);
enum EpdDrawError e0470_page_turn(EpdiyHighlevelState* hl, EpdRect area, e0470_turn_dir_t dir);
enum EpdDrawError e0470_page_turn_with_waveform(
    EpdiyHighlevelState* hl, EpdRect area, e0470_turn_dir_t dir,
    const EpdWaveform* waveform, unsigned bands);
enum EpdDrawError e0470_page_turn_with_waveform_compact(
    EpdiyHighlevelState* hl, EpdRect area, e0470_turn_dir_t dir,
    const EpdWaveform* waveform, unsigned bands);
