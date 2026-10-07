/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 *
 * 锁屏、浅睡等待、软睡/关机下电。
 *
 * Lock, light-sleep wait, and soft-sleep / power-off rail drop.
 */

#pragma once

#include <stdint.h>

#include "cst836u.h"
#include "epd_highlevel.h"
#include "sc7a20h.h"
#include "settings.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    APP_WAKE_NONE = 0,
    APP_WAKE_KEY,
    APP_WAKE_PICKUP,
    APP_WAKE_TIMER, ///< 分钟到点定时唤醒，仅刷新锁屏时钟 / Minute-boundary timer wake, lock-clock refresh only
} app_wake_source_t;

/// 进入锁屏与睡眠；tp 非空时浅睡唤醒后先校验 PIN 再清屏返回。
/// / Enter lock and sleep; with a touch handle, challenge the PIN after a light-sleep wake.
void enter_lock_and_sleep(
    EpdiyHighlevelState* hl, int64_t* ignore_until_ms, sc7a20h_handle_t acc,
    cst836u_handle_t tp
);

/// 等电源键松开，避免进睡瞬间被同一下按住立刻唤醒。
/// Wait for the power key to release so the same press does not wake immediately.
void app_lock_wait_key_idle(int timeout_ms);
/// ESP 浅睡，按键或拿起唤醒。acc 为空则只等按键。
/// ESP light sleep; wake on key or pickup. Key only when acc is NULL.
app_wake_source_t app_light_sleep_wait(sc7a20h_handle_t acc);
/// 退出锁屏分钟等待时取消 ESP 定时唤醒及轮询期限。
/// Cancel the ESP minute wake and polling deadline on exit from the lock wait.
void app_sleep_disarm_minute_wake(void);
/// 旧版 PMU 时钟闹钟控制；开机和进入锁屏时关闭，动态锁屏使用 ESP 定时唤醒。
/// Legacy PMU clock alarm control; disabled at boot/lock entry, with dynamic faces using ESP timer wakes.
void app_sleep_alarm_clock(bool on);
app_wake_source_t app_last_wake_source(void);
/// 软睡或关机，拉掉 EN 后停住，不会返回。
/// Soft sleep or power-off: drop EN and halt; does not return.
void app_enter_host_sleep(app_sleep_mode_t mode);

/// 开机阻塞校验；未设密码直接通过，输错留在本页，正确后返回。
/// 键盘绘制/命中见 ui_product 的 ui_product_lock_keypad*。
/// Blocking boot gate; unarmed passes at once, wrong entries stay, success returns.
/// Keypad draw/hit live in ui_product as ui_product_lock_keypad*.
bool app_lock_pin_challenge(EpdiyHighlevelState* hl, cst836u_handle_t tp);

#ifdef __cplusplus
}
#endif
