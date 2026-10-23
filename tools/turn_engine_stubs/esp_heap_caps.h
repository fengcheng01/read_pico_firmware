/* SPDX-License-Identifier: Apache-2.0
 * 中文：可注入失败的相位表分配。/ English: Injectable phase-table allocation.
 */
#pragma once
#include <stddef.h>
#define MALLOC_CAP_SPIRAM 1
#define MALLOC_CAP_8BIT 2
void* heap_caps_aligned_alloc(size_t,size_t,unsigned);
void heap_caps_free(void*);
