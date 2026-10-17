/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 *
 * 中文：无封面书的确定性生成封面：FNV 路径种子 + splitmix64 选版式，三种纯几何构图，
 * 纯 C 无平台依赖，重绘成本低于一次卡片刷新，故不做 TF 缩略图缓存。
 * English: Deterministic generated covers for coverless books: an FNV path seed plus
 * splitmix64 picks one of three pure-geometry layouts. Plain C without platform
 * dependencies; redrawing costs less than one card refresh, so no SD thumbnail cache.
 */
#include "book_cover_auto.h"

#include <stdlib.h>
#include <string.h>

#include "book_cover.h"

// 版式种子版本：构图调整时递增，只触发重新生成，不改变书与版式的绑定。
// / Layout seed version: bump when compositions change; books keep their layout binding.
#define BOOK_COVER_AUTO_VERSION 1u

typedef struct {
    uint64_t state; ///< splitmix64 状态 / splitmix64 state
} seed_stream_t;

static uint64_t seed_next(seed_stream_t* s) {
    s->state += UINT64_C(0x9e3779b97f4a7c15);
    uint64_t z = s->state;
    z = (z ^ (z >> 30)) * UINT64_C(0xbf58476d1ce4e5b9);
    z = (z ^ (z >> 27)) * UINT64_C(0x94d049bb133111eb);
    return z ^ (z >> 31);
}

static uint64_t cover_seed(const char* identity) {
    uint64_t hash = UINT64_C(14695981039346656037);
    for (const unsigned char* p = (const unsigned char*)(identity ? identity : ""); *p; ++p)
        hash = (hash ^ *p) * UINT64_C(1099511628211);
    hash ^= BOOK_COVER_AUTO_VERSION;
    seed_stream_t mix = { .state = hash };
    return seed_next(&mix);
}

static void fill(uint8_t* gray, int x0, int y0, int x1, int y1, uint8_t value) {
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > BOOK_COVER_W) x1 = BOOK_COVER_W;
    if (y1 > BOOK_COVER_H) y1 = BOOK_COVER_H;
    for (int y = y0; y < y1; ++y)
        memset(gray + (size_t)y * BOOK_COVER_W + x0, value, (size_t)(x1 - x0));
}

static void outline(uint8_t* gray, int inset, uint8_t value) {
    fill(gray, inset, inset, BOOK_COVER_W - inset, inset + 1, value);
    fill(gray, inset, BOOK_COVER_H - inset - 1, BOOK_COVER_W - inset, BOOK_COVER_H - inset, value);
    fill(gray, inset, inset, inset + 1, BOOK_COVER_H - inset, value);
    fill(gray, BOOK_COVER_W - inset - 1, inset, BOOK_COVER_W - inset, BOOK_COVER_H - inset, value);
}

// 纸纹：每 64 个噪声命中点加深一档，远低于灰阶可辨阈值，只消除大面积平涂的塑料感。
// / Paper grain: one shade darker on every 64th noise hit, well below discernible gray steps,
// only to keep large flats from reading as plastic.
static void paper_grain(uint8_t* gray, seed_stream_t* s, uint8_t base) {
    const uint8_t darker = base ? base - 1 : base;
    for (int y = 0; y < BOOK_COVER_H; ++y)
        for (int x = 0; x < BOOK_COVER_W; ++x) {
            uint64_t noise = s->state ^ ((uint64_t)x * UINT64_C(0x9e3779b97f4a7c15)) ^
                              ((uint64_t)y * UINT64_C(0xc2b2ae3d27d4eb4f));
            noise ^= noise >> 29;
            if ((noise & 63u) == 0) gray[(size_t)y * BOOK_COVER_W + x] = darker;
        }
}

// 藏书票：深底双线框，下缘横线，经典书卷气。/ Bookplate: dark ground, double frame, lower rule.
static void dark_library(uint8_t* gray, seed_stream_t* s) {
    uint8_t bg = (uint8_t)(0x30 + (int)(seed_next(s) & 1u) * 0x08);
    fill(gray, 0, 0, BOOK_COVER_W, BOOK_COVER_H, bg);
    paper_grain(gray, s, bg);
    outline(gray, 7, 0xe8);
    outline(gray, 10, 0xc0);
    fill(gray, 55, 163, 91, 164, 0xd8);
}

// 留白：近白底细框，左竖线，右下双色块。/ Whitespace: near-white ground, thin frames,
// a left rule and two-tone blocks at the lower right.
static void modern_whitespace(uint8_t* gray, seed_stream_t* s) {
    uint8_t bg = (uint8_t)(0xf4 - (int)(seed_next(s) & 1u) * 0x06);
    fill(gray, 0, 0, BOOK_COVER_W, BOOK_COVER_H, bg);
    paper_grain(gray, s, bg);
    outline(gray, 6, 0x70);
    outline(gray, 8, 0xb8);
    fill(gray, 24, 46, 25, 133, 0x50);
    int accent = 96 + (int)(seed_next(s) % 12u);
    fill(gray, accent, 130, accent + 18, 168, 0x60);
    fill(gray, accent + 13, 150, accent + 26, 174, 0x90);
}

// 分割：上深下浅两段，底段细横纹，上段角落色块。/ Split: dark panel over light ground with
// fine ruled lines below and a corner block above.
static void split_dark(uint8_t* gray, seed_stream_t* s) {
    int panel = 80 + (int)(seed_next(s) % 16u);
    fill(gray, 0, 0, BOOK_COVER_W, BOOK_COVER_H, 0xfa);
    fill(gray, 0, 0, BOOK_COVER_W, panel, 0x40);
    fill(gray, 0, panel, BOOK_COVER_W, panel + 3, 0x90);
    for (int y = panel + 8; y < BOOK_COVER_H; y += 8)
        fill(gray, 14, y, BOOK_COVER_W - 14, y + 1, 0xe0);
    int block = 14 + (int)(seed_next(s) % 24u);
    fill(gray, block, 12, block + 16, 28, 0xd8);
}

uint8_t* book_cover_auto_alloc(const char* identity) {
    if (!identity || !*identity) return NULL;
    uint8_t* gray = malloc((size_t)BOOK_COVER_W * BOOK_COVER_H);
    if (!gray) return NULL;
    seed_stream_t s = { .state = cover_seed(identity) };
    switch (seed_next(&s) % 3u) {
        case 0: dark_library(gray, &s); break;
        case 1: modern_whitespace(gray, &s); break;
        default: split_dark(gray, &s); break;
    }
    return gray;
}
