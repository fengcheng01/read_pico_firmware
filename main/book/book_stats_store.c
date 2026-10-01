/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 *
 * 中文：阅读统计的 NVS 持久化。键为 st_YYYYMMDD，值为分钟；只做有界清理，
 * 不触碰其他命名空间。
 * English: NVS persistence for reading stats. Keys are st_YYYYMMDD holding
 * minutes; pruning stays bounded and never touches other namespaces.
 *
 * 冻结：读写失败静默返回，统计是尽力而为的数据；不擦除命名空间。
 * Frozen: Failures return quietly since stats are best-effort; never erase the namespace.
 */
#include "book_stats.h"
#include "nvs.h"
#include <stdio.h>

#define BOOK_STATS_NS "rp_stats"
#define BOOK_STATS_KEY_MAX 16

static void key_of(uint32_t yyyymmdd, char* out) {
    snprintf(out, BOOK_STATS_KEY_MAX, "st_%lu", (unsigned long)yyyymmdd);
}

bool book_stats_store_get(uint32_t yyyymmdd, uint16_t* minutes) {
    if (!minutes) return false;
    char key[BOOK_STATS_KEY_MAX];
    key_of(yyyymmdd, key);
    nvs_handle_t h;
    if (nvs_open(BOOK_STATS_NS, NVS_READONLY, &h) != ESP_OK) return false;
    bool ok = nvs_get_u16(h, key, minutes) == ESP_OK;
    nvs_close(h);
    return ok;
}

bool book_stats_store_put(uint32_t yyyymmdd, uint16_t minutes) {
    char key[BOOK_STATS_KEY_MAX];
    key_of(yyyymmdd, key);
    nvs_handle_t h;
    if (nvs_open(BOOK_STATS_NS, NVS_READWRITE, &h) != ESP_OK) return false;
    esp_err_t err = nvs_set_u16(h, key, minutes);
    if (err == ESP_OK) err = nvs_commit(h);
    nvs_close(h);
    return err == ESP_OK;
}

typedef struct {
    uint32_t dates[128];
    size_t count;
} collect_ctx_t;

static bool collect(uint32_t yyyymmdd, uint16_t minutes, void* ctx) {
    // 零分钟记录顺带清理。/ Zero-minute records are collected for deletion too.
    (void)minutes;
    collect_ctx_t* found = ctx;
    if (found->count < sizeof(found->dates) / sizeof(found->dates[0])) found->dates[found->count++] = yyyymmdd;
    return true;
}

void book_stats_store_prune(void) {
    collect_ctx_t found = {0};
    book_stats_store_visit(collect, &found);
    if (found.count <= BOOK_STATS_KEEP_DAYS) return;
    // 简单选择排序取最旧的记录删除，记录数很小。/ A tiny selection sort finds the oldest entries; the record count is small.
    for (size_t i = 0; i < found.count - BOOK_STATS_KEEP_DAYS; ++i) {
        size_t oldest = i;
        for (size_t j = i + 1; j < found.count; ++j)
            if (found.dates[j] < found.dates[oldest]) oldest = j;
        uint32_t tmp = found.dates[i];
        found.dates[i] = found.dates[oldest];
        found.dates[oldest] = tmp;
    }
    nvs_handle_t h;
    if (nvs_open(BOOK_STATS_NS, NVS_READWRITE, &h) != ESP_OK) return;
    for (size_t i = 0; i < found.count - BOOK_STATS_KEEP_DAYS; ++i) {
        char key[BOOK_STATS_KEY_MAX];
        key_of(found.dates[i], key);
        nvs_erase_key(h, key);
    }
    nvs_commit(h);
    nvs_close(h);
}

void book_stats_store_visit(bool (*fn)(uint32_t yyyymmdd, uint16_t minutes, void* ctx), void* ctx) {
    if (!fn) return;
    nvs_handle_t h;
    if (nvs_open(BOOK_STATS_NS, NVS_READONLY, &h) != ESP_OK) return;
    nvs_iterator_t it = NULL;
    esp_err_t res = nvs_entry_find(BOOK_STATS_NS, NULL, NVS_TYPE_U16, &it);
    while (res == ESP_OK && it) {
        nvs_entry_info_t info;
        nvs_entry_info(it, &info);
        res = nvs_entry_next(&it);
        uint32_t date = 0;
        uint16_t minutes = 0;
        if (sscanf(info.key, "st_%lu", (unsigned long*)&date) == 1 && date >= 10000101UL && date <= 99991231UL &&
            nvs_get_u16(h, info.key, &minutes) == ESP_OK && !fn(date, minutes, ctx)) {
            nvs_release_iterator(it);
            break;
        }
        res = nvs_entry_next(&it);
    }
    nvs_close(h);
}
