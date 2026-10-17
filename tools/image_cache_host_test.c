/* SPDX-License-Identifier: Apache-2.0
 * 中文：真实三槽缓存与单任务生命周期，文件和线程使用POSIX，解码器只在边界替换。
 * English: Actual three-slot cache and sole-task lifecycle using POSIX files/threads with only decoder boundaries mocked.
 */
#include "ui_image_cache.h"
#include "app_sleep_hooks.h"
#include <assert.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

size_t test_cover_free = 8u * 1024u * 1024u;
static atomic_bool gate = true;
static atomic_int loads, active, peak;
static bool fail_spawn, fail_decode;
static pthread_t tasks[64];
static unsigned count;
static void* run(void* f) { ((void(*)(void*))f)(NULL); return NULL; }
int xTaskCreatePinnedToCore(void(*fn)(void*),const char* name,unsigned stack,void* arg,int priority,void* handle,int core) {
    assert(name&&stack==16384&&!arg&&priority==1&&!handle&&!core);
    if(fail_spawn)return 0;
    assert(count<64);assert(!pthread_create(&tasks[count++],NULL,run,(void*)fn));return 1;
}
void vTaskDelete(void* arg){(void)arg;}
void vTaskDelay(uint32_t ticks){usleep(ticks*1000);}
bool ui_image_load_path(const char* path,uint8_t** gray,uint16_t* width,uint16_t* height) {
    ++loads;int a=atomic_fetch_add(&active,1)+1;if(a>peak)peak=a;
    while(!gate)usleep(1000);
    if(!fail_decode){FILE* f=fopen(path,"rb");assert(f);int pixel=fgetc(f);fclose(f);*gray=malloc(4);assert(*gray);memset(*gray,pixel,4);*width=*height=2;}
    --active;return !fail_decode;
}
static uint8_t* get(const char* path) {
    uint8_t* gray=NULL;uint16_t width=0,height=0;
    for(int n=0;!ui_image_cache_poll(path,&gray,&width,&height);++n){assert(n<5000);usleep(1000);}
    assert(!gray||(width==2&&height==2));return gray;
}
static void wait_loads(int n){for(int i=0;loads<n;++i){assert(i<5000);usleep(1000);}}
int main(void) {
    char root[]="/tmp/pico-image-cache-XXXXXX";assert(mkdtemp(root));char paths[4][192];
    for(int i=0;i<4;++i){snprintf(paths[i],sizeof(paths[i]),"%s/%c.png",root,'a'+i);FILE* f=fopen(paths[i],"wb");assert(f);fputc('a'+i,f);fclose(f);}
    uint8_t* a=get(paths[0]);assert(a&&*a=='a'&&loads==1);
    gate=false;ui_image_cache_prefetch(paths[1]);wait_loads(2);
    assert(get(paths[0])==a&&*a=='a');ui_image_cache_prefetch(paths[2]);assert(loads==2);
    gate=true;uint8_t* b=get(paths[1]);assert(b&&*b=='b'&&loads==2);
    uint8_t* c=get(paths[2]);assert(c&&*c=='c'&&loads==3);
    assert(get(paths[0])==a&&loads==3);
    FILE* f=fopen(paths[0],"ab");assert(f);fputc('x',f);fclose(f);
    assert(get(paths[0])&&loads==4);
    test_cover_free=0;ui_image_cache_prefetch(paths[3]);assert(loads==4);test_cover_free=8u*1024u*1024u;
    fail_decode=true;assert(!get(paths[3])&&loads==5);assert(!get(paths[3])&&loads==5);
    fail_decode=false;ui_image_cache_retry(paths[3]);assert(get(paths[3])&&loads==6);
    ui_image_cache_clear();fail_spawn=true;assert(!get(paths[0]));fail_spawn=false;
    a=get(paths[0]);assert(a&&loads==7);
    gate=false;ui_image_cache_prefetch(paths[1]);wait_loads(8);gate=true;
    app_sleep_prepare_run();assert(*a=='a'&&get(paths[0])==a&&loads==8);
    gate=false;ui_image_cache_prefetch(paths[1]);wait_loads(9);gate=true;ui_image_cache_clear();
    assert(peak==1);
    for(unsigned i=0;i<count;++i)pthread_join(tasks[i],NULL);
    for(int i=0;i<4;++i)unlink(paths[i]);rmdir(root);
    puts("Actual image cache:3 slots, zero-copy hits, single decoder, adjacent prefetch, identity invalidation, failure retry, low memory, task failure and sleep/exit join PASS");
}
