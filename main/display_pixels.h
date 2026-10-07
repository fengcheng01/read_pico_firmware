/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 *
 * 中文：无硬件IO的帧像素转换，设备与预览共用。
 * English: Frame pixel conversions shared by the device and preview, without hardware IO.
 *
 * 冻结：用户选择黑白直刷；转换结果必须是扫描和提交的实际帧，不修改旧参考。
 * Frozen: The user chooses black/white direct turns; the converted result must be the actual frame scanned and committed, without modifying prior references.
 */
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/// 按低于8为黑、其余为白转为0/15；扫描与提交同帧，日夜不翻转。/ Threshold below 8 to black and all others to white at 0/15; scan and commit the same frame without day/night inversion.
void display_prepare_direct_frame(uint8_t* fb, int width, int height, bool white_on_black);

/// 诊断实验的墨迹预算，每像素2bit；产品已停用以避免白字累积。/ Diagnostic ink budgets, two bits per pixel; disabled in the product to avoid accumulating white glyphs.
typedef struct {
    uint8_t* ages; ///< 调用方提供的零初始化存储。/ Caller-provided zeroed storage.
    int width, height; ///< 物理尺寸，宽度必须为偶数。/ Physical dimensions, with even width.
} display_ghost_history_t;

/// 所需字节数；无效尺寸返回0。/ Required bytes, zero for invalid dimensions.
size_t display_ghost_history_bytes(int width, int height);
/// 完整清理成功后丢弃旧墨迹预算。/ Discard old ink budgets after successful full cleanup.
void display_ghost_history_reset(display_ghost_history_t* h);
/// 记录非白像素及一像素邻域，保留三次页面擦白预算。/ Record nonwhite pixels and their one-pixel neighborhood with three page-erase budgets.
void display_ghost_history_observe(display_ghost_history_t* h, const uint8_t* fb);
/// 生成1字节/像素动作掩码，只选择目标为15的最近墨迹邻域；返回像素数。/ Build a one-byte/pixel action mask selecting only recent-ink neighborhoods now at 15; return the pixel count.
size_t display_ghost_history_mask(const display_ghost_history_t* h, const uint8_t* target, uint8_t* mask);
/// 仅擦白成功后递减白目标预算；失败不得调用。/ Decrement white-target budgets only after erase success; never call on failure.
void display_ghost_history_commit(display_ghost_history_t* h, const uint8_t* target);
/// 预先生成1bit选择图，低位先对应像素；返回选中数。/ Precompute a one-bit selector map with low bits first; return selected count.
size_t display_ghost_history_pack(const display_ghost_history_t* h, const uint8_t* target, uint8_t* packed);
/// 顺序展开选择图到差分动作缓冲；不访问历史或参考帧。/ Expand selectors sequentially into diff action storage without reading history or reference frames.
void display_ghost_history_expand(const uint8_t* packed, uint8_t* mask, size_t pixels);
