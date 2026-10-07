/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 * 中文：正文摘录与章节字节锚点，独立于书签和阅读进度。
 * English: Body excerpts with chapter byte anchors, independent of bookmarks and progress.
 * 冻结：UI 线程串行调用；保存失败保留旧记录，不擦分区。
 * Frozen: Serialized UI callers; failed saves retain old records, never erase partitions.
 */
#pragma once
#include "book_store.h"
#define BOOK_QUOTES_MAX 16
#define BOOK_QUOTE_TEXT_MAX 384
typedef struct {
    /// 自有文件路径与 UTF-8 摘录。/ Owned file path and UTF-8 excerpt.
    char path[BOOK_STORE_PATH_MAX], text[BOOK_QUOTE_TEXT_MAX];
    /// 章节、句首与句尾字节锚点。/ Chapter, sentence start and end byte anchors.
    uint16_t chapter;
    uint32_t byte_off, end_off;
    /// 保存时的全书进度与排序序号。/ Whole-book progress at save time and ordering sequence.
    uint8_t pct;
    uint32_t seq;
} book_quote_t;
/// 最近摘录优先；只返回完整可验证记录。/ Newest excerpts first; return only complete validated records.
size_t book_quotes_list(book_quote_t* out, size_t cap);
/// 同句更新；满员替换最旧摘录，失败可重试。/ Update the same sentence; replace the oldest when full, allow retry on failure.
esp_err_t book_quotes_add(const book_quote_t* quote);
/// 本次开机的摘录变化序号。/ Excerpt revision for this boot.
unsigned book_quotes_revision(void);
/// 按命中文字取所在句子的有界完整 UTF-8 范围。/ Select a bounded complete UTF-8 sentence around a hit byte.
bool book_quote_sentence(const char* text, size_t len, size_t hit, size_t* start, size_t* end);
