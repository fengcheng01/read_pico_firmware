/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 *
 * 中文：EPUB 封面提取，输出书架卡片尺寸的灰度位图；TXT 或无封面返回 false，
 * 由界面回退文件名排版。单次调用有界（ZIP 限量 + 解码预算），可安全在 tick 中逐本加载。
 * English: EPUB cover extraction into a shelf-card-sized grayscale bitmap; TXT
 * or coverless books return false so the UI falls back to typographic covers.
 * Each call is bounded (ZIP limits + decode budget) so ticks may load covers one by one.
 *
 * 冻结：不改 EPUB 文件、不缓存到磁盘；失败不阻塞书架，仅影响该卡片回退。
 * Frozen: Never modify EPUB files or cache on disk; failures never block the
 * shelf and only downgrade that card to the fallback.
 */
#pragma once
#include <stdbool.h>
#include <stdint.h>

#define BOOK_COVER_W 146
#define BOOK_COVER_H 188

/// 提取并缩放封面；成功时 *gray_out 为 W×H 灰度（调用方 free），失败返回 false 且输出为空。
/// / Extract and scale a cover; success yields a W×H grayscale buffer (caller frees), failure returns false with empty output.
bool book_cover_load(const char* path, uint8_t** gray_out);
