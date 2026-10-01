/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 *
 * 中文：1900-2100 农历年表与公历换算。年表是流传的公共农历数据表，
 * 位编码：[3:0] 闰月序号（0 无闰）、bit16 闰月天数（1=30）、[16:1] 右移为
 * 1..12 月大月标记（1=30 天）。锚点已由宿主测试对齐春节与中秋。
 * English: 1900-2100 lunar year table and solar conversion. The table is the
 * widely published public lunar dataset; bits: [3:0] leap month (0 none),
 * bit16 leap-month length (1=30 days), the remaining bits shifted right give
 * big-month flags for months 1..12 (1=30 days). Host tests pin the table
 * against Spring Festival and Mid-Autumn anchors.
 */
#include "os_lunar.h"

#include <stdio.h>
#include <string.h>

#define LUNAR_FIRST_YEAR 1900
#define LUNAR_LAST_YEAR 2100

// 基准：1900-01-31 为农历 1900 年正月初一。/ Epoch: 1900-01-31 is lunar 1900-01-01.
static const uint32_t k_lunar_info[] = {
    0x04bd8, 0x04ae0, 0x0a570, 0x054d5, 0x0d260, 0x0d950, 0x16554, 0x056a0, 0x09ad0, 0x055d2,
    0x04ae0, 0x0a5b6, 0x0a4d0, 0x0d250, 0x1d255, 0x0b540, 0x0d6a0, 0x0ada2, 0x095b0, 0x14977,
    0x04970, 0x0a4b0, 0x0b4b5, 0x06a50, 0x06d40, 0x1ab54, 0x02b60, 0x09570, 0x052f2, 0x04970,
    0x06566, 0x0d4a0, 0x0ea50, 0x06e95, 0x05ad0, 0x02b60, 0x186e3, 0x092e0, 0x1c8d7, 0x0c950,
    0x0d4a0, 0x1d8a6, 0x0b550, 0x056a0, 0x1a5b4, 0x025d0, 0x092d0, 0x0d2b2, 0x0a950, 0x0b557,
    0x06ca0, 0x0b550, 0x15355, 0x04da0, 0x0a5b0, 0x14573, 0x052b0, 0x0a9a8, 0x0e950, 0x06aa0,
    0x0aea6, 0x0ab50, 0x04b60, 0x0aae4, 0x0a570, 0x05260, 0x0f263, 0x0d950, 0x05b57, 0x056a0,
    0x096d0, 0x04dd5, 0x04ad0, 0x0a4d0, 0x0d4d4, 0x0d250, 0x0d558, 0x0b540, 0x0b6a0, 0x195a6,
    0x095b0, 0x049b0, 0x0a974, 0x0a4b0, 0x0b27a, 0x06a50, 0x06d40, 0x0af46, 0x0ab60, 0x09570,
    0x04af5, 0x04970, 0x064b0, 0x074a3, 0x0ea50, 0x06b58, 0x055c0, 0x0ab60, 0x096d5, 0x092e0,
    0x0c960, 0x0d954, 0x0d4a0, 0x0da50, 0x07552, 0x056a0, 0x0abb7, 0x025d0, 0x092d0, 0x0cab5,
    0x0a950, 0x0b4a0, 0x0baa4, 0x0ad50, 0x055d9, 0x04ba0, 0x0a5b0, 0x15176, 0x052b0, 0x0a930,
    0x07954, 0x06aa0, 0x0ad50, 0x05b52, 0x04b60, 0x0a6e6, 0x0a4e0, 0x0d260, 0x0ea65, 0x0d530,
    0x05aa0, 0x076a3, 0x096d0, 0x04afb, 0x04ad0, 0x0a4d0, 0x1d0b6, 0x0d250, 0x0d520, 0x0dd45,
    0x0b5a0, 0x056d0, 0x055b2, 0x049b0, 0x0a577, 0x0a4b0, 0x0aa50, 0x1b255, 0x06d20, 0x0ada0,
    0x14b63, 0x09370, 0x049f8, 0x04970, 0x064b0, 0x168a6, 0x0ea50, 0x06b20, 0x1a6c4, 0x0aae0,
    0x0a2e0, 0x0d2e3, 0x0c960, 0x0d557, 0x0d4a0, 0x0da50, 0x05d55, 0x056a0, 0x0a6d0, 0x055d4,
    0x052d0, 0x0a9b8, 0x0a950, 0x0b4a0, 0x0b6a6, 0x0ad50, 0x055a0, 0x0aba4, 0x0a5b0, 0x052b0,
    0x0b273, 0x06930, 0x07337, 0x06aa0, 0x0ad50, 0x14b55, 0x04b60, 0x0a570, 0x054e4, 0x0d160,
    0x0e968, 0x0d520, 0x0daa0, 0x16aa6, 0x056d0, 0x04ae0, 0x0a9d4, 0x0a2d0, 0x0d150, 0x0f252,
    0x0d520,
};

static const char* const k_gan[] = {"甲", "乙", "丙", "丁", "戊", "己", "庚", "辛", "壬", "癸"};
static const char* const k_zhi[] = {"子", "丑", "寅", "卯", "辰", "巳", "午", "未", "申", "酉", "戌", "亥"};
static const char* const k_zodiac[] = {"鼠", "牛", "虎", "兔", "龙", "蛇", "马", "羊", "猴", "鸡", "狗", "猪"};
static const char* const k_months[] = {"正", "二", "三", "四", "五", "六", "七", "八", "九", "十", "冬", "腊"};
static const char* const k_day_ones[] = {"一", "二", "三", "四", "五", "六", "七", "八", "九", "十"};

static int leap_month_of(int year) { return (int)(k_lunar_info[year - LUNAR_FIRST_YEAR] & 0xf); }
static int leap_month_days(int year) {
    return leap_month_of(year) ? ((k_lunar_info[year - LUNAR_FIRST_YEAR] & 0x10000) ? 30 : 29) : 0;
}
static int month_days_of(int year, int month) {
    return (k_lunar_info[year - LUNAR_FIRST_YEAR] & (0x10000 >> month)) ? 30 : 29;
}
static int year_days_of(int year) {
    int days = leap_month_days(year);
    for (int month = 1; month <= 12; ++month) days += month_days_of(year, month);
    return days;
}

// 公历日期到 1969-12-31 前一天的天数（proleptic Gregorian，无库依赖）。
// Days since 1969-12-31 for a proleptic Gregorian date; no library dependency.
static int64_t days_from_civil(int64_t y, unsigned m, unsigned d) {
    y -= m <= 2;
    const int64_t era = (y >= 0 ? y : y - 399) / 400;
    const unsigned yoe = (unsigned)(y - era * 400);
    const unsigned doy = (153u * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + (int64_t)doe - 719468;
}

static bool solar_valid(uint16_t year, uint8_t month, uint8_t day) {
    if (year < LUNAR_FIRST_YEAR || year > LUNAR_LAST_YEAR || month < 1 || month > 12 || day < 1)
        return false;
    static const uint8_t lengths[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    uint8_t len = lengths[month - 1];
    if (month == 2 && year % 4 == 0 && (year % 100 != 0 || year % 400 == 0)) len = 29;
    return day <= len;
}

bool os_lunar_from_solar(uint16_t year, uint8_t month, uint8_t day, os_lunar_date_t* out) {
    if (!out || !solar_valid(year, month, day)) return false;
    // 相对基准 1900-01-31 的天数。/ Days since the 1900-01-31 epoch.
    int64_t offset = days_from_civil(year, month, day) - days_from_civil(1900, 1, 31);
    if (offset < 0) return false;
    int lunar_year = LUNAR_FIRST_YEAR;
    for (;;) {
        int days = year_days_of(lunar_year);
        if (offset < days) break;
        offset -= days;
        ++lunar_year;
        if (lunar_year > LUNAR_LAST_YEAR) return false;
    }
    // 按正月..十二月枚举，闰M月紧跟M月之后。/ Enumerate 正月..十二月 with 闰M right after M.
    int leap = leap_month_of(lunar_year);
    int remaining = (int)offset;
    for (int m = 1; m <= 12; ++m) {
        int days = month_days_of(lunar_year, m);
        if (remaining < days) {
            *out = (os_lunar_date_t){.year = (uint16_t)lunar_year, .month = (uint8_t)m, .leap = false,
                                     .day = (uint8_t)(remaining + 1)};
            return true;
        }
        remaining -= days;
        if (leap == m) {
            days = leap_month_days(lunar_year);
            if (remaining < days) {
                *out = (os_lunar_date_t){.year = (uint16_t)lunar_year, .month = (uint8_t)m, .leap = true,
                                         .day = (uint8_t)(remaining + 1)};
                return true;
            }
            remaining -= days;
        }
    }
    return false;
}

// 年表从 1900 起，干支用 (年-4) 取模即可。/ The table starts at 1900, so (year-4) modulo is always safe.
void os_lunar_year_ganzhi(uint16_t lunar_year, char out[8]) {
    if (!out) return;
    snprintf(out, 8, "%s%s", k_gan[(lunar_year - 4) % 10], k_zhi[(lunar_year - 4) % 12]);
}

const char* os_lunar_zodiac(uint16_t lunar_year) {
    return k_zodiac[(lunar_year - 4) % 12];
}

const char* os_lunar_month_name(uint8_t month, bool leap) {
    static char name[16];
    if (month < 1 || month > 12) return "";
    snprintf(name, sizeof(name), "%s%s月", leap ? "闰" : "", k_months[month - 1]);
    return name;
}

const char* os_lunar_day_name(uint8_t day) {
    static char name[16];
    if (day < 1 || day > 30) return "";
    // 初一..初十、十一..十九、二十、廿一..廿九、三十。/ 初一..初十, 十一..十九, 二十, 廿一..廿九, 三十.
    if (day <= 10) snprintf(name, sizeof(name), "初%s", k_day_ones[day - 1]);
    else if (day < 20) snprintf(name, sizeof(name), "十%s", k_day_ones[day - 11]);
    else if (day == 20) snprintf(name, sizeof(name), "二十");
    else if (day < 30) snprintf(name, sizeof(name), "廿%s", k_day_ones[day - 21]);
    else snprintf(name, sizeof(name), "三十");
    return name;
}

void os_lunar_format(char* out, size_t cap, const os_lunar_date_t* date) {
    if (!out || !cap) return;
    if (!date || date->month < 1 || date->month > 12 || date->day < 1 || date->day > 30) {
        out[0] = 0;
        return;
    }
    char ganzhi[8];
    os_lunar_year_ganzhi(date->year, ganzhi);
    snprintf(out, cap, "%s%s年 %s%s", ganzhi, os_lunar_zodiac(date->year),
             os_lunar_month_name(date->month, date->leap), os_lunar_day_name(date->day));
}
