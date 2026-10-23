/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 *
 * 中文：阅读统计的 NVS 持久化。键为 st_YYYYMMDD，值为分钟；只做有界清理，
 * 不触碰其他命名空间。
 * English: NVS persistence for reading stats. Keys are st_YYYYMMDD holding
 * minutes; pruning stays bounded and never touches other namespaces.
 *
 * 冻结修订：用户授权长期归档；NVS保留最近30个记录，旧记录只有归档确认后才可删。
 * Frozen revision: User authorizes long-term archives; keep 30 recent NVS records and prune older ones only after archive confirmation.
 */
#include "book_stats.h"
#include "book_stats_archive.h"
#include "nvs.h"
#include <stdio.h>

#define BOOK_STATS_NS "rp_stats"
#define BOOK_STATS_KEY_MAX 16
static unsigned s_write_revision, s_archived_revision=~0u, s_archived_media=~0u;

static void key_of(uint32_t yyyymmdd, char* out) {
    snprintf(out, BOOK_STATS_KEY_MAX, "st_%lu", (unsigned long)yyyymmdd);
}

static bool nvs_minutes(uint32_t yyyymmdd, uint16_t* minutes) {
    if (!minutes) return false;
    char key[BOOK_STATS_KEY_MAX];
    key_of(yyyymmdd, key);
    nvs_handle_t h;
    if (nvs_open(BOOK_STATS_NS, NVS_READONLY, &h) != ESP_OK) return false;
    bool ok = nvs_get_u16(h, key, minutes) == ESP_OK;
    nvs_close(h);
    return ok;
}

bool book_stats_store_get(uint32_t date, uint16_t* minutes) {
    if (!minutes) return false;
    uint16_t local=0, archived=0;
    bool a=nvs_minutes(date,&local), b=book_stats_archive_get(date,&archived);
    *minutes=local>archived?local:archived;
    return a||b;
}

bool book_stats_store_put(uint32_t yyyymmdd, uint16_t minutes) {
    char key[BOOK_STATS_KEY_MAX];
    key_of(yyyymmdd, key);
    nvs_handle_t h;
    if (nvs_open(BOOK_STATS_NS, NVS_READWRITE, &h) != ESP_OK) return false;
    esp_err_t err = nvs_set_u16(h, key, minutes);
    if (err == ESP_OK) err = nvs_commit(h);
    nvs_close(h);
    if(err==ESP_OK)++s_write_revision;
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
    if (!book_stats_archive_ready()) return;
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
        uint16_t minutes=0;
        if (!nvs_minutes(found.dates[i],&minutes) || !book_stats_archive_merge(found.dates[i],minutes)) continue;
        key_of(found.dates[i], key);
        nvs_erase_key(h, key);
    }
    nvs_commit(h);
    nvs_close(h);
}

static bool migrate(uint32_t date, uint16_t minutes, void* ctx) {
    bool* ok=ctx;
    uint16_t archived=0;
    if (book_stats_archive_get(date,&archived) && archived>=minutes) return true;
    if (!book_stats_archive_merge(date,minutes)) { *ok=false; return false; }
    return true;
}
void book_stats_store_maintain(void) {
    if (!book_stats_archive_ready()) return;
    unsigned media=book_stats_archive_generation();
    if(s_archived_revision==s_write_revision&&s_archived_media==media)return;
    book_stats_archive_clear_error();
    bool ok=true; book_stats_store_visit(migrate,&ok);
    if (ok) { book_stats_store_prune();s_archived_revision=s_write_revision;s_archived_media=media; }
}
typedef struct {uint32_t from,to,total;} range_t;
static bool range_extra(uint32_t date,uint16_t minutes,void* ctx) {
    range_t* r=ctx;
    if (date>=r->from&&date<=r->to) {
        uint16_t archived=0;book_stats_archive_get(date,&archived);
        if (minutes>archived)r->total+=minutes-archived;
    }
    return true;
}
uint32_t book_stats_store_total(uint32_t from,uint32_t to) {
    range_t r={from,to,book_stats_archive_total(from,to)};
    book_stats_store_visit(range_extra,&r);return r.total;
}

void book_stats_store_visit(bool (*fn)(uint32_t yyyymmdd, uint16_t minutes, void* ctx), void* ctx) {
    if (!fn) return;
    nvs_handle_t h;
    if (nvs_open(BOOK_STATS_NS, NVS_READONLY, &h) != ESP_OK) return;
    nvs_iterator_t it = NULL;
    esp_err_t res = nvs_entry_find("nvs", BOOK_STATS_NS, NVS_TYPE_U16, &it);
    while (res == ESP_OK && it) {
        nvs_entry_info_t info = {0};
        nvs_entry_info(it, &info);
        unsigned long date = 0;
        uint16_t minutes = 0;
        char extra;
        if (sscanf(info.key, "st_%lu%c", &date, &extra) == 1 && date >= 10000101UL && date <= 99991231UL &&
            nvs_get_u16(h, info.key, &minutes) == ESP_OK && !fn((uint32_t)date, minutes, ctx)) {
            nvs_release_iterator(it);
            break;
        }
        res = nvs_entry_next(&it);
    }
    nvs_close(h);
}
