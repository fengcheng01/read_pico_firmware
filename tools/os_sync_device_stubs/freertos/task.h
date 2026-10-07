/* 宿主设备边界替身。/ Host device-boundary fake. */
#pragma once
int xTaskCreate(void (*fn)(void*),const char*,unsigned,void*,int,void*);
void vTaskDelete(void*);

void vTaskDelay(uint32_t ticks);

int xTaskCreatePinnedToCore(void (*fn)(void*),const char*,unsigned,void*,int,void*,int);
