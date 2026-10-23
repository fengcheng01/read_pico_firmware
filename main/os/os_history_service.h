/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 * 中文：运行期历史维护的统一入口，设备桥只提供缓存快照和卡根目录。
 * English: Shared awake-history maintenance; the device port supplies cached snapshots and a card root only.
 * 冻结：不唤醒、不联网、不主动读取PMU；仅UI任务调用，USB/睡眠期间不调度。
 * Frozen: No wakeups, networking or active PMU reads; UI-task only, never scheduled during USB/sleep.
 */
#pragma once
#include "os_battery_history.h"
typedef struct {
    bool valid; ///< 时间和电量都有效。/ Time and gauge are both valid.
    uint32_t utc; ///< 当前缓存UTC。/ Current cached UTC.
    int permille; ///< 电量，未知为负。/ Charge, negative when unknown.
    uint16_t millivolts; ///< 缓存电压。/ Cached voltage.
    uint8_t charge_state; ///< PMU充电状态。/ PMU charging state.
} os_history_snapshot_t;
/// 平台边界：不发起IO。/ Platform seam without IO.
void os_history_snapshot(int64_t now_ms, os_history_snapshot_t* out);
/// 已挂载TF卡内的历史目录，不能是用户书籍目录。/ History directory on the mounted card, never a user book root.
const char* os_history_directory(void);
/// 共享事件循环提供已有卡状态，不增加探测。/ The shared loop feeds existing media state without extra probing.
void os_history_service_poll(int64_t now_ms, bool mounted);
/// 返回只读会话样本，可能尚未分配。/ Read-only session samples, possibly not allocated yet.
const os_battery_history_t* os_history_battery(void);
/// 修订号供页面按需刷新。/ Revision for on-demand page refresh.
unsigned os_history_revision(void);
/// 当前可落卡及待重试状态。/ Current card availability and pending persistence.
bool os_history_media_ready(void);
bool os_history_battery_pending(void);
/// 最近一次读取错误；不把损坏旧记录当作空白记录覆盖。/ Last read failure; never overwrite corrupt old history as empty.
bool os_history_battery_read_error(void);
