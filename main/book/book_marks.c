/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 *
 * 中文：版本化小端书签记录，独立 rp_marks 命名空间；以完整路径校验哈希碰撞，
 * 淘汰与更新都在整条记录内完成，失败保留旧记录。
 * English: Versioned little-endian bookmark records in the rp_marks namespace;
 * full paths guard hash collisions, eviction and updates rewrite the whole
 * record, and failures keep the previous one.
 */
#include "book_marks.h"
#include "book_store.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "esp_log.h"
#include "nvs.h"

#define PATH_CAP BOOK_STORE_PATH_MAX
#define MAGIC_SIZE 8
#define ENTRY_SIZE 11
#define RECORD_CAP (MAGIC_SIZE + BOOK_MARKS_MAX * ENTRY_SIZE + 2 + PATH_CAP)
static const char* TAG = "book_marks";
static const char* NS = "rp_marks";

static bool path_valid(const char* path) {
    return path != NULL && path[0] != '\0' && strnlen(path, PATH_CAP) < PATH_CAP;
}

static void make_key(const char* path, char key[11]) {
    uint32_t hash = UINT32_C(2166136261);
    for (const unsigned char* p = (const unsigned char*)path; *p; ++p) {
        hash = (hash ^ *p) * UINT32_C(16777619);
    }
    snprintf(key, 11, "m_%08" PRIx32, hash);
}

static uint32_t get32(const uint8_t* p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static void put32(uint8_t* p, uint32_t value) {
    for (unsigned i = 0; i < 4; ++i) p[i] = (uint8_t)(value >> (8 * i));
}

static void write_entry(uint8_t* p, const book_mark_t* mark) {
    p[0] = (uint8_t)mark->chapter;
    p[1] = (uint8_t)(mark->chapter >> 8);
    put32(p + 2, mark->byte_off);
    p[6] = mark->pct;
    put32(p + 7, mark->seq);
}

static book_mark_t read_entry(const uint8_t* p) {
    return (book_mark_t){
        .chapter = (uint16_t)(p[0] | (p[1] << 8)),
        .byte_off = get32(p + 2),
        .pct = p[6],
        .seq = get32(p + 7),
    };
}

// 记录布局：magic8 + entries(count*11) + pathlen2 + path\0。
// Record layout: magic8 + entries(count*11) + pathlen2 + path\0.
static bool decode(const uint8_t* data, size_t len, const char* path,
                   book_mark_t out[], size_t cap, size_t* count) {
    if (len < MAGIC_SIZE + 2 + 2 || memcmp(data, "RPM", 3) != 0 || data[3] != 1) return false;
    size_t n = data[4];
    if (n > BOOK_MARKS_MAX || len < MAGIC_SIZE + n * ENTRY_SIZE + 3) return false;
    size_t path_len = (size_t)data[MAGIC_SIZE + n * ENTRY_SIZE] |
                      ((size_t)data[MAGIC_SIZE + n * ENTRY_SIZE + 1] << 8);
    if (path_len < 2 || path_len > PATH_CAP || len != MAGIC_SIZE + n * ENTRY_SIZE + 2 + path_len) return false;
    const char* stored = (const char*)data + MAGIC_SIZE + n * ENTRY_SIZE + 2;
    if (strnlen(stored, path_len) != path_len - 1 || strcmp(stored, path) != 0) return false;
    if (count) *count = n;
    if (!out) return true;
    for (size_t i = 0; i < n && i < cap; ++i)
        out[i] = read_entry(data + MAGIC_SIZE + i * ENTRY_SIZE);
    return true;
}

static esp_err_t warn_error(esp_err_t err) {
    if (err != ESP_OK) ESP_LOGW(TAG, "marks persistence: %s", esp_err_to_name(err));
    return err;
}

bool book_marks_list(const char* path, book_mark_t out[], size_t cap, size_t* n) {
    if (n) *n = 0;
    if (!path_valid(path)) return false;
    nvs_handle_t h;
    if (nvs_open(NS, NVS_READONLY, &h) != ESP_OK) return false;
    char key[11];
    make_key(path, key);
    uint8_t data[RECORD_CAP];
    size_t len = sizeof(data);
    esp_err_t err = nvs_get_blob(h, key, data, &len);
    nvs_close(h);
    size_t count = 0;
    if (err != ESP_OK || !decode(data, len, path, out, cap, &count)) return false;
    if (n) *n = count;
    return true;
}

// 既有记录必须属于同一路径；未知版本或碰撞一律拒绝覆盖。
// Existing records must belong to the same path; unknown versions or collisions refuse to overwrite.
static esp_err_t check_owner(nvs_handle_t h, const char* key, const char* path) {
    uint8_t data[RECORD_CAP];
    size_t len = sizeof(data);
    esp_err_t err = nvs_get_blob(h, key, data, &len);
    if (err == ESP_ERR_NVS_NOT_FOUND) return ESP_OK;
    if (err != ESP_OK) return err;
    return decode(data, len, path, NULL, 0, NULL) ? ESP_OK : ESP_ERR_INVALID_STATE;
}

static esp_err_t store(nvs_handle_t h, const char* key, const char* path,
                       const book_mark_t marks[], size_t count) {
    uint8_t data[RECORD_CAP] = {'R', 'P', 'M', 1, (uint8_t)count, 0, 0, 0};
    for (size_t i = 0; i < count; ++i) write_entry(data + MAGIC_SIZE + i * ENTRY_SIZE, &marks[i]);
    size_t at = MAGIC_SIZE + count * ENTRY_SIZE;
    size_t path_len = strlen(path) + 1;
    data[at] = (uint8_t)path_len;
    data[at + 1] = (uint8_t)(path_len >> 8);
    memcpy(data + at + 2, path, path_len);
    esp_err_t err = nvs_set_blob(h, key, data, at + 2 + path_len);
    if (err == ESP_OK) err = nvs_commit(h);
    return err;
}

esp_err_t book_marks_add(const char* path, uint16_t chapter, uint32_t byte_off, uint8_t pct) {
    if (!path_valid(path) || pct > 100) return ESP_ERR_INVALID_ARG;
    char key[11];
    make_key(path, key);
    nvs_handle_t h;
    esp_err_t err = nvs_open(NS, NVS_READWRITE, &h);
    if (err != ESP_OK) return warn_error(err);
    book_mark_t marks[BOOK_MARKS_MAX];
    size_t count = 0;
    if (check_owner(h, key, path) == ESP_OK) {
        uint8_t data[RECORD_CAP];
        size_t len = sizeof(data);
        if (nvs_get_blob(h, key, data, &len) == ESP_OK)
            (void)decode(data, len, path, marks, BOOK_MARKS_MAX, &count);
    } else {
        nvs_close(h);
        return warn_error(ESP_ERR_INVALID_STATE);
    }
    uint32_t seq = 0;
    err = nvs_get_u32(h, "seq", &seq);
    if (err == ESP_ERR_NVS_NOT_FOUND) err = ESP_OK;
    if (err != ESP_OK) { nvs_close(h); return warn_error(err); }
    if (seq == UINT32_MAX) { nvs_close(h); return warn_error(ESP_ERR_INVALID_STATE); }
    // 先提交序号再写记录；失败可留空洞，重启后不倒序（与进度序号同策略）。
    // Commit the sequence before the record; failures may leave gaps but never reverse order.
    err = nvs_set_u32(h, "seq", ++seq);
    if (err == ESP_OK) err = nvs_commit(h);
    if (err != ESP_OK) { nvs_close(h); return warn_error(err); }
    // 原位更新或按位置插入。/ Update in place or insert by position.
    size_t at = count;
    for (size_t i = 0; i < count; ++i) {
        if (marks[i].chapter == chapter && marks[i].byte_off == byte_off) { at = i; break; }
        if (marks[i].chapter > chapter ||
            (marks[i].chapter == chapter && marks[i].byte_off > byte_off)) { at = i; break; }
    }
    book_mark_t mark = {.chapter = chapter, .byte_off = byte_off, .pct = pct, .seq = seq};
    if (at < count && marks[at].chapter == chapter && marks[at].byte_off == byte_off) {
        marks[at] = mark;  // 原位更新不增容量。/ In-place update keeps the count.
    } else {
        if (count == BOOK_MARKS_MAX) {
            // 满员淘汰最旧。/ Evict the oldest when full.
            size_t oldest = 0;
            for (size_t i = 1; i < count; ++i) if (marks[i].seq < marks[oldest].seq) oldest = i;
            if (oldest + 1 < count) memmove(&marks[oldest], &marks[oldest + 1], (count - oldest - 1) * sizeof(*marks));
            --count;
            if (oldest < at) --at;
        }
        memmove(&marks[at + 1], &marks[at], (count - at) * sizeof(*marks));
        marks[at] = mark;
        ++count;
    }
    err = store(h, key, path, marks, count);
    nvs_close(h);
    return warn_error(err);
}

esp_err_t book_marks_remove_at(const char* path, size_t index) {
    if (!path_valid(path)) return ESP_ERR_INVALID_ARG;
    char key[11];
    make_key(path, key);
    nvs_handle_t h;
    esp_err_t err = nvs_open(NS, NVS_READWRITE, &h);
    if (err != ESP_OK) return warn_error(err);
    err = check_owner(h, key, path);
    book_mark_t marks[BOOK_MARKS_MAX];
    size_t count = 0;
    if (err == ESP_OK) {
        uint8_t data[RECORD_CAP];
        size_t len = sizeof(data);
        if (nvs_get_blob(h, key, data, &len) == ESP_OK)
            (void)decode(data, len, path, marks, BOOK_MARKS_MAX, &count);
    }
    if (err != ESP_OK || index >= count) { nvs_close(h); return err == ESP_OK ? warn_error(ESP_ERR_INVALID_SIZE) : warn_error(err); }
    if (count > 1) {
        memmove(&marks[index], &marks[index + 1], (count - index - 1) * sizeof(*marks));
        err = store(h, key, path, marks, count - 1);
    } else {
        err = nvs_erase_key(h, key);
        if (err == ESP_ERR_NVS_NOT_FOUND) err = ESP_OK;
        if (err == ESP_OK) err = nvs_commit(h);
    }
    nvs_close(h);
    return warn_error(err);
}

esp_err_t book_marks_forget(const char* path) {
    if (!path_valid(path)) return ESP_ERR_INVALID_ARG;
    char key[11];
    make_key(path, key);
    nvs_handle_t h;
    esp_err_t err = nvs_open(NS, NVS_READWRITE, &h);
    if (err != ESP_OK) return warn_error(err);
    err = check_owner(h, key, path);
    if (err == ESP_OK) {
        err = nvs_erase_key(h, key);
        if (err == ESP_ERR_NVS_NOT_FOUND) err = ESP_OK;
        if (err == ESP_OK) err = nvs_commit(h);
    }
    nvs_close(h);
    return warn_error(err);
}
