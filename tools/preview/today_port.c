/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 * 中文：今日/时间页的宿主夹具：固定展示时钟与统计，不模拟 NVS、PMU 或网络。
 * English: Host fixtures for the Today/time pages: a fixed display clock and stats, never simulating NVS, PMU or network.
 * 冻结：默认夹具静态，仅显式 time_step 命令步进，用于验证阅读底栏分钟刷新；不写统计。
 * Frozen: The fixture stays static unless explicit time_step tests footer minute updates; never write stats.
 */
#include "os_time.h"

// 2026-09-30 02:09:30 UTC；默认 UTC+8 下显示 10:09 周三。/ 2026-09-30 02:09:30 UTC, shown 10:09 Wednesday at UTC+8.
#define TODAY_FIXTURE_UNIX 1790734170U

static int16_t s_tz = 32;
static int s_seconds;
void preview_time_step(int seconds) { s_seconds = seconds; }

void os_time_poll(int64_t now_ms) {
    (void)now_ms;
    os_time_apply(TODAY_FIXTURE_UNIX + s_seconds, s_tz);
}

void os_time_invalidate(void) {}
void os_time_force_poll(void) {}

void os_time_set_tz(int16_t qh) {
    s_tz = os_time_tz_valid(qh) ? qh : 32;
    os_time_apply(TODAY_FIXTURE_UNIX, s_tz);
}

void os_time_network(bool sta_uplink) { (void)sta_uplink; }
