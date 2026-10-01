/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 * 中文：产品层读取的机型能力与存储探测边界，不暴露引脚和芯片句柄。
 * English: Device capabilities and storage-probe boundary without pins or chip handles.
 * 冻结：未知能力不当作支持；型号配置不是第二台设备的驱动。
 * Frozen: Unknown is not supported; a profile is not a second-device driver.
 */
#pragma once
#include <stdbool.h>
#include <stdint.h>

typedef enum {
    OS_CAP_UNKNOWN, ///< 未知 / Unknown
    OS_CAP_ABSENT, ///< 无此能力 / Absent
    OS_CAP_PRESENT, ///< 具备能力，非实时健康状态 / Present, not runtime health
} os_capability_t;

typedef struct {
    /// 产品机型名。/ Product model name.
    const char* name;
    /// 逻辑画布。/ Logical canvas.
    uint16_t width, height;
    /// 产品所需能力。/ Product capabilities.
    os_capability_t touch, transfer, removable_storage;
} os_device_t;

/// 当前构建机型；每个机型单独编译，不在运行时猜测硬件。/ Build target; compile per model, never guess hardware.
const os_device_t* os_device(void);
/// 发起异步存储探测，不格式化。/ Start asynchronous storage probing without formatting.
void os_storage_probe(void);
/// 探测未结束时为 false；不等同于有可用卡。/ False until probing ends; not a mounted-card guarantee.
bool os_storage_probe_complete(void);
