#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 mindreset
# SPDX-License-Identifier: Apache-2.0
"""真实布局/字体的跨页辅助线网格回归；不模拟物理刷新。/ Cross-page guide grid using production layout/fonts; no physical refresh model."""
from pathlib import Path
import os
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
FONT_SOURCE = (ROOT / "main/font/ttf_font.c").read_text(encoding="utf-8")


def font_function(name):
    match = re.search(r"^(?:static )?[^\n]+\b" + name + r"\([^\n]*\) \{", FONT_SOURCE, re.M)
    assert match, name
    at, depth = match.end(), 1
    while depth:
        depth += (FONT_SOURCE[at] == "{") - (FONT_SOURCE[at] == "}")
        at += 1
    return FONT_SOURCE[match.start():at]


UNIT = r'''
#include <assert.h>
#include <limits.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "book_layout.h"
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"
// 绘图替身只记录实际轮廓纵向范围，字宽、字号和基线使用固件函数。
// The drawing seam records real outline bounds; advances, size clamping and baselines use firmware functions.
static stbtt_fontinfo font_info;
static bool font_ready = true;
static int raw_ascent_units;
#define SCREEN_ROWS 1600
#define MASK_WORDS ((SCREEN_ROWS + 63) / 64)
typedef struct { uint64_t ink[MASK_WORDS], rules[MASK_WORDS]; } Bands;
static Bands bands, saved[256];
static EpdRect body;
static int base_px, leading, current_px, current_baseline, current_ink_bottom;
static bool grid_active, binary, night;
static size_t groups, solids, dashes, night_rules, day_rules, binary_rules, cases, pair_checks;
static size_t image_pixels;
'''
UNIT += "\n".join(font_function(name) for name in ("clamp_px", "decode_utf8", "resolve_glyph_index", "measure_width", "ttf_ascender_px"))
UNIT += r'''
static int pitch(void) { return base_px + base_px/2 + base_px*leading/100; }
static void set_band(uint64_t* mask, int top, int end) {
    assert(top >= 0 && end <= SCREEN_ROWS);
    for (int y=top;y<end;++y) mask[y/64] |= UINT64_C(1) << (y%64);
}
int ttf_text_width_px(int px, const char* text) { return measure_width(clamp_px(px), text); }
void ttf_draw_text_px(uint8_t* fb, int x, int baseline, int px, const char* text,
                      enum EpdFontFlags align, uint8_t fg, uint8_t bg) {
    (void)fb; (void)x; (void)align; assert(fg <= 15 && bg <= 15);
    px = clamp_px(px);
    if (baseline != current_baseline || px != current_px) {
        current_px=px; current_baseline=baseline; current_ink_bottom=INT_MIN; ++groups;
    }
    int top = baseline - ttf_ascender_px(px);
    if (grid_active) assert((top-24)%pitch()==0);
    float scale = stbtt_ScaleForPixelHeight(&font_info, (float)px);
    while (*text) {
        int x0,y0,x1,y1;
        stbtt_GetCodepointBitmapBox(&font_info,(int)decode_utf8(&text),scale,scale,&x0,&y0,&x1,&y1);
        if (x1<=x0 || y1<=y0) continue;
        assert(baseline+y0 >= body.y && baseline+y1 <= body.y+body.height);
        set_band(bands.ink,baseline+y0,baseline+y1);
        if (baseline+y1>current_ink_bottom) current_ink_bottom=baseline+y1;
    }
}
void epd_draw_pixel(int x, int y, uint8_t color, uint8_t* fb) {
    (void)color; (void)fb;
    assert(x>=body.x && x<body.x+body.width && y>=body.y && y<body.y+body.height);
    ++image_pixels;
}
void epd_fill_rect(EpdRect rect, uint8_t color, uint8_t* fb) {
    (void)fb;
    if (night && color==0 && !memcmp(&rect,&body,sizeof(rect))) return;
    uint8_t expected=binary ? (night ? 0xF0 : 0) : (night ? 0x50 : 0xB0);
    assert(color==expected && current_ink_bottom!=INT_MIN);
    int row_top=current_baseline-ttf_ascender_px(current_px);
    int height=grid_active ? pitch() : current_px+current_px/2+current_px*leading/100;
    assert(rect.y>current_ink_bottom && rect.y+rect.height<=row_top+height);
    if (grid_active) assert(rect.y==row_top+pitch()-3 && (rect.y-24+3)%pitch()==0);
    assert(rect.x>=body.x && rect.x+rect.width<=body.x+body.width);
    assert(rect.y>=body.y && rect.y+rect.height<=body.y+body.height);
    assert(rect.height==2 && (rect.width==body.width || rect.width==8));
    if (rect.width==body.width) ++solids; else ++dashes;
    if (night) ++night_rules; else ++day_rules;
    if (binary) { assert((color>>4)!=(night ? 0 : 15)); ++binary_rules; }
    set_band(bands.rules,rect.y,rect.y+rect.height);
}
static void draw_page(size_t page) {
    uint8_t fb=0; memset(&bands,0,sizeof(bands));
    current_px=current_baseline=INT_MIN; current_ink_bottom=INT_MIN;
    book_layout_draw_page(&fb,page,body,base_px);
    for (int i=0;i<MASK_WORDS;++i) assert(!(bands.ink[i]&bands.rules[i]));
}
static void check_font_slots(void) {
    // 检查全部字形及+8px标题可落入同一文字带，而不只测夹具里碰巧用到的字。
    // Check every glyph and +8px heading against one text band, beyond the fixture's selected characters.
    for (int heading=0;heading<2;++heading) {
        int px=base_px+(heading ? 8 : 0), baseline=ttf_ascender_px(px);
        float scale=stbtt_ScaleForPixelHeight(&font_info,(float)px);
        for (int g=0;g<font_info.numGlyphs;++g) {
            int x0,y0,x1,y1; stbtt_GetGlyphBitmapBox(&font_info,g,scale,scale,&x0,&y0,&x1,&y1);
            if(x1<=x0 || y1<=y0) continue;
            assert(baseline+y0>=0 && baseline+y1<pitch()-3);
        }
    }
}
static void configure(int style, bool contrast, bool inverted) {
    binary=contrast; night=inverted;
    book_layout_set_guide(style); book_layout_set_guide_contrast(binary); book_layout_set_night(night);
}
int main(int argc, char** argv) {
    assert(argc==3);
    char chapter[32000]; size_t len=0;
    blk_t blocks[60];
    const char* para[]={"苏灿说道，屏幕文字需要清晰。Who care?", "李昌隆摆摆手，阅读之后返回下一段。", "这是一段更长的正文，其中包含中文和 gypq descenders 以及标点。"};
    for(int i=0;i<60;++i) {
        size_t start=len;
        for(int j=0;j<1+i%4;++j) { size_t n=strlen(para[i%3]); memcpy(chapter+len,para[i%3],n); len+=n; }
        blocks[i]=(blk_t){.offset=start,.len=len-start,.heading=i%7==0};
        if(i<59) chapter[len++]='\n';
    }
    chapter[len]=0;
    book_layout_set_guide_origin(24);
    for(int file=1;file<argc;++file) {
        FILE* f=fopen(argv[file],"rb"); assert(f); fseek(f,0,SEEK_END); long n=ftell(f); rewind(f);
        unsigned char* data=malloc((size_t)n); assert(data); assert(fread(data,1,(size_t)n,f)==(size_t)n); fclose(f);
        assert(stbtt_InitFont(&font_info,data,0)); int descent,gap;
        stbtt_GetFontVMetrics(&font_info,&raw_ascent_units,&descent,&gap);
        puts(argv[file]);
        for(base_px=36;base_px<=80;base_px+=4) for(leading=0;leading<=60;leading+=30) {
            check_font_slots();
            for(int tier=0;tier<2;++tier) for(int indent=0;indent<2;++indent) {
                size_t saved_count=0;
                book_layout_set_leading(leading); book_layout_set_paragraph(tier); book_layout_set_indent(indent);
                for(int warning=0;warning<2;++warning) {
                    body=(EpdRect){40,warning ? 88 : 24,604,warning ? 992 : 1056};
                    configure(1,false,false); grid_active=true;
                    assert(book_layout_build_blocks(chapter,len,blocks,60,body,base_px));
                    size_t count=book_layout_page_count(),offsets[256]; assert(count>1 && count<256);
                    for(size_t page=0;page<count;++page) offsets[page]=book_layout_page_start_offset(page);
                    for(int align=0;align<3;++align) {
                        book_layout_set_align(align);
                        for(size_t page=0;page<count;++page) {
                            configure(1,false,false); draw_page(page); Bands original=bands;
                            if(align==0) { assert(saved_count<256); saved[saved_count++]=bands; }
                            for(int style=1;style<=2;++style) for(int contrast=0;contrast<2;++contrast) for(int inverted=0;inverted<2;++inverted) {
                                configure(style,contrast,inverted); draw_page(page);
                                assert(!memcmp(original.ink,bands.ink,sizeof(bands.ink)));
                                assert(!memcmp(original.rules,bands.rules,sizeof(bands.rules)));
                                assert(book_layout_page_count()==count && book_layout_page_start_offset(page)==offsets[page]);
                                ++cases;
                            }
                        }
                    }
                    // 分批排版也必须得到完全相同的网格页面与偏移。
                    // Incremental pagination must produce identical grid pages and byte offsets.
                    assert(book_layout_begin_blocks(chapter,len,blocks,60,body,base_px));
                    while(!book_layout_complete()) assert(book_layout_extend(2));
                    assert(book_layout_page_count()==count);
                    for(size_t page=0;page<count;++page) assert(book_layout_page_start_offset(page)==offsets[page]);
                }
                // 任意前后页、正反翻和跨缺字提示顶部均不能让旧线进入新文字带。
                // Any page pair, either turn direction and either notice/body top must keep old rules outside new ink.
                for(size_t old=0;old<saved_count;++old) for(size_t next=0;next<saved_count;++next) {
                    for(int word=0;word<MASK_WORDS;++word) assert(!(saved[old].rules[word]&saved[next].ink[word]));
                    ++pair_checks;
                }
            }
        }
        // 含图章节明确回退；开启辅助线不改变图文基线或分页。
        // Image chapters explicitly fall back; guides preserve their image/text baselines and pagination.
        uint8_t image[2000]; memset(image,128,sizeof(image));
        blk_t illustrated[]={{.offset=0,.len=1},{.offset=2,.len=3,.image=image,.image_width=40,.image_height=50},{.offset=6,.len=1}};
        base_px=48; leading=0; grid_active=false; body=(EpdRect){40,24,604,1000};
        book_layout_set_leading(0); book_layout_set_paragraph(0); book_layout_set_indent(false); book_layout_set_align(0);
        configure(0,false,false); assert(book_layout_build_blocks("a\nimg\nb",7,illustrated,3,body,base_px));
        draw_page(0); Bands plain=bands; size_t original_count=book_layout_page_count();
        configure(1,false,false); assert(book_layout_build_blocks("a\nimg\nb",7,illustrated,3,body,base_px));
        image_pixels=0; draw_page(0);
        assert(image_pixels==2000 && book_layout_page_count()==original_count && !memcmp(plain.ink,bands.ink,sizeof(bands.ink)));
        illustrated[1].image=NULL; illustrated[1].image_src="missing.png";
        configure(0,false,false); assert(book_layout_build_blocks("a\nimg\nb",7,illustrated,3,body,base_px)); draw_page(0); plain=bands;
        configure(1,false,false); assert(book_layout_build_blocks("a\nimg\nb",7,illustrated,3,body,base_px)); draw_page(0);
        assert(!memcmp(plain.ink,bands.ink,sizeof(bands.ink)));
        EpdRect hit; assert(book_layout_image_at(0,body,100,150,&hit)==1);
        assert(book_layout_text_at(0,body,100,150)==SIZE_MAX);
        book_layout_free(); free(data);
    }
    assert(solids && dashes && day_rules && night_rules && binary_rules && pair_checks);
    printf("book guide grid: %zu page/style/palette/night cases, %zu text groups PASS\n",cases,groups);
    printf("book guide grid: %zu arbitrary old/new page pairs, both body tops, headings/paragraph tiers, zero crossings PASS\n",pair_checks);
}
'''

with tempfile.TemporaryDirectory(prefix="pico-book-guide-") as folder:
    folder = Path(folder)
    source, binary = folder / "guide.c", folder / "guide"
    source.write_text(UNIT, encoding="utf-8")
    subprocess.run([
        os.environ.get("CC", "cc"), "-std=c11", "-D_DEFAULT_SOURCE", "-Wall", "-Wextra", "-Werror",
        "-g", "-fsanitize=address,undefined", "-fno-omit-frame-pointer",
        "-Itools/book_layout_stubs", "-Itools/book_source_host_stubs", "-Imain/book", "-Imain/font",
        str(source), "main/book/book_layout.c", "-lm", "-o", str(binary),
    ], cwd=ROOT, check=True)
    subprocess.run([str(binary), str(ROOT / "main/assets/builtin.ttf"), str(ROOT / "sdcard/fonts/ChillDuanSansVF.ttf")], cwd=ROOT, check=True)
