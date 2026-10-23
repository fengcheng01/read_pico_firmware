/* 宿主设备边界替身。/ Host device-boundary fake. */
#pragma once
#include "FreeRTOS.h"
int xTaskCreate(void (*fn)(void*),const char*,unsigned,void*,int,void*);
TaskHandle_t xTaskCreateStatic(void (*fn)(void*),const char*,unsigned,void*,int,StackType_t*,StaticTask_t*);
void vTaskDelete(void*);

void vTaskDelay(uint32_t ticks);

int xTaskCreatePinnedToCore(void (*fn)(void*),const char*,unsigned,void*,int,TaskHandle_t*,int);
unsigned ulTaskNotifyTake(int clear, uint32_t ticks);
void xTaskNotifyGive(TaskHandle_t task);
unsigned uxTaskGetStackHighWaterMark(TaskHandle_t task);
