/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 * 中文：编译真实 Pico 能力桥接，区分检测完成与检测到卡。
 * English: Compile the real Pico capability bridge, distinguishing probe completion from card presence.
 */
#include "os_device.h"
#include "read_pico_sd.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static unsigned s_probes;
static esp_err_t s_result = ESP_ERR_NOT_FINISHED;
esp_err_t read_pico_sd_start_probe(void) { ++s_probes; return ESP_OK; }
esp_err_t read_pico_sd_get_info(read_pico_sd_info_t* info) { memset(info, 0, sizeof(*info)); return s_result; }
int main(void) {
    const os_device_t* device = os_device();
    assert(device->width == 684 && device->height == 1216);
    assert(device->touch == OS_CAP_PRESENT && device->transfer == OS_CAP_PRESENT);
    assert(device->removable_storage == OS_CAP_PRESENT);
    os_storage_probe(); assert(s_probes == 1 && !os_storage_probe_complete());
    s_result = ESP_OK; assert(os_storage_probe_complete());
    s_result = ESP_FAIL; assert(os_storage_probe_complete());
    puts("os_device: real Pico profile and probe bridge passed");
    return 0;
}
