/* SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 * 中文：主机堆平台接口替身。/ English: Host heap platform shim.
 * 冻结：仅用于测试。/ Frozen: Tests only.
 */
#pragma once
#include <stdlib.h>
#define MALLOC_CAP_SPIRAM 1
#define MALLOC_CAP_8BIT 2
#ifdef BOOK_HEAP_TRACK
void *book_test_malloc(size_t n);
void *book_test_calloc(size_t n, size_t s);
void *book_test_realloc(void *p, size_t n);
#define malloc book_test_malloc
#define calloc book_test_calloc
#define realloc book_test_realloc
#ifdef BOOK_HEAP_DIRECT_FREE
void book_test_free(void *p);
#define free book_test_free
#endif
#endif
static inline void *heap_caps_malloc(size_t n, int caps) { (void)caps; return malloc(n); }
static inline void *heap_caps_calloc(size_t n, size_t s, int caps) { (void)caps; return calloc(n,s); }
static inline void *heap_caps_realloc(void *p, size_t n, int caps) { (void)caps; return realloc(p,n); }

// 查询函数仅出现在日志实参里；补内联桩避免宿主隐式声明。/ Query calls appear only in log arguments; inline stubs avoid implicit declarations.
static inline size_t heap_caps_get_free_size(int caps) { (void)caps; return 8u * 1024u * 1024u; }
static inline size_t heap_caps_get_largest_free_block(int caps) { (void)caps; return 4u * 1024u * 1024u; }
