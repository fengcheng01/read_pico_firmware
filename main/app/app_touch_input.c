/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 * 中文：有界触摸队列独立于刷屏，保持按下、移动、抬起的采样顺序。
 * English: A bounded touch queue independent of display scans preserves press/move/release sampling order.
 * 冻结：不解释点击，不在采样任务绘制；读错/溢出丢弃旧队列并取消手势。
 * Frozen: Never interpret taps or draw from the sampler; errors/overflow discard stale queued input and cancel gestures.
 * 冻结修订：第二批省电让禁用任务阻塞等启用通知；输入信号可提前结束页面的静止心跳，不放慢采样。
 * Frozen revision: Batch-two power saving blocks a disabled sampler until notified; input signals interrupt page idle heartbeats without slowing sampling.
 * 实机功耗审查：静止时 125Hz 盲轮询是持续的 I2C 与 CPU 唤醒压力；改为触摸中全速、松开转
 * 低速轮询，设备端用 CST836U INT 沿即时唤回（中断不工作时由低速超时兜底，行为不变慢于旧值）。
 * Power review: the idle 125Hz blind poll kept both I2C and the CPU busy; the sampler now runs
 * full speed only while touching, drops to a slow poll after release, and on device the CST836U
 * INT edge wakes it instantly (a dead INT falls back to the slow timeout, never slower than before).
 */
#include "app_touch_input.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include <string.h>

#define INPUT_CAPACITY 128
#define INPUT_ACTIVE_POLL_MS 8
#define INPUT_IDLE_POLL_MS 20

typedef struct {
    cst836u_touch_t touch;
    int64_t ms;
} input_sample_t;
static input_sample_t s_queue[INPUT_CAPACITY];
static unsigned s_head, s_count;
static cst836u_touch_t s_latest;
static bool s_enabled, s_suppress, s_fault;
static SemaphoreHandle_t s_mutex;
static SemaphoreHandle_t s_ready;
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
        bool touching = false;
        bool enabled;
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
            touching = s_latest.touched;
            if ((s_count || s_fault) && s_ready) xSemaphoreGive(s_ready);
        }
        enabled = s_enabled;
        xSemaphoreGive(s_mutex);
        if (!enabled) {
            ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
            continue;
        }
        if (touching) {
            // 触摸进行中保持全速；清掉挂起的沿避免下一轮误醒。
            // Full speed while touching; drain a pending edge so the next round wakes on a real one.
            ulTaskNotifyTake(pdTRUE, 0);
            vTaskDelay(pdMS_TO_TICKS(INPUT_ACTIVE_POLL_MS));
        } else {
            // 静止：等中断沿或低速超时，空闲 I2C 从 125Hz 降到 50Hz 兜底。
            // Idle: wait for an INT edge or the slow timeout; idle I2C drops from 125Hz to a 50Hz fallback.
            ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(INPUT_IDLE_POLL_MS));
        }
    }
}
#ifdef ESP_PLATFORM
#include "driver/gpio.h"
// CST836U INT 沿唤醒采样任务：芯片按下拉低 INT，读取本身兼作应答。INT 失灵由低速轮询兜底。
// The CST836U INT edge wakes the sampler: the chip pulls INT low on touch-down and the
// read itself acknowledges it. A dead INT falls back to the slow poll.
static void IRAM_ATTR touch_int_isr(void* arg) {
    BaseType_t woken = pdFALSE;
    vTaskNotifyGiveFromISR((TaskHandle_t)arg, &woken);
    portYIELD_FROM_ISR(woken);
}
static void wire_touch_interrupt(void) {
    gpio_num_t pin = cst836u_int_gpio(s_tp);
    if (pin < 0) return;
    esp_err_t service = gpio_install_isr_service(ESP_INTR_FLAG_IRAM);
    if (service != ESP_OK && service != ESP_ERR_INVALID_STATE) return;
    if (gpio_set_intr_type(pin, GPIO_INTR_ANYEDGE) != ESP_OK) return;
    (void)gpio_isr_handler_add(pin, touch_int_isr, s_task);
}
#endif
void app_touch_input_init(cst836u_handle_t tp) {
    if (s_task || !tp) return;
    if (!s_mutex) s_mutex = xSemaphoreCreateMutex();
    if (!s_mutex) return;
    if (!s_ready) s_ready = xSemaphoreCreateBinary();
    s_tp = tp;
    if (xTaskCreatePinnedToCore(sampler, "touch_input", 3072, NULL, 5, &s_task, 1) != pdPASS)
        s_task = NULL;
#ifdef ESP_PLATFORM
    if (s_task) wire_touch_interrupt();
#endif
}
void app_touch_input_enable(bool enabled) {
    if (!s_task) return;
    bool changed;
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    changed = s_enabled != enabled;
    if (changed) {
        s_enabled = enabled; s_count = s_head = 0; s_fault = false;
        s_suppress = false; memset(&s_latest, 0, sizeof(s_latest));
    }
    xSemaphoreGive(s_mutex);
    if (changed) xTaskNotifyGive(s_task);
}
void app_touch_input_wait(uint32_t timeout_ms) {
    if (!s_task || !s_ready) { vTaskDelay(pdMS_TO_TICKS(5)); return; }
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    bool enabled = s_enabled;
    bool pending = s_count || s_fault;
    xSemaphoreGive(s_mutex);
    if (!enabled) { vTaskDelay(pdMS_TO_TICKS(5)); return; }
    if (pending) return;
    xSemaphoreTake(s_ready, pdMS_TO_TICKS(timeout_ms > 50 ? 50 : timeout_ms));
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
