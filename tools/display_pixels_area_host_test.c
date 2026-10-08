/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 *
 * 中文：独立像素预期检查真实物理区域量化，覆盖边界裁剪、半字节及存储护栏。
 * English: Independent pixel expectations check real physical-area quantization, clipping, nibbles and storage guards.
 *
 * 冻结：只调用真实像素API，不模拟旋转或显示硬件。
 * Frozen: Call only the real pixel API without simulating rotation or display hardware.
 */
#include <assert.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "display_pixels.h"

#define GUARD 16
#define MAX_BYTES 108

static unsigned checks;

static void check_area(int width, int height, int x, int y, int w, int h, unsigned seed) {
    uint8_t actual[GUARD + MAX_BYTES + GUARD], expected[sizeof(actual)];
    memset(actual, 0xA5, sizeof(actual));
    assert(width > 0 && !(width & 1) && height > 0);
    size_t bytes = (size_t)width * (size_t)height / 2;
    assert(bytes <= MAX_BYTES);
    for (size_t i = 0; i < bytes; ++i) actual[GUARD + i] = (uint8_t)(seed + 17 * i);
    memcpy(expected, actual, sizeof(actual));
    // 按逐像素区间成员关系独立算预期，不复用实现的裁剪或字节遍历。
    // Derive expectations independently by per-pixel interval membership, without reusing implementation clipping or byte traversal.
    if (w > 0 && h > 0) for (int py = 0; py < height; ++py) for (int px = 0; px < width; ++px) {
        if ((int64_t)px < x || (int64_t)px >= (int64_t)x + w ||
            (int64_t)py < y || (int64_t)py >= (int64_t)y + h) continue;
        size_t i = GUARD + ((size_t)py * (size_t)width + (size_t)px) / 2;
        unsigned shift = (unsigned)(px % 2) * 4;
        unsigned gray = (actual[i] >> shift) & 15;
        unsigned converted = gray < 8 ? 0 : 15;
        expected[i] = (uint8_t)((expected[i] & ~(15u << shift)) | (converted << shift));
    }
    display_prepare_direct_area(actual + GUARD, width, height, x, y, w, h);
    assert(!memcmp(actual, expected, sizeof(actual)));
    // 再次量化幂等；前后护栏与裁剪区外的两侧半字节始终原样。
    // Quantization is idempotent; guards and both outside edge nibbles remain unchanged.
    display_prepare_direct_area(actual + GUARD, width, height, x, y, w, h);
    assert(!memcmp(actual, expected, sizeof(actual)));
    ++checks;
}

static void check_invalid(void) {
    uint8_t actual[GUARD + MAX_BYTES + GUARD], expected[sizeof(actual)];
    memset(actual, 0x78, sizeof(actual));
    memcpy(expected, actual, sizeof(actual));
    const int dims[][2] = {{0, 2}, {-2, 2}, {2, 0}, {2, -1}, {3, 2}, {INT_MAX, INT_MAX}};
    for (size_t i = 0; i < sizeof(dims) / sizeof(*dims); ++i)
        display_prepare_direct_area(actual + GUARD, dims[i][0], dims[i][1], 0, 0, 2, 2);
    display_prepare_direct_area(NULL, 18, 12, 0, 0, 18, 12);
    assert(!memcmp(actual, expected, sizeof(actual)));
    ++checks;
}

int main(void) {
    // 每个打包字节都检查全覆盖、低半字节单独覆盖及高半字节单独覆盖。
    // Check every packed byte with both pixels covered, only its low nibble and only its high nibble.
    for (unsigned packed = 0; packed < 256; ++packed) {
        check_area(2, 1, 0, 0, 2, 1, packed);
        check_area(2, 1, 0, 0, 1, 1, packed);
        check_area(2, 1, 1, 0, 1, 1, packed);
    }
    // 相邻三行、所有起止奇偶组合与不同尾像素；额外字节亦由memcmp保护。
    // Exercise neighboring rows, all start/end parity combinations and different trailing pixels; memcmp also guards extra bytes.
    for (int x = 0; x < 8; ++x) for (int w = 1; w <= 18; ++w)
        for (int y = 0; y < 3; ++y) check_area(18, 12, x, y, w, 3, (unsigned)(x * 31 + w * 7 + y));
    const int rects[][4] = {
        {-3, -2, 8, 7}, {-1, 0, 1, 12}, {-1, 0, 2, 12}, {0, -1, 18, 1}, {0, -1, 18, 2},
        {17, 11, INT_MAX, INT_MAX}, {0, 0, INT_MAX, INT_MAX}, {-7, -6, INT_MAX, INT_MAX},
        {INT_MIN, INT_MIN, INT_MAX, INT_MAX}, {INT_MIN, 0, INT_MAX, 12}, {0, INT_MIN, 18, INT_MAX},
        {INT_MAX, 0, INT_MAX, 12}, {0, INT_MAX, 18, INT_MAX}, {18, 0, 1, 12}, {0, 12, 18, 1},
        {0, 0, 0, 12}, {0, 0, 18, 0}, {0, 0, -1, 12}, {0, 0, 18, -1},
        {0, 0, INT_MIN, INT_MIN}, {INT_MAX, INT_MAX, INT_MIN, INT_MIN},
        {-18, -12, 36, 24}, {16, 10, 2, 2}, {17, 11, 1, 1}, {0, 0, 18, 12}
    };
    for (size_t i = 0; i < sizeof(rects) / sizeof(*rects); ++i)
        for (unsigned seed = 0; seed < 256; seed += 17)
            check_area(18, 12, rects[i][0], rects[i][1], rects[i][2], rects[i][3], seed);
    check_invalid();
    printf("direct physical area: %u checks PASS; all packed values, odd nibbles, clipping/extremes, invalid dimensions and guards\n", checks);
    return 0;
}
