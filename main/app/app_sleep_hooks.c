/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 *
 * 中文：睡眠保存钩子的固定槽位表；无动态分配，注册/注销由页面配对调用。
 * English: Fixed-slot table for sleep prepare hooks; no allocation, pages pair
 * register/unregister calls.
 */
#include "app_sleep_hooks.h"
#include <stddef.h>

static app_sleep_prepare_fn s_slots[APP_SLEEP_PREPARE_SLOTS];

bool app_sleep_prepare_register(app_sleep_prepare_fn fn) {
    if (!fn) return false;
    for (int i = 0; i < APP_SLEEP_PREPARE_SLOTS; ++i) {
        if (s_slots[i] == fn) return false;
        if (!s_slots[i]) {
            s_slots[i] = fn;
            return true;
        }
    }
    return false;
}

void app_sleep_prepare_unregister(app_sleep_prepare_fn fn) {
    for (int i = 0; i < APP_SLEEP_PREPARE_SLOTS; ++i)
        if (s_slots[i] == fn) s_slots[i] = NULL;
}

void app_sleep_prepare_run(void) {
    for (int i = 0; i < APP_SLEEP_PREPARE_SLOTS; ++i)
        if (s_slots[i]) s_slots[i]();
}
