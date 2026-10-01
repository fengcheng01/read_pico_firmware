/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 * 中文：时间换算、校验窗口与格式化的纯回归，不涉及设备 IO。
 * English: Pure regressions for time conversion, validity windows and formatting without device IO.
 */
#include "os_time.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

int main(void) {
    // 2026-09-30 00:00:00 UTC = 1790726400，当天是周三。/ 2026-09-30 00:00:00 UTC, a Wednesday.
    const uint32_t base = 1790726400U;
    char text[48];

    os_time_apply(base, 32);
    const os_time_info_t* info = os_time_info();
    assert(info->state == OS_TIME_VALID);
    assert(info->year == 2026 && info->month == 9 && info->day == 30 && info->weekday == 3);
    assert(info->hour == 8 && info->minute == 0);
    os_time_format_clock(text, sizeof(text)); assert(!strcmp(text, "08:00"));
    os_time_format_date(text, sizeof(text)); assert(!strcmp(text, "2026年9月30日 周三"));
    uint32_t key = 0;
    assert(os_time_date_key(&key) && key == 20260930);

    // 半时区与负时区的日期回退。/ Half-hour zones and negative-zone date rollback.
    os_time_apply(base, 2);
    info = os_time_info();
    assert(info->hour == 0 && info->minute == 30 && info->day == 30);
    os_time_apply(base, -32);
    info = os_time_info();
    assert(info->day == 29 && info->hour == 16);
    assert(os_time_date_key(&key) && key == 20260929);

    // 时区文本。/ Timezone text.
    os_time_format_tz(32, text, sizeof(text)); assert(!strcmp(text, "UTC+8:00"));
    os_time_format_tz(22, text, sizeof(text)); assert(!strcmp(text, "UTC+5:30"));
    os_time_format_tz(-12, text, sizeof(text)); assert(!strcmp(text, "UTC-3:00"));
    os_time_format_tz(0, text, sizeof(text)); assert(!strcmp(text, "UTC+0:00"));

    // 未校时与超窗时间。/ Uncalibrated and out-of-window stamps.
    os_time_apply(0, 32);
    info = os_time_info();
    assert(info->state == OS_TIME_UNSYNCED && !os_time_date_key(&key));
    os_time_format_clock(text, sizeof(text)); assert(!strcmp(text, "--:--"));
    os_time_format_date(text, sizeof(text)); assert(!strcmp(text, "时间未校时"));
    os_time_apply(OS_TIME_UNIX_MIN - 1, 32);
    assert(os_time_info()->state == OS_TIME_UNSYNCED);
    os_time_apply(OS_TIME_UNIX_MAX, 32);
    assert(os_time_info()->state == OS_TIME_UNSYNCED);

    // 非法时区回落 UTC+8。/ Invalid zones fall back to UTC+8.
    os_time_apply(base, 48);
    info = os_time_info();
    assert(info->tz_qh == 32 && info->hour == 8);

    // 星期与闰年日期平移。/ Weekdays and leap-aware date shifts.
    assert(os_time_date_shift(20260930, -6) == 20260924);
    assert(os_time_date_shift(20260301, -1) == 20260228);
    assert(os_time_date_shift(20240301, -1) == 20240229);
    assert(os_time_date_shift(20260101, -1) == 20251231);
    assert(os_time_date_shift(20261231, 1) == 20270101);
    assert(os_time_date_shift(20261231, 1000) == 20290926);
    assert(os_time_date_shift(0, 5) == 0);
    assert(os_time_date_shift(99999999, 5) == 99999999);

    assert(os_time_tz_valid(47) && os_time_tz_valid(-47) && !os_time_tz_valid(48) && !os_time_tz_valid(-48));

    puts("os_time: zones, weekdays, windows, shifts and formats passed");
    return 0;
}
