/* SPDX-License-Identifier: Apache-2.0
 * 中文：设备与原生预览共享的正文显示档位。/ English: Reader modes shared by device and native preview.
 */
#pragma once
typedef enum {
    DISPLAY_READER_STANDARD, ///< 标准灰阶 / Standard grayscale
    DISPLAY_READER_BINARY, ///< 黑白快刷 / Binary fast
    DISPLAY_READER_GRAY, ///< 灰缘快刷 / Gray-edge fast
    DISPLAY_READER_ANIMATED, ///< 揭页动画 / Page-turn animation
} display_reader_mode_t;
