/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 * 中文：UI 线程上的一次性书架/开书请求，路径复制且结果可查询。
 * English: One-shot shelf/open requests on the UI thread, with copied paths and queryable results.
 * 冻结：只有用户明确开书请求绕过续读询问；请求不执行 IO。
 * Frozen: Only explicit user open requests bypass resume confirmation; requests perform no IO.
 */
#pragma once
#include <stdbool.h>
#include "book_store.h"

typedef enum {
    BOOK_ENTRY_SHELF, ///< 书架，不弹续读 / Shelf without resume prompt
    BOOK_ENTRY_OPEN, ///< 用户明确打开路径 / Explicit user open
} book_entry_kind_t;
typedef enum {
    BOOK_ENTRY_IDLE, ///< 没有请求 / No request
    BOOK_ENTRY_QUEUED, ///< 等待进入书架 / Awaiting shelf entry
    BOOK_ENTRY_LOADING, ///< 已消费，正在扫描/开书 / Consumed, scanning/opening
    BOOK_ENTRY_OPENED, ///< 正文已就绪 / Reader ready
    BOOK_ENTRY_SHELF_READY, ///< 书架已就绪 / Shelf ready
    BOOK_ENTRY_NOT_FOUND, ///< 路径已失效 / Missing path
    BOOK_ENTRY_FAILED, ///< 开书失败 / Open failed
    BOOK_ENTRY_CANCELLED, ///< 离页/丢卡取消 / Cancelled on exit/media loss
} book_entry_status_t;
typedef struct {
    /// 进入语义。/ Entry intent.
    book_entry_kind_t kind;
    /// 自有路径副本。/ Owned path copy.
    char path[BOOK_STORE_PATH_MAX];
    /// 可选的原文锚点。/ Optional source anchor.
    bool has_position;
    uint16_t chapter;
    uint32_t byte_off;
} book_entry_request_t;

/// 忙时拒绝替换；仅接受书根内直接 TXT/EPUB 路径。/ Reject replacement while busy; accept only direct TXT/EPUB paths in book roots.
bool book_entry_request(book_entry_kind_t kind, const char* path);
/// 单次消费，切页前后均不借用调用方内存。/ Consume once without borrowing caller memory across transitions.
bool book_entry_take(book_entry_request_t* out);
/// 结束已消费请求；非法终态不改变状态。/ Finish a consumed request; invalid terminal states leave it unchanged.
void book_entry_finish(book_entry_status_t status);
/// 只读结果，不触发开书。/ Read the result without opening a book.
book_entry_status_t book_entry_status(void);

/// 明确开书并定位到章节字节锚点。/ Explicitly open and seek to a chapter byte anchor.
bool book_entry_request_position(const char* path, uint16_t chapter, uint32_t byte_off);
