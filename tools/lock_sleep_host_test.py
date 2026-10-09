#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""真实锁屏循环回归，替换睡眠/电源硬件。/ Real lock-loop regression with sleep/power hardware mocked."""
from pathlib import Path
import argparse
import os
import re
import subprocess

root = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser()
parser.add_argument('--verify-regression', action='store_true')
args = parser.parse_args()
source = (root / 'main/sleep.c').read_text()
def function(name):
    match = re.search(r'^(?:static )?[^\n]+\b' + name + r'\([^;]*?\) \{', source, re.M)
    assert match, name
    at, depth = match.end(), 1
    while depth:
        depth += (source[at] == '{') - (source[at] == '}')
        at += 1
    return source[match.start():at]

harness = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <setjmp.h>
#define ESP_OK 0
#define ESP_SLEEP_WAKEUP_TIMER 1
#define READ_PICO_IOE_INT_GPIO 41
#define READ_PICO_PMU_WAKE_KEY 1
#define READ_PICO_PMU_WAKE_ALARM 2
#define OS_TIME_VALID 2
#define UI_LOCK_WIDTH 684
#define UI_LOCK_HEIGHT 1216
#define APP_LOCK_IGNORE_BOOT_MS 2000
#define TAG "test"
#define ESP_LOGI(...) ((void)0)
#define ESP_LOGW(...) ((void)0)
#define pdMS_TO_TICKS(x) (x)
typedef int sc7a20h_handle_t;
typedef int cst836u_handle_t;
typedef int EpdiyHighlevelState;
typedef int gpio_num_t;
typedef int esp_err_t;
typedef struct { int x,y,width,height; } EpdRect;
typedef struct { int ths_mg,duration; } sc7a20h_motion_cfg_t;
#define SC7A20H_MOTION_DEFAULT() ((sc7a20h_motion_cfg_t){0})
typedef struct {int unused;} sc7a20h_events_t;
typedef enum { APP_WAKE_NONE, APP_WAKE_KEY, APP_WAKE_PICKUP, APP_WAKE_TIMER } app_wake_source_t;
typedef enum { APP_SLEEP_LIGHT, APP_SLEEP_DEEP, APP_SLEEP_OFF } app_sleep_mode_t;
typedef struct { int state; unsigned day,unix_utc; } info_t;
typedef info_t os_time_info_t;
static info_t info;
static int64_t now, key_at, timer_us, s_minute_deadline_ms;
static int style, mode, cause, sleeps, delays, polls, bands, fulls, faces, disarms, alarms, prepare;
static bool stuck, reject_sleep, spurious, maintenance_busy;
static int64_t maintenance_delay = 3600000;
static unsigned maintenance_ticks, maintenance_cancels;
static unsigned painted[8];
static uint8_t fb;
static jmp_buf powered_off;
static int64_t esp_timer_get_time(void) { return now*1000; }
static void os_time_record_sleep(int64_t us) { assert(us>0); }
static bool os_time_lock_sync_tick(int64_t ms) {(void)ms;maintenance_ticks++;return maintenance_busy;}
static int64_t os_time_lock_sync_delay_ms(int64_t ms) {(void)ms;return maintenance_delay;}
static void os_time_lock_sync_cancel(void) {maintenance_busy=false;maintenance_cancels++;}
static void os_time_poll(int64_t ms) { info=(info_t){OS_TIME_VALID,4,180+(unsigned)(ms/1000)}; }
static const info_t* os_time_info(void) { return &info; }
static void esp_sleep_enable_timer_wakeup(uint64_t us) { assert(us>0); timer_us=(int64_t)us; }
static void esp_sleep_disable_wakeup_source(int src) { assert(src==ESP_SLEEP_WAKEUP_TIMER);timer_us=0;disarms++; }
static int esp_sleep_get_wakeup_cause(void) { return cause; }
static int esp_light_sleep_start(void) {
    assert(++sleeps<10000);
    if(reject_sleep) return 1;
    if(spurious) {spurious=false;now+=5000;cause=0;return 0;}
    int64_t until=timer_us ? now+timer_us/1000 : key_at;
    if(key_at>=0 && key_at<=until) {now=key_at;cause=0;}
    else {now=until;cause=ESP_SLEEP_WAKEUP_TIMER;}
    return 0;
}
static void vTaskDelay(int ms) { assert(ms>=1);now+=ms;assert(++delays<10000); }
static void lock_arm_ioe_wakeup(void) {}
static void read_pico_clear_ioe_int(void) {}
static uint8_t read_pico_pmu_take_wake_events(void) {polls++;return key_at>=0 && now>=key_at ? READ_PICO_PMU_WAKE_KEY : 0;}
static void read_pico_pmu_drain_events(void) {}
static int gpio_get_level(int gpio) {(void)gpio;return stuck?0:1;}
static void gpio_wakeup_disable(int gpio) {(void)gpio;}
static bool app_settings_pickup_wake(void) {return false;}
static void pickup_ack(int acc) {(void)acc;}
static bool pickup_ia(int acc) {(void)acc;return false;}
static bool pickup_wait_quiet(int acc,int quiet,int timeout) {(void)acc;(void)quiet;(void)timeout;return true;}
static void sc7a20h_arm_pickup_wake(int acc,const sc7a20h_motion_cfg_t* cfg) {(void)acc;(void)cfg;}
static void sc7a20h_config_light_sleep_wakeup(int acc) {(void)acc;}
static int sc7a20h_int1_gpio(int acc) {(void)acc;return 1;}
static int sc7a20h_int1_level(int acc) {(void)acc;return 0;}
static void sc7a20h_read_events(int acc,sc7a20h_events_t* ev) {(void)acc;(void)ev;}
static void sc7a20h_power_down(int acc) {(void)acc;}
static void sc7a20h_int1_begin(int acc) {(void)acc;}
static void app_settings_set_last_wake(uint8_t wake) {assert(wake==APP_WAKE_KEY || wake==APP_WAKE_TIMER);}
static bool app_sleep_prepare_run(void) {prepare++;return true;}
static uint8_t* epd_hl_get_framebuffer(int* hl) {(void)hl;return &fb;}
static void draw_lock_face(uint8_t* b) {(void)b;os_time_poll(now);painted[faces++]=info.unix_utc/60;}
static int update_display_full(int* hl) {(void)hl;fulls++;return 0;}
static void guard_draw_result(int* hl,int err) {(void)hl;assert(err==0);}
static void app_lock_wait_key_idle(int ms) {(void)ms;}
static app_sleep_mode_t app_settings_sleep_mode(void) {return mode;}
static uint8_t app_settings_lock_style(void) {return style;}
static void app_sleep_alarm_clock(bool on) {assert(!on);alarms++;}
static void app_enter_host_sleep(int mode_) {assert(mode_==APP_SLEEP_OFF || mode_==APP_SLEEP_DEEP);longjmp(powered_off,1);}
static void epd_poweroff(void) {}
static int update_display_area_quiet(int* hl,EpdRect r) {(void)hl;assert(r.y==260 && r.height==380 && r.width==684);bands++;return 0;}
static bool app_settings_lock_pin_wake(void) {return false;}
static bool app_lock_pin_challenge(int* hl,int tp) {(void)hl;(void)tp;return true;}
'''
production = '\n'.join(function(n) for n in ('arm_minute_wake', 'arm_lock_wake', 'app_sleep_disarm_minute_wake', 'app_light_sleep_wait', 'enter_lock_and_sleep'))
# NULL 是指针，仅在真实硬件上句柄为指针；测试桩以整数模拟。/ NULL is a pointer on device; these mocks use integer handles.
production = production.replace('acc != NULL', 'acc != 0')
tests = r'''
static void reset(void) {
    now=13000;key_at=-1;timer_us=s_minute_deadline_ms=0;
    style=1;mode=APP_SLEEP_DEEP;cause=sleeps=delays=polls=bands=fulls=faces=disarms=alarms=prepare=0;
    stuck=reject_sleep=spurious=maintenance_busy=false;maintenance_ticks=maintenance_cancels=0;maintenance_delay=3600000;os_time_poll(now);
}
int main(void) {
    reset();arm_minute_wake();assert(timer_us==47000000);assert(app_light_sleep_wait(0)==APP_WAKE_TIMER);assert(now==60000 && !timer_us && !s_minute_deadline_ms);
    reset();arm_minute_wake();key_at=13000;assert(app_light_sleep_wait(0)==APP_WAKE_KEY && sleeps==0);
    reset();arm_minute_wake();key_at=60000;assert(app_light_sleep_wait(0)==APP_WAKE_KEY);
    reset();arm_minute_wake();spurious=true;assert(app_light_sleep_wait(0)==APP_WAKE_TIMER && now==60000 && sleeps==2);
    reset();arm_minute_wake();stuck=true;assert(app_light_sleep_wait(0)==APP_WAKE_TIMER && now==60000 && sleeps==0 && delays>0);
    reset();arm_minute_wake();reject_sleep=true;assert(app_light_sleep_wait(0)==APP_WAKE_TIMER && now==60000 && delays>0);
    reset();key_at=120010;int hl=0;int64_t ignored=0;enter_lock_and_sleep(&hl,&ignored,0,0);
    assert(prepare==1 && fulls==1 && faces==3 && bands==2 && alarms==2);
    assert(painted[0]==3 && painted[1]==4 && painted[2]==5 && ignored==122010 && !timer_us);
    reset();maintenance_busy=true;key_at=61000;enter_lock_and_sleep(&hl,0,0,0);
    assert(now==61000&&maintenance_ticks>1&&maintenance_cancels==1&&!maintenance_busy&&faces==1);
    reset();style=0;mode=APP_SLEEP_LIGHT;key_at=14000;enter_lock_and_sleep(&hl,0,0,0);assert(bands==0 && faces==1 && !timer_us);
    reset();style=0;mode=APP_SLEEP_LIGHT;key_at=11LL*3600000+13001;enter_lock_and_sleep(&hl,0,0,0);
    assert(maintenance_ticks==11 && faces==1 && bands==0 && fulls==1 && !timer_us);
    reset();style=0;mode=APP_SLEEP_LIGHT;maintenance_delay=0;key_at=11LL*3600000+13001;enter_lock_and_sleep(&hl,0,0,0);
    assert(maintenance_ticks==0 && sleeps==1 && faces==1 && bands==0 && fulls==1 && !timer_us);
    reset();style=0;mode=APP_SLEEP_DEEP;if(!setjmp(powered_off)) {enter_lock_and_sleep(&hl,0,0,0);assert(0);}
    assert(sleeps==0 && maintenance_ticks==0 && !timer_us && alarms==1);
    reset();mode=APP_SLEEP_OFF;if(!setjmp(powered_off)) {enter_lock_and_sleep(&hl,0,0,0);assert(0);}assert(sleeps==0 && !timer_us && alarms==1);
    puts("lock sleep: minute alignment, static 11h maintenance without repaint, auto-off/static-deep isolation, key priority, rejected/spurious sleep and power-off PASS");
}
'''
out = root / 'build-host/lock-sleep-test'
out.mkdir(parents=True, exist_ok=True)
(out / 'test.c').write_text(harness + production + tests)
subprocess.run([os.environ.get('CC', 'cc'), '-std=c11', '-Wall', '-Wextra', '-Werror', '-fsanitize=address,undefined', str(out / 'test.c'), '-o', str(out / 'test')], check=True)
subprocess.run([str(out / 'test')], check=True)
if args.verify_regression:
    old = production.replace('                arm_lock_wake();', '                if (app_settings_lock_style() != 0) arm_lock_wake();')
    assert old != production
    (out / 'negative.c').write_text(harness + old + tests)
    subprocess.run([os.environ.get('CC', 'cc'), '-std=c11', '-Wall', '-Wextra', '-Werror', '-fsanitize=address,undefined', str(out / 'negative.c'), '-o', str(out / 'negative')], check=True)
    result = subprocess.run([str(out / 'negative')], capture_output=True, text=True)
    assert result.returncode != 0 and 'maintenance_ticks==11' in result.stderr, result.stderr
    print('negative control: prior static-only timer omission fails the 11h maintenance assertion PASS')
