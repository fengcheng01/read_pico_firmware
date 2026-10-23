/* SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 * 中文：日间文字动画的可撤回柔化对照，不替换厂家表或扫描引擎。
 * English: Reversible daytime text-animation softening without replacing vendor tables or the scan engine.
 * 冻结：只交换白到灰的少量反向脉冲，不删相或动作，不改首尾与保持相。
 * Frozen: Swap only a few white-to-gray opposite pulses; retain phases, action counts, endpoints and holds.
 */
#pragma once
#include "e0470_epaper_waveform.h"
#include "epdiy.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
/// 按原始64字节/相格式派生；非法输入不写dst，支持不超过64相。
/// Derive the original64-byte/phase layout; invalid input leaves dst untouched, up to64 phases.
bool display_animation_build(uint8_t* dst, size_t capacity, const uint8_t* source, unsigned phases);
/// 按需分配PSRAM；不可用返回本地原波形，不影响用户选择。/ Allocate PSRAM on demand; unavailable resources return the local original without changing preferences.
const EpdWaveform* display_animation_waveform(void);
/// 同步动画返回后立即释放派生表，不占常驻内部内存。/ Free the derived table as soon as the synchronous animation returns; no persistent internal memory.
void display_animation_release(void);
