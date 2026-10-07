#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# 真实密码循环回归：首帧、局部输入和错误提示清理。/ Real PIN loop regression: first frame, partial input and error-message cleanup.
import os
from pathlib import Path
import subprocess
root=Path(__file__).resolve().parents[1]
s=(root/'main/sleep.c').read_text()
a=s.index('bool app_lock_pin_challenge('); b=s.index('\nvoid enter_lock_and_sleep(',a)
production=s[a:b]
harness=r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define UI_LOCK_WIDTH 684
#define UI_SETTLE_DU_MAX 6
#define ESP_OK 0
#define MODE_GL16 5
#define MODE_DU 2
#define pdMS_TO_TICKS(x) (x)
typedef struct {int x,y,width,height;} EpdRect;
typedef int EpdiyHighlevelState;
typedef int esp_err_t;
typedef int cst836u_handle_t;
typedef struct {bool touched;int count,x,y;} cst836u_touch_t;
static int E0470_FOLLOW_WAVEFORM;
static uint8_t fb[1];
static const int keys[]={1,1,1,1,1,2,3,4};
static unsigned reads,fulls,areas,pages,idles,paints,highlights,restores;
static int failure,prefix;
static bool wrong;
static int64_t now;
static bool app_settings_lock_pin(char* out,size_t cap){snprintf(out,cap,"1234");return true;}
static uint8_t* epd_hl_get_framebuffer(EpdiyHighlevelState* hl){(void)hl;return fb;}
static void ui_product_lock_keypad(uint8_t* p,const char* title,const char* msg,unsigned n,bool back){(void)p;(void)title;(void)n;(void)back;paints++;wrong=msg[0]!=0;}
static EpdRect ui_product_lock_key_rect(int key){(void)key;return (EpdRect){10,400,200,100};}
static void ui_product_lock_key(uint8_t* p,int key,bool pressed){(void)p;(void)key;if(pressed)highlights++;else restores++;}
static void guard_draw_result(EpdiyHighlevelState* hl,int e){(void)hl;assert(!e);}
static int update_display_full(EpdiyHighlevelState* hl){(void)hl;assert(paints==1);fulls++;return 0;}
static int update_display_mode(EpdiyHighlevelState* hl,int mode){(void)hl;assert(mode==MODE_GL16);pages++;return 0;}
static int update_display_area_with(EpdiyHighlevelState* hl,const int* waveform,int mode,EpdRect area){(void)hl;assert(waveform==&E0470_FOLLOW_WAVEFORM&&mode==MODE_DU);
bool key_only=area.x==10&&area.y==400&&area.width==200&&area.height==100;
assert(key_only||(!wrong&&area.x<=164&&area.x+area.width>=524&&area.y<=216&&area.y+area.height>=500));areas++;return 0;}
static void vTaskDelay(int ms){now+=ms;}
static int64_t esp_timer_get_time(void){return now*1000;}
static void rails_idle_check(int64_t ms){assert(ms==now);idles++;}
static void read_pico_pmu_drain_events(void){}
static int cst836u_read(int tp,cst836u_touch_t* t){(void)tp;
if(failure && prefix<2){prefix++;*t=(cst836u_touch_t){.touched=true,.count=prefix==2&&failure==2?2:1,.x=9};return prefix==2&&failure==1?1:0;}
assert(reads<16);*t=(cst836u_touch_t){.touched=reads%2==0,.count=1,.x=keys[reads/2]};reads++;return 0;}
static int ui_product_lock_keypad_hit(int x,int y){(void)y;return x;}
'''
tests=r'''
int main(void){EpdiyHighlevelState hl=0;assert(app_lock_pin_challenge(&hl,1));assert(fulls==1&&areas==13&&pages==2&&idles==8&&reads==16&&highlights==8&&restores==0);
for(failure=1;failure<=2;failure++){prefix=0;reads=fulls=areas=pages=idles=paints=highlights=restores=0;wrong=false;now=0;
assert(app_lock_pin_challenge(&hl,1));assert(reads==16&&fulls==1&&highlights==9&&restores==1);}
puts("lock PIN: highlights, dot unions, cleanup, read-error and multitouch cancellation PASS");}
'''
out=root/'build-host/lock-pin-test';out.mkdir(parents=True,exist_ok=True)
c=out/'test.c';c.write_text(harness+production+tests)
subprocess.run([os.environ.get('CC','cc'),'-std=c11','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',str(c),'-o',str(out/'test')],check=True)
subprocess.run([str(out/'test')],check=True)
