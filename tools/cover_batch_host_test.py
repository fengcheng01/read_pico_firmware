#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""中文：实际书架封面收取回调的合并刷新、期限和切页回归。
English: Actual shelf cover collection regression for grouped updates, deadlines and navigation.
"""
from pathlib import Path
import os
import subprocess
import tempfile
ROOT=Path(__file__).resolve().parents[1]
s=(ROOT/'main/apps/app_book.c').read_text();a=s.index('static app_redraw_t shelf_covers_tick(');production=s[a:s.index('\n}',a)+2]
unit=r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
typedef struct {int x,y,width,height;} EpdRect;
typedef struct {int leaf;int64_t now_ms;uint8_t* fb;} app_ctx_t;
typedef enum {APP_REDRAW_NONE,APP_REDRAW_AREA} app_redraw_t;
static bool s_covers_dirty,s_cover_batch_grid,s_shelf_grid,ready[12],stored[12];
static int s_cover_batch_leaf=-1,s_visible_count=12,s_mode,renders,polls;
static int64_t s_cover_batch_due;
static EpdRect s_area;
static struct {char path[16];bool removed;} s_shelf[12];
#define MODE_GL16 7
static int shelf_rows(void){return s_shelf_grid?6:3;}
static bool cover_tried(const char* path){return stored[atoi(path)];}
static bool book_cover_poll(const char* path,uint8_t** gray){++polls;if(!ready[atoi(path)])return false;*gray=malloc(1);assert(*gray);return true;}
static void cover_store(const char* path,uint8_t* gray,int leaf){assert(atoi(path)/shelf_rows()==leaf);stored[atoi(path)]=true;free(gray);}
static EpdRect row_rect(int row,bool header){(void)header;return(EpdRect){40,308+row*100,604,90};}
static EpdRect ui_rect_union(EpdRect a,EpdRect b){int bottom=a.y+a.height>b.y+b.height?a.y+a.height:b.y+b.height;return(EpdRect){40,a.y,604,bottom-a.y};}
static void render(app_ctx_t* ctx,uint8_t* fb){assert(ctx->fb==fb);++renders;}
'''
unit+=production+r'''
static void reset(void){memset(ready,0,sizeof(ready));memset(stored,0,sizeof(stored));s_covers_dirty=false;s_cover_batch_leaf=-1;renders=polls=0;}
int main(void){
 for(int i=0;i<12;++i)snprintf(s_shelf[i].path,sizeof(s_shelf[i].path),"%d",i);
 uint8_t fb=0;app_ctx_t ctx={0,100,&fb};s_shelf_grid=true;
 for(int i=0;i<6;++i)ready[i]=true;
 assert(shelf_covers_tick(&ctx)==APP_REDRAW_AREA&&renders==1&&polls==6);
 assert(s_area.y==308&&s_area.height==590&&s_mode==MODE_GL16);
 assert(shelf_covers_tick(&ctx)==APP_REDRAW_NONE&&renders==1);
 reset();s_shelf_grid=false;ready[0]=true;
 assert(shelf_covers_tick(&ctx)==APP_REDRAW_NONE&&!renders);
 ctx.now_ms=349;assert(shelf_covers_tick(&ctx)==APP_REDRAW_NONE&&!renders);
 ctx.now_ms=350;assert(shelf_covers_tick(&ctx)==APP_REDRAW_AREA&&renders==1);
 ready[1]=ready[2]=true;assert(shelf_covers_tick(&ctx)==APP_REDRAW_AREA&&renders==2);
 assert(s_area.height==290);
 reset();ctx.leaf=0;ctx.now_ms=100;ready[0]=true;
 assert(shelf_covers_tick(&ctx)==APP_REDRAW_NONE&&s_covers_dirty);
 ctx.leaf=1;ctx.now_ms=900;assert(shelf_covers_tick(&ctx)==APP_REDRAW_NONE&&!s_covers_dirty&&!renders);
 ready[3]=ready[4]=ready[5]=true;
 assert(shelf_covers_tick(&ctx)==APP_REDRAW_AREA&&renders==1);
 puts("cover batch: six warm covers/one update, cold deadline, list/grid bounds, no repeat updates or stale page refresh PASS");
}
'''
with tempfile.TemporaryDirectory(prefix='rp-cover-batch-',dir='/tmp') as folder:
 c=Path(folder)/'test.c';binary=Path(folder)/'test';c.write_text(unit)
 subprocess.run([os.environ.get('CC','cc'),'-std=c11','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',str(c),'-o',str(binary)],cwd=ROOT,check=True)
 subprocess.run([str(binary)],cwd=ROOT,check=True)
