/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 * 中文：首页目录扫描，四条候选记录与一个目录句柄构成固定预算。
 * English: Home directory scan with a fixed budget of four candidates and one directory handle.
 */
#include "book_home.h"
#include "book_progress.h"
#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>

static book_home_snapshot_t s_snapshot;
static bool s_valid, s_scanning;
static uint32_t s_store_revision, s_progress_revision;
bool book_home_cached(void) {
    if (!s_valid || s_store_revision != book_store_revision()) return false;
    if (s_progress_revision != book_progress_revision()) return false;
    return true;
}
void book_home_invalidate(void) { s_valid = false; }
static book_store_root_t s_roots[2];
static int s_root_count, s_root;
static DIR* s_dir;
static char s_last[BOOK_STORE_PATH_MAX];
static book_home_item_t s_candidates[BOOK_HOME_RECENT_MAX + 1];
static unsigned s_candidates_count;
static book_home_item_t s_cursor;
static int s_direction;
static unsigned s_recent_page;
static bool s_move;

void book_home_cancel(void) {
    s_scanning = false;
    if (s_dir) { closedir(s_dir); s_dir = NULL; }
    if (!s_snapshot.complete) s_valid = false;
    s_snapshot.complete = true;
}
void book_home_begin(void) {
    if (book_home_cached() || (s_scanning && s_store_revision == book_store_revision() &&
        s_progress_revision == book_progress_revision())) return;
    book_home_cancel();
    s_valid = false;
    s_scanning = true;
    s_store_revision = book_store_revision();
    s_progress_revision = book_progress_revision();
    if (!s_move) { s_cursor = (book_home_item_t){0}; s_direction = 0; s_recent_page = 0; }
    s_move = false;
    memset(&s_snapshot, 0, sizeof(s_snapshot));
    memset(s_candidates, 0, sizeof(s_candidates));
    s_candidates_count = 0;
    s_root = s_root_count = 0;
    if (!s_cursor.path[0]) book_progress_last_path(s_last, sizeof(s_last));
    esp_err_t err = book_store_read_roots(s_roots, &s_root_count);
    s_snapshot.degraded = err != ESP_OK || book_store_roots_degraded();
}
static bool before(const book_home_item_t* a, const book_home_item_t* b) {
    return a->sequence != b->sequence ? a->sequence > b->sequence : strcmp(a->path, b->path) < 0;
}
static void consider(const book_home_item_t* item) {
    if (!strcmp(item->path, s_last)) return;
    if (s_cursor.path[0] && (s_direction > 0 ? !before(&s_cursor, item) : !before(item, &s_cursor))) return;
    unsigned at = 0;
    while (at < s_candidates_count && (s_direction < 0 ? !before(&s_candidates[at], item) : !before(item, &s_candidates[at]))) ++at;
    if (at >= BOOK_HOME_RECENT_MAX + 1) return;
    if (s_candidates_count < BOOK_HOME_RECENT_MAX + 1) ++s_candidates_count;
    for (unsigned i = s_candidates_count - 1; i > at; --i) s_candidates[i] = s_candidates[i - 1];
    s_candidates[at] = *item;
}
static void finish(void) {
    if (!s_snapshot.current.path[0] && s_candidates_count && !s_recent_page) {
        s_snapshot.current = s_candidates[0];
        memmove(s_candidates, s_candidates + 1, --s_candidates_count * sizeof(*s_candidates));
    }
    unsigned n = s_candidates_count > BOOK_HOME_RECENT_MAX ? BOOK_HOME_RECENT_MAX : s_candidates_count;
    for (unsigned i = 0; i < n; ++i)
        s_snapshot.recent[s_snapshot.recent_count++] = s_candidates[s_direction < 0 ? n - i - 1 : i];
    s_snapshot.recent_page = s_recent_page;
    s_snapshot.recent_more = s_snapshot.history_count - (s_snapshot.current.has_progress ? 1u : 0u) >
        (s_recent_page + 1) * BOOK_HOME_RECENT_MAX;
    s_snapshot.complete = true;
    s_valid = true;
    s_scanning = false;
}
bool book_home_recent_move(int direction) {
    if (!book_home_cached() || !s_snapshot.complete || !s_snapshot.recent_count ||
        (direction > 0 ? !s_snapshot.recent_more : direction < 0 ? !s_recent_page : true)) return false;
    snprintf(s_last, sizeof(s_last), "%s", s_snapshot.current.path);
    s_cursor = s_snapshot.recent[direction > 0 ? s_snapshot.recent_count - 1 : 0];
    s_direction = direction;
    s_recent_page = direction > 0 ? s_recent_page + 1 : s_recent_page - 1;
    s_valid = false;
    s_move = true;
    book_home_begin();
    return true;
}
bool book_home_step(void) {
    if (s_snapshot.complete) return true;
    for (unsigned budget = 0; budget < 4; ++budget) {
        if (!s_dir) {
            if (s_root >= s_root_count) { finish(); return true; }
            s_dir = opendir(s_roots[s_root].path);
            if (!s_dir) { s_snapshot.degraded = true; ++s_root; continue; }
        }
        errno = 0;
        struct dirent* ent = readdir(s_dir);
        if (!ent) {
            if (errno) s_snapshot.degraded = true;
            closedir(s_dir); s_dir = NULL; ++s_root;
            continue;
        }
        const char* ext = strrchr(ent->d_name, '.');
        if (!ext || (strcasecmp(ext, ".txt") && strcasecmp(ext, ".epub"))) continue;
        book_home_item_t item = {0};
        int n = snprintf(item.path, sizeof(item.path), "%s/%s", s_roots[s_root].path, ent->d_name);
        if (n < 0 || (size_t)n >= sizeof(item.path)) { s_snapshot.degraded = true; continue; }
        struct stat st;
        if (stat(item.path, &st)) { s_snapshot.degraded = true; continue; }
        if (!S_ISREG(st.st_mode)) continue;
        if (st.st_size < 0 || (uint64_t)st.st_size > UINT32_MAX ||
            (s_roots[s_root].is_flash && (uint64_t)st.st_size > BOOK_STORE_FLASH_FILE_MAX)) continue;
        if (s_snapshot.book_count < UINT_MAX) ++s_snapshot.book_count;
        size_t len = (size_t)(ext - ent->d_name);
        if (len >= sizeof(item.title)) len = sizeof(item.title) - 1;
        memcpy(item.title, ent->d_name, len);
        if (!len) snprintf(item.title, sizeof(item.title), "%s", ent->d_name);
        book_progress_t progress;
        item.has_progress = book_progress_load(item.path, (uint32_t)st.st_size, &progress);
        if (item.has_progress) {
            ++s_snapshot.history_count;
            item.sequence = progress.last_open_s;
            item.percent = progress.pct > 100 ? 100 : progress.pct;
            consider(&item);
        }
        if (!strcmp(item.path, s_last)) s_snapshot.current = item;
    }
    return false;
}
const book_home_snapshot_t* book_home_snapshot(void) { return &s_snapshot; }
