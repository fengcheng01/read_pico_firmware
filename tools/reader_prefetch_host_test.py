#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""中文：真实预绘任务与布局的逐行中断、续绘和失效回归。
English: Production prefetch worker/layout regression for row-boundary stop, resume and invalidation.
"""
from pathlib import Path
import os
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
source = (ROOT / 'main/apps/app_book.c').read_text()
def function(name):
    start = re.search(r'^static [^\n]*\b' + name + r'\([^\n]*\) \{', source, re.M).start()
    return source[start:source.index('\n}', start) + 2]
unit = r'''
#include <assert.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "book_layout.h"
size_t host_layout_max_request,host_layout_fail_above;
#define MALLOC_CAP_SPIRAM 1
#define MALLOC_CAP_8BIT 2
#define pdPASS 1
#define pdTRUE 1
#define portMAX_DELAY 1
static bool low_memory;
static size_t heap_caps_get_free_size(int caps){(void)caps;return low_memory?0:8u*1024u*1024u;}
static void* heap_caps_aligned_alloc(size_t alignment,size_t n,int caps){(void)alignment;(void)caps;return malloc(n);}
typedef struct {pthread_mutex_t lock;pthread_cond_t changed;int count;} sem_t;
static void* sem_new(int count){sem_t* s=calloc(1,sizeof(*s));assert(s);pthread_mutex_init(&s->lock,0);pthread_cond_init(&s->changed,0);s->count=count;return s;}
static void* xSemaphoreCreateMutex(void){return sem_new(1);}
static void* xSemaphoreCreateBinary(void){return sem_new(0);}
static void xSemaphoreTake(void* ptr,int wait){sem_t* s=ptr;pthread_mutex_lock(&s->lock);while(!s->count&&wait)pthread_cond_wait(&s->changed,&s->lock);if(s->count)s->count=0;pthread_mutex_unlock(&s->lock);}
static void xSemaphoreGive(void* ptr){sem_t* s=ptr;pthread_mutex_lock(&s->lock);s->count=1;pthread_cond_signal(&s->changed);pthread_mutex_unlock(&s->lock);}
static void sem_free(void* ptr){sem_t* s=ptr;pthread_mutex_destroy(&s->lock);pthread_cond_destroy(&s->changed);free(s);}
static pthread_mutex_t notify_lock=PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t notify_changed=PTHREAD_COND_INITIALIZER;
static bool notified,quit;
static unsigned ulTaskNotifyTake(int clear,int wait){(void)clear;(void)wait;pthread_mutex_lock(&notify_lock);while(!notified&&!quit)pthread_cond_wait(&notify_changed,&notify_lock);bool done=quit;notified=false;pthread_mutex_unlock(&notify_lock);if(done)pthread_exit(NULL);return 1;}
static void xTaskNotifyGive(void* handle){(void)handle;pthread_mutex_lock(&notify_lock);notified=true;pthread_cond_signal(&notify_changed);pthread_mutex_unlock(&notify_lock);}
static pthread_t thread;
static void* run(void* fn){((void(*)(void*))fn)(NULL);return NULL;}
static int xTaskCreatePinnedToCore(void(*fn)(void*),const char* name,int stack,void* arg,int priority,void** handle,int core){(void)name;(void)stack;(void)arg;(void)priority;(void)core;assert(!pthread_create(&thread,NULL,run,(void*)fn));*handle=&thread;return 1;}
static void vTaskDelete(void* handle){assert(handle);pthread_mutex_lock(&notify_lock);quit=true;pthread_cond_signal(&notify_changed);pthread_mutex_unlock(&notify_lock);pthread_join(thread,NULL);quit=false;notified=false;}
static void *s_draw_lock,*s_prep_done,*s_prep_task;
static atomic_bool s_prep_stop;
static bool s_prep_sync_suspended;
static book_layout_cursor_t s_prep_cursor;
static uint8_t* s_next_fb;
static int s_next_page=-1,s_prep_page=-1;
static size_t s_page;
static const char* s_text;
static int s_px=10,s_view;
enum {READING};
static bool s_image_open,s_toolbar,s_clear_confirm,s_quote_selecting,s_ended;
static bool s_shake_enabled,s_scrub,test_sync_busy;
static unsigned s_du_count;
static int64_t s_size_settle_ms;
static bool os_sync_job_busy(void){return test_sync_busy;}
typedef struct {bool touched;} touch_t;
typedef struct {bool consumed,request_app,request_menu;touch_t* touch;} app_ctx_t;
static EpdRect body_rect(void){return (EpdRect){0,0,100,600};}
static size_t fb_bytes(void){return 100u*600u;}
static void ui_clear_page(uint8_t* fb){memset(fb,255,fb_bytes());}
static void apply_typography(void){}
static int app_settings_book_stroke(void){return 0;}
static bool reader_gray_direct_enabled(void){return false;}
static void ttf_set_edge_style(uint8_t style){assert(style==0);}
static void ttf_set_precise_curves(bool precise){(void)precise;}
void ttf_set_stroke(int step){(void)step;}
int ttf_get_stroke(void){return 0;}
static int64_t esp_timer_get_time(void){static int64_t time;return ++time;}
static void lock_draw(void){if(s_draw_lock)xSemaphoreTake(s_draw_lock,1);}
static void unlock_draw(void){if(s_draw_lock)xSemaphoreGive(s_draw_lock);}
static atomic_bool block_row,row_entered,release_row;
static atomic_int rows_drawn;
int ttf_text_width_px(int px,const char* text){return (int)strlen(text)*px;}
int ttf_ascender_px(int px){return px;}
void ttf_draw_text_px(uint8_t* fb,int x,int y,int px,const char* text,enum EpdFontFlags align,uint8_t fg,uint8_t bg){
 (void)align;(void)fg;(void)bg;atomic_fetch_add(&rows_drawn,1);
 if(atomic_load(&block_row)){atomic_store(&row_entered,true);while(!atomic_load(&release_row))usleep(1000);}
 for(size_t i=0;i<strlen(text);++i){assert(x+(int)i*px<100&&y<600);fb[y*100+x+(int)i*px]=(uint8_t)text[i];}
}
void epd_fill_rect(EpdRect rect,uint8_t color,uint8_t* fb){for(int y=rect.y;y<rect.y+rect.height;++y)memset(fb+y*100+rect.x,color,rect.width);}
void epd_draw_pixel(int x,int y,uint8_t c,uint8_t* fb){fb[y*100+x]=c;}
'''
unit += '\n'.join(function(n) for n in ['reader_body_font','invalidate_prep','prep_step','prep_task','ensure_prep','prep_suspend_for_sync','prep_resume_after_sync','prep_allowed','prep_target','kick_prep','prep_end','prep_resume','prep_idle','idle_wait_ms'])
unit += r'''
int main(void){
 char text[1200];memset(text,'a',sizeof(text));s_text=text;EpdRect rect=body_rect();
 book_layout_set_indent(false);assert(book_layout_build(text,sizeof(text),rect,s_px));
 low_memory=true;assert(!kick_prep()&&!s_next_fb);low_memory=false;
 atomic_store(&block_row,true);assert(kick_prep());
 for(int n=0;!atomic_load(&row_entered);++n){assert(n<5000);usleep(1000);}
 atomic_store(&s_prep_stop,true);atomic_store(&release_row,true);prep_end(true);
 assert(atomic_load(&rows_drawn)==1&&s_next_page==-1&&s_prep_cursor.started);
 atomic_store(&block_row,false);assert(prep_resume(1));
 uint8_t* full=malloc(fb_bytes());ui_clear_page(full);book_layout_draw_page(full,1,rect,s_px);
 assert(!memcmp(full,s_next_fb,fb_bytes()));
 assert(book_layout_build(text,sizeof(text),rect,s_px));assert(!prep_resume(1));
 invalidate_prep();s_toolbar=true;assert(!kick_prep());s_toolbar=false;
 s_quote_selecting=true;assert(!kick_prep());s_quote_selecting=false;s_image_open=true;assert(!kick_prep());s_image_open=false;s_ended=true;assert(!kick_prep());s_ended=false;
 app_ctx_t ctx={0};int before=atomic_load(&rows_drawn);prep_idle(&ctx);assert(atomic_load(&rows_drawn)==before+4);
 ctx.consumed=true;before=atomic_load(&rows_drawn);prep_idle(&ctx);assert(atomic_load(&rows_drawn)==before);ctx.consumed=false;
 ctx.request_menu=true;prep_idle(&ctx);assert(atomic_load(&rows_drawn)==before);ctx.request_menu=false;
 assert(prep_resume(1));assert(!memcmp(full,s_next_fb,fb_bytes()));
 assert(idle_wait_ms(&ctx)==50);
 s_next_page=-1;assert(!idle_wait_ms(&ctx));assert(prep_resume(1));
 s_toolbar=true;assert(!idle_wait_ms(&ctx));s_toolbar=false;
 s_du_count=1;assert(!idle_wait_ms(&ctx));s_du_count=0;
 s_size_settle_ms=1;assert(!idle_wait_ms(&ctx));s_size_settle_ms=0;
 s_shake_enabled=true;assert(!idle_wait_ms(&ctx));s_shake_enabled=false;
 test_sync_busy=true;assert(!idle_wait_ms(&ctx));test_sync_busy=false;
 assert(s_prep_task);prep_suspend_for_sync();assert(s_prep_sync_suspended&&!s_prep_task);
 prep_resume_after_sync();assert(!s_prep_sync_suspended&&!s_prep_task);
 ensure_prep();assert(s_prep_task);vTaskDelete(s_prep_task);s_prep_task=NULL;
 free(full);free(s_next_fb);sem_free(s_prep_done);sem_free(s_draw_lock);book_layout_free();
 puts("prefetch: real worker stops after one row, partial/full pixels equal, generation invalidation, overlay gates, four-row idle budget and low-memory fallback PASS");
}
'''
with tempfile.TemporaryDirectory(prefix='rp-prefetch-', dir='/tmp') as folder:
    c = Path(folder)/'test.c'; binary = Path(folder)/'test'; c.write_text(unit)
    subprocess.run([os.environ.get('CC','cc'),'-std=c11','-D_DEFAULT_SOURCE','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-g','-pthread','-Itools/book_layout_stubs','-Itools/book_source_host_stubs','-Imain/book','-Imain/ui',str(c),'main/book/book_layout.c','main/book/book_hyphen.c','main/ui/ui_dither.c','-o',str(binary)],cwd=ROOT,check=True)
    subprocess.run([str(binary)],cwd=ROOT,check=True)
