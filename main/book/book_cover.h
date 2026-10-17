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
 * 冻结：不改 EPUB 文件；用户授权减少跨开机解码，允许 TF 卡独立可再生缩略图缓存。
 * Frozen: Never modify EPUB files; user-authorized cold-start acceleration permits separate regenerable SD thumbnail caches.
 * 缓存须核对 ZIP 内容身份、尺寸与完整性；失败沿原提取路径，不阻塞书架。
 * Caches must match ZIP identity, dimensions and integrity; failures use normal extraction without blocking the shelf.
 */
#pragma once
#include <stdbool.h>
#include <stdint.h>

// 六封面使用原生尺寸，列表等小槽位按比例缩小；缓存身份包含尺寸。
// Six-cover shelves use native-sized artwork; smaller slots scale down and cache identities include dimensions.
#define BOOK_COVER_W 180
#define BOOK_COVER_H 232

typedef enum {
    BOOK_COVER_READY, ///< 原始封面已就绪 / Original artwork ready
    BOOK_COVER_ABSENT, ///< 已确认无封面 / Confirmed coverless
    BOOK_COVER_RETRY, ///< 提取失败，可稍后重试 / Extraction failed; retry later
} book_cover_status_t;

/// 区分原始封面、确认缺失和提取失败，失败不等同无封面。
/// Distinguish artwork, confirmed absence and extraction failure; failure never establishes absence.
book_cover_status_t book_cover_read(const char* path, uint8_t** gray_out);

/// 提取并缩放封面；成功时 *gray_out 为 W×H 灰度（调用方 free），失败返回 false 且输出为空。
/// / Extract and scale a cover; success yields a W×H grayscale buffer (caller frees), failure returns false with empty output.
bool book_cover_load(const char* path, uint8_t** gray_out);

/// 非阻塞领取成品封面（含无封面书的生成图），最多复用八张；true 表示完成，输出由调用方 free。
/// Poll prepared covers including generated artwork; reuse at most eight; true means complete, with caller-owned output.
bool book_cover_poll(const char* path, uint8_t** gray);
/// 等待唯一提取任务释放文件并丢弃结果；仅在读写/卸载/睡眠边界使用。
/// Join the sole extractor and discard its result at file mutation, unmount, reading or sleep boundaries.
void book_cover_join(void);
