/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 *
 * 中文：阅读完整帧反色和直刷真实黑白转换；历史掩码保留作诊断对照，产品不再调用。
 * English: Whole-reader frame inversion and actual binary direct conversion; history masks remain a diagnostic reference, outside product updates.
 *
 * 冻结：用户本次明确优先无闪速度，直刷阈值量化为0/15而不保留灰阶抗锯齿；只改目标帧，不访问旧参考或硬件。
 * Frozen: The user now prioritizes flicker-free speed, thresholding direct targets to 0/15 without gray antialiasing; modify only the target frame, without accessing prior references or hardware.
 */
#include "display_pixels.h"
#include <stddef.h>
#include <string.h>

void display_invert_frame(uint8_t* fb, int width, int height) {
    if (!fb || width <= 0 || height <= 0 || (width & 1) || (size_t)width > SIZE_MAX / (size_t)height) return;
    size_t bytes = (size_t)width * (size_t)height / 2;
    for (size_t i = 0; i < bytes; ++i) fb[i] ^= 0xFF;
}

void display_prepare_direct_frame(uint8_t* fb, int width, int height, bool white_on_black) {
    if (!fb || width <= 0 || height <= 0 || (width & 1)) return;
    (void)white_on_black;
    // 厂家DU只支持黑白目标；按实际灰码阈值转换，日夜不翻转，也不引入空间抖动。
    // Vendor DU supports only black/white targets; threshold actual gray codes without day/night inversion or spatial dithering.
    for (int y = 0; y < height; ++y) for (int x = 0; x < width; x += 2) {
        uint8_t* p = fb + (size_t)y * (width / 2) + x / 2;
        unsigned lo = (*p & 15) < 8 ? 0 : 15, hi = (*p >> 4) < 8 ? 0 : 15;
        *p = (uint8_t)(lo | hi << 4);
    }
}

size_t display_ghost_history_bytes(int width, int height) {
    if (width <= 0 || height <= 0 || (width & 1) || (size_t)width > (SIZE_MAX - 3) / (size_t)height) return 0;
    return ((size_t)width * height + 3) / 4;
}

static void set_age(display_ghost_history_t* h, size_t i, unsigned age) {
    unsigned shift = 2 * (i % 4);
    h->ages[i / 4] = (uint8_t)((h->ages[i / 4] & ~(3u << shift)) | age << shift);
}

static unsigned pixel_at(const uint8_t* fb, size_t i) {
    return fb[i / 2] >> (4 * (i % 2)) & 15;
}

void display_ghost_history_reset(display_ghost_history_t* h) {
    if (h && h->ages) memset(h->ages, 0, display_ghost_history_bytes(h->width, h->height));
}

void display_ghost_history_observe(display_ghost_history_t* h, const uint8_t* fb) {
    if (!h || !h->ages || !fb || !display_ghost_history_bytes(h->width, h->height)) return;
    // 四像素合并读取相邻三行，避免每像素九次重复读取PSRAM。
    // Combine four pixels from three rows instead of nine repeated PSRAM reads per pixel.
    if (!(h->width & 3)) {
        const int stride = h->width / 2;
        for (int y = 0; y < h->height; ++y) for (int x = 0; x < h->width; x += 4) {
            unsigned ink = 0;
            for (int dy = -1; dy <= 1 && ink != 63; ++dy) {
                int ny = y + dy;
                if (ny < 0 || ny >= h->height) continue;
                const uint8_t* row = fb + (size_t)ny * stride + x / 2;
                unsigned a = row[0], b = row[1];
                ink |= ((a & 15) != 15) << 1 | ((a >> 4) != 15) << 2 |
                       ((b & 15) != 15) << 3 | ((b >> 4) != 15) << 4;
                if (x) ink |= (row[-1] >> 4) != 15;
                if (x + 4 < h->width) ink |= ((row[2] & 15) != 15) << 5;
            }
            unsigned bits = ((ink | ink << 1 | ink >> 1) >> 1) & 15;
            static const uint8_t budgets[16] = {
                0,3,12,15,48,51,60,63,192,195,204,207,240,243,252,255
            };
            h->ages[((size_t)y * h->width + x) / 4] |= budgets[bits];
        }
        return;
    }
    // 一像素邻域覆盖文字边缘的串扰，边界不得绕到另一行。
    // A one-pixel neighborhood covers text-edge interaction without wrapping at row boundaries.
    for (int y = 0; y < h->height; ++y) for (int x = 0; x < h->width; ++x) {
        if (pixel_at(fb, (size_t)y * h->width + x) == 15) continue;
        for (int dy = -1; dy <= 1; ++dy) for (int dx = -1; dx <= 1; ++dx) {
            int nx = x + dx, ny = y + dy;
            if (nx >= 0 && nx < h->width && ny >= 0 && ny < h->height)
                set_age(h, (size_t)ny * h->width + nx, 3);
        }
    }
}

size_t display_ghost_history_mask(const display_ghost_history_t* h, const uint8_t* target, uint8_t* mask) {
    if (!h || !h->ages || !target || !mask || !display_ghost_history_bytes(h->width, h->height)) return 0;
    size_t selected = 0, pixels = (size_t)h->width * h->height;
    for (size_t i = 0; i < pixels; i += 4) {
        unsigned ages = h->ages[i / 4];
        if (!ages && i + 4 <= pixels) { memset(mask + i, 0, 4); continue; }
        for (unsigned j = 0; j < 4 && i + j < pixels; ++j) {
            bool erase = (ages >> (2 * j) & 3) && pixel_at(target, i + j) == 15;
            mask[i + j] = erase ? 0xff : 0;
            selected += erase;
        }
    }
    return selected;
}

void display_ghost_history_commit(display_ghost_history_t* h, const uint8_t* target) {
    if (!h || !h->ages || !target || !display_ghost_history_bytes(h->width, h->height)) return;
    size_t pixels = (size_t)h->width * h->height;
    for (size_t i = 0; i < pixels; i += 4) {
        unsigned age = h->ages[i / 4];
        if (!age) continue;
        unsigned decrement = 0;
        for (unsigned j = 0; j < 4 && i + j < pixels; ++j)
            if ((age >> (2 * j) & 3) && pixel_at(target, i + j) == 15) decrement |= 1u << (2 * j);
        h->ages[i / 4] = (uint8_t)(age - decrement);
    }
}

size_t display_ghost_history_pack(const display_ghost_history_t* h, const uint8_t* target, uint8_t* packed) {
    if (!h || !h->ages || !target || !packed || !display_ghost_history_bytes(h->width, h->height)) return 0;
    size_t selected = 0, pixels = (size_t)h->width * h->height;
    for (size_t i = 0; i < pixels; i += 8) {
        unsigned bits = 0;
        for (unsigned j = 0; j < 8 && i + j < pixels; ++j) {
            unsigned age = h->ages[(i + j) / 4] >> (2 * ((i + j) % 4)) & 3;
            bool erase = age && pixel_at(target, i + j) == 15;
            bits |= (unsigned)erase << j;
            selected += erase;
        }
        packed[i / 8] = (uint8_t)bits;
    }
    return selected;
}

void display_ghost_history_expand(const uint8_t* packed, uint8_t* mask, size_t pixels) {
    if (!packed || !mask) return;
    static const uint8_t expand[16][4] = {
        {0,0,0,0}, {255,0,0,0}, {0,255,0,0}, {255,255,0,0},
        {0,0,255,0}, {255,0,255,0}, {0,255,255,0}, {255,255,255,0},
        {0,0,0,255}, {255,0,0,255}, {0,255,0,255}, {255,255,0,255},
        {0,0,255,255}, {255,0,255,255}, {0,255,255,255}, {255,255,255,255}
    };
    size_t i = 0;
    for (; i + 8 <= pixels; i += 8) {
        unsigned bits = packed[i / 8];
        memcpy(mask + i, expand[bits & 15], 4);
        memcpy(mask + i + 4, expand[bits >> 4], 4);
    }
    for (; i < pixels; ++i) mask[i] = packed[i / 8] & (1u << (i % 8)) ? 255 : 0;
}
