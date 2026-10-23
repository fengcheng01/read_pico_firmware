/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 * 中文：会话内有效向前阅读的有界速度样本与剩余时间格式化，不读写持久统计。
 * English: Bounded session samples of effective forward reading and ETA formatting, without persistent statistics.
 * 冻结：只接受3至120秒的正常向前翻页；至少8个样本才显示估计，不把长时间截成999分钟。
 * Frozen: Accept only normal forward turns lasting 3–120 seconds; require eight samples and never clamp long ETAs to 999 minutes.
 */
#pragma once
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    uint32_t ms[32], bytes[32]; ///< 最近32次有效向前翻页。/ Last 32 effective forward turns.
    uint64_t total_ms, total_bytes; ///< 有界窗口总量。/ Bounded window totals.
    unsigned count, next; ///< 有效数与替换位置。/ Sample count and replacement slot.
} book_eta_t;

/// 排除秒翻、挂机与非向前定位。/ Exclude quick flips, idle hangs and non-forward moves.
static inline void book_eta_sample(book_eta_t* eta, int64_t dwell, uint32_t before, uint32_t after) {
    if (dwell < 3000 || dwell > 120000 || after <= before) return;
    unsigned i = eta->next;
    eta->total_ms -= eta->ms[i]; eta->total_bytes -= eta->bytes[i];
    eta->ms[i] = (uint32_t)dwell; eta->bytes[i] = after - before;
    eta->total_ms += eta->ms[i]; eta->total_bytes += eta->bytes[i];
    eta->next = (i + 1) % 32;
    if (eta->count < 32) ++eta->count;
}

/// 无可靠样本时留空；完成显示0分，长书使用小时或天。/ Leave unknown ETA blank; show zero at completion and hours/days for long books.
static inline void book_eta_format(const book_eta_t* eta, uint32_t remaining, char* out, size_t size) {
    if (!size) return;
    out[0] = 0;
    if (eta->count < 8 || !eta->total_bytes) return;
    uint64_t denominator = eta->total_bytes * 60000;
    uint64_t minutes = ((uint64_t)remaining * eta->total_ms + denominator - 1) / denominator;
    if (minutes < 60) snprintf(out, size, " 剩%u分", (unsigned)minutes);
    else if (minutes < 1440) snprintf(out, size, " 剩%u时%02u分", (unsigned)(minutes / 60), (unsigned)(minutes % 60));
    else snprintf(out, size, " 剩%llu天%u时", (unsigned long long)(minutes / 1440), (unsigned)(minutes % 1440 / 60));
}
