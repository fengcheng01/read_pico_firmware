/* SPDX-License-Identifier: Apache-2.0
 * 中文：设备与原生预览共享的正文显示档位。/ English: Reader modes shared by device and native preview.
 */
#pragma once
typedef enum {
    DISPLAY_READER_STANDARD, ///< 标准灰阶 / Standard grayscale
    DISPLAY_READER_BINARY, ///< 黑白快刷 / Binary fast
    DISPLAY_READER_GRAY, ///< 快速灰阶 / Fast Gray
    DISPLAY_READER_ANIMATED, ///< 揭页动画 / Page-turn animation
    DISPLAY_READER_GRAY_BALANCED, ///< 细腻灰阶 / Fine Gray
    DISPLAY_READER_GRAY_STAGGERED, ///< 已撤回，兼容回均衡 / Withdrawn; compatibility fallback to balance
    DISPLAY_READER_GRAY_SYNC, ///< 已撤回，兼容回均衡 / Withdrawn; compatibility fallback to balance
    DISPLAY_READER_GRAY_MILD, ///< 已撤回，兼容回均衡 / Withdrawn; compatibility fallback to balance
    DISPLAY_READER_GRAY_COMPACT, ///< 已撤回，兼容回均衡 / Withdrawn; compatibility fallback to balance
} display_reader_mode_t;
