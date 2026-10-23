/* SPDX-License-Identifier: Apache-2.0
 * 中文：封面任务内存故障替身。/ English: Cover-worker heap fault adapter.
 */
#pragma once
#include <stddef.h>
#define MALLOC_CAP_SPIRAM 1
#define MALLOC_CAP_8BIT 2
#define MALLOC_CAP_INTERNAL 4
void* test_cover_alloc(size_t bytes);
void* test_cover_calloc(size_t count, size_t bytes);
extern size_t test_cover_free;
static inline void* heap_caps_malloc(size_t bytes, unsigned caps) { (void)caps; return test_cover_alloc(bytes); }
static inline void* heap_caps_calloc(size_t count, size_t bytes, unsigned caps) { (void)caps; return test_cover_calloc(count, bytes); }
static inline size_t heap_caps_get_free_size(unsigned caps) { (void)caps; return test_cover_free; }
static inline size_t heap_caps_get_largest_free_block(unsigned caps) { (void)caps; return test_cover_free; }
