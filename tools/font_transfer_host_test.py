#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# 真实字体切换的逐处分配失败及首次绘字回归。/ Actual font switching with allocation failures and first-frame drawing regressions.
import ast
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
syntax = ast.parse((ROOT / 'tools/test_font_punctuation.py').read_text())
stubs = ast.literal_eval(next(node.value for node in syntax.body if isinstance(node, ast.Assign) and any(isinstance(n, ast.Name) and n.id == 'STUBS' for n in node.targets)))
stubs['esp_heap_caps.h'] = '''#pragma once
#include <stddef.h>
#define MALLOC_CAP_SPIRAM 1
#define MALLOC_CAP_8BIT 2
void* font_test_malloc(size_t,unsigned);
void* font_test_calloc(size_t,size_t,unsigned);
void font_test_free(void*);
static inline size_t heap_caps_get_largest_free_block(unsigned caps){(void)caps;return 64u*1024u*1024u;}
#define heap_caps_malloc font_test_malloc
#define heap_caps_calloc font_test_calloc
#define heap_caps_free font_test_free
'''
unit = r'''
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <fcntl.h>
static unsigned allocation,fail_at,live;
void* font_test_malloc(size_t n,unsigned caps){
 (void)caps;if(++allocation==fail_at)return NULL;
 void* p=malloc(n);if(p)++live;return p;
}
void* font_test_calloc(size_t n,size_t size,unsigned caps){
 void* p=font_test_malloc(n*size,caps);if(p)memset(p,0,n*size);return p;
}
void font_test_free(void* p){if(p){assert(live);--live;free(p);}}
static size_t host_strlcpy(char* out,const char* in,size_t cap){
 size_t n=strlen(in);if(cap){size_t z=n<cap-1?n:cap-1;memcpy(out,in,z);out[z]=0;}return n;
}
#define strlcpy host_strlcpy
#include "main/font/ttf_font.c"
static const char* selected;
const char* app_settings_font_path(void){return selected;}
static unsigned ink;
void epd_draw_pixel(int x,int y,uint8_t color,uint8_t* fb){(void)x;(void)y;(void)fb;if(color<15)++ink;}
static void readable(void){
 assert(ttf_font_ready());ink=0;
 uint8_t fb[1]={0};ttf_draw_text_px(fb,10,10,32,"连接与传输 停止并返回",EPD_DRAW_ALIGN_LEFT,0,15);
 assert(ink>100);
}
int main(int argc,char**argv){
 assert(argc==2);selected=argv[1];
 assert(ttf_font_open(selected)==ESP_OK);ttf_font_unload();unsigned retained=live;
 assert(ttf_font_open(selected)==ESP_OK);allocation=0;
 assert(ttf_font_suspend_sd(true)==ESP_OK&&sd_suspended&&ttf_font_is_builtin());
 unsigned steps=allocation;assert(steps>=4);readable();
 assert(ttf_font_resume_sd()==ESP_OK&&!sd_suspended&&!ttf_font_is_builtin());ttf_font_unload();assert(live==retained);
 for(unsigned failure=1;failure<=steps;++failure){
   assert(ttf_font_open(selected)==ESP_OK);readable();allocation=0;fail_at=failure;
   assert(ttf_font_suspend_sd(true)!=ESP_OK);
   fail_at=0;
   assert(!sd_suspended&&!ttf_font_is_builtin()&&!strcmp(ttf_font_path(),selected));
   assert(font_fd>=0&&fcntl(font_fd,F_GETFD)>=0);readable();
   assert(ttf_font_resume_sd()==ESP_OK);readable();
   assert(ttf_font_suspend_sd(true)==ESP_OK&&sd_suspended&&ttf_font_is_builtin());readable();
   unsigned before=allocation;assert(ttf_font_open_builtin()==ESP_OK&&allocation==before);
   assert(ttf_font_resume_sd()==ESP_OK&&!sd_suspended&&!ttf_font_is_builtin());readable();
   ttf_font_unload();assert(live==retained);
 }
 selected=TTF_FONT_BUILTIN;assert(!font_ready);
 assert(ttf_font_resume_sd()==ESP_OK&&ttf_font_is_builtin());readable();
 ttf_font_unload();assert(live==retained);
 printf("Real font transfer: %u allocation/decode/table failure points preserve readable SD text, gate transfer, recover/retry without leaks; built-in no-op and missing-font resume PASS\n",steps);
}
'''
with tempfile.TemporaryDirectory(prefix='pico-font-transfer-') as folder:
    out=Path(folder)
    for name,content in stubs.items():(out/name).write_text(content)
    (out/'test.c').write_text(unit)
    section='.section __TEXT,__const' if os.uname().sysname=='Darwin' else '.section .rodata'
    (out/'assets.S').write_text(section+'\n.globl _binary_builtin_pack_start\n_binary_builtin_pack_start:\n.incbin "main/assets/builtin.pack"\n.globl _binary_builtin_pack_end\n_binary_builtin_pack_end:\n.byte 0\n')
    command=[os.environ.get('CC','cc'),'-std=gnu11','-Wall','-Wextra','-Werror','-g','-fsanitize=address,undefined',
             '-I'+str(out),'-I'+str(ROOT),'-Imain','-Itools/zip_host_stubs',str(out/'test.c'),'main/asset_pack.c',str(out/'assets.S'),'-lm','-lz','-o',str(out/'test')]
    subprocess.run(command,cwd=ROOT,check=True)
    subprocess.run([str(out/'test'),str(ROOT/'main/assets/builtin.ttf')],check=True)
