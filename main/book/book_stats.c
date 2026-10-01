/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 *
 * 中文：阅读时长的内存累计与按日落盘；所有状态只有一份，单线程调用。
 * English: In-memory reading accumulation with per-date persistence; one state set, single-threaded callers.
 */
#include "book_stats.h"

static bool s_active;
static int64_t s_last_ms;
static uint32_t s_date;
static uint32_t s_pending_ms;
static uint16_t s_stored_minutes;

static void store_date(uint32_t yyyymmdd) {
    uint16_t minutes = (uint16_t)(s_pending_ms / 60000);
    if (minutes == s_stored_minutes) return;
    if (!book_stats_store_put(yyyymmdd, minutes)) return;
    s_stored_minutes = minutes;
    book_stats_store_prune();
}

static void enter_date(uint32_t yyyymmdd) {
    if (yyyymmdd == s_date) return;
    book_stats_flush();
    s_date = yyyymmdd;
    uint16_t minutes = 0;
    s_stored_minutes = book_stats_store_get(yyyymmdd, &minutes) ? minutes : 0;
    s_pending_ms = (uint32_t)s_stored_minutes * 60000U;
}

void book_stats_observe(int64_t now_ms, bool reading, bool date_valid, uint32_t yyyymmdd) {
    if (!date_valid) {
        // 未校时：先落盘当前归日，再挂起累计等待有效日期。/ Uncalibrated: flush the current bucket, then suspend until a valid date returns.
        book_stats_flush();
        s_date = 0;
        s_last_ms = now_ms;
        s_active = reading;
        return;
    }
    enter_date(yyyymmdd);
    int64_t delta = now_ms - s_last_ms;
    // 上一拍与这一拍都在阅读且间隔未超空闲阈值才计入；阈值内的大间隔按单拍上限折算。
    // Count only when both samples read with a sub-cutoff gap; larger in-cutoff gaps fold into the per-sample cap.
    if (s_active && reading && delta > 0 && delta <= BOOK_STATS_IDLE_MS) {
        if (delta > BOOK_STATS_SAMPLE_MAX_MS) delta = BOOK_STATS_SAMPLE_MAX_MS;
        s_pending_ms += (uint32_t)delta;
        store_date(yyyymmdd);
    }
    s_last_ms = now_ms;
    s_active = reading;
}

void book_stats_flush(void) {
    if (s_date) store_date(s_date);
}

uint16_t book_stats_minutes(uint32_t yyyymmdd) {
    if (yyyymmdd == s_date && s_date) return (uint16_t)(s_pending_ms / 60000);
    uint16_t minutes = 0;
    return book_stats_store_get(yyyymmdd, &minutes) ? minutes : 0;
}

typedef struct {
    uint32_t from, to;
    uint32_t total;
} range_ctx_t;

static bool range_add(uint32_t yyyymmdd, uint16_t minutes, void* ctx) {
    range_ctx_t* range = ctx;
    if (yyyymmdd >= range->from && yyyymmdd <= range->to) range->total += minutes;
    return true;
}

uint32_t book_stats_total_minutes(uint32_t from_yyyymmdd, uint32_t to_yyyymmdd) {
    if (from_yyyymmdd > to_yyyymmdd) return 0;
    range_ctx_t range = {.from = from_yyyymmdd, .to = to_yyyymmdd};
    book_stats_store_visit(range_add, &range);
    if (s_date >= from_yyyymmdd && s_date <= to_yyyymmdd) {
        // 存储里已含今日落盘部分，这里只补未落盘的差值。/ The store already holds today's flushed minutes; add only the unflushed remainder.
        uint32_t live = s_pending_ms / 60000;
        range.total += live > s_stored_minutes ? live - s_stored_minutes : 0;
    }
    return range.total;
}
