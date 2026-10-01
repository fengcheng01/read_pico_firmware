/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 * 中文：阅读时长累计、空闲截断、跨日归日与有界落盘的回归，存储用内存替身。
 * English: Regressions for reading accumulation, idle cutoff, date rollover and bounded persistence via an in-memory store.
 */
#include "book_stats.h"
#include <assert.h>
#include <stdio.h>

typedef struct {
    uint32_t date;
    uint16_t minutes;
    bool used;
} slot_t;
static slot_t s_slots[64];
static unsigned s_puts, s_prunes;

static slot_t* find(uint32_t date) {
    for (unsigned i = 0; i < sizeof(s_slots) / sizeof(s_slots[0]); ++i)
        if (s_slots[i].used && s_slots[i].date == date) return &s_slots[i];
    return NULL;
}
bool book_stats_store_get(uint32_t date, uint16_t* minutes) {
    slot_t* slot = find(date);
    if (!slot) return false;
    *minutes = slot->minutes;
    return true;
}
bool book_stats_store_put(uint32_t date, uint16_t minutes) {
    ++s_puts;
    slot_t* slot = find(date);
    if (!slot) {
        for (unsigned i = 0; i < sizeof(s_slots) / sizeof(s_slots[0]); ++i)
            if (!s_slots[i].used) { slot = &s_slots[i]; slot->date = date; slot->used = true; break; }
    }
    if (!slot) return false;
    slot->minutes = minutes;
    return true;
}
void book_stats_store_prune(void) { ++s_prunes; }
void book_stats_store_visit(bool (*fn)(uint32_t, uint16_t, void*), void* ctx) {
    for (unsigned i = 0; i < sizeof(s_slots) / sizeof(s_slots[0]); ++i)
        if (s_slots[i].used && !fn(s_slots[i].date, s_slots[i].minutes, ctx)) return;
}

int main(void) {
    const uint32_t day = 20260930, next = 20261001;
    // 连续阅读 10 分钟：30 秒一拍。/ Ten minutes of continuous reading in 30 s samples.
    book_stats_observe(0, true, true, day);
    for (int64_t t = 30000; t <= 600000; t += 30000) book_stats_observe(t, true, true, day);
    assert(book_stats_minutes(day) == 10);
    assert(book_stats_total_minutes(20260924, day) == 10);
    // 每新增分钟才落盘：10 分钟数据最多 11 次写。/ Writes only on new minutes: at most 11 puts for 10 minutes.
    assert(s_puts <= 11 && s_prunes >= 1);

    // 空闲超过阈值的时间不计入。/ Idle gaps beyond the cutoff never count.
    book_stats_observe(600000 + 20 * 60 * 1000, true, true, day);
    assert(book_stats_minutes(day) == 10);

    // 书架/目录视图不计时。/ Shelf or TOC views never count.
    int64_t t = 600000 + 20 * 60 * 1000;
    book_stats_observe(t, false, true, day);
    book_stats_observe(t + 120000, false, true, day);
    book_stats_observe(t + 120000, true, true, day);
    book_stats_observe(t + 180000, true, true, day);
    assert(book_stats_minutes(day) == 11);

    // 未校时期间不累计也不归日，恢复后继续当日桶。/ Uncalibrated time suspends; a valid date resumes the same bucket.
    book_stats_observe(t + 240000, true, false, 0);
    book_stats_observe(t + 300000, true, false, 0);
    assert(book_stats_minutes(day) == 11);
    book_stats_observe(t + 360000, true, true, day);
    book_stats_observe(t + 420000, true, true, day);
    assert(book_stats_minutes(day) == 13);

    // 跨日：旧日落盘，新日从零开始并能在恢复后接续。/ Rollover flushes the old date and starts the new one at zero.
    book_stats_observe(t + 480000, true, true, next);
    assert(book_stats_minutes(day) == 13 && book_stats_minutes(next) == 1);
    book_stats_flush();
    assert(find(next)->minutes == 1);
    assert(book_stats_total_minutes(20260924, next) == 14);
    assert(book_stats_total_minutes(20261001, 20261001) == 1);
    assert(book_stats_total_minutes(next, day) == 0);

    // 单拍上限：阈值内的大间隔也只计一分钟。/ The per-sample cap folds large in-cutoff gaps to one minute.
    book_stats_observe(t + 480000 + 240000, true, true, next);
    assert(book_stats_minutes(next) == 2);

    puts("book_stats: accumulation, cutoffs, rollover and bounded writes passed");
    return 0;
}
