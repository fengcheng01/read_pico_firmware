/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 * 中文：有界 HID 报告描述符与输入解析，不依赖无线或页面。
 * English: Bounded HID descriptor and input parsing, independent of radios and pages.
 * 冻结：未知布局不猜字节；修饰键、报告号与释放不触发翻页。
 * Frozen: Never guess unknown layouts; modifiers, report IDs and releases never turn pages.
 */
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define BLE_HID_FIELDS 48
#define BLE_HID_REPORT_BYTES 32
typedef struct {
    uint16_t offset, page, usage, count;
    uint8_t bits, id;
    bool variable;
} ble_hid_field_t;
typedef struct {
    ble_hid_field_t fields[BLE_HID_FIELDS];
    uint8_t count;
    bool ids, valid;
} ble_hid_map_t;
typedef struct {
    int direction; ///< -1 上一页、1 下一页、0 未映射 / Previous, next, or unmapped
    bool known, down; ///< 可识别布局与按下状态 / Recognised layout and pressed state
    uint16_t page, usage; ///< 测试反馈用原始用法 / Raw usage for test feedback
} ble_hid_key_t;

/// 解析短项、全局栈、报告号、数组与位字段；越界或不完整布局整体拒绝。
/// Parse short items, global stacks, IDs, arrays and bitfields; reject malformed or overflowing maps.
bool ble_hid_parse(ble_hid_map_t* map, const uint8_t* data, size_t size);
/// 报告号由 Report Reference 给出，输入字节不包含报告号。/ ID comes from Report Reference; input bytes exclude it.
ble_hid_key_t ble_hid_decode(const ble_hid_map_t* map, uint8_t id, const uint8_t* data, size_t size);
/// 独立 Boot Keyboard 布局。/ Standalone Boot Keyboard layout.
ble_hid_key_t ble_hid_boot(const uint8_t* data, size_t size);
