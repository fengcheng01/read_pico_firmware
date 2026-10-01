/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 * 中文：同步流处理，拒绝部分响应和超长响应。
 * English: Sync stream handling that rejects partial and oversized responses.
 */
#include "os_sync_http.h"
#include <string.h>
bool os_sync_http_write(const os_sync_stream_t* io, const char* body) {
    size_t size = body ? strlen(body) : 0, at = 0;
    while (at < size) {
        if (!io->active(io->ctx)) return false;
        int n = io->write(io->ctx, body + at, size - at);
        if (n <= 0 || (size_t)n > size - at) return false;
        at += (size_t)n;
    }
    return true;
}
bool os_sync_http_read(const os_sync_stream_t* io, char* out, size_t cap) {
    char chunk[128];
    size_t used = 0;
    if (out) { if (!cap) return false; out[0] = 0; }
    size_t limit = out ? cap - 1 : 4096;
    for (;;) {
        if (!io->active(io->ctx)) break;
        int n = io->read(io->ctx, chunk, sizeof(chunk));
        if (n < 0 || (size_t)n > sizeof(chunk)) break;
        if (!n) {
            if (!io->complete(io->ctx)) break;
            if (out) out[used] = 0;
            return true;
        }
        if ((size_t)n > limit - used) break;
        if (out) memcpy(out + used, chunk, (size_t)n);
        used += (size_t)n;
    }
    if (out) out[0] = 0;
    return false;
}
