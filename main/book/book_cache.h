/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 * 中文：TF 卡可再生书籍缓存及流式内容身份，不包含阅读进度。
 * English: Regenerable SD book caches and streaming content identity, without reading progress.
 * 冻结：只写独立隐藏目录；调用方串行化文件访问；损坏或写失败均回退原功能。
 * Frozen: Write only the separate hidden directory; callers serialize file access; corruption or write failure falls back.
 */
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

typedef enum {
    BOOK_CACHE_COVER = 1, ///< 封面缩略图 / Cover thumbnail
    BOOK_CACHE_TXT_TOC = 2, ///< TXT 原始字节目录 / TXT source-byte index
} book_cache_kind_t;

typedef struct {
    uint64_t bytes; ///< 源字节数 / Source byte count
    uint64_t digest; ///< 内容散列，非安全认证 / Content hash, not authentication
    uint32_t crc; ///< 独立内容校验 / Independent content checksum
    uint32_t variant; ///< 尺寸或解析规格 / Dimensions or parsing specification
} book_cache_identity_t;

/// 初始化流式身份；TXT 首次扫描可同时累积。/ Initialize streaming identity; TXT can accumulate it during initial indexing.
void book_cache_fingerprint_begin(book_cache_identity_t* identity);
/// 累积新读取的原始字节，不包含重复缓冲。/ Accumulate newly read source bytes, excluding repeated buffer tails.
void book_cache_fingerprint_update(book_cache_identity_t* identity, const void* bytes, size_t size);
/// 完整顺序校验源文件，结束后回到开头；失败不允许缓存命中。
/// Verify all source bytes sequentially and rewind; failure disallows cache reuse.
bool book_cache_fingerprint_file(FILE* file, book_cache_identity_t* identity);
/// 只读检查候选文件是否存在，不验证身份。/ Inspect a candidate without writes or identity verification.
bool book_cache_exists(const char* source, book_cache_kind_t kind);
/// 校验身份、长度和内容后返回调用方所有的 payload；失败输出为空。
/// Return caller-owned payload after identity, length and checksum verification; failure leaves outputs empty.
bool book_cache_load(const char* source, book_cache_kind_t kind, const book_cache_identity_t* identity,
                     size_t limit, void** payload, size_t* size);
/// 临时文件完整同步后提交；最多 128 项/8 MiB，失败仅丢弃缓存，不触及源和进度。
/// Commit a fully synchronized temporary file; cap at 128 entries/8 MiB; failure affects only regenerable cache data.
bool book_cache_save(const char* source, book_cache_kind_t kind, const book_cache_identity_t* identity,
                     const void* payload, size_t size);
