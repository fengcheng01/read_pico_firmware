/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 * 中文：真实采样任务与手势识别器回归；显示阻塞期间仍采到连续轻点，切页/睡眠/溢出安全取消。
 * English: Test the production sampler and recognizer across blocked display, transitions, sleep and overflow.
 */
#include "app_touch_input.h"
#include "ui_gesture.h"
#include <pthread.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

static pthread_mutex_t device_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t read_condition = PTHREAD_COND_INITIALIZER;
static cst836u_touch_t device_touch;
static int device_error, reads;
static int64_t fake_ms;
int64_t esp_timer_get_time(void) {
    pthread_mutex_lock(&device_mutex); int64_t result = fake_ms * 1000;
    pthread_mutex_unlock(&device_mutex); return result;
}
int cst836u_read(void* tp, cst836u_touch_t* out) {
    assert(tp); pthread_mutex_lock(&device_mutex);
    *out = device_touch; int err = device_error; ++reads;
    pthread_cond_broadcast(&read_condition); pthread_mutex_unlock(&device_mutex); return err;
}
void* xSemaphoreCreateMutex(void) {
    pthread_mutex_t* m = malloc(sizeof(*m)); assert(m); pthread_mutex_init(m, NULL); return m;
}
int xSemaphoreTake(void* m, unsigned ticks) {(void)ticks; return !pthread_mutex_lock(m);}
int xSemaphoreGive(void* m) {return !pthread_mutex_unlock(m);}
static void* start(void* arg) { ((void (*)(void*))arg)(NULL); return NULL; }
int xTaskCreatePinnedToCore(void (*fn)(void*), const char* name, unsigned stack, void* arg, unsigned priority, void** task, int core) {
    (void)name;(void)stack;(void)arg;(void)priority;(void)core;
    pthread_t thread; int result = pthread_create(&thread, NULL, start, (void*)fn);
    if (!result) {pthread_detach(thread); *task = (void*)1;} return result ? 0 : 1;
}
void vTaskDelay(int ms) {usleep((unsigned)ms * 1000);}
static void send_contact(int count, int x, int64_t ms, int error) {
    pthread_mutex_lock(&device_mutex);
    device_touch = (cst836u_touch_t){.touched=count>0,.count=count,.x=x,.y=400};
    device_touch.points[0].active = count > 0; device_touch.points[0].x=x; device_touch.points[0].y=400;
    device_error = error; fake_ms = ms;
    int goal = reads + 2;
    while (reads < goal) pthread_cond_wait(&read_condition, &device_mutex);
    pthread_mutex_unlock(&device_mutex);
}
int main(void) {
    cst836u_touch_t t = {0}; int64_t ms; cst836u_handle_t tp = (void*)1;
    app_touch_input_init(tp); app_touch_input_enable(true);
    // 模拟扫描期间没有任何前台读点，仍保留三次完整轻点及真实时刻。
    // Simulate a scan with no foreground reads, retaining three complete taps and their capture times.
    for (int i=0;i<3;++i) {send_contact(1,400,1000+i*100,0);send_contact(0,400,1040+i*100,0);}
    ui_gesture_t g={0}; bool down=false; int taps=0, presses=0;
    for (int i=0;i<6;++i) {
        assert(app_touch_input_read(tp,&t,&ms)==ESP_OK);
        assert(ms==1000+(i/2)*100+(i%2)*40);
        app_ctx_t ctx={.touch=&t,.now_ms=ms,.pressed=t.touched&&!down,.released=!t.touched&&down};
        ui_gesture_event_t event;
        if (ui_gesture_feed(&g,&ctx,&event)) {taps+=event.type==UI_GESTURE_TAP; presses+=event.type==UI_GESTURE_PRESS;assert(event.type!=UI_GESTURE_LONG_PRESS);}
        down=t.touched;
    }
    assert(taps==3&&presses==3&&!g.active);
    // 切页压住的原手指必须松开，下一轻点前先返回抬起沿。
    // A finger held across navigation must release; deliver that lift before the next tap.
    send_contact(1,400,2000,0); app_touch_input_reset();
    assert(app_touch_input_read(tp,&t,&ms)==ESP_OK&&!t.touched);
    send_contact(0,400,2040,0);send_contact(1,410,2100,0);send_contact(0,410,2140,0);
    for (int i=0;i<3;++i) {assert(app_touch_input_read(tp,&t,&ms)==ESP_OK);assert(t.touched==(i==1));}
    // 读错丢弃未完成动作；暂停后芯片仅由睡眠/诊断消费者访问。
    // Errors drop incomplete input; pause yields the chip exclusively to sleep/diagnostic consumers.
    send_contact(1,400,3000,0);send_contact(0,400,3040,9);
    assert(app_touch_input_read(tp,&t,&ms)==ESP_ERR_INVALID_STATE&&!t.touched);
    send_contact(0,400,3100,0);app_touch_input_reset();
    app_touch_input_enable(false);
    pthread_mutex_lock(&device_mutex);int stopped=reads;pthread_mutex_unlock(&device_mutex);
    usleep(30000);
    pthread_mutex_lock(&device_mutex);assert(reads==stopped);pthread_mutex_unlock(&device_mutex);
    app_touch_input_enable(true);
    send_contact(0,400,4000,0);app_touch_input_reset();
    for (int i=0;i<130;++i) send_contact(1,400+i,4100+i*10,0);
    assert(app_touch_input_read(tp,&t,&ms)==ESP_ERR_INVALID_STATE&&!t.touched);
    send_contact(0,500,5600,0);app_touch_input_reset();
    // 队列恰好在抬起时溢出，也必须保留恢复抬起沿，不能吞掉下一次轻点。
    // Overflow exactly on release must retain the recovery lift rather than swallow the next tap.
    for (int i=0;i<128;++i) send_contact(1,530+i,6000+i*10,0);
    send_contact(0,657,7300,0);
    assert(app_touch_input_read(tp,&t,&ms)==ESP_ERR_INVALID_STATE);
    assert(app_touch_input_read(tp,&t,&ms)==ESP_OK&&!t.touched);
    send_contact(1,400,7400,0);send_contact(0,400,7440,0);
    assert(app_touch_input_read(tp,&t,&ms)==ESP_OK&&t.touched);
    assert(app_touch_input_read(tp,&t,&ms)==ESP_OK&&!t.touched);
    app_touch_input_enable(false);
    puts("touch input: three taps across blocked display, timestamps, transition lift, read-error/overflow cancellation and sleep pause passed");
}
