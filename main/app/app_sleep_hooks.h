/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 *
 * 中文：锁屏/睡眠网关的统一保存钩子。页面在进入时注册、离页时注销，
 * 睡眠入口只遍历调用，不知道任何页面细节。
 * English: Unified save hooks for the lock/sleep gateway. Pages register on
 * enter and unregister on exit; the sleep entry only iterates, knowing no page.
 *
 * 冻结：钩子在 UI 任务上同步执行且有界；失败不阻塞睡眠，只由页面自身机制兜底；
 * 不允许在钩子里刷屏或等待网络。
 * Frozen: Hooks run synchronously and bounded on the UI task; failure never
 * blocks sleep, pages keep their own retry paths; no display pushes or network
 * waits inside a hook.
 */
#pragma once
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/// 固定槽位，防止注册表无界增长。/ Fixed slots so the registry cannot grow unbounded.
#define APP_SLEEP_PREPARE_SLOTS 4

/// 返回 true 表示已尽力保存完成；false 仅用于记录，不阻止睡眠。/ True means saved as far as possible; false is logged and never blocks sleep.
typedef bool (*app_sleep_prepare_fn)(void);

/// 注册；重复或满槽返回 false。/ Register; duplicates or a full table return false.
bool app_sleep_prepare_register(app_sleep_prepare_fn fn);
/// 注销不存在的函数是空操作。/ Unregistering an unknown function is a no-op.
void app_sleep_prepare_unregister(app_sleep_prepare_fn fn);
/// 逐槽调用；空表或空指针安全。/ Invoke each slot; safe on an empty table or null slots.
void app_sleep_prepare_run(void);

#ifdef __cplusplus
}
#endif
