/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 * 中文：小纸 Pico 产品能力与既有 BSP 的窄桥接。
 * English: Read Pico capabilities and a narrow bridge to the existing BSP.
 * 冻结：保留官方探测与显式重挂策略；不写 PMU、显示电压或波形。
 * Frozen: Keep BSP probing and explicit remount policy; do not write PMU, display voltage or waveforms.
 */
#include "os_device.h"
#include "read_pico_sd.h"

static const os_device_t s_device = {
    .name = "小纸 Pico", .width = 684, .height = 1216,
    .touch = OS_CAP_PRESENT, .transfer = OS_CAP_PRESENT,
    .removable_storage = OS_CAP_PRESENT,
};
const os_device_t* os_device(void) { return &s_device; }
void os_storage_probe(void) { read_pico_sd_start_probe(); }
bool os_storage_probe_complete(void) {
    read_pico_sd_info_t info = {0};
    return read_pico_sd_get_info(&info) != ESP_ERR_NOT_FINISHED;
}
