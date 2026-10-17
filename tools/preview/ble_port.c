/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 * 中文：预览无线边界，明确使用合成设备；真实偏好、输入闸门与页面仍共享。
 * English: Preview radio boundary with synthetic peers; preferences, input gate and pages remain shared.
 */
#include "os_ble_radio.h"
#include <stdio.h>
#include <string.h>
#include "esp_timer.h"
static os_ble_snapshot_t s = {.battery = -1};
static os_ble_input_t events[8];
static unsigned head, tail;
static int failure;
void preview_ble_failure(int value) { failure = value; }
// failure==1 同时模拟内存不足：门槛先拒绝，回收后仍由 start 报同一错误。
// failure==1 also simulates short memory: the floor refuses first and start reports the same error after reclaiming.
bool os_ble_radio_headroom(void) { return failure != 1; }
esp_err_t os_ble_radio_start(void) {
    if (failure == 1) { s.phase = OS_BLE_ERROR; snprintf(s.error, sizeof(s.error), "蓝牙内存不足，请退出图书后重试"); return ESP_ERR_NO_MEM; }
    s.phase = OS_BLE_IDLE; ++s.revision; return ESP_OK;
}
esp_err_t os_ble_radio_stop(void) {
    if (failure == 2) return ESP_FAIL;
    s.phase = OS_BLE_OFF; ++s.revision; head = tail = 0; return ESP_OK;
}
esp_err_t os_ble_radio_scan(void) {
    if (s.phase == OS_BLE_OFF) return ESP_ERR_INVALID_STATE;
    s.phase = OS_BLE_IDLE;
    s.count = 6;
    memset(s.peers, 0, sizeof(s.peers));
    for (unsigned i = 0; i < s.count; ++i) {
        s.peers[i].addr[0] = i + 1; s.peers[i].type = 1; s.peers[i].rssi = -40 - i * 5;
        s.peers[i].hid = i != 3;
        snprintf(s.peers[i].name, sizeof(s.peers[i].name), i ? "模拟设备 %u" : "BLE Page Turner（模拟）", i + 1);
    }
    ++s.revision; return ESP_OK;
}
esp_err_t os_ble_radio_connect(const os_ble_peer_t* peer) {
    if (!peer || s.phase == OS_BLE_OFF) return ESP_ERR_INVALID_STATE;
    s.peer = *peer; s.phase = OS_BLE_READY; s.error[0] = 0; ++s.revision; return ESP_OK;
}
esp_err_t os_ble_radio_disconnect(void) { s.phase = OS_BLE_IDLE; ++s.revision; head = tail = 0; return ESP_OK; }
esp_err_t os_ble_radio_forget(const os_ble_peer_t* peer) {
    (void)peer;
    if (failure == 3) { s.phase = OS_BLE_ERROR; snprintf(s.error, sizeof(s.error), "无法删除配对"); ++s.revision; return ESP_OK; }
    return os_ble_radio_disconnect();
}
void os_ble_radio_snapshot(os_ble_snapshot_t* out) { *out = s; }
bool os_ble_radio_pop(os_ble_input_t* out) {
    if (head == tail) return false;
    *out = events[tail]; tail = (tail + 1) % 8; return true;
}
void os_ble_radio_clear(void) { head = tail = 0; }
void preview_ble_key(int usage) {
    os_ble_input_t input = {.at_ms = (uint32_t)(esp_timer_get_time() / 1000), .len = 8};
    input.bytes[2] = usage; input.key = ble_hid_boot(input.bytes, input.len);
    events[head] = input; head = (head + 1) % 8;
}
