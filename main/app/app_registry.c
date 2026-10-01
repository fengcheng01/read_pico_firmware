/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 *
 * 全部页面的唯一目录。产品入口在前，诊断页保留；稳定 ID 不依赖菜单序号。
 *
 * Single page catalog: product entries first, diagnostics retained; stable IDs do not depend on menu indices.
 * 冻结：用户确认阅读优先，普通开机进正在读；工厂标定与自检续跑优先级不变。
 * Frozen: user-approved reading-first boot enters Now reading; retain factory calibration and self-test resume priority.
 * 用户要求完整产品效果：四根入口固定在目录头部；测试页只能从设置中的诊断目录进入。
 * User-requested product experience: four roots lead the catalog; test pages are reached through Settings diagnostics.
 * 用户修订：设置子项指向产品页（阅读/时间/睡眠/存储）；原睡眠、TF 演示页改挂 OS_APP_NONE 归入诊断。
 * User revision: Settings children point at product pages (reading/time/sleep/storage); the sleep and TF demos carry OS_APP_NONE under diagnostics.
 */

#include "app_registry.h"

extern const app_desc_t app_home;
extern const app_desc_t app_os_home;
extern const app_desc_t app_os_today;
extern const app_desc_t app_os_settings;
extern const app_desc_t app_os_time;
extern const app_desc_t app_os_reading;
extern const app_desc_t app_os_sleep;
extern const app_desc_t app_os_pin;
extern const app_desc_t app_os_storage;
extern const app_desc_t app_os_tools;
extern const app_desc_t app_refresh;
extern const app_desc_t app_reading;
extern const app_desc_t app_touch;
extern const app_desc_t app_axis;
extern const app_desc_t app_axis_lab;
extern const app_desc_t app_power;
extern const app_desc_t app_pmu;
extern const app_desc_t app_key;
extern const app_desc_t app_sleep;
extern const app_desc_t app_sd;
extern const app_desc_t app_font_pick;
extern const app_desc_t app_ioe;
extern const app_desc_t app_selftest;
extern const app_desc_t app_book;
extern const app_desc_t app_transfer;

// 四根入口对应全局菜单行；其余条目经设置进入。/ Four roots map to global menu rows; Settings exposes the rest.
static const struct { os_app_id_t id; const app_desc_t* app; } s_apps[] = {
    {OS_APP_HOME, &app_os_home},
    {OS_APP_LIBRARY, &app_book},
    {OS_APP_TODAY, &app_os_today},
    {OS_APP_SETTINGS, &app_os_settings},
    {OS_APP_TRANSFER, &app_transfer},
    {OS_APP_FONTS, &app_font_pick},
    {OS_APP_TIME, &app_os_time},
    {OS_APP_READING, &app_os_reading},
    {OS_APP_SLEEP, &app_os_sleep},
    {OS_APP_PIN, &app_os_pin},
    {OS_APP_STORAGE, &app_os_storage},
    {OS_APP_TOOLS, &app_os_tools},
    {OS_APP_DIAGNOSTICS, &app_home},
    {OS_APP_NONE, &app_refresh},
    {OS_APP_NONE, &app_reading},
    {OS_APP_NONE, &app_touch},
    {OS_APP_NONE, &app_axis},
    {OS_APP_NONE, &app_axis_lab},
    {OS_APP_NONE, &app_power},
    {OS_APP_NONE, &app_pmu},
    {OS_APP_NONE, &app_key},
    {OS_APP_NONE, &app_sleep},
    {OS_APP_NONE, &app_sd},
    {OS_APP_NONE, &app_ioe},
    {OS_APP_NONE, &app_selftest},
};

#define APP_COUNT ((int)(sizeof(s_apps) / sizeof(s_apps[0])))
#define PRODUCT_COUNT 4
#define DIAGNOSTIC_FIRST 12

int app_product_count(void) { return PRODUCT_COUNT; }
int app_diagnostic_count(void) { return APP_COUNT - DIAGNOSTIC_FIRST; }
const app_desc_t* app_diagnostic_at(int index) {
    return index >= 0 && index < app_diagnostic_count() ? s_apps[DIAGNOSTIC_FIRST + index].app : NULL;
}

int app_count(void) {
    return APP_COUNT;
}

const app_desc_t* app_at(int index) {
    if (index < 0 || index >= APP_COUNT) return NULL;
    return s_apps[index].app;
}

int app_index_of(const app_desc_t* app) {
    for (int i = 0; i < APP_COUNT; i++) {
        if (s_apps[i].app == app) return i;
    }
    return -1;
}

const app_desc_t* app_home_page(void) {
    return &app_os_home;
}

const app_desc_t* app_by_id(os_app_id_t id) {
    if (id == OS_APP_NONE) return NULL;
    for (int i = 0; i < APP_COUNT; ++i) if (s_apps[i].id == id) return s_apps[i].app;
    return NULL;
}

const app_desc_t* app_selftest_page(void) {
    return &app_selftest;
}
