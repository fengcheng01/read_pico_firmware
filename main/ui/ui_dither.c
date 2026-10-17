/*
 * SPDX-FileCopyrightText: 2026 mindreset
 *
 * 中文：8x8 Bayer 有序抖动：16 级量化时用 6 位余数对 64 档阈值取进位，
 * 比原先 4x4/4 位余数的梯度更细腻。
 * English: 8x8 Bayer ordered dither: the 16-level quantizer promotes its 6-bit
 * remainder against 64 threshold phases, giving finer gradients than the old
 * 4x4 matrix with a 4-bit remainder.
 *
 * 冻结：见头文件——只服务图片像素。/ Frozen: see the header — image pixels only.
 */
#include "ui_dither.h"

// 标准 8x8 Bayer 阈值阵（0..63）。/ Standard 8x8 Bayer threshold matrix (0..63).
static const uint8_t k_bayer8[64] = {
     0, 32,  8, 40,  2, 34, 10, 42,
    48, 16, 56, 24, 50, 18, 58, 26,
    12, 44,  4, 36, 14, 46,  6, 38,
    60, 28, 52, 20, 62, 30, 54, 22,
     3, 35, 11, 43,  1, 33,  9, 41,
    51, 19, 59, 27, 49, 17, 57, 25,
    15, 47,  7, 39, 13, 45,  5, 37,
    63, 31, 55, 23, 61, 29, 53, 21,
};

uint8_t ui_image_dither(uint8_t gray, int x, int y) {
    // (v*15 + b) >> 8 等价于对 v*15/255 的余数按 64 档相位取进位；b 取 2..254，端点 0/255 不动。
    // (v*15 + b) >> 8 promotes the remainder of v*15/255 against 64 phases; b spans 2..254 and endpoints 0/255 stay fixed.
    unsigned t = (unsigned)k_bayer8[(((unsigned)y & 7u) << 3) | ((unsigned)x & 7u)] * 4u + 2u;
    unsigned q = ((unsigned)gray * 15u + t) >> 8;
    return (uint8_t)(q * 17u);
}
