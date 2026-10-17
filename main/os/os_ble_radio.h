/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 * 中文：BLE HID 无线边界，回调只更新快照与有界输入队列。
 * English: BLE HID radio boundary; callbacks update snapshots and a bounded input queue only.
 * 冻结：无绘图、分页或产品 NVS；同一时刻只连一个对端。
 * Frozen: No drawing, pagination or product NVS; connect one peer at a time.
 */
#pragma once
#include "ble_hid.h"
#include "esp_err.h"
#define OS_BLE_PEERS 12
typedef struct {
    /// 原始身份/广播地址及类型，name 始终以零结尾。/ Raw identity/advertising address and type; name is always terminated.
    uint8_t addr[6], type;
    char name[40];
    int8_t rssi;
    bool hid;
} os_ble_peer_t;
typedef enum { OS_BLE_OFF, OS_BLE_STARTING, OS_BLE_IDLE, OS_BLE_SCANNING,
    OS_BLE_CONNECTING, OS_BLE_DISCOVERING, OS_BLE_READY, OS_BLE_ERROR } os_ble_phase_t;
typedef struct {
    /// 一次完整复制，不保留 host 内部指针。/ One complete copy, retaining no host-owned pointers.
    os_ble_phase_t phase;
    os_ble_peer_t peer, peers[OS_BLE_PEERS];
    uint8_t count;
    int battery; ///< -1 表示未知 / Unknown is -1
    uint32_t revision;
    char error[88];
} os_ble_snapshot_t;
typedef struct {
    /// 单调毫秒、输入报告通道与完整有界帧；不包含外加报告号。/ Monotonic milliseconds, input channel and bounded frame, without a prepended report ID.
    uint32_t at_ms;
    uint8_t channel, len, bytes[BLE_HID_REPORT_BYTES];
    ble_hid_key_t key;
} os_ble_input_t;
/// UI 任务串行调用生命周期；错误返回给界面，不重启、不擦除全局 NVS。
/// UI serialises lifecycle calls; report errors without restarting or erasing global NVS.
esp_err_t os_ble_radio_start(void);
esp_err_t os_ble_radio_stop(void);
/// 启动前可查询内存门槛；不足时调用方可先释放可重建缓存再重试判断。
/// Query the start memory floors; callers may release rebuildable caches and re-check before starting.
bool os_ble_radio_headroom(void);
esp_err_t os_ble_radio_scan(void);
esp_err_t os_ble_radio_connect(const os_ble_peer_t* peer);
esp_err_t os_ble_radio_disconnect(void);
esp_err_t os_ble_radio_forget(const os_ble_peer_t* peer);
/// 短临界区复制状态；不会驱动无线。/ Copy status under a short critical section, without driving the radio.
void os_ble_radio_snapshot(os_ble_snapshot_t* out);
/// 消费一帧；为空返回 false。/ Consume one frame; false when empty.
bool os_ble_radio_pop(os_ble_input_t* out);
/// 丢弃队列，避免跨页面回放。/ Discard queued input to avoid cross-page replay.
void os_ble_radio_clear(void);
