/* 中文：调度测试替身。/ English: Scheduler test shim. */
#pragma once
#include "common.h"
void epd_draw_pixel(int x, int y, uint8_t color, uint8_t* framebuffer);
void epd_draw_line(int x0, int y0, int x1, int y1, uint8_t color, uint8_t* framebuffer);
