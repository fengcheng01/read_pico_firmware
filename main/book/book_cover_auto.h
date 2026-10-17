/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 *
 * 中文：无封面书的确定性生成封面——按书路径种子在三版式里选一并绘制纯几何灰阶组合。
 * English: Deterministic generated covers for coverless books — pick one of three layouts
 * from a path seed and draw pure geometric grayscale compositions.
 *
 * 冻结：不画文字（书架卡侧已有书名排版）；同一路径必须逐字节可复现，版式变更须递增种子版本。
 * Frozen: No text (shelf cards already typeset the title beside the image); the same path must
 * reproduce byte for byte, and layout revisions must bump the seed version.
 */
#pragma once
#include <stdint.h>

/// 生成 BOOK_COVER_W×BOOK_COVER_H 灰度封面（调用方 free）；失败返回 NULL。
/// / Generate a BOOK_COVER_W×BOOK_COVER_H grayscale cover (caller frees); NULL on failure.
uint8_t* book_cover_auto_alloc(const char* identity);
