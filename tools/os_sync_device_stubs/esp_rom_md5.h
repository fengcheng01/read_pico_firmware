/* 宿主设备边界替身。/ Host device-boundary fake. */
#pragma once
#include <stdint.h>
typedef struct {unsigned sum;} md5_context_t;
void esp_rom_md5_init(md5_context_t*);
void esp_rom_md5_update(md5_context_t*,const void*,uint32_t);
void esp_rom_md5_final(unsigned char[16],md5_context_t*);
