/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 * 中文：睡眠保存钩子的注册、注销与执行顺序回归。
 * English: Registration, unregistration and run-order regressions for sleep prepare hooks.
 */
#include "app_sleep_hooks.h"
#include <assert.h>
#include <stdio.h>

static int s_log[8], s_count;

static bool first(void) { s_log[s_count++] = 1; return true; }
static bool second(void) { s_log[s_count++] = 2; return false; }
static bool third(void) { s_log[s_count++] = 3; return true; }
static bool fourth(void) { s_log[s_count++] = 4; return true; }

int main(void) {
    app_sleep_prepare_run();
    assert(s_count == 0);
    assert(app_sleep_prepare_register(NULL) == false);
    assert(app_sleep_prepare_register(first) && !app_sleep_prepare_register(first));
    assert(app_sleep_prepare_register(second) && app_sleep_prepare_register(third));
    assert(app_sleep_prepare_register(fourth));
    assert(!app_sleep_prepare_register(first) && !app_sleep_prepare_register(fourth));

    app_sleep_prepare_run();
    assert(s_count == 4 && s_log[0] == 1 && s_log[1] == 2 && s_log[2] == 3 && s_log[3] == 4);

    app_sleep_prepare_unregister(second);
    app_sleep_prepare_unregister(second);
    s_count = 0;
    app_sleep_prepare_run();
    assert(s_count == 3 && s_log[0] == 1 && s_log[1] == 3 && s_log[2] == 4);

    app_sleep_prepare_unregister(first);
    app_sleep_prepare_unregister(third);
    // 注销后槽位可复用；仍在册的 fourth 保持末位。/ Freed slots are reusable; the still-registered fourth keeps its slot.
    assert(app_sleep_prepare_register(third) && app_sleep_prepare_register(first));
    s_count = 0;
    app_sleep_prepare_run();
    assert(s_count == 3 && s_log[0] == 3 && s_log[1] == 1 && s_log[2] == 4);

    puts("app_sleep_hooks: register, dedup, order, reuse and empty runs passed");
    return 0;
}
