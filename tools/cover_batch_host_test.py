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
s=(ROOT/'main/apps/app_book.c').read_text()
def function(signature):
    start=s.index(signature);brace=s.index('{',start);depth=0
    for i in range(brace,len(s)):
        if s[i]=='{':depth+=1
        elif s[i]=='}':
            depth-=1
            if depth==0:return s[start:i+1]
    raise RuntimeError(signature)
production='\n'.join(function(sig) for sig in (
    'static void shelf_cover_restore(',
    'static void shelf_cover_prefetch(',
    'static app_redraw_t shelf_covers_tick(',
))
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
static bool s_covers_dirty,s_cover_batch_grid,s_shelf_grid,ready[18],stored[18];
static int seen[18];
static int s_cover_batch_leaf=-1,s_visible_count=12,s_mode,renders,polls;
static int64_t s_cover_batch_due;
static int s_cover_prefetch_leaf=-1,s_cover_prefetch_pos;
static EpdRect s_area;
static struct {char path[16];bool removed;} s_shelf[18];
#define MODE_GL16 7
#define ESP_LOGI(...) ((void)0)
static int shelf_rows(void){return s_shelf_grid?6:3;}
static int leaves(void){return (s_visible_count+shelf_rows()-1)/shelf_rows();}
static bool cover_tried(const char* path){return stored[atoi(path)];}
static bool book_cover_poll(const char* path,uint8_t** gray){int i=atoi(path);++polls;++seen[i];if(!ready[i])return false;*gray=malloc(1);assert(*gray);return true;}
static void cover_store(const char* path,uint8_t* gray,int leaf){assert(atoi(path)/shelf_rows()==leaf);stored[atoi(path)]=true;free(gray);}
static EpdRect row_rect(int row,bool header){(void)header;return(EpdRect){40,308+row*100,604,90};}
static EpdRect ui_rect_union(EpdRect a,EpdRect b){int bottom=a.y+a.height>b.y+b.height?a.y+a.height:b.y+b.height;return(EpdRect){40,a.y,604,bottom-a.y};}
static void render(app_ctx_t* ctx,uint8_t* fb){assert(ctx->fb==fb);++renders;}
'''
unit+=production+r'''
static void reset(void){memset(ready,0,sizeof(ready));memset(stored,0,sizeof(stored));memset(seen,0,sizeof(seen));s_covers_dirty=false;s_cover_batch_leaf=-1;s_cover_prefetch_leaf=-1;s_cover_prefetch_pos=0;renders=polls=0;}
int main(void){
 for(int i=0;i<18;++i)snprintf(s_shelf[i].path,sizeof(s_shelf[i].path),"%d",i);
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
 // 中页首帧恢复六张成品，随后按下一页、上一页顺序逐张预取；未完成项不会推进。
 // Restore six prepared covers for the middle page, then prefetch next and previous
 // one at a time; a pending item never advances the cursor.
 reset();s_shelf_grid=true;s_visible_count=18;ctx.leaf=1;
 for(int i=0;i<18;++i)ready[i]=true;
 shelf_cover_restore(&ctx);
 for(int i=0;i<18;++i)assert(stored[i]==(i>=6&&i<12));
 memset(seen,0,sizeof(seen));
 for(int i=0;i<12;++i)shelf_cover_prefetch(&ctx);
 for(int i=0;i<18;++i)assert(seen[i]==(i<6||i>=12));
 assert(s_cover_prefetch_pos==12);
 ctx.leaf=0;s_cover_prefetch_leaf=-1;s_cover_prefetch_pos=0;ready[6]=false;
 shelf_cover_prefetch(&ctx);assert(s_cover_prefetch_pos==0);
 ready[6]=true;shelf_cover_prefetch(&ctx);assert(s_cover_prefetch_pos==1);
 puts("cover batch: grouped updates, cold deadline, first-frame restore and ordered adjacent prefetch PASS");
}
'''
with tempfile.TemporaryDirectory(prefix='rp-cover-batch-',dir='/tmp') as folder:
 c=Path(folder)/'test.c';binary=Path(folder)/'test';c.write_text(unit)
 subprocess.run([os.environ.get('CC','cc'),'-std=c11','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',str(c),'-o',str(binary)],cwd=ROOT,check=True)
 subprocess.run([str(binary)],cwd=ROOT,check=True)
