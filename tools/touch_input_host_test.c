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
#include <time.h>
#include <errno.h>
#include <stdatomic.h>

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
typedef struct { pthread_mutex_t mutex; pthread_cond_t cond; bool binary; unsigned count; } semaphore_t;
static atomic_uint binary_takes;
static atomic_bool binary_received;
void* xSemaphoreCreateMutex(void) {
    semaphore_t* s = calloc(1,sizeof(*s)); assert(s);
    pthread_mutex_init(&s->mutex,NULL); pthread_cond_init(&s->cond,NULL); return s;
}
void* xSemaphoreCreateBinary(void) { semaphore_t* s=xSemaphoreCreateMutex();s->binary=true;return s; }
static struct timespec deadline(unsigned ticks) {
    struct timespec t;clock_gettime(CLOCK_REALTIME,&t);t.tv_sec+=ticks/1000;t.tv_nsec+=(ticks%1000)*1000000L;
    if(t.tv_nsec>=1000000000L){++t.tv_sec;t.tv_nsec-=1000000000L;}return t;
}
int xSemaphoreTake(void* m, unsigned ticks) {
    semaphore_t* s=m;if(!s->binary)return !pthread_mutex_lock(&s->mutex);
    pthread_mutex_lock(&s->mutex);struct timespec until=deadline(ticks);
    atomic_fetch_add(&binary_takes,1);
    while(!s->count){if(!ticks||pthread_cond_timedwait(&s->cond,&s->mutex,&until)==ETIMEDOUT)break;}
    int ready=s->count;if(ready)--s->count;atomic_store(&binary_received,ready!=0);
    pthread_mutex_unlock(&s->mutex);return ready;
}
int xSemaphoreGive(void* m) {
    semaphore_t* s=m;if(!s->binary)return !pthread_mutex_unlock(&s->mutex);
    pthread_mutex_lock(&s->mutex);s->count=1;pthread_cond_signal(&s->cond);pthread_mutex_unlock(&s->mutex);return 1;
}
static void* start(void* arg) { ((void (*)(void*))arg)(NULL); return NULL; }
int xTaskCreatePinnedToCore(void (*fn)(void*), const char* name, unsigned stack, void* arg, unsigned priority, void** task, int core) {
    (void)name;(void)stack;(void)arg;(void)priority;(void)core;
    pthread_t thread; int result = pthread_create(&thread, NULL, start, (void*)fn);
    if (!result) {pthread_detach(thread); *task = (void*)1;} return result ? 0 : 1;
}
void vTaskDelay(int ms) {assert(ms==8);usleep((unsigned)ms * 1000);}
static pthread_mutex_t notify_mutex=PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t notify_condition=PTHREAD_COND_INITIALIZER;
static unsigned notifications, notify_waits;
void xTaskNotifyGive(void* task) {
    assert(task);pthread_mutex_lock(&notify_mutex);++notifications;
    pthread_cond_signal(&notify_condition);pthread_mutex_unlock(&notify_mutex);
}
unsigned long ulTaskNotifyTake(int clear, unsigned ticks) {
    pthread_mutex_lock(&notify_mutex);++notify_waits;struct timespec until=deadline(ticks);
    while(!notifications){if(!ticks)break;
        if(ticks==UINT32_MAX)pthread_cond_wait(&notify_condition,&notify_mutex);
        else if(pthread_cond_timedwait(&notify_condition,&notify_mutex,&until)==ETIMEDOUT)break;
    }
    unsigned result=notifications;if(clear)notifications=0;else if(notifications)--notifications;
    pthread_mutex_unlock(&notify_mutex);return result;
}
static void send_contact(int count, int x, int64_t ms, int error) {
    pthread_mutex_lock(&device_mutex);
    device_touch = (cst836u_touch_t){.touched=count>0,.count=count,.x=x,.y=400};
    device_touch.points[0].active = count > 0; device_touch.points[0].x=x; device_touch.points[0].y=400;
    device_error = error; fake_ms = ms;
    int goal = reads + 2;
    while (reads < goal) pthread_cond_wait(&read_condition, &device_mutex);
    pthread_mutex_unlock(&device_mutex);
}
static void* idle_waiter(void* unused) {(void)unused;app_touch_input_wait(50);return NULL;}
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
    // 静止心跳有界，排队触摸不等待超时；该信号不替代采样队列。
    // Idle waits are bounded and queued input bypasses the timeout; the signal never replaces queued samples.
    app_touch_input_wait(1);
    app_touch_input_wait(500);
    pthread_t waiting;unsigned takes=atomic_load(&binary_takes);
    assert(!pthread_create(&waiting,NULL,idle_waiter,NULL));
    while(atomic_load(&binary_takes)==takes)usleep(1000);
    send_contact(1,400,4010,0);
    pthread_join(waiting,NULL);assert(atomic_load(&binary_received));
    app_touch_input_wait(50);
    assert(app_touch_input_read(tp,&t,&ms)==ESP_OK&&t.touched&&ms==4010);
    send_contact(0,400,4020,0);app_touch_input_reset();
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
    // 功耗审查回归：静止时读频显著低于触摸中（125Hz→50Hz 兜底），禁用后完全停读。
    // Power-review regression: idle reads land well below touching reads (125Hz to the
    // 50Hz fallback), and disabling stops reads entirely.
    app_touch_input_enable(true);
    pthread_mutex_lock(&device_mutex); device_touch.touched=false; device_error=0; pthread_mutex_unlock(&device_mutex);
    usleep(60000);
    pthread_mutex_lock(&device_mutex); int idle_before=reads; pthread_mutex_unlock(&device_mutex);
    usleep(200000);
    pthread_mutex_lock(&device_mutex); int idle_after=reads; pthread_mutex_unlock(&device_mutex);
    assert(idle_after - idle_before <= 14);
    pthread_mutex_lock(&device_mutex);
    device_touch.touched=true; device_touch.count=1; device_touch.points[0].active=true;
    pthread_mutex_unlock(&device_mutex);
    usleep(20000);
    pthread_mutex_lock(&device_mutex); int active_before=reads; pthread_mutex_unlock(&device_mutex);
    usleep(200000);
    pthread_mutex_lock(&device_mutex); int active_after=reads; pthread_mutex_unlock(&device_mutex);
    // 宿主定时器会合并唤醒；精确请求已在vTaskDelay断言，墙钟只比较两档而非假定实时调度。
    // Host timers coalesce wakeups; vTaskDelay asserts the exact request, while wall time compares rates without assuming real-time scheduling.
    assert(active_after - active_before >= 10 && active_after - active_before > idle_after - idle_before);
    app_touch_input_enable(false);
    usleep(30000);
    pthread_mutex_lock(&device_mutex); assert(reads == active_after); pthread_mutex_unlock(&device_mutex);
    pthread_mutex_lock(&notify_mutex);unsigned waits=notify_waits;pthread_mutex_unlock(&notify_mutex);
    usleep(80000);
    pthread_mutex_lock(&notify_mutex);assert(notify_waits==waits);pthread_mutex_unlock(&notify_mutex);
    puts("touch input: three taps across blocked display, timestamps, transition lift, read-error/overflow cancellation, sleep pause and adaptive idle rate passed");
}
