/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 *
 * 中文：公历→农历的纯换算（1900-2100 表），输出农历月日、干支年与生肖；
 * 不读硬件、不依赖时间缓存，调用方自行校验时间有效后再用。
 * English: Pure solar→lunar conversion over the 1900-2100 table, producing the
 * lunar month/day, the ganzhi year and the zodiac; no hardware reads and no
 * time-cache dependency — callers validate time before use.
 *
 * 冻结：不伪造农历；表外年份一律失败，不做外推。
 * Frozen: Never invent lunar dates; years outside the table fail without extrapolation.
 */
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    /// 农历年（以正月初一为界）。/ Lunar year bounded by Spring Festival.
    uint16_t year;
    /// 农历月 1..12。/ Lunar month 1..12.
    uint8_t month;
    /// 闰月标记。/ Leap-month marker.
    bool leap;
    /// 农历日 1..30。/ Lunar day 1..30.
    uint8_t day;
} os_lunar_date_t;

/// 公历转农历；非法日期或表外年份返回 false 且不改 out。/ Convert solar to lunar; invalid dates or out-of-table years return false and leave out unchanged.
bool os_lunar_from_solar(uint16_t year, uint8_t month, uint8_t day, os_lunar_date_t* out);
/// 干支年，如 "丙午"；year 为农历年。/ Ganzhi year such as 丙午 for a lunar year.
void os_lunar_year_ganzhi(uint16_t lunar_year, char out[8]);
/// 生肖名，如 "马"。/ Zodiac name such as 马.
const char* os_lunar_zodiac(uint16_t lunar_year);
/// 农历月名："正月".."腊月"，闰月前缀 "闰"。/ Lunar month name 正月..腊月 with a 闰 prefix for leap months.
const char* os_lunar_month_name(uint8_t month, bool leap);
/// 农历日名："初一".."三十"。/ Lunar day name 初一..三十.
const char* os_lunar_day_name(uint8_t day);
/// 一行农历摘要："丙午马年 八月廿一"。/ One-line summary such as 丙午马年 八月廿一.
void os_lunar_format(char* out, size_t cap, const os_lunar_date_t* date);
