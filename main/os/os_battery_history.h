/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 * 中文：有界真实电量序列与双槽文件校验，不读取硬件或主动唤醒。
 * English: Bounded measured battery series and checked dual-slot files, without hardware reads or wakeups.
 * 冻结：未知值不入库，不补休眠样本；写坏新槽不能覆盖最后有效槽。
 * Frozen: Reject unknown values, never fabricate sleep samples, and preserve the last valid slot on failed writes.
 */
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define OS_BATTERY_SAMPLES 672
#define OS_BATTERY_INTERVAL 900u
#define OS_BATTERY_WINDOW (7u * 86400u)
typedef struct {
    uint32_t at; ///< UTC秒。/ UTC seconds.
    uint16_t permille, millivolts; ///< 有效电量与电压。/ Valid charge and voltage.
    uint8_t charge_state, reserved[3]; ///< PMU状态及零填充。/ PMU state and zero padding.
} os_battery_sample_t;
typedef struct {
    uint32_t magic, version, sequence, count, checksum, reserved[3]; ///< 版本化文件头。/ Versioned file header.
    os_battery_sample_t samples[OS_BATTERY_SAMPLES]; ///< 按时间递增的真实样本。/ Actual samples in increasing time order.
} os_battery_history_t;
typedef enum {
    OS_BATTERY_LOAD_ERROR = -1, ///< IO或校验失败。/ IO or integrity failure.
    OS_BATTERY_LOAD_EMPTY = 0, ///< 两槽都不存在。/ Neither slot exists.
    OS_BATTERY_LOAD_OK = 1, ///< 至少一槽有效。/ At least one valid slot.
} os_battery_load_t;

/// 初始化空序列，不分配。/ Initialize an empty series without allocations.
void os_battery_history_init(os_battery_history_t* history);
/// 按15分钟和七天窗口收样；校时回退仅丢弃未来样本。/ Sample at fifteen-minute intervals within seven days; a clock rollback drops future samples only.
bool os_battery_history_add(os_battery_history_t* history, const os_battery_sample_t* sample);
/// work由调用方提供；失败保持history不变。/ Caller supplies work; failure leaves history unchanged.
os_battery_load_t os_battery_history_load(const char* directory, os_battery_history_t* history,
                                        os_battery_history_t* work, int* slot);
/// 只写非活动槽并同步/回读，成功后才推进槽号。/ Write only the inactive slot, sync/read back, then advance the slot on success.
bool os_battery_history_save(const char* directory, os_battery_history_t* history,
                             os_battery_history_t* work, int* slot);
