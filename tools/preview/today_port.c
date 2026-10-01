/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 * 中文：今日/时间页的宿主夹具：固定展示时钟与统计，不模拟 NVS、PMU 或网络。
 * English: Host fixtures for the Today/time pages: a fixed display clock and stats, never simulating NVS, PMU or network.
 * 冻结：夹具时间静态不走动；不提供校时或统计写入通道。
 * Frozen: The fixture clock is static; no calibration or stats-write path exists.
 */
#include "os_time.h"

// 2026-09-30 02:09:30 UTC；默认 UTC+8 下显示 10:09 周三。/ 2026-09-30 02:09:30 UTC, shown 10:09 Wednesday at UTC+8.
#define TODAY_FIXTURE_UNIX 1790734170U

static int16_t s_tz = 32;

void os_time_poll(int64_t now_ms) {
    (void)now_ms;
    os_time_apply(TODAY_FIXTURE_UNIX, s_tz);
}

void os_time_invalidate(void) {}
void os_time_force_poll(void) {}

void os_time_set_tz(int16_t qh) {
    s_tz = os_time_tz_valid(qh) ? qh : 32;
    os_time_apply(TODAY_FIXTURE_UNIX, s_tz);
}

void os_time_network(bool sta_uplink) { (void)sta_uplink; }
