/* SPDX-License-Identifier: Apache-2.0
 * 中文：图片解码按已运行时间让出CPU，避免固定图块次数累积休眠。
 * English: Yield image decoding by elapsed runtime instead of accumulating sleeps per fixed block count.
 */
#pragma once
#include <stdint.h>
#ifdef ESP_PLATFORM
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#endif
static inline void book_image_yield(int64_t* last) {
#ifdef ESP_PLATFORM
    int64_t now = esp_timer_get_time();
    if (!*last) *last = now;
    if (now - *last >= 40000) {
        vTaskDelay(1);
        *last = esp_timer_get_time();
    }
#else
    (void)last;
#endif
}
