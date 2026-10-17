#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""真实锁屏分流与静态摘录唤醒策略。/ Production lock routing and static excerpt wake policy."""
import os
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]
source = (ROOT / "main/sleep.c").read_text()


def function(name):
    result_type = "EpdRect " if name == "lock_time_band" else "bool " if name.startswith("draw_") or name == "lock_idle_tick" else "void "
    start = source.index("static " + result_type + name + "(")
    return source[start:source.index("\n}", start) + 2]


production = "\n".join(function(n) for n in ("draw_lock_excerpt", "draw_lock_face", "arm_lock_wake", "lock_idle_tick", "lock_time_band"))
harness = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define APP_LOCK_STYLE_QUOTE 5
#define OS_TIME_VALID 2
#define APP_SLEEP_OFF 2
#define LOCK_IDLE_SOFT_SLEEP_MIN 30
#define UI_LOCK_WIDTH 684
#define UI_LOCK_HEIGHT 1216
typedef struct {int x,y,width,height;} EpdRect;
static int app_settings_effective_lock_mode(void){return 0;}
typedef struct { char path[288], text[384]; char chapter_title[64]; } book_quote_t;
static book_quote_t saved={"/sdcard/books/真实书名.epub","这是已保存的句子。","第十二章 星群"};
static bool has_quote, wallpaper;
static uint8_t style;
static unsigned reads, paintings, statics, force_polls, polls, minutes, timers;
static unsigned clocks, calendars, almanacs;
static int64_t delay_ms, s_minute_deadline_ms;
static uint64_t timer_us;
typedef struct {int state;unsigned year,month,day;} os_time_info_t;
static os_time_info_t info;
static uint8_t app_settings_lock_style(void){return style;}
static const char* app_settings_lock_wake_hint(void){return "短按电源键开机";}
static bool book_quotes_lock_get(book_quote_t* out){++reads;if(!has_quote)return false;*out=saved;return true;}
static void ui_product_excerpt_lock(uint8_t* fb,const char* text,const char* path,const char* chapter,const char* wake,const os_time_info_t* time,unsigned mins,unsigned days,bool live,bool preview){
    assert(time==&info&&!mins&&!days&&live&&!preview);
    assert(fb&&strcmp(text,saved.text)==0&&strcmp(path,saved.path)==0);
    assert(chapter&&strcmp(chapter,saved.chapter_title)==0);
    assert(strcmp(wake,"短按电源键开机")==0);++paintings;
}
static void draw_static_lock(uint8_t* fb){assert(fb);++statics;}
static bool draw_lock_wallpaper(uint8_t* fb){assert(fb);return wallpaper;}
static void draw_lock_clock(uint8_t* fb){assert(fb);++clocks;}
static void draw_lock_calendar(uint8_t* fb){assert(fb);++calendars;}
static void draw_lock_almanac(uint8_t* fb){assert(fb);++almanacs;}
static uint32_t os_time_date_shift(uint32_t d,int n){(void)n;return d;}
static unsigned book_stats_minutes(uint32_t d){(void)d;return 0;}
static void os_time_force_poll(void){++force_polls;}
static void os_time_poll(int64_t now){assert(now==1000);++polls;}
#define os_time_info() (&info)
static int64_t esp_timer_get_time(void){return 1000000;}
static int64_t os_time_lock_sync_delay_ms(int64_t now){assert(now==1000);return delay_ms;}
static void esp_sleep_enable_timer_wakeup(uint64_t us){++timers;timer_us=us;}
static void arm_minute_wake(void){++minutes;}
'''
tests = r'''
int main(void){
    uint8_t fb[1]={0};style=APP_LOCK_STYLE_QUOTE;
    assert(!draw_lock_face(fb)&&statics==1&&reads==1&&paintings==0);
    has_quote=true;
    for(int state=0;state<3;++state){info.state=state;assert(!draw_lock_face(fb));}
    assert(paintings==3&&reads==4&&statics==1&&force_polls==0&&polls==3);
    unsigned idle=0;for(int i=0;i<120;++i)assert(!lock_idle_tick(&idle));assert(idle==0);
    EpdRect band=lock_time_band(style);assert(band.y==1036&&band.height==180&&band.width==684);
    arm_lock_wake();assert(minutes==1&&timers==0);
    delay_ms=5000;arm_lock_wake();assert(minutes==2&&timers==0);
    style=0;arm_lock_wake();assert(timers==1&&timer_us==5000000&&s_minute_deadline_ms==6000);
    style=0;for(int i=0;i<29;++i)assert(!lock_idle_tick(&idle));assert(lock_idle_tick(&idle));wallpaper=true;assert(draw_lock_face(fb));
    wallpaper=false;assert(!draw_lock_face(fb)&&statics==2);
    for(style=1;style<=3;++style){
        info.state=OS_TIME_VALID;assert(!draw_lock_face(fb));arm_lock_wake();
        info.state=0;assert(!draw_lock_face(fb));
    }
    assert(clocks==1&&calendars==1&&almanacs==1&&minutes==5&&statics==5);
    puts("Personal lock: actual selected-snapshot loader, empty fallback, unsynced rendering, minute excerpt wakes, static-image sync deadline and existing face routes PASS");
}
'''
out = ROOT / "build-host/quote-lock-test"
out.mkdir(parents=True, exist_ok=True)
c = out / "test.c"
c.write_text(harness + production + tests)
subprocess.run([os.environ.get("CC", "cc"), "-std=c11", "-Wall", "-Wextra", "-Werror",
                "-fsanitize=address,undefined", str(c), "-o", str(out / "test")], check=True)
subprocess.run([str(out / "test")], check=True)
