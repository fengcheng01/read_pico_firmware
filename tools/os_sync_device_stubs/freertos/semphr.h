/* 宿主设备边界替身。/ Host device-boundary fake. */
#pragma once
#include <stdint.h>
typedef struct test_sem* SemaphoreHandle_t;
SemaphoreHandle_t xSemaphoreCreateBinary(void);
int xSemaphoreGive(SemaphoreHandle_t);
int xSemaphoreTake(SemaphoreHandle_t,uint32_t);
