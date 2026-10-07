/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 * 中文：有界触摸队列独立于刷屏，保持按下、移动、抬起的采样顺序。
 * English: A bounded touch queue independent of display scans preserves press/move/release sampling order.
 * 冻结：不解释点击，不在采样任务绘制；读错/溢出丢弃旧队列并取消手势。
 * Frozen: Never interpret taps or draw from the sampler; errors/overflow discard stale queued input and cancel gestures.
 */
#include "app_touch_input.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include <string.h>

#define INPUT_CAPACITY 128
#define INPUT_POLL_MS 8

typedef struct {
    cst836u_touch_t touch;
    int64_t ms;
} input_sample_t;
static input_sample_t s_queue[INPUT_CAPACITY];
static unsigned s_head, s_count;
static cst836u_touch_t s_latest;
static bool s_enabled, s_suppress, s_fault;
static SemaphoreHandle_t s_mutex;
static TaskHandle_t s_task;
static cst836u_handle_t s_tp;

static bool same_contact(const cst836u_touch_t* a, const cst836u_touch_t* b) {
    if (a->touched != b->touched || a->count != b->count) return false;
    if (!a->touched) return true;
    for (int i = 0; i < CST836U_MAX_POINTS; ++i)
        if (a->points[i].active != b->points[i].active ||
            (a->points[i].active && (a->points[i].x != b->points[i].x || a->points[i].y != b->points[i].y))) return false;
    return a->x == b->x && a->y == b->y;
}
static void sampler(void* unused) {
    (void)unused;
    for (;;) {
        xSemaphoreTake(s_mutex, portMAX_DELAY);
        if (s_enabled) {
            cst836u_touch_t touch = {0};
            esp_err_t err = cst836u_read(s_tp, &touch);
            int64_t now = esp_timer_get_time() / 1000;
            if (err != ESP_OK) {
                s_count = s_head = 0; s_fault = true;
                s_suppress = true; memset(&s_latest, 0, sizeof(s_latest));
            } else {
                bool changed = !same_contact(&touch, &s_latest);
                s_latest = touch;
                if (s_suppress) {
                    if (!touch.touched) {
                        s_suppress = false;
                        s_queue[(s_head + s_count) % INPUT_CAPACITY] = (input_sample_t){touch, now};
                        ++s_count;
                    }
                } else if (changed) {
                    if (s_count == INPUT_CAPACITY) {
                        s_count = s_head = 0; s_fault = true;
                        s_suppress = touch.touched;
                        if (!touch.touched) {
                            s_queue[0] = (input_sample_t){touch, now}; s_count = 1;
                        }
                    } else {
                        s_queue[(s_head + s_count) % INPUT_CAPACITY] = (input_sample_t){touch, now};
                        ++s_count;
                    }
                }
            }
        }
        xSemaphoreGive(s_mutex);
        vTaskDelay(pdMS_TO_TICKS(INPUT_POLL_MS));
    }
}
void app_touch_input_init(cst836u_handle_t tp) {
    if (s_task || !tp) return;
    if (!s_mutex) s_mutex = xSemaphoreCreateMutex();
    if (!s_mutex) return;
    s_tp = tp;
    if (xTaskCreatePinnedToCore(sampler, "touch_input", 3072, NULL, 5, &s_task, 1) != pdPASS)
        s_task = NULL;
}
void app_touch_input_enable(bool enabled) {
    if (!s_task) return;
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    if (s_enabled != enabled) {
        s_enabled = enabled; s_count = s_head = 0; s_fault = false;
        s_suppress = false; memset(&s_latest, 0, sizeof(s_latest));
    }
    xSemaphoreGive(s_mutex);
}
void app_touch_input_reset(void) {
    if (!s_task) return;
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    s_count = s_head = 0; s_fault = false; s_suppress = s_latest.touched;
    xSemaphoreGive(s_mutex);
}
esp_err_t app_touch_input_read(cst836u_handle_t tp, cst836u_touch_t* out, int64_t* sampled_ms) {
    if (s_task) {
        xSemaphoreTake(s_mutex, portMAX_DELAY);
        if (s_enabled) {
            *sampled_ms = esp_timer_get_time() / 1000;
            esp_err_t err = ESP_OK;
            if (s_fault) {
                s_fault = false; memset(out, 0, sizeof(*out)); err = ESP_ERR_INVALID_STATE;
            } else if (s_count) {
                input_sample_t* sample = &s_queue[s_head];
                *out = sample->touch; *sampled_ms = sample->ms;
                s_head = (s_head + 1) % INPUT_CAPACITY; --s_count;
            } else if (s_suppress) memset(out, 0, sizeof(*out));
            else *out = s_latest;
            xSemaphoreGive(s_mutex);
            return err;
        }
        xSemaphoreGive(s_mutex);
    }
    esp_err_t err = cst836u_read(tp, out);
    *sampled_ms = esp_timer_get_time() / 1000;
    return err;
}
