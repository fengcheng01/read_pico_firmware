#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""真实首页和书架失败槽重试回归。/ Regress actual home and shelf failed-slot retry decisions."""
from pathlib import Path
import os
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
home = (ROOT / 'main/apps/app_os_home.c').read_text()
shelf = (ROOT / 'main/apps/app_book.c').read_text()
def function(source, name):
    at = source.index(name)
    return source[at:source.index('\n}', at) + 2]
unit = r'''
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
static uint8_t* s_cover;
static char s_cover_path[128];
static unsigned s_cover_revision, revision;
static int64_t s_cover_retry_ms, now;
static bool ready, fail;
static int polls;
static struct {struct {char path[128];}current;} snapshot;
static void* book_home_snapshot(void){return &snapshot;}
#define book_home_snapshot() (&snapshot)
static unsigned book_store_revision(void){return revision;}
static bool book_cover_poll(const char* p,uint8_t** gray){(void)p;++polls;*gray=NULL;if(!ready)return false;if(!fail){*gray=malloc(1);assert(*gray);}return true;}
static struct {char path[128];bool failed;int64_t retry_ms;}s_cover_slots[6];
static int64_t esp_timer_get_time(void){return now*1000;}
'''.replace('static void* book_home_snapshot(void){return &snapshot;}\n', '')
unit += function(home, 'static bool load_cover(') + '\n' + function(shelf, 'static bool cover_tried(')
unit += r'''
int main(void){
 strcpy(snapshot.current.path,"book.epub");
 assert(!load_cover(0)&&polls==1&&!s_cover_path[0]);
 ready=true;fail=true;assert(!load_cover(1)&&!s_cover&&polls==2);
 assert(!load_cover(3000)&&polls==2);
 fail=false;assert(load_cover(3001)&&s_cover&&polls==3);
 assert(!load_cover(100000)&&polls==3);
 ++revision;assert(load_cover(100001)&&polls==4);
 strcpy(s_cover_slots[0].path,"book.epub");s_cover_slots[0].failed=true;s_cover_slots[0].retry_ms=3000;
 now=2999;assert(cover_tried("book.epub"));now=3000;assert(!cover_tried("book.epub"));
 s_cover_slots[0].failed=false;assert(cover_tried("book.epub"));free(s_cover);
 puts("Actual home/shelf: pending does not stick,3s failed-slot retry, success reuse and revision invalidation PASS");
}
'''
with tempfile.TemporaryDirectory(prefix='rp-cover-retry-') as folder:
    c = Path(folder) / 'test.c'
    binary = Path(folder) / 'test'
    c.write_text(unit)
    subprocess.run([os.environ.get('CC', 'cc'), '-std=gnu11', '-Wall', '-Wextra', '-Werror',
                    '-fsanitize=address,undefined', str(c), '-o', str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
