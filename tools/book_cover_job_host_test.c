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
static pthread_t workers[64];
size_t test_cover_free = 8u*1024u*1024u;
static bool fail_alloc;
static int fail_reads;
void* test_cover_alloc(size_t n) {return fail_alloc ? NULL : malloc(n);}
static unsigned worker_count;
unsigned book_store_revision(void) { return revision; }
static void* run(void* fn) { ((void(*)(void*))fn)(NULL); return NULL; }
int xTaskCreatePinnedToCore(void(*fn)(void*),const char* name,unsigned stack,void* arg,int priority,void* handle,int core) {
    (void)name;(void)stack;(void)arg;(void)priority;(void)handle;(void)core;
    if (fail_spawn) return 0;
    assert(worker_count<64);
    assert(!pthread_create(&workers[worker_count++],NULL,run,(void*)fn));
    return 1;
}
void vTaskDelay(uint32_t ticks) {usleep(ticks*1000);}
void vTaskDelete(void* task) {(void)task;}
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

    uint8_t* out=NULL;
    assert(!book_cover_poll("a.epub",&out));wait_loads(1);
    for(int i=0;i<100;i++) assert(!book_cover_poll("b.epub",&out));
    assert(worker_count==1);atomic_store(&gate,true);
    out=result("b.epub");assert(out&&*out=='b'&&worker_count==2);free(out);
    // 已放弃的a结果也可跨页复用，不再打开解码器；调用方持有独立副本。
    // Abandoned a-results are reusable across pages without decoder work; callers own independent copies.
    int before=atomic_load(&loads);
    assert(book_cover_poll("a.epub",&out)&&out&&*out=='a');*out=0;free(out);
    assert(book_cover_poll("a.epub",&out)&&out&&*out=='a'&&atomic_load(&loads)==before);free(out);
    atomic_store(&gate,false);assert(!book_cover_poll("c.epub",&out));wait_loads(3);
    ++revision;atomic_store(&gate,true);out=result("c.epub");assert(out&&*out=='c'&&worker_count==4);free(out);
    assert(!book_cover_poll("d.epub",&out));app_sleep_prepare_run();
    assert(!book_cover_poll("e.epub",&out));book_cover_join();
    fail_spawn=true;assert(book_cover_poll("f.epub",&out)&&!out);book_cover_join();fail_spawn=false;
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

    for(unsigned i=0;i<worker_count;i++) pthread_join(workers[i],NULL);
    for(char c='a';c<='g';++c){char path[32];snprintf(path,sizeof(path),"%c.%s",c,c=='g'?"txt":"epub");unlink(path);}
    assert(!chdir("/"));assert(!rmdir(folder));
    puts("cover worker: nonblocking tabs, single job, stale result discard, repeat jobs, sleep join and OOM PASS");
}
