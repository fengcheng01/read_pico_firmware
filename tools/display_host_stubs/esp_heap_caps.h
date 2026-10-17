/* SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 * 中文：显示历史与夜间掩码存储分配边界。/ English: Display history and night-mask allocation boundary.
 */
#pragma once
#include <stddef.h>
#include <stdlib.h>
#define MALLOC_CAP_SPIRAM 1
#define MALLOC_CAP_8BIT 2
void* heap_caps_calloc(size_t n, size_t size, unsigned caps);
void* heap_caps_malloc(size_t size, unsigned caps);

// 查询函数仅出现在日志实参里；补内联桩避免宿主隐式声明。/ Query calls appear only in log arguments; inline stubs avoid implicit declarations.
static inline size_t heap_caps_get_free_size(int caps) { (void)caps; return 8u * 1024u * 1024u; }
static inline size_t heap_caps_get_largest_free_block(int caps) { (void)caps; return 4u * 1024u * 1024u; }
