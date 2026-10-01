/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 * 中文：串行单槽进入请求，不依赖页面私有状态或硬件。
 * English: Serialized single-slot entry requests, independent of page internals and hardware.
 */
#include "book_entry.h"
#include <string.h>
#include <strings.h>

static book_entry_request_t s_request;
static book_entry_status_t s_status;

// 合法路径 = 挂载书根的直接 TXT/EPUB 普通文件；根列表来自 book_store，不复制前缀字面量。
// A valid path is a direct TXT/EPUB file under a mounted book root; roots come
// from book_store instead of duplicated prefix literals.
static bool valid_path(const char* path) {
    if (!path || strnlen(path, BOOK_STORE_PATH_MAX) >= BOOK_STORE_PATH_MAX) return false;
    book_store_root_t roots[2];
    int count = 0;
    if (book_store_read_roots(roots, &count) != ESP_OK) return false;
    const char* name = NULL;
    for (int i = 0; i < count; ++i) {
        size_t len = strnlen(roots[i].path, sizeof(roots[i].path));
        if (strncmp(path, roots[i].path, len) || path[len] != '/') continue;
        name = path + len + 1;
        break;
    }
    if (!name) return false;
    if (!*name || strlen(name) > 255) return false;
    for (const unsigned char* p = (const unsigned char*)name; *p; ++p)
        if (*p < 32 || *p == 127 || *p == '/' || *p == '\\' || *p == ':') return false;
    const char* ext = strrchr(name, '.');
    return ext && (!strcasecmp(ext, ".txt") || !strcasecmp(ext, ".epub"));
}
bool book_entry_request(book_entry_kind_t kind, const char* path) {
    if (s_status == BOOK_ENTRY_QUEUED || s_status == BOOK_ENTRY_LOADING) return false;
    if (kind != BOOK_ENTRY_SHELF && kind != BOOK_ENTRY_OPEN) return false;
    if (kind == BOOK_ENTRY_OPEN && !valid_path(path)) return false;
    s_request = (book_entry_request_t){.kind = kind};
    if (kind == BOOK_ENTRY_OPEN) memcpy(s_request.path, path, strlen(path) + 1);
    s_status = BOOK_ENTRY_QUEUED;
    return true;
}
bool book_entry_take(book_entry_request_t* out) {
    if (!out || s_status != BOOK_ENTRY_QUEUED) return false;
    *out = s_request;
    memset(&s_request, 0, sizeof(s_request));
    s_status = BOOK_ENTRY_LOADING;
    return true;
}
void book_entry_finish(book_entry_status_t status) {
    if (s_status != BOOK_ENTRY_LOADING) return;
    if (status < BOOK_ENTRY_OPENED || status > BOOK_ENTRY_CANCELLED) return;
    s_status = status;
}
book_entry_status_t book_entry_status(void) { return s_status; }
