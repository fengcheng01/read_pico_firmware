/* 宿主设备边界替身。/ Host device-boundary fake. */
#pragma once
#include <stdint.h>
#define pdPASS 1
#define pdTRUE 1
#define portMAX_DELAY UINT32_MAX

#define pdMS_TO_TICKS(ms) (ms)
