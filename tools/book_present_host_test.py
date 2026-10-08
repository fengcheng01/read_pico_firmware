#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 mindreset
# SPDX-License-Identifier: Apache-2.0
"""真实阅读推屏的布局边界与失败重试回归。/ Production reader-present layout boundaries and failed-draw retries."""
import os
from pathlib import Path
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
source = (ROOT / 'main/apps/app_book.c').read_text()
def production_function(name):
    match = re.search(r'^static (?:bool|void) ' + re.escape(name) + r'\([^\n]*\) \{', source, re.M)
    assert match, name
    end = source.index('\n}', match.end()) + 2
    return source[match.start():end]
production = '\n'.join(production_function(name) for name in ('finish_reader_frame', 'reader_direct_enabled', 'present'))
harness = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#define MODE_GL16 5
#define MODE_DU 1
#define APP_PAGE_REFRESH_MODE MODE_GL16
#define ESP_LOGI(tag,...) do { (void)tag; if (0) printf(__VA_ARGS__); } while (0)
#define portMAX_DELAY 1
#define TAG "test"
typedef enum { APP_REDRAW_NONE,APP_REDRAW_DONE,APP_REDRAW_PAGE,APP_REDRAW_FULL,APP_REDRAW_AREA } app_redraw_t;
typedef enum { SHELF,READING,TOC,LAYOUT,SEARCH } book_view_t;
enum EpdDrawError { EPD_DRAW_SUCCESS=0, EPD_DRAW_FAILURE=1 };
typedef struct { int x,y,width,height; } EpdRect;
typedef struct { int* hl; uint8_t* fb; } app_ctx_t;
typedef struct { bool image; } blk_t;
static book_view_t s_view,s_presented_view;
static bool s_presented_valid,s_presented_reading_overlay,s_text_turn,s_image_open,s_toolbar,s_clear_confirm,direct;
static bool s_quote_selecting,s_ended;
static bool s_reader_target_night,s_reader_target_binary;
static bool s_search_dirty,s_search_fast;
static int64_t s_search_edit_ms, processing_us;
static char* s_text;
static blk_t* s_blocks;
static size_t s_block_count;
static EpdRect s_area,s_du_area;
static int s_mode=MODE_GL16,s_du_count;
static int64_t s_du_ms;
static void* s_prep_done;
static char s_footer_status[8];
typedef int EpdWaveform;
static int E0470_WAVEFORM,E0470_NAVIGATION_WAVEFORM,E0470_TEXTTURN_WAVEFORM,E0470_TEXTTURN_NIGHT_WAVEFORM,E0470_DIRECT_WAVEFORM;
static unsigned fulls,turns,directs,navs,areas,renders,joins,entries;
static unsigned quantizes,serial,last_render_serial,last_quantize_serial,last_draw_serial;
static uint8_t* last_quantize_frame;
static int last_quantize_width,last_quantize_height;
static bool last_quantize_night;
static void display_request_navigation_settle(void) { entries++; }
static bool fail,prepare,night_setting,last_direct_night,last_standard_night;
static bool last_page_night;
static int64_t esp_timer_get_time(void) { return processing_us; }
static int epd_width(void) { return 16; }
static int epd_height(void) { return 8; }
static void finish_reader_frame(app_ctx_t* ctx,uint8_t* fb);
static void display_invert_frame(uint8_t* fb,int width,int height) {(void)fb;(void)width;(void)height;}
static void render(app_ctx_t* ctx,uint8_t* fb) {
 assert(ctx->fb==fb);renders++;last_render_serial=++serial;finish_reader_frame(ctx,fb);
}
static void display_prepare_direct_frame(uint8_t* fb,int width,int height,bool night) {
 quantizes++;last_quantize_frame=fb;last_quantize_width=width;last_quantize_height=height;
 last_quantize_night=night;last_quantize_serial=++serial;
}
static bool kick_prep(void) {return prepare;}
static void xSemaphoreTake(void* s,int wait) { (void)s;(void)wait;joins++; }
static void footer_status(char* s,size_t cap) {(void)s;(void)cap;}
static bool app_settings_book_direct(void) {return direct;}
static bool app_settings_book_night(void) {return night_setting;}
static enum EpdDrawError result(void) {last_draw_serial=++serial;return fail?EPD_DRAW_FAILURE:EPD_DRAW_SUCCESS;}
static enum EpdDrawError update_display_full(int* h) {(void)h;fulls++;return result();}
static enum EpdDrawError update_display_clean(int* h) {(void)h;fulls++;return result();}
static enum EpdDrawError update_display_text_turn(int* h,bool night) {(void)h;last_standard_night=night;turns++;return result();}
static enum EpdDrawError update_display_text_direct(int* h,bool night) {(void)h;last_direct_night=night;directs++;return result();}
static enum EpdDrawError update_display_with(int* h,const int* w,int mode) {
 (void)h;assert((w==&E0470_NAVIGATION_WAVEFORM||w==&E0470_TEXTTURN_NIGHT_WAVEFORM)&&mode==MODE_GL16);
 last_page_night=w==&E0470_TEXTTURN_NIGHT_WAVEFORM;navs++;return result();
}
static enum EpdDrawError update_display_mode(int* h,int mode) {(void)h;(void)mode;assert(0);return result();}
static enum EpdDrawError update_display_area_with(int* h,const int* w,int mode,EpdRect r) {(void)h;(void)r;assert(w==(s_view==SEARCH&&s_search_fast?&E0470_DIRECT_WAVEFORM:s_view==READING&&s_text?(s_reader_target_night?&E0470_TEXTTURN_NIGHT_WAVEFORM:&E0470_TEXTTURN_WAVEFORM):&E0470_WAVEFORM)&&mode==MODE_GL16);areas++;return result();}
static EpdRect ui_rect_union(EpdRect a,EpdRect b) {(void)b;return a;}
static void guard_draw_result(int* h,enum EpdDrawError err) {(void)h;(void)err;}
''' + production + r'''
int main(void) {
 uint8_t frame[16*8/2]={0};app_ctx_t ctx={.fb=frame};s_view=SHELF;
 present(&ctx,APP_REDRAW_PAGE);assert(navs==1&&fulls==0&&s_presented_valid);
 present(&ctx,APP_REDRAW_AREA);assert(areas==1&&fulls==0);
 s_view=READING;s_text="body";prepare=true;
 present(&ctx,APP_REDRAW_PAGE);assert(navs==2&&fulls==0&&joins==1&&entries==1);
 prepare=false;
 for(int i=0;i<12;i++){s_text_turn=true;present(&ctx,APP_REDRAW_AREA);}
 assert(turns==12&&fulls==0&&!s_text_turn&&entries==1);
 direct=true;s_text_turn=true;present(&ctx,APP_REDRAW_AREA);assert(directs==1&&fulls==0);
 present(&ctx,APP_REDRAW_PAGE);assert(navs==3);
 s_view=LAYOUT;present(&ctx,APP_REDRAW_PAGE);assert(navs==4);
 s_view=READING;fail=true;s_text_turn=true;present(&ctx,APP_REDRAW_AREA);
 assert(navs==5&&fulls==0&&s_presented_view==LAYOUT&&!s_text_turn&&entries==3);
 fail=false;present(&ctx,APP_REDRAW_AREA);assert(navs==6&&s_presented_view==READING&&entries==4);
 present(&ctx,APP_REDRAW_AREA);assert(areas==2&&fulls==0);
 s_view=SHELF;s_text=NULL;present(&ctx,APP_REDRAW_PAGE);assert(navs==7&&fulls==0&&entries==5);
 present(&ctx,APP_REDRAW_PAGE);assert(navs==8&&fulls==0);
 s_presented_valid=false;s_view=READING;s_text="body";present(&ctx,APP_REDRAW_FULL);
 assert(fulls==1);present(&ctx,APP_REDRAW_PAGE);assert(navs==9&&fulls==1&&entries==5);
 // 日夜正文直刷先量化真实帧；入口、页面重绘与翻页共用同一黑白基准。
 // Day/night direct reading quantizes the real frame before entries, page redraws and turns.
 for(int night=0;night<2;++night){
   night_setting=night;direct=true;
   unsigned before_direct=directs,before_turn=turns,before_full=fulls,before_entries=entries;
   unsigned before_nav=navs,before_area=areas;
   unsigned before_quantize=quantizes,before_render=renders;
   blk_t blocks[3]={{0},{0},{0}};s_blocks=blocks;s_block_count=3;
   s_presented_view=SHELF;s_presented_valid=true;
   present(&ctx,APP_REDRAW_PAGE);
   assert(navs==before_nav+1&&entries==before_entries+1&&fulls==before_full);
   assert(last_page_night==(bool)night&&s_reader_target_night==(bool)night&&s_reader_target_binary);
   assert(quantizes==before_quantize+1&&renders==before_render+1);
   assert(last_quantize_frame==ctx.fb&&last_quantize_width==epd_width()&&last_quantize_height==epd_height());
   assert(last_quantize_night==(bool)night&&last_render_serial<last_quantize_serial&&last_quantize_serial<last_draw_serial);
   // 同视图整页重绘按真实目标主题选波形；量化仍在绘制后、推屏前。
   // Same-view page redraws select the actual target theme; quantization remains between painting and display.
   present(&ctx,APP_REDRAW_PAGE);
   assert(navs==before_nav+2&&entries==before_entries+1&&fulls==before_full);
   assert(last_page_night==(bool)night);
   assert(quantizes==before_quantize+2&&renders==before_render+2);
   assert(last_quantize_frame==ctx.fb&&last_quantize_night==(bool)night);
   assert(last_render_serial<last_quantize_serial&&last_quantize_serial<last_draw_serial);
   s_text_turn=true;present(&ctx,APP_REDRAW_AREA);
   assert(directs==before_direct+1&&turns==before_turn&&last_direct_night==(bool)night);
   assert(fulls==before_full&&entries==before_entries+1&&navs==before_nav+2&&areas==before_area);
   assert(quantizes==before_quantize+3&&renders==before_render+2&&last_quantize_serial<last_draw_serial);
   assert(last_quantize_frame==ctx.fb&&last_quantize_night==(bool)night);
   assert(!s_text_turn&&s_presented_valid&&s_presented_view==READING);
   assert(s_reader_target_binary);
   // 正文叠层保留原灰阶，关闭后再使用纯正文黑白帧。
   // Reading overlays preserve grayscale before returning to the binary body frame.
   bool* overlay_flags[]={&s_clear_confirm,&s_toolbar,&s_image_open,&s_quote_selecting,&s_ended};
   for(size_t i=0;i<sizeof(overlay_flags)/sizeof(overlay_flags[0]);++i){
     *overlay_flags[i]=true;
     present(&ctx,APP_REDRAW_PAGE);
     assert(quantizes==before_quantize+3&&navs==before_nav+3+i);
     assert(fulls==before_full&&entries==before_entries+1);
     assert(last_page_night==(bool)night&&!s_reader_target_binary);
     *overlay_flags[i]=false;
   }
   // 最后一块含图片时入口和正文翻页均保留灰阶，正文翻页不触发入口或GC16。
   // An image in the last block preserves grayscale for entries and turns without entry cleanup or GC16 on turns.
   blocks[2].image=true;
   s_presented_view=SHELF;
   present(&ctx,APP_REDRAW_PAGE);
   assert(navs==before_nav+8&&entries==before_entries+2&&fulls==before_full);
   assert(quantizes==before_quantize+3&&renders==before_render+8);
   s_text_turn=true;present(&ctx,APP_REDRAW_AREA);
   assert(directs==before_direct+1&&turns==before_turn+1&&last_standard_night==(bool)night);
   assert(fulls==before_full&&entries==before_entries+2&&navs==before_nav+8&&areas==before_area);
   assert(quantizes==before_quantize+3);
   assert(!s_text_turn&&s_presented_valid&&s_presented_view==READING);
   // 标准模式纯文字及非正文视图均不量化。
   // Standard text and non-reading views remain unquantized.
   blocks[2].image=false;direct=false;
   present(&ctx,APP_REDRAW_PAGE);
   s_text_turn=true;present(&ctx,APP_REDRAW_AREA);
   assert(turns==before_turn+2&&directs==before_direct+1&&quantizes==before_quantize+3&&last_standard_night==(bool)night);
   direct=true;s_view=LAYOUT;present(&ctx,APP_REDRAW_PAGE);
   assert(quantizes==before_quantize+3&&fulls==before_full&&entries==before_entries+3);
   s_view=SHELF;s_text=NULL;present(&ctx,APP_REDRAW_PAGE);
   assert(quantizes==before_quantize+3&&fulls==before_full&&entries==before_entries+4);
   s_view=READING;s_text="body";
   s_blocks=NULL;s_block_count=0;
 }
 // 仅成功呈现的覆盖层退出正文时请求一次边界清理，失败退出仍重试。
 // Only exits from successfully presented overlays request one body-entry cleanup, retrying failed exits.
 for(int night=0;night<2;++night)for(int effect=0;effect<2;++effect){
   night_setting=night;direct=effect;s_view=READING;s_text="body";
   s_presented_valid=true;s_presented_view=READING;s_presented_reading_overlay=false;
   bool* overlays[]={&s_toolbar,&s_image_open,&s_clear_confirm,&s_quote_selecting,&s_ended};
   for(size_t i=0;i<sizeof(overlays)/sizeof(overlays[0]);++i){
     unsigned before_entries=entries,before_full=fulls,before_nav=navs;
     *overlays[i]=true;present(&ctx,APP_REDRAW_PAGE);
     assert(entries==before_entries&&navs==before_nav+1&&s_presented_reading_overlay);
     present(&ctx,APP_REDRAW_PAGE);
     assert(entries==before_entries&&s_presented_reading_overlay);
     *overlays[i]=false;fail=true;present(&ctx,APP_REDRAW_PAGE);
     assert(entries==before_entries+1&&s_presented_reading_overlay&&fulls==before_full);
     fail=false;present(&ctx,APP_REDRAW_PAGE);
     assert(entries==before_entries+2&&!s_presented_reading_overlay&&fulls==before_full);
     present(&ctx,APP_REDRAW_PAGE);
     assert(entries==before_entries+2&&fulls==before_full);
     s_text_turn=true;present(&ctx,APP_REDRAW_AREA);
     assert(entries==before_entries+2&&fulls==before_full&&!s_text_turn);
     assert(effect?last_direct_night==(bool)night:last_standard_night==(bool)night);
     *overlays[i]=true;fail=true;present(&ctx,APP_REDRAW_PAGE);
     assert(!s_presented_reading_overlay&&entries==before_entries+2);
     *overlays[i]=false;fail=false;present(&ctx,APP_REDRAW_PAGE);
     assert(!s_presented_reading_overlay&&entries==before_entries+2);
   }
 }
 // 非正文内部视图切换也标记边界；同视图普通重绘不追加。
 // Internal non-body view transitions mark boundaries without adding requests to same-view redraws.
 s_view=LAYOUT;unsigned before_entries=entries;present(&ctx,APP_REDRAW_PAGE);
 assert(entries==before_entries+1);present(&ctx,APP_REDRAW_PAGE);assert(entries==before_entries+1);
 s_view=TOC;present(&ctx,APP_REDRAW_PAGE);assert(entries==before_entries+2);
 // 旧队列采样不用于停输计时；只有成功闲置定稿消费待定稿状态，失败保留并延后重试。
 // Ignore old queued sample time for idle timing; only a successful idle settle consumes pending state, with failed settles retained and deferred.
 s_view=s_presented_view=SEARCH;s_reader_target_night=false;
 s_search_dirty=s_search_fast=true;processing_us=8000000;
 unsigned before_areas=areas;present(&ctx,APP_REDRAW_AREA);
 assert(areas==before_areas+1&&s_search_dirty&&!s_search_fast&&s_search_edit_ms==8000);
 fail=true;processing_us=11000000;present(&ctx,APP_REDRAW_AREA);
 assert(s_search_dirty&&s_search_edit_ms==11000);
 fail=false;processing_us=14000000;present(&ctx,APP_REDRAW_AREA);
 assert(!s_search_dirty&&!s_search_fast&&s_search_edit_ms==14000);
 puts("reader present: view/overlay-entry requests and failed retry, manual cleanup, preparation join, day/night binary direct frames before entry/redraw/turn, grayscale image fallback and ordinary-turn preservation PASS");
}
'''
with tempfile.TemporaryDirectory(prefix='pico-reader-present-', dir='/tmp') as folder:
    work=Path(folder);(work/'test.c').write_text(harness)
    flags=['-std=gnu11','-Wall','-Wextra','-Werror','-fsanitize=address,undefined']
    if os.uname().sysname=='Darwin':
        flags += ['-isysroot',os.environ.get('PREVIEW_MACOS_SDK','/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk')]
    subprocess.run([os.environ.get('CC','cc'),*flags,str(work/'test.c'),'-o',str(work/'test')],check=True)
    subprocess.run([str(work/'test')],check=True)
