/* SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 * 中文：图片解码的分配失败注入。/ English: Allocation failure injection for image decoding.
 * 冻结：仅用于宿主测试。/ Frozen: Host tests only.
 */
#pragma once
#include <stdlib.h>
#define MALLOC_CAP_SPIRAM 1
#define MALLOC_CAP_8BIT 2
extern int image_fail_after;
static inline int image_allow(void) {
    if (image_fail_after == 0) return 0;
    if (image_fail_after > 0) --image_fail_after;
    return 1;
}
static inline void* heap_caps_malloc(size_t n, int caps) { (void)caps; return image_allow() ? malloc(n) : NULL; }
static inline void* heap_caps_calloc(size_t n, size_t s, int caps) { (void)caps; return image_allow() ? calloc(n,s) : NULL; }
static inline void* heap_caps_realloc(void* p, size_t n, int caps) { (void)caps; return image_allow() ? realloc(p,n) : NULL; }
static inline size_t heap_caps_get_free_size(int caps) { (void)caps; return 8u * 1024u * 1024u; }
static inline size_t heap_caps_get_largest_free_block(int caps) { (void)caps; return 4u * 1024u * 1024u; }
