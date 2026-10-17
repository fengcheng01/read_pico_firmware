/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 * 中文：只解释键盘与消费者用法，其他报告供显式按键学习。
 * English: Interpret keyboard and consumer usages; other reports require explicit learning.
 * 冻结：所有循环、移位、报告位偏移均有界，不合并不同报告号的状态。
 * Frozen: Bound loops, shifts and bit offsets; never combine states from distinct report IDs.
 */
#include "ble_hid.h"
#include <string.h>

typedef struct { uint32_t page, bits, count; uint8_t id; } globals_t;
static int direction(uint16_t page, uint16_t usage) {
    if (page == 7) {
        switch (usage) {
            case 0x4b: case 0x50: case 0x52: return -1;
            case 0x4e: case 0x4f: case 0x51: case 0x2c: return 1;
            default: return 0;
        }
    }
    if (page == 0x0c) {
        switch (usage) {
            case 0xe9: case 0xb6: case 0x38: return -1;
            case 0xea: case 0xb5: case 0x37: return 1;
            default: return 0;
        }
    }
    return 0;
}
bool ble_hid_parse(ble_hid_map_t* map, const uint8_t* data, size_t size) {
    if (!map) return false;
    memset(map, 0, sizeof(*map));
    if (!data || !size || size > 2048) return false;
    globals_t g = {0}, stack[4];
    unsigned sp = 0;
    unsigned collections = 0;
    uint16_t offsets[256] = {0}, usages[64] = {0};
    unsigned usage_count = 0;
    uint32_t usage_min = 0, usage_max = 0;
    bool range = false;
    for (size_t at = 0; at < size;) {
        uint8_t prefix = data[at++];
        if (prefix == 0xfe) return false;
        unsigned n = prefix & 3; if (n == 3) n = 4;
        if (size - at < n) return false;
        uint32_t value = 0;
        for (unsigned j = 0; j < n; ++j) value |= (uint32_t)data[at++] << (j * 8);
        unsigned type = (prefix >> 2) & 3, tag = prefix >> 4;
        if (type == 1) {
            switch (tag) {
                case 0: if (value > 65535) return false; g.page = value; break;
                case 7: g.bits = value; break;
                case 8: if (!value || value > 255) return false; g.id = value; map->ids = true; break;
                case 9: g.count = value; break;
                case 10: if (sp == 4) return false; stack[sp++] = g; break;
                case 11: if (!sp) return false; g = stack[--sp]; break;
                default: break;
            }
        } else if (type == 2) {
            if (value > 65535) return false;
            if (tag == 0) { if (usage_count == 64) return false; usages[usage_count++] = value; }
            else if (tag == 1) { usage_min = value; range = true; }
            else if (tag == 2) usage_max = value;
        } else if (type == 0) {
            if (tag == 10) { if (++collections > 16) return false; }
            if (tag == 12) { if (!collections) return false; --collections; }
            if (tag == 8) {
                if (!g.bits || g.bits > 32 || !g.count || g.count > 256 ||
                    g.bits * g.count > BLE_HID_REPORT_BYTES * 8 - offsets[g.id]) return false;
                if (!(value & 1) && (g.page == 7 || g.page == 0x0c)) {
                    bool variable = (value & 2) != 0;
                    unsigned fields = variable ? g.count : 1;
                    if (range && usage_max < usage_min) return false;
                    for (unsigned i = 0; i < fields; ++i) {
                        if (map->count == BLE_HID_FIELDS) return false;
                        uint32_t usage = i < usage_count ? usages[i] : range ?
                            (usage_min + i <= usage_max ? usage_min + i : usage_max) :
                            usage_count ? usages[usage_count - 1] : 0;
                        if (usage > 65535) return false;
                        map->fields[map->count++] = (ble_hid_field_t){
                            .offset = offsets[g.id] + (variable ? i * g.bits : 0),
                            .page = g.page, .usage = usage,
                            .count = variable ? 1 : g.count, .bits = g.bits,
                            .id = g.id, .variable = variable};
                    }
                }
                offsets[g.id] += g.bits * g.count;
            }
            usage_count = 0; usage_min = usage_max = 0; range = false;
        }
    }
    if (sp || collections) return false;
    map->valid = true;
    return true;
}
static uint32_t bits_at(const uint8_t* data, unsigned bit, unsigned n) {
    uint32_t value = 0;
    for (unsigned j = 0; j < n; ++j) value |= (uint32_t)((data[(bit + j) / 8] >> ((bit + j) % 8)) & 1) << j;
    return value;
}
ble_hid_key_t ble_hid_decode(const ble_hid_map_t* map, uint8_t id, const uint8_t* data, size_t size) {
    ble_hid_key_t result = {0};
    if (!map || !map->valid || !data || !size || size > BLE_HID_REPORT_BYTES) return result;
    for (unsigned i = 0; i < map->count; ++i) {
        const ble_hid_field_t* f = &map->fields[i];
        if (f->id != id) continue;
        if ((unsigned)f->offset + f->bits * f->count > size * 8) return (ble_hid_key_t){0};
        result.known = true;
        for (unsigned j = 0; j < f->count; ++j) {
            uint32_t value = bits_at(data, f->offset + j * f->bits, f->bits);
            uint16_t usage = f->variable ? f->usage : value > 65535 ? 0 : value;
            // 修饰键与键盘错误码不作为可学习按下。/ Modifiers and keyboard error codes are never learnable presses.
            if (!value || (f->page == 7 && (usage <= 3 || (usage >= 0xe0 && usage <= 0xe7)))) continue;
            int dir = direction(f->page, usage);
            if (!result.down) { result.page = f->page; result.usage = usage; result.down = true; }
            if (dir && result.direction && dir != result.direction) return (ble_hid_key_t){.known = true};
            if (dir) { result.direction = dir; result.page = f->page; result.usage = usage; }
        }
    }
    return result;
}
ble_hid_key_t ble_hid_boot(const uint8_t* data, size_t size) {
    if (!data || size != 8) return (ble_hid_key_t){0};
    ble_hid_map_t map = {.valid = true, .count = 1, .fields = {
        {.offset = 16, .page = 7, .bits = 8, .count = 6}}};
    return ble_hid_decode(&map, 0, data, size);
}
