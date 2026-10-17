/*
 * SPDX-FileCopyrightText: 2026 mindreset
 *
 * 中文：8 位灰度到 16 级面板灰的有序抖动，供所有图片路径共用。
 * English: Ordered dither from 8-bit gray to the panel's 16 levels, shared by every image path.
 *
 * 冻结：抖动只用于图片像素；文字、界面线条与图标直写不得经过。
 * Frozen: Images only; text, UI rules and icons never pass through the dither.
 */
#pragma once
#include <stdint.h>

/// 8 位灰按 8x8 Bayer 相位映射到相邻 16 级；端点保持纯黑纯白，同坐标结果稳定。
/// Map 8-bit gray to adjacent 16 levels by 8x8 Bayer phase; endpoints stay pure and per-coordinate stable.
uint8_t ui_image_dither(uint8_t gray, int x, int y);
