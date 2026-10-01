/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 *
 * 中文：按书籍路径保存的 NVS 书签列表，最多 16 条，按章节内位置排序，
 * 满员淘汰最旧；与进度独立，「清进度」保留书签，「删文件」须遗忘书签。
 * English: Per-path NVS bookmark list, at most 16 entries sorted by in-chapter
 * position, evicting the oldest when full; independent of progress — clearing
 * progress keeps bookmarks while deleting the file must forget them.
 *
 * 冻结：不擦除分区、不覆盖碰撞路径；调用方（UI 线程）串行化所有写操作。
 * Frozen: Never erase partitions or overwrite colliding paths; the UI thread
 * serializes all writes.
 */
#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"

/// 每本书的书签上限。/ Per-book bookmark capacity.
#define BOOK_MARKS_MAX 16

typedef struct {
    /// 章节索引。/ Chapter index.
    uint16_t chapter;
    /// 章节内 UTF-8 字节偏移。/ UTF-8 byte offset within the chapter.
    uint32_t byte_off;
    /// 全书百分比。/ Whole-book percentage.
    uint8_t pct;
    /// 插入序号；满员时最小者被淘汰。/ Insertion sequence; the smallest is evicted when full.
    uint32_t seq;
} book_mark_t;

/// 读取书签（已按位置排序）；返回是否读到记录（无记录时 n 置 0）。
/// Load bookmarks (position-sorted); true when a record exists, with n zeroed when absent.
bool book_marks_list(const char* path, book_mark_t out[], size_t cap, size_t* n);
/// 新增或原位更新书签（同章节同偏移只刷新百分比与序号）；满员淘汰最旧。
/// Add or update in place (same chapter and offset refresh percent and sequence); evict the oldest when full.
esp_err_t book_marks_add(const char* path, uint16_t chapter, uint32_t byte_off, uint8_t pct);
/// 删除指定序号（0 起，以最近一次 list 顺序为准）的书签。/ Remove the bookmark at index (0-based, per the latest list order).
esp_err_t book_marks_remove_at(const char* path, size_t index);
/// 遗忘此路径的全部书签；删除文件时与进度一起清理。/ Forget all bookmarks for this path; file deletion cleans them with progress.
esp_err_t book_marks_forget(const char* path);
