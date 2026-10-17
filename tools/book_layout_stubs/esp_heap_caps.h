/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 * 中文：宿主布局测试的内存适配。/ English: Memory adapter for host layout tests.
 * 冻结：仅供测试。/ Frozen: Tests only.
 */
#pragma once
#include <stdlib.h>
#include <stdbool.h>
#define MALLOC_CAP_SPIRAM 1
#define MALLOC_CAP_8BIT 2
extern size_t host_layout_max_request, host_layout_fail_above;
static inline bool layout_allocation_ok(size_t n) {
    if (n > host_layout_max_request) host_layout_max_request = n;
    return !host_layout_fail_above || n <= host_layout_fail_above;
}
static inline void* heap_caps_malloc(size_t n, int caps) { (void)caps; return layout_allocation_ok(n) ? malloc(n) : NULL; }
static inline void* heap_caps_realloc(void* p, size_t n, int caps) { (void)caps; return layout_allocation_ok(n) ? realloc(p, n) : NULL; }

// 查询函数仅出现在日志实参里；补内联桩避免宿主隐式声明。/ Query calls appear only in log arguments; inline stubs avoid implicit declarations.
static inline size_t heap_caps_get_free_size(int caps) { (void)caps; return 8u * 1024u * 1024u; }
static inline size_t heap_caps_get_largest_free_block(int caps) { (void)caps; return 4u * 1024u * 1024u; }
