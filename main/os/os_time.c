/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 *
 * 中文：时间换算与格式化的纯实现，不依赖 NVS、PMU 或网络；宿主测试直接编译。
 * English: Pure time conversion and formatting without NVS, PMU or network; host tests compile it directly.
 */
#include "os_time.h"
#include <stdio.h>
#include <stdlib.h>

static os_time_info_t s_info;

// 公历天数换算（Howard Hinnant 算法），避免依赖平台 gmtime 的本地实现差异。
// Civil-date day arithmetic (Howard Hinnant) to avoid platform-local gmtime differences.
static int64_t days_from_civil(int y, int m, int d) {
    y -= m <= 2;
    int64_t era = (y >= 0 ? y : y - 399) / 400;
    unsigned yoe = (unsigned)(y - era * 400);
    unsigned doy = (unsigned)((153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1);
    unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + (int64_t)doe - 719468;
}
static void civil_from_days(int64_t z, int* y, unsigned* m, unsigned* d) {
    z += 719468;
    int64_t era = (z >= 0 ? z : z - 146096) / 146097;
    unsigned doe = (unsigned)(z - era * 146097);
    unsigned yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    int64_t yy = (int64_t)yoe + era * 400;
    unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    unsigned mp = (5 * doy + 2) / 153;
    *d = doy - (153 * mp + 2) / 5 + 1;
    *m = mp + (mp < 10 ? 3 : -9);
    *y = (int)(yy + (*m <= 2));
}

void os_time_apply(uint32_t unix_utc, int16_t tz_qh) {
    if (!os_time_tz_valid(tz_qh)) tz_qh = 32;
    os_time_info_t info = {.state = OS_TIME_UNSYNCED, .unix_utc = unix_utc, .tz_qh = tz_qh};
    if (unix_utc >= OS_TIME_UNIX_MIN && unix_utc < OS_TIME_UNIX_MAX) {
        info.state = OS_TIME_VALID;
        // 先加时区再拆日历，保证本地字段与显示一致。/ Add the timezone before splitting so local fields match the display.
        int64_t local = (int64_t)unix_utc + (int64_t)tz_qh * 15 * 60;
        int64_t days = local / 86400;
        int64_t sec = local % 86400;
        int y;
        unsigned mo, di;
        civil_from_days(days, &y, &mo, &di);
        info.year = (uint16_t)y;
        info.month = (uint8_t)mo;
        info.day = (uint8_t)di;
        info.hour = (uint8_t)(sec / 3600);
        info.minute = (uint8_t)(sec % 3600 / 60);
        info.weekday = (uint8_t)(((days % 7) + 7 + 4) % 7);
    }
    s_info = info;
}

const os_time_info_t* os_time_info(void) { return &s_info; }

void os_time_format_clock(char* out, size_t cap) {
    if (!out || !cap) return;
    if (s_info.state != OS_TIME_VALID) snprintf(out, cap, "--:--");
    else snprintf(out, cap, "%02u:%02u", s_info.hour, s_info.minute);
}

void os_time_format_date(char* out, size_t cap) {
    if (!out || !cap) return;
    if (s_info.state != OS_TIME_VALID) {
        snprintf(out, cap, "时间未校时");
        return;
    }
    // 周日为 0；表面从“一”开始排列。/ Sunday is 0; the face lists Monday first.
    static const char* names[] = {"日", "一", "二", "三", "四", "五", "六"};
    snprintf(out, cap, "%u年%u月%u日 周%s", s_info.year, s_info.month, s_info.day, names[s_info.weekday % 7]);
}

void os_time_format_tz(int16_t tz_qh, char* out, size_t cap) {
    if (!out || !cap) return;
    if (!os_time_tz_valid(tz_qh)) tz_qh = 0;
    int minutes = tz_qh * 15;
    snprintf(out, cap, "UTC%s%d:%02d", minutes < 0 ? "-" : "+", abs(minutes) / 60, abs(minutes) % 60);
}

bool os_time_date_key(uint32_t* yyyymmdd) {
    if (!yyyymmdd || s_info.state != OS_TIME_VALID) return false;
    *yyyymmdd = (uint32_t)s_info.year * 10000U + s_info.month * 100U + s_info.day;
    return true;
}

uint32_t os_time_date_shift(uint32_t yyyymmdd, int days) {
    if (yyyymmdd < 10000101U || yyyymmdd > 99991231U) return yyyymmdd;
    int y = (int)(yyyymmdd / 10000U);
    int m = (int)(yyyymmdd / 100U % 100U);
    int d = (int)(yyyymmdd % 100U);
    if (m < 1 || m > 12 || d < 1 || d > 31) return yyyymmdd;
    int64_t shifted = days_from_civil(y, m, d) + days;
    int ny;
    unsigned nm, nd;
    civil_from_days(shifted, &ny, &nm, &nd);
    if (ny < 1000 || ny > 9999) return yyyymmdd;
    return (uint32_t)ny * 10000U + nm * 100U + nd;
}

bool os_time_tz_valid(int16_t tz_qh) { return tz_qh >= OS_TIME_TZ_MIN && tz_qh <= OS_TIME_TZ_MAX; }
