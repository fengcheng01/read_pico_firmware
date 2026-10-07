/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 *
 * 中文：从两次可信网络时间与实际浅睡累计学习走时比例，只修正睡眠时段。
 * English: Learn sleep-only clock rates from two trusted network anchors and accumulated light sleep.
 *
 * 冻结：不使用用户估计或PMU漂移推算补偿；至少一小时且主要睡眠的样本才生效。
 * Frozen: Never derive compensation from user estimates or PMU drift; require at least an hour dominated by sleep.
 */
#pragma once
#include <stdbool.h>
#include <stdint.h>

/// 纯计算状态；比例单位ppm，累计修正保持亚毫秒精度。/ Pure computation state; rates are ppm and accumulated correction retains submillisecond precision.
typedef struct {
    int32_t ppm;
    int64_t sleep_us, correction_scaled;
    int64_t sample_utc_ms, sample_tick_ms, sample_sleep_us;
    bool sampled;
    int64_t window_ms, window_sleep_us, error_ms;
    enum { OS_CLOCK_FIRST, OS_CLOCK_SHORT, OS_CLOCK_AWAKE, OS_CLOCK_OUTLIER, OS_CLOCK_LEARNED } result;
} os_clock_rate_t;

static inline void os_clock_rate_sleep(os_clock_rate_t* c, int64_t us) {
    if (us <= 0 || us > 86400000000LL) return;
    c->sleep_us += us;
    c->correction_scaled += us * c->ppm;
}

static inline int64_t os_clock_rate_ms(const os_clock_rate_t* c, int64_t tick_ms) {
    return tick_ms + c->correction_scaled / 1000000000LL;
}

/// 更新网络样本，合格时返回已测得的新比例；异常样本也重置观察窗口。/ Update the network sample and return true for a qualified measured rate; anomalous samples restart observation too.
static inline bool os_clock_rate_sync(os_clock_rate_t* c, int64_t utc_ms, int64_t tick_ms) {
    bool learned = false;
    c->result = OS_CLOCK_FIRST;
    if (c->sampled) {
        int64_t elapsed = tick_ms - c->sample_tick_ms;
        int64_t slept = c->sleep_us - c->sample_sleep_us;
        c->window_ms = elapsed; c->window_sleep_us = slept;
        c->error_ms = utc_ms - c->sample_utc_ms - elapsed;
        // 短样本保留起点，频繁校时也能累计到完整学习窗口。
        // Keep the anchor for short samples so frequent syncs can accumulate a full learning window.
        if (elapsed >= 0 && elapsed < 3600000) { c->result = OS_CLOCK_SHORT; return false; }
        c->result = OS_CLOCK_AWAKE;
        if (elapsed >= 3600000 && elapsed <= 7LL * 86400000 &&
            slept >= 1800000000LL && slept / 1000 >= elapsed / 2) {
            int64_t error = utc_ms - c->sample_utc_ms - elapsed;
            c->result = OS_CLOCK_OUTLIER;
            // 先限误差再乘，网络大幅跳变不得产生溢出或错误比例。
            // Bound errors before multiplication so large network steps cannot overflow or train a false rate.
            if (error >= -elapsed / 50 && error <= elapsed / 50) {
                int64_t ppm = error * 1000000000LL / slept;
                if (ppm >= -10000 && ppm <= 10000) {
                    c->ppm = (int32_t)ppm;
                    learned = true;
                    c->result = OS_CLOCK_LEARNED;
                }
            }
        }
    }
    c->sampled = true;
    c->sample_utc_ms = utc_ms; c->sample_tick_ms = tick_ms; c->sample_sleep_us = c->sleep_us;
    return learned;
}
