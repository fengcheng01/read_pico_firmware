#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# 真实时间页状态机；替换网络与绘图边界。/ Real time-page state machine with network and painting boundaries replaced.
import os
from pathlib import Path
import subprocess

root = Path(__file__).resolve().parents[1]
source = (root / 'main/apps/app_os_time.c').read_text()
def function(name):
    import re
    match = re.search(r'^static [^\n]+\b' + name + r'\([^\n]*\) \{', source, re.M)
    assert match, name
    end = source.index('\n}', match.end()) + 2
    return source[match.start():end]

harness = r'''
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "os_time.h"
#include "read_pico_transfer.h"
typedef struct { int x,y,width,height; } EpdRect;
typedef struct {int64_t now_ms; bool consumed; uint8_t* fb;} app_ctx_t;
typedef enum {APP_REDRAW_NONE,APP_REDRAW_PAGE,APP_REDRAW_AREA} app_redraw_t;
static read_pico_transfer_status_t status;
static bool configured, claim_ok=true, claimed, synced, ntp_success;
static int starts, stops, releases, ntp_calls, polls, start_error;
static os_time_info_t info;
static bool app_settings_clock_auto(void) {return true;}
static void app_loop_stay_awake(void) {}
static void app_sleep_prepare_unregister(bool (*fn)(void)) {(void)fn;}
static void paint_clock(uint8_t* fb) {(void)fb;}
static EpdRect clock_rect(void) {return (EpdRect){0};}
void os_time_poll(int64_t now) {(void)now;}
const os_time_info_t* os_time_info(void) {return &info;}
void os_time_network(bool up) {if(!up) synced=false;else {ntp_calls++;synced=ntp_success;}}
bool os_time_recently_synced(void) {return synced;}
esp_err_t read_pico_transfer_get_saved_wifi(char ssid[33],bool* out) {(void)ssid;*out=configured;return ESP_OK;}
bool read_pico_transfer_claim_sync(void) {if(!claim_ok)return false;assert(!claimed);claimed=true;return true;}
void read_pico_transfer_release_sync(void) {assert(claimed);claimed=false;releases++;}
void read_pico_transfer_get_status(read_pico_transfer_status_t* out) {*out=status;}
esp_err_t read_pico_transfer_start_saved_network(void) {starts++;status.mode=READ_PICO_TRANSFER_MODE_STA;status.state=READ_PICO_TRANSFER_STARTING;return start_error;}
void read_pico_transfer_stop(void) {assert(claimed);stops++;status=(read_pico_transfer_status_t){0};}
void read_pico_transfer_service_poll(void) {polls++;}
'''
state = source[source.index('static int s_drawn_minute'):source.index('static void stop_sync')]
production = '\n'.join(function(n) for n in ['stop_sync','prepare_sleep','time_on_exit','start_sync','on_tick'])
tests = r'''
int main(void) {
 app_ctx_t ctx={0};
 start_sync(&ctx); assert(!s_sync_active && !starts && !claimed);
 configured=true;claim_ok=false;start_sync(&ctx);assert(!starts&&!claimed);
 claim_ok=true;status.state=READ_PICO_TRANSFER_READY;start_sync(&ctx);assert(!starts&&!claimed&&!stops);
 status.state=READ_PICO_TRANSFER_STOPPED;start_error=1;start_sync(&ctx);assert(starts==1&&!claimed&&!s_sync_active);
 status.state=READ_PICO_TRANSFER_STOPPED;start_error=0;start_sync(&ctx);assert(s_sync_active&&claimed);
 ctx.now_ms=500;on_tick(&ctx);assert(polls==1&&!ntp_calls);
 status.network_ready=true;on_tick(&ctx);assert(ntp_calls==1&&s_sync_active);
 ntp_success=true;assert(on_tick(&ctx)==APP_REDRAW_PAGE);assert(!s_sync_active&&!claimed&&stops==1);
 assert(strstr(s_sync_note,"已校准"));
 ntp_success=false;ctx.now_ms=1000;start_sync(&ctx);ctx.now_ms=31000;
 on_tick(&ctx);assert(!s_sync_active&&!claimed&&stops==2&&strstr(s_sync_note,"失败"));
 start_sync(&ctx);status.state=READ_PICO_TRANSFER_ERROR;on_tick(&ctx);assert(stops==3&&!claimed);
 start_sync(&ctx);time_on_exit(&ctx);assert(stops==4&&!claimed);
 start_sync(&ctx);prepare_sleep();assert(stops==5&&!claimed);
 time_on_exit(&ctx);assert(stops==5);
 s_sync_requested=true;ctx.consumed=true;on_tick(&ctx);assert(s_sync_requested&&!s_sync_active);
 ctx.consumed=false;on_tick(&ctx);assert(s_sync_active&&!s_sync_requested);stop_sync();
 puts("time page: no WiFi, busy, start failure, success, timeout, disconnect, exit, sleep and deferred start PASS");
}
'''
out = root / 'build-host/time-page-test'; out.mkdir(parents=True,exist_ok=True)
c = out / 'test.c'; c.write_text(harness+state+production+tests)
subprocess.run([os.environ.get('CC','cc'),'-std=c11','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',
               '-Itools/book_storage_stubs','-Imain/os','-Icomponents/read_pico_transfer/include',str(c),'-o',str(out/'test')],cwd=root,check=True)
subprocess.run([str(out/'test')],check=True)
