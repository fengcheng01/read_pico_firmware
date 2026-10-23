/*
 * SPDX-License-Identifier: Apache-2.0
 * 中文：真实封面任务的跨页、单任务、失效和睡眠汇合回归。
 * English: Real cover-worker regression for tab switches, single-job ownership, invalidation and sleep joins.
 */
#include "book_cover.h"
#include "book_store.h"
#include "app_sleep_hooks.h"
#include "freertos/FreeRTOS.h"
#include <assert.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static atomic_bool gate;
static atomic_int loads;
static unsigned revision;
static bool fail_spawn;
static pthread_t worker;
static pthread_mutex_t notify_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t notify_cond = PTHREAD_COND_INITIALIZER;
static unsigned notifications;
static size_t largest_internal = 15872;
static bool worker_active;
size_t test_cover_free = 8u*1024u*1024u;
static bool fail_alloc;
static int fail_reads;
void* test_cover_alloc(size_t n) {return fail_alloc ? NULL : malloc(n);}
void* test_cover_calloc(size_t count,size_t n) {return fail_alloc ? NULL : calloc(count,n);}
static unsigned worker_count, delete_count, max_active;
unsigned book_store_revision(void) { return revision; }
static void* run(void* fn) { ((void(*)(void*))fn)(NULL); return NULL; }
int xTaskCreatePinnedToCore(void(*fn)(void*),const char* name,unsigned stack,void* arg,int priority,TaskHandle_t* handle,int core) {
    (void)name;(void)arg;(void)priority;(void)core;
    assert(stack == 15u*1024u && handle);
    if (fail_spawn || largest_internal < stack) return 0;
    assert(!worker_active);
    assert(!pthread_create(&worker,NULL,run,(void*)fn));
    ++worker_count;
    worker_active = true;
    max_active = 1;
    *handle = &worker;
    return pdPASS;
}
static void unlock(void* lock) { pthread_mutex_unlock(lock); }
unsigned ulTaskNotifyTake(int clear, uint32_t ticks) {
    assert(clear == pdTRUE && ticks == portMAX_DELAY);
    pthread_mutex_lock(&notify_lock);
    pthread_cleanup_push(unlock, &notify_lock);
    while (!notifications) pthread_cond_wait(&notify_cond, &notify_lock);
    notifications = 0;
    pthread_cleanup_pop(1);
    return 1;
}
void xTaskNotifyGive(TaskHandle_t task) {
    assert(task == &worker && worker_active);
    pthread_mutex_lock(&notify_lock);
    ++notifications;
    pthread_cond_signal(&notify_cond);
    pthread_mutex_unlock(&notify_lock);
}
void vTaskDelete(void* task) {
    assert(task == &worker && worker_active);
    pthread_cancel(worker); assert(!pthread_join(worker,NULL));
    worker_active = false; ++delete_count;
}
unsigned uxTaskGetStackHighWaterMark(TaskHandle_t task) {(void)task; return 4096;}
void vTaskDelay(uint32_t ticks) {usleep(ticks*1000);}
book_cover_status_t book_cover_read(const char* path,uint8_t** out) {
    atomic_fetch_add(&loads,1);
    while(!atomic_load(&gate)) usleep(1000);
    if (fail_reads) { --fail_reads; return BOOK_COVER_RETRY; }
    if (!strcmp(path, "g.txt")) return BOOK_COVER_ABSENT;
    *out=malloc((size_t)BOOK_COVER_W*BOOK_COVER_H);assert(*out);
    memset(*out,(uint8_t)path[0],(size_t)BOOK_COVER_W*BOOK_COVER_H);return BOOK_COVER_READY;
}
static void wait_loads(int expected) {for(int n=0;atomic_load(&loads)<expected;++n){assert(n<5000);usleep(1000);}}
static uint8_t* result(const char* path) {
    uint8_t* out=NULL;
    for(int n=0;!book_cover_poll(path,&out);++n){assert(n<5000);usleep(1000);}
    return out;
}
int main(void) {
    char folder[]="/tmp/rp-cover.XXXXXX"; assert(mkdtemp(folder)); assert(!chdir(folder));
    for(char c='a';c<='g';++c) {char path[32];snprintf(path,sizeof(path),"%c.%s",c,c=='g'?"txt":"epub");FILE* f=fopen(path,"wb");assert(f);fputc(c,f);fclose(f);}
    for(unsigned i=0;i<19;++i){char path[32];snprintf(path,sizeof(path),"cache%02u.epub",i);FILE* f=fopen(path,"wb");assert(f);fputc((int)i,f);fclose(f);}

    uint8_t* out=NULL;
    fail_spawn=true;assert(book_cover_poll("a.epub",&out)&&!out);fail_spawn=false;
    largest_internal=15u*1024u-1;assert(book_cover_poll("a.epub",&out)&&!out);
    largest_internal=15872;
    assert(!book_cover_poll("a.epub",&out));wait_loads(1);
    for(int i=0;i<100;i++) assert(!book_cover_poll("b.epub",&out));
    assert(worker_count==1);atomic_store(&gate,true);
    out=result("b.epub");assert(out&&*out=='b'&&worker_count==1);free(out);
    // 已放弃的a结果也可跨页复用，不再打开解码器；调用方持有独立副本。
    // Abandoned a-results are reusable across pages without decoder work; callers own independent copies.
    int before=atomic_load(&loads);
    assert(book_cover_poll("a.epub",&out)&&out&&*out=='a');*out=0;free(out);
    assert(book_cover_poll("a.epub",&out)&&out&&*out=='a'&&atomic_load(&loads)==before);free(out);
    atomic_store(&gate,false);assert(!book_cover_poll("c.epub",&out));wait_loads(3);
    ++revision;atomic_store(&gate,true);out=result("c.epub");assert(out&&*out=='c'&&worker_count==1);free(out);
    assert(!book_cover_poll("d.epub",&out));app_sleep_prepare_run();assert(!worker_active);
    assert(!book_cover_poll("e.epub",&out));book_cover_join();assert(!worker_active);
    // 汇合释放WiFi所需内部RAM；返回后的15,872B连续块可重建15KiB任务。
    // Join releases internal RAM for WiFi; the observed15,872-byte return block can rebuild the15KiB worker.
    largest_internal = 15872;
    out=result("a.epub");assert(out&&*out=='a');free(out);
    before=atomic_load(&loads);
    FILE* changed=fopen("a.epub","ab");assert(changed);fputc('x',changed);fclose(changed);
    out=result("a.epub");assert(out&&*out=='a'&&atomic_load(&loads)==before+1);free(out);
    before=atomic_load(&loads); fail_alloc=true;
    out=result("a.epub");assert(out&&*out=='a'&&atomic_load(&loads)==before+1);free(out);fail_alloc=false;
    out=result("g.txt");assert(out);free(out);
    before=atomic_load(&loads);assert(book_cover_poll("g.txt",&out)&&out&&atomic_load(&loads)==before);free(out);
    assert(!unlink("g.txt"));out=result("g.txt");assert(!out);
    test_cover_free=0;out=result("b.epub");assert(out);free(out);test_cover_free=8u*1024u*1024u;
    book_cover_join();
    // 一次失败必须重取原图，连续失败不能缓存生成图；下一次轮询仍可恢复。
    // One transient failure must recover original art; repeated failures never cache generated art and can recover on a later poll.
    before=atomic_load(&loads); fail_reads=1;
    out=result("a.epub");assert(out&&*out=='a'&&atomic_load(&loads)==before+2);free(out);
    before=atomic_load(&loads);out=result("a.epub");assert(out&&*out=='a'&&atomic_load(&loads)==before);free(out);
    book_cover_join();before=atomic_load(&loads);fail_reads=3;
    out=result("b.epub");assert(!out&&atomic_load(&loads)==before+3);
    out=result("b.epub");assert(out&&*out=='b'&&atomic_load(&loads)==before+4);free(out);book_cover_join();

    // 三页18张全部命中PSRAM复用；第19张按LRU淘汰，刚访问过的第1张仍保留。
    // Three pages/18 covers all hit PSRAM reuse. The nineteenth evicts by LRU,
    // while the just-touched first entry remains present.
    atomic_store(&gate,true);
    for(unsigned i=0;i<18;++i){char path[32];snprintf(path,sizeof(path),"cache%02u.epub",i);out=result(path);assert(out);free(out);}
    before=atomic_load(&loads);out=result("cache00.epub");assert(out&&atomic_load(&loads)==before);free(out);
    out=result("cache18.epub");assert(out);free(out);
    before=atomic_load(&loads);out=result("cache00.epub");assert(out&&atomic_load(&loads)==before);free(out);
    out=result("cache01.epub");assert(out&&atomic_load(&loads)==before+1);free(out);
    book_cover_join();

    for (unsigned cycle=0; cycle<200; ++cycle) {
        book_cover_join();
        assert(!worker_active && worker_count == delete_count);
        out=result(cycle%2 ? "a.epub" : "b.epub");assert(out);free(out);
        assert(worker_active && worker_count == delete_count + 1 && max_active == 1);
    }
    book_cover_join();
    assert(!worker_active && worker_count == delete_count);
    before=atomic_load(&loads);usleep(30000);assert(atomic_load(&loads)==before);
    for(char c='a';c<='g';++c){char path[32];snprintf(path,sizeof(path),"%c.%s",c,c=='g'?"txt":"epub");unlink(path);}
    for(unsigned i=0;i<19;++i){char path[32];snprintf(path,sizeof(path),"cache%02u.epub",i);unlink(path);}
    assert(!chdir("/"));assert(!rmdir(folder));
    puts("cover worker: nonblocking tabs,15KiB boundary, join stack release, stale discard, OOM and200 fragmented-heap transfer cycles PASS");
}
