#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 mindreset
# SPDX-License-Identifier: Apache-2.0
"""封面缓存与设备时间锚点回归。/ Actual cover-cache and device-time anchor regressions."""
from pathlib import Path
import os
import subprocess
import tempfile
ROOT = Path(__file__).resolve().parents[1]
sdk = '/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk'
flags = ['-std=gnu11', '-Wall', '-Wextra', '-Werror', '-fsanitize=address,undefined']
if os.uname().sysname == 'Darwin': flags += ['-isysroot', sdk]
book = (ROOT / 'main/apps/app_book.c').read_text()
cache = book[book.index('typedef struct {', book.index('/* ---- 书架封面缓存')):book.index('static bool is_epub_entry')]
cover = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#define BOOK_STORE_PATH_MAX 288
#define BOOK_ROWS 3
static struct {char path[288];} s_shelf[30];
static int s_visible_count=30;
''' + cache + r'''
int main(void){
    for(int i=0;i<30;++i)snprintf(s_shelf[i].path,288,"/books/%d.epub",i);
    for(int leaf=0;leaf<10;++leaf){
        for(int row=0;row<3;++row){int i=leaf*3+row;uint8_t* gray=row==0?malloc(4):NULL;cover_store(s_shelf[i].path,gray,leaf);}
        for(int tick=0;tick<100;++tick)for(int row=0;row<3;++row){
            int i=leaf*3+row;assert(cover_tried(s_shelf[i].path));assert((cover_for(s_shelf[i].path)!=NULL)==(row==0));
        }
    }
    covers_reset();for(int i=0;i<30;++i)assert(!cover_tried(s_shelf[i].path));
    puts("cover cache: visible successes/failures stay pinned across 10 pages and 100 idle ticks PASS");
}
'''
time_source = (ROOT / 'main/os/os_time_pico.c').read_text()
time_core = time_source[time_source.index('static const char* TAG'):time_source.index('int os_time_battery_permille')]
time = r'''
#include "os_time.h"
#include "os_clock_rate.h"
#include <assert.h>
#include <stdio.h>
#include <time.h>
#include <sys/time.h>
#define ESP_LOGI(...) ((void)TAG)
#define ESP_OK 0
static int16_t app_settings_tz_qh(void){return 32;}
static int32_t app_settings_sleep_clock_ppm(void){return 0;}
static struct {bool time_ok;uint32_t unix_sec;} snapshot;
static bool read_pico_pmu_ready(void){return true;}
static int read_pico_pmu_refresh(void){return ESP_OK;}
typedef __typeof__(snapshot) pmu_snapshot_t;
static const pmu_snapshot_t* read_pico_pmu_get(void){return &snapshot;}
''' + time_core + r'''
int main(void){
    const uint32_t base=1791079200;
    snapshot.time_ok=true;snapshot.unix_sec=base;os_time_poll(0);assert(os_time_info()->unix_utc==base);
    // PMU 快 120 秒，浅睡分钟更新与唤醒查询不能追它。/ A PMU 120 seconds fast must not step minute/wake polls.
    for(int minute=1;minute<=360;++minute){snapshot.unix_sec=base+minute*60+120;os_time_force_poll();os_time_poll(minute*60000LL);assert(os_time_info()->unix_utc==base+minute*60);}
    os_time_rtc_reanchor();os_time_poll(360*60000LL);assert(os_time_info()->unix_utc==snapshot.unix_sec);
    os_time_invalidate();snapshot.unix_sec=base;os_time_poll(0);assert(os_time_info()->unix_utc==base);
    puts("time anchor: six hours of minute/wake polls ignore a fast PMU, explicit calibration still works PASS");
}
'''
with tempfile.TemporaryDirectory(prefix='pico-feedback-', dir='/tmp') as folder:
    for name, source in [('cover', cover), ('time', time)]:
        path=Path(folder)/f'{name}.c';path.write_text(source);binary=Path(folder)/name
        extra=['main/os/os_time.c'] if name=='time' else []
        subprocess.run([os.environ.get('CC','cc'),*flags,'-Imain/os',str(path),*extra,'-o',str(binary)],cwd=ROOT,check=True)
        subprocess.run([str(binary)],check=True)
