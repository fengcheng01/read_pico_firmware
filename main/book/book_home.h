/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 * 中文：首页真实阅读摘要；固定内存，分批扫描，不解析正文或封面。
 * English: Real home reading summaries; fixed memory, incremental scans, no text or cover parsing.
 * 冻结：最近顺序使用现有持久序号，不冒充日期；不写进度、不格式化。
 * Frozen: Recent order uses existing persistent sequences, never dates; no progress writes or formatting.
 */
#pragma once
#include "book_store.h"

#define BOOK_HOME_RECENT_MAX 3
typedef struct {
    /// 文件标识。/ File identity.
    char path[BOOK_STORE_PATH_MAX];
    /// 文件名回退，不解析 EPUB。/ Filename fallback without EPUB parsing.
    char title[256];
    /// 阅读顺序，非时间。/ Reading order, not time.
    uint32_t sequence;
    /// 已保存的全书进度。/ Saved whole-book progress.
    uint8_t percent;
    /// 进度有效。/ Valid progress.
    bool has_progress;
} book_home_item_t;
typedef struct {
    /// last 路径优先，失效则回退最近有效记录。/ Last path first, fallback to latest valid progress.
    book_home_item_t current;
    /// 不含当前书。/ Excludes current book.
    book_home_item_t recent[BOOK_HOME_RECENT_MAX];
    /// 有界摘要与可用文件数。/ Bounded summaries and available file count.
    unsigned recent_count, book_count;
    /// 扫描完成与部分来源失败。/ Scan finished and partial-source failure.
    bool complete, degraded;
} book_home_snapshot_t;

/// UI 线程开始/停止扫描；只查询已有书目录，允许无卡。/ Start/stop on UI thread; query existing roots, tolerate absent media.
void book_home_begin(void);
void book_home_cancel(void);
/// 每次最多检查 16 个目录项；完成后不再访问文件。/ Inspect at most 16 entries per call; no file access after completion.
bool book_home_step(void);
/// 只读快照；未完成时不得据此发起开书。/ Read-only snapshot; never open from incomplete data.
const book_home_snapshot_t* book_home_snapshot(void);
