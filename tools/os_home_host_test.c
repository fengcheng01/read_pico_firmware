/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 * 中文：真实首页扫描与一次性入口的文件夹隔离回归，不接触用户书库。
 * English: Isolated-folder regressions for real home scanning and one-shot entry, without user-library access.
 */
#include "book_home.h"
#include "book_entry.h"
#include "book_progress.h"
#include <assert.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static book_store_root_t s_roots[2];
static int s_count = 2;
static bool s_degraded;
static char s_last[BOOK_STORE_PATH_MAX];
static unsigned s_loads;
esp_err_t book_store_read_roots(book_store_root_t out[2], int* n) {
    memcpy(out, s_roots, sizeof(s_roots)); *n = s_count; return ESP_OK;
}
bool book_store_roots_degraded(void) { return s_degraded; }
bool book_progress_last_path(char* out, size_t cap) {
    snprintf(out, cap, "%s", s_last); return *out != 0;
}
bool book_progress_load(const char* path, uint32_t size, book_progress_t* out) {
    ++s_loads;
    const char* name = strrchr(path, '/'); unsigned seq;
    if (!name || sscanf(name + 1, "book%u.txt", &seq) != 1 || size != 1) return false;
    *out = (book_progress_t){.file_size = size, .last_open_s = seq, .pct = seq > 60 ? 255 : 38, .px = 48};
    return true;
}
static void file(const char* root, const char* name, size_t bytes) {
    char path[BOOK_STORE_PATH_MAX]; snprintf(path, sizeof(path), "%s/%s", root, name);
    FILE* f = fopen(path, "wb"); assert(f); assert(ftruncate(fileno(f), (off_t)bytes) == 0); assert(fclose(f) == 0);
}
static const book_home_snapshot_t* scan(void) {
    book_home_begin(); unsigned calls = 0;
    while (!book_home_step()) assert(++calls < 100);
    assert(calls > 0); return book_home_snapshot();
}
int main(int argc, char** argv) {
    assert(argc == 2 && strlen(argv[1]) + 7 < sizeof(s_roots[0].path));
    snprintf(s_roots[0].path, sizeof(s_roots[0].path), "%s/sd", argv[1]);
    snprintf(s_roots[1].path, sizeof(s_roots[1].path), "%s/flash", argv[1]);
    s_roots[1].is_flash = true;
    assert(mkdir(s_roots[0].path, 0700) == 0); assert(mkdir(s_roots[1].path, 0700) == 0);
    for (unsigned i = 1; i <= 65; ++i) { char name[24]; snprintf(name, sizeof(name), "book%03u.txt", i); file(s_roots[0].path, name, 1); }
    file(s_roots[0].path, "unread.epub", 1); file(s_roots[0].path, "ignored.bin", 1);
    file(s_roots[1].path, "too-large.txt", BOOK_STORE_FLASH_FILE_MAX + 1);
    file(s_roots[1].path, "small.txt", 1);
    book_home_begin(); unsigned initial_loads = s_loads;
    assert(!book_home_step() && !book_home_snapshot()->complete && s_loads - initial_loads <= 16);
    book_home_cancel(); unsigned cancelled_loads = s_loads;
    assert(book_home_step() && s_loads == cancelled_loads);
    const book_home_snapshot_t* data = scan();
    assert(data->book_count == 67 && !data->degraded && data->recent_count == 3);
    assert(strstr(data->current.path, "book065.txt") && data->current.percent == 100);
    assert(strstr(data->recent[0].path, "book064.txt") && strstr(data->recent[2].path, "book062.txt"));
    snprintf(s_last, sizeof(s_last), "%s/book005.txt", s_roots[0].path);
    data = scan(); assert(strstr(data->current.path, "book005.txt") && strstr(data->recent[0].path, "book065.txt"));
    snprintf(s_last, sizeof(s_last), "%s/book065.txt", s_roots[0].path);
    file(s_roots[0].path, "book065.txt", 2);
    data = scan(); assert(!data->current.has_progress && data->current.percent == 0 && strstr(data->recent[0].path, "book064.txt"));
    strcpy(s_last, "/missing/book.txt");
    data = scan(); assert(strstr(data->current.path, "book064.txt"));
    s_degraded = true; data = scan(); assert(data->degraded && data->current.path[0]);
    s_count = 0; book_home_begin(); assert(book_home_step()); data = book_home_snapshot();
    assert(!data->book_count && !data->recent_count && !data->current.path[0]);
    s_count = 1; snprintf(s_roots[0].path, sizeof(s_roots[0].path), "%s/missing", argv[1]);
    book_home_begin(); assert(book_home_step() && book_home_snapshot()->degraded);

    // 恢复桩根为 sd 目录：上面的扫描用例把它改成了 missing。
    // Restore the stub root to the sd dir: earlier scan cases repointed it to missing.
    s_count = 1;
    snprintf(s_roots[0].path, sizeof(s_roots[0].path), "%s/sd", argv[1]);
    char root_sd[BOOK_STORE_PATH_MAX];
    snprintf(root_sd, sizeof(root_sd), "%s", s_roots[0].path);
    const char* invalid[] = {NULL, "", "/other/book.txt", "book.txt",
                             "/sdcard/books/中文.epub", "/flash/books/x.bin",
                             "/sdcard/books/../book.txt", "/sdcard/books/a\\b.txt", "/sdcard/books/a\nb.txt"};
    for (unsigned i = 0; i < sizeof(invalid) / sizeof(*invalid); ++i) assert(!book_entry_request(BOOK_ENTRY_OPEN, invalid[i]));
    assert(book_entry_status() == BOOK_ENTRY_IDLE);
    char path[BOOK_STORE_PATH_MAX];
    snprintf(path, sizeof(path), "%s/中文.epub", root_sd);
    assert(book_entry_request(BOOK_ENTRY_OPEN, path)); memset(path, 'x', sizeof(path));
    assert(!book_entry_request(BOOK_ENTRY_SHELF, NULL));
    book_entry_request_t request;
    assert(!book_entry_take(NULL) && book_entry_take(&request));
    book_entry_finish(BOOK_ENTRY_IDLE); assert(book_entry_status() == BOOK_ENTRY_LOADING);
    book_entry_finish(BOOK_ENTRY_OPENED); assert(book_entry_status() == BOOK_ENTRY_OPENED);
    assert(book_entry_request(BOOK_ENTRY_SHELF, NULL) && book_entry_take(&request) && !request.path[0]);
    book_entry_finish(BOOK_ENTRY_SHELF_READY);
    puts("os_home: bounded scan, real progress, fallbacks, cancellation and typed entry passed");
    return 0;
}
