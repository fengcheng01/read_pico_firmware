/*
 * SPDX-License-Identifier: Apache-2.0
 * 中文：真实封面任务的跨页、单任务、失效和睡眠汇合回归。
 * English: Real cover-worker regression for tab switches, single-job ownership, invalidation and sleep joins.
 */
#include "book_cover.h"
#include "book_store.h"
#include "app_sleep_hooks.h"
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
static pthread_t workers[16];
static unsigned worker_count;
unsigned book_store_revision(void) { return revision; }
static void* run(void* fn) { ((void(*)(void*))fn)(NULL); return NULL; }
int xTaskCreatePinnedToCore(void(*fn)(void*),const char* name,unsigned stack,void* arg,int priority,void* handle,int core) {
    (void)name;(void)stack;(void)arg;(void)priority;(void)handle;(void)core;
    if (fail_spawn) return 0;
    assert(worker_count<16);
    assert(!pthread_create(&workers[worker_count++],NULL,run,(void*)fn));
    return 1;
}
void vTaskDelay(uint32_t ticks) {usleep(ticks*1000);}
void vTaskDelete(void* task) {(void)task;}
bool book_cover_load(const char* path,uint8_t** out) {
    atomic_fetch_add(&loads,1);
    while(!atomic_load(&gate)) usleep(1000);
    *out=malloc(1);assert(*out);**out=(uint8_t)path[0];return true;
}
static void wait_loads(int expected) {for(int n=0;atomic_load(&loads)<expected;++n){assert(n<5000);usleep(1000);}}
static uint8_t* result(const char* path) {
    uint8_t* out=NULL;
    for(int n=0;!book_cover_poll(path,&out);++n){assert(n<5000);usleep(1000);}
    return out;
}
int main(void) {
    uint8_t* out=NULL;
    assert(!book_cover_poll("a.epub",&out));wait_loads(1);
    for(int i=0;i<100;i++) assert(!book_cover_poll("b.epub",&out));
    assert(worker_count==1);atomic_store(&gate,true);
    out=result("b.epub");assert(out&&*out=='b'&&worker_count==2);free(out);
    atomic_store(&gate,false);assert(!book_cover_poll("c.epub",&out));wait_loads(3);
    ++revision;atomic_store(&gate,true);out=result("c.epub");assert(out&&*out=='c'&&worker_count==4);free(out);
    assert(!book_cover_poll("d.epub",&out));app_sleep_prepare_run();
    assert(!book_cover_poll("e.epub",&out));book_cover_join();
    fail_spawn=true;assert(book_cover_poll("f.epub",&out)&&!out);book_cover_join();
    for(unsigned i=0;i<worker_count;i++) pthread_join(workers[i],NULL);
    puts("cover worker: nonblocking tabs, single job, stale result discard, repeat jobs, sleep join and OOM PASS");
}
