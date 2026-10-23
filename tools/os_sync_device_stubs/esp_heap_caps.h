/* 宿主设备内存替身。/ Host device-memory fake. */
#pragma once
#include <stddef.h>
#define MALLOC_CAP_SPIRAM 1
#define MALLOC_CAP_8BIT 2
void* heap_caps_malloc(size_t size, int caps);
void heap_caps_free(void* ptr);
