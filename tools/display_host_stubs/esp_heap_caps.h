/* SPDX-License-Identifier: Apache-2.0
 * 中文：显示历史存储分配边界。/ English: Display history allocation boundary.
 */
#pragma once
#include <stdlib.h>
#define MALLOC_CAP_SPIRAM 1
#define MALLOC_CAP_8BIT 2
void* heap_caps_calloc(size_t n, size_t size, unsigned caps);
