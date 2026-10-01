/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 *
 * 中文：按本地日聚合的阅读时长统计。时间戳来自调用方注入的单调毫秒，日期来自
 * 有效本地时间；未校时期间不归日、不累计。
 * English: Reading-time totals aggregated by local date. Timestamps are injected
 * monotonic milliseconds; dates come from valid local time; uncalibrated periods
 * are neither attributed nor accumulated.
 *
 * 冻结：空闲超过 5 分钟截断，不把挂机当阅读；分钟粒度落盘，只在有新增分钟时写；
 * 不用开机时长冒充阅读时长。
 * Frozen: Idle gaps over 5 minutes cut off and never count as reading; persist
 * in minute granularity and only on new minutes; uptime never impersonates
 * reading time.
 */
#pragma once
#include <stdbool.h>
#include <stdint.h>

/// 空闲截断阈值。/ Idle cutoff.
#define BOOK_STATS_IDLE_MS (5LL * 60LL * 1000LL)
/// 单次采样最多计入的间隔，防扫描/卡顿一次性灌入。/ Per-sample cap so scans or stalls cannot dump time at once.
#define BOOK_STATS_SAMPLE_MAX_MS 60000LL
/// 保留最近 30 天。/ Keep the most recent 30 days.
#define BOOK_STATS_KEEP_DAYS 30

/// 每轮 tick 调用；reading 表示当前处于正文阅读视图。/ Call every tick; reading means the reader view is active.
void book_stats_observe(int64_t now_ms, bool reading, bool date_valid, uint32_t yyyymmdd);
/// 立即落盘当前累计；离页、睡眠前调用。/ Flush accumulated time now; call on page exit and before sleep.
void book_stats_flush(void);
/// 某日分钟数；当日含未落盘累计。/ Minutes for a date; today includes unflushed time.
uint16_t book_stats_minutes(uint32_t yyyymmdd);
/// 闭区间日期范围内的总分钟数。/ Total minutes within the inclusive date range.
uint32_t book_stats_total_minutes(uint32_t from_yyyymmdd, uint32_t to_yyyymmdd);

/* 持久化接口由设备侧 book_stats_store.c 实现；宿主测试提供自己的实现。
 * Persistence implemented by book_stats_store.c on device; host tests supply their own. */
/// 读某日分钟；无记录返回 false。/ Read a day's minutes; false when absent.
bool book_stats_store_get(uint32_t yyyymmdd, uint16_t* minutes);
/// 写某日分钟；失败返回 false。/ Write a day's minutes; false on failure.
bool book_stats_store_put(uint32_t yyyymmdd, uint16_t minutes);
/// 只保留最近 BOOK_STATS_KEEP_DAYS 个日期记录。/ Keep only the newest BOOK_STATS_KEEP_DAYS dated records.
void book_stats_store_prune(void);
/// 遍历全部记录，fn 返回 false 提前结束。/ Visit all records; fn returning false stops early.
void book_stats_store_visit(bool (*fn)(uint32_t yyyymmdd, uint16_t minutes, void* ctx), void* ctx);
