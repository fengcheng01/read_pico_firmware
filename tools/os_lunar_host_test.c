/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 *
 * 农历表锚点测试：春节、端午、中秋与闰月，全部来自公开历法事实。
 * Lunar table anchor tests: Spring Festival, Dragon Boat, Mid-Autumn and leap
 * months, all pinned to publicly known calendar facts.
 */
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "os_lunar.h"

// 锚点：公历 → 农历年/月/闰/日。/ Anchors: solar → lunar year/month/leap/day.
static const struct {
    uint16_t year;
    uint8_t month, day;
    uint16_t want_year;
    uint8_t want_month, want_day;
    bool want_leap;
    const char* note;
} cases[] = {
    {2024, 2, 10, 2024, 1, 1, false, "春节 2024 · 甲辰龙年"},
    {2025, 1, 29, 2025, 1, 1, false, "春节 2025 · 乙巳蛇年"},
    {2026, 2, 17, 2026, 1, 1, false, "春节 2026 · 丙午马年"},
    {2023, 6, 22, 2023, 5, 5, false, "端午 2023 · 癸卯兔年"},
    {2025, 10, 6, 2025, 8, 15, false, "中秋 2025"},
    {2026, 10, 1, 2026, 8, 21, false, "国庆 2026 · 中秋后六天"},
    {2020, 5, 23, 2020, 4, 1, true, "闰四月初一 2020"},
    {2025, 7, 25, 2025, 6, 1, true, "闰六月初一 2025"},
};

int main(void) {
    for (unsigned i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        os_lunar_date_t got = {0};
        assert(os_lunar_from_solar(cases[i].year, cases[i].month, cases[i].day, &got));
        assert(got.year == cases[i].want_year);
        assert(got.month == cases[i].want_month);
        assert(got.day == cases[i].want_day);
        assert(got.leap == cases[i].want_leap);
        printf("ok %s -> %u-%u-%u%s\n", cases[i].note, got.year, got.month, got.day, got.leap ? " (闰)" : "");
    }
    // 干支与生肖。/ Ganzhi and zodiac.
    char ganzhi[8] = {0};
    os_lunar_year_ganzhi(2026, ganzhi);
    assert(strcmp(ganzhi, "丙午") == 0);
    assert(strcmp(os_lunar_zodiac(2026), "马") == 0);
    os_lunar_year_ganzhi(2024, ganzhi);
    assert(strcmp(ganzhi, "甲辰") == 0);
    assert(strcmp(os_lunar_zodiac(2024), "龙") == 0);
    // 月/日名。/ Month and day names.
    assert(strcmp(os_lunar_month_name(1, false), "正月") == 0);
    assert(strcmp(os_lunar_month_name(11, false), "冬月") == 0);
    assert(strcmp(os_lunar_month_name(12, false), "腊月") == 0);
    assert(strcmp(os_lunar_month_name(6, true), "闰六月") == 0);
    assert(strcmp(os_lunar_day_name(1), "初一") == 0);
    assert(strcmp(os_lunar_day_name(11), "十一") == 0);
    assert(strcmp(os_lunar_day_name(20), "二十") == 0);
    assert(strcmp(os_lunar_day_name(21), "廿一") == 0);
    assert(strcmp(os_lunar_day_name(30), "三十") == 0);
    // 摘要串。/ Summary string.
    char line[48];
    os_lunar_format(line, sizeof(line), &(os_lunar_date_t){.year = 2026, .month = 8, .day = 21});
    assert(strcmp(line, "丙午马年 八月廿一") == 0);
    // 非法与表外输入不改 out。/ Invalid and out-of-table inputs leave out unchanged.
    os_lunar_date_t keep = {9, 9, 9, 9};
    os_lunar_date_t probe = keep;
    assert(!os_lunar_from_solar(2026, 2, 30, &probe) && probe.year == 9);
    assert(!os_lunar_from_solar(1899, 6, 1, &probe) && probe.year == 9);
    assert(!os_lunar_from_solar(2101, 6, 1, &probe) && probe.year == 9);
    assert(!os_lunar_from_solar(2026, 13, 1, &probe) && probe.year == 9);
    printf("os_lunar ok\n");
    return 0;
}
