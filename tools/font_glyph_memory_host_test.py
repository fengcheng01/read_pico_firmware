#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# 真实字形光栅化分配失败及缓存恢复。/ Actual glyph rasterization allocation failures and cache recovery.
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
static unsigned allocation,fail_at,live;
void* font_test_malloc(size_t n,unsigned caps){(void)caps;if(++allocation==fail_at)return NULL;void*p=malloc(n);if(p){++live;memset(p,0xa5,n);}return p;}
void* font_test_calloc(size_t n,size_t z,unsigned caps){void*p=font_test_malloc(n*z,caps);if(p)memset(p,0,n*z);return p;}
void font_test_free(void*p){if(p){assert(live);--live;free(p);}}
static size_t host_strlcpy(char*out,const char*in,size_t cap){size_t n=strlen(in);if(cap){size_t z=n<cap-1?n:cap-1;memcpy(out,in,z);out[z]=0;}return n;}
#define strlcpy host_strlcpy
#include "main/font/ttf_font.c"
const char*app_settings_font_path(void){return TTF_FONT_BUILTIN;}
void epd_draw_pixel(int x,int y,uint8_t c,uint8_t*fb){(void)x;(void)y;(void)c;(void)fb;}
int main(int argc,char**argv){
 assert(argc==2);assert(ttf_font_open(argv[1])==ESP_OK);ttf_font_unload();unsigned retained=live;assert(ttf_font_open(argv[1])==ESP_OK);
 unsigned points=0;
 const uint32_t codepoints[]={'A','0','1','2','3','4','5','6','7','8','9',0x9605,0x84dd,0x7259};
 for(unsigned cp=0;cp<sizeof(codepoints)/sizeof(*codepoints);++cp) for(int precise=0;precise<2;++precise){
  ttf_set_precise_curves(precise);ttf_font_cache_clear();allocation=0;
  const glyph_entry_t*g=get_glyph(codepoints[cp],44);assert(g&&g->bitmap_bytes);unsigned steps=allocation;
  size_t bytes=g->bitmap_bytes;uint8_t*reference=malloc(bytes);memcpy(reference,g->bitmap,bytes);
  for(unsigned n=1;n<=steps;++n){
   ttf_font_cache_clear();allocation=0;fail_at=n;
   get_glyph(codepoints[cp],44);fail_at=0;
   g=get_glyph(codepoints[cp],44);
   if(!g||g->bitmap_bytes!=bytes||memcmp(reference,g->bitmap,bytes)){fprintf(stderr,"poisoned glyph after allocation failure %u/%u precise=%d\n",n,steps,precise);return 3;}
   ++points;
  }
  free(reference);
 }
 ttf_font_unload();assert(live==retained);printf("Real glyph allocation failures: %u points recover exact pixels without caching failed raster output PASS\n",points);
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
