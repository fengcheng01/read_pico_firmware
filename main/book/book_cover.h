/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 *
 * 中文：EPUB 封面提取，输出书架卡片尺寸的灰度位图；TXT 或无封面返回 false，
 * 由界面回退文件名排版。单次调用有界（ZIP 限量 + 解码预算），UI 使用后台任务领取结果。
 * English: EPUB cover extraction into a shelf-card-sized grayscale bitmap; TXT
 * or coverless books return false so the UI falls back to typographic covers.
 * Each call is bounded (ZIP limits + decode budget); the UI polls a background worker.
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

/// 非阻塞领取一张封面；true 表示已完成（无封面时输出为空），输出由调用方 free。
/// Nonblocking cover poll; true means completed (NULL for absent covers), with caller-owned output.
bool book_cover_poll(const char* path, uint8_t** gray);
/// 等待唯一提取任务释放文件并丢弃结果；仅在读写/卸载/睡眠边界使用。
/// Join the sole extractor and discard its result at file mutation, unmount, reading or sleep boundaries.
void book_cover_join(void);
