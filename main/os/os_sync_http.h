/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 * 中文：有界 HTTP 流读写；传输、取消和期限检查由调用方提供。
 * English: Bounded HTTP stream IO; callers provide transport, cancellation and deadline checks.
 */
#pragma once
#include <stdbool.h>
#include <stddef.h>
typedef struct {
    /// 返回已写字节数，零或负数为失败。/ Return bytes written; zero or negative fails.
    int (*write)(void* ctx, const char* data, size_t size);
    /// 返回已读字节数，零为 EOF，负数为失败。/ Return bytes read, zero at EOF, negative on failure.
    int (*read)(void* ctx, char* data, size_t size);
    /// EOF 时检查消息是否完整。/ Validate message completeness at EOF.
    bool (*complete)(void* ctx);
    /// 是否仍可继续（期限和取消）。/ Whether work may continue (deadline and cancellation).
    bool (*active)(void* ctx);
    void* ctx;
} os_sync_stream_t;
/// 循环短写，失败立即停止。/ Loop over short writes and stop on failure.
bool os_sync_http_write(const os_sync_stream_t* io, const char* body);
/// 有界收齐响应；超长、截断或失败清空输出；NULL 输出则丢弃有界响应。
/// Read a bounded complete response; overflow, truncation or failure clears output; NULL discards a bounded body.
bool os_sync_http_read(const os_sync_stream_t* io, char* out, size_t cap);
