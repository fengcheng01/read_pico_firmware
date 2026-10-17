/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 * 中文：HTML 宿主测试分配器。/ English: Allocator for HTML host tests.
 * 冻结：仅供测试。/ Frozen: Tests only.
 */
#pragma once
#include <stdlib.h>
#define MALLOC_CAP_SPIRAM 1
#define MALLOC_CAP_8BIT 2
extern int html_test_fail_after;
static inline void* heap_caps_realloc(void* p, size_t n, int caps) {
    (void)caps;
    if (html_test_fail_after == 0) return NULL;
    if (html_test_fail_after > 0) html_test_fail_after--;
    return realloc(p, n);
}

// 查询函数仅出现在日志实参里；补内联桩避免宿主隐式声明。/ Query calls appear only in log arguments; inline stubs avoid implicit declarations.
static inline size_t heap_caps_get_free_size(int caps) { (void)caps; return 8u * 1024u * 1024u; }
static inline size_t heap_caps_get_largest_free_block(int caps) { (void)caps; return 4u * 1024u * 1024u; }
