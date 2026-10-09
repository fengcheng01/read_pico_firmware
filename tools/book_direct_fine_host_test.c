/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 * 中文：真实字形与分页的直刷细边对照，保留灰阶参考、墨芯及文本锚点。
 * English: Compare fine direct edges through real glyphs/layout, retaining the gray reference, ink cores and text anchors.
 * 冻结：仅测试像素与软件边界，不模拟面板或声称物理抗锯齿效果。
 * Frozen: Test pixels and software boundaries only; never simulate the panel or claim optical antialiasing.
 */
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

size_t host_largest_block;
static size_t host_strlcpy(char* out, const char* text, size_t cap) {
    size_t n = strlen(text);
    if (cap) { size_t copy = n < cap - 1 ? n : cap - 1; memcpy(out, text, copy); out[copy] = 0; }
    return n;
}
#define strlcpy host_strlcpy
#define ttf_draw_text_px real_ttf_draw_text_px
#define ttf_draw_text_px_bw real_ttf_draw_text_px_bw
#include "main/font/ttf_font.c"
#undef ttf_draw_text_px
#undef ttf_draw_text_px_bw
#include "book_layout.h"
#include "display_pixels.h"
#ifdef HOST_LEGACY_REFERENCE
#include "legacy_api.h"
#endif

const uint8_t builtin_pack_start[] asm("_binary_builtin_pack_start") = {0};
const uint8_t builtin_pack_end[] asm("_binary_builtin_pack_end") = {0};
const char* app_settings_font_path(void) { return TTF_FONT_BUILTIN; }
size_t asset_pack_size(const uint8_t* data, size_t size) { (void)data; (void)size; return 0; }
bool asset_pack_unpack(const uint8_t* data, size_t size, uint8_t* out, size_t cap) {
    (void)data; (void)size; (void)out; (void)cap; return false;
}

#define WIDTH 684
#define HEIGHT 1216
#define PIXELS ((size_t)WIDTH * HEIGHT)
#define BYTES (PIXELS / 2)
#define MAX_CALLS 512
typedef struct {
    int x, y, px, align, width;
    uint8_t fg, bg;
    char text[512];
} draw_call_t;
static uint8_t actual[BYTES], original[BYTES], reference[BYTES], raw_core[(PIXELS + 7) / 8];
#ifdef HOST_LEGACY_REFERENCE
static uint8_t legacy_frame[BYTES];
#endif
static draw_call_t calls[MAX_CALLS], saved_calls[MAX_CALLS];
static size_t call_count, saved_count, gray_calls, bw_calls;
static size_t cases, removed_edges, restored_overlap, core_checks, binary_pixels;
static bool observe_reference, in_font;
static size_t hit_offsets[512];
static EpdRect body = {40,24,604,1056};

static uint8_t pixel(const uint8_t* fb, size_t p) { return fb[p / 2] >> (4 * (p % 2)) & 15; }
static void write_pixel(int x, int y, uint8_t color, uint8_t* fb) {
    if (x < 0 || x >= WIDTH || y < 0 || y >= HEIGHT) return;
    size_t p = (size_t)y * WIDTH + (unsigned)x;
    unsigned shift = 4 * (unsigned)(p % 2);
    fb[p / 2] = (uint8_t)((fb[p / 2] & ~(15u << shift)) | (unsigned)(color >> 4) << shift);
}
void epd_draw_pixel(int x, int y, uint8_t color, uint8_t* fb) {
    write_pixel(x,y,color,fb);
    if(observe_reference&&!in_font)write_pixel(x,y,color,reference);
}
void epd_fill_rect(EpdRect rect, uint8_t color, uint8_t* fb) {
    for (int y = rect.y; y < rect.y + rect.height; ++y)
        for (int x = rect.x; x < rect.x + rect.width; ++x) epd_draw_pixel(x, y, color, fb);
}
static void record_call(int x, int y, int px, const char* text, int align, uint8_t fg, uint8_t bg) {
    assert(call_count < MAX_CALLS && strlen(text) < sizeof(calls[0].text));
    draw_call_t* c = &calls[call_count++]; memset(c, 0, sizeof(*c));
    c->x=x; c->y=y; c->px=px; c->align=align; c->width=ttf_text_width_px(px,text); c->fg=fg; c->bg=bg;
    memcpy(c->text,text,strlen(text)+1);
}
static void observe_core(int x, int baseline, int px, const char* text, int align) {
    int pen = x;
    if (align & EPD_DRAW_ALIGN_CENTER) pen -= ttf_text_width_px(px,text)/2;
    else if (align & EPD_DRAW_ALIGN_RIGHT) pen -= ttf_text_width_px(px,text);
    while (*text) {
        const glyph_entry_t* g=get_glyph(decode_utf8(&text),px); assert(g);
        for (int gy=0;gy<g->height;++gy) for (int gx=0;gx<g->width;++gx) {
            if (g->bitmap[(size_t)gy*g->width+gx] < 224) continue;
            int xx=pen+g->left+gx, yy=baseline-g->top+gy;
            if (xx<0||xx>=WIDTH||yy<0||yy>=HEIGHT) continue;
            size_t p=(size_t)yy*WIDTH+(unsigned)xx; raw_core[p/8] |= 1u << (p%8);
        }
        pen += g->advance_x;
    }
}
// 自包含旧灰阶参考用原始字形覆盖率独立计算0.6伽马；候选按覆盖率过半合成，不调用生产draw。
// A self-contained old-gray reference computes 0.6 gamma from raw coverage; the candidate composites over-half coverage without calling production draw.
static void oracle_text(int x, int baseline, int px, const char* text, int align, uint8_t fg, uint8_t bg, bool binary) {
    int pen=x;if(align&EPD_DRAW_ALIGN_CENTER)pen-=ttf_text_width_px(px,text)/2;
    else if(align&EPD_DRAW_ALIGN_RIGHT)pen-=ttf_text_width_px(px,text);
    while(*text){const glyph_entry_t* g=get_glyph(decode_utf8(&text),px);assert(g);
        for(int gy=0;gy<g->height;++gy)for(int gx=0;gx<g->width;++gx){
            uint8_t a=g->bitmap[(size_t)gy*g->width+gx], ink;
            if(binary){if(a<128)continue;ink=fg;}
            else {if(!a)continue;int coverage=(int)(255.f*powf((float)a/255.f,0.6f)+0.5f);
                ink=(uint8_t)(bg+coverage*((int)fg-(int)bg)/255);}
            write_pixel(pen+g->left+gx,baseline-g->top+gy,ink<<4,reference);
        }
        pen+=g->advance_x;
    }
}
void ttf_draw_text_px(uint8_t* fb, int x, int y, int px, const char* text,
                      enum EpdFontFlags align, uint8_t fg, uint8_t bg) {
    record_call(x,y,px,text,align,fg,bg); ++gray_calls;
    if(observe_reference)oracle_text(x,y,px,text,align,fg,bg,false);
    in_font=true;
    real_ttf_draw_text_px(fb,x,y,px,text,align,fg,bg);
    in_font=false;
}
void ttf_draw_text_px_bw(uint8_t* fb, int x, int y, int px, const char* text,
                         enum EpdFontFlags align, uint8_t fg, uint8_t bg) {
    record_call(x,y,px,text,align,fg,bg); ++bw_calls; observe_core(x,y,px,text,align);
    if(observe_reference)oracle_text(x,y,px,text,align,fg,bg,true);
    in_font=true;
    real_ttf_draw_text_px_bw(fb,x,y,px,text,align,fg,bg);
    in_font=false;
}
static void reset_draw(uint8_t* fb) {
    memset(fb,0xff,BYTES); memset(raw_core,0,sizeof(raw_core));
    call_count=gray_calls=bw_calls=0;observe_reference=in_font=false;
}
static void save_geometry(void) { saved_count=call_count; memcpy(saved_calls,calls,call_count*sizeof(*calls)); }
static void check_geometry(void) {
    assert(call_count==saved_count && !memcmp(calls,saved_calls,call_count*sizeof(*calls)));
}
static void configure(int guide, bool night, int align, bool fine) {
    book_layout_set_night(night);book_layout_set_guide(guide);book_layout_set_guide_contrast(true);
    book_layout_set_guide_origin(24);book_layout_set_align(align);book_layout_set_indent(true);
    book_layout_set_leading(0);book_layout_set_paragraph(0);
#ifdef HOST_LEGACY_REFERENCE
    legacy_book_layout_set_night(night);legacy_book_layout_set_guide(guide);legacy_book_layout_set_guide_contrast(true);
    legacy_book_layout_set_guide_origin(24);legacy_book_layout_set_align(align);legacy_book_layout_set_indent(true);
    legacy_book_layout_set_leading(0);legacy_book_layout_set_paragraph(0);
#endif
    book_layout_set_direct_fine(fine);
}
static void snapshot_hits(size_t page, bool compare) {
    size_t i=0;
    for (int y=body.y;y<body.y+body.height;y+=48) for (int x=body.x;x<body.x+body.width;x+=32) {
        size_t fresh=book_layout_text_at(page,body,x,y);assert(i<512);
        if(compare)assert(hit_offsets[i]==fresh);else hit_offsets[i]=fresh;
#ifdef HOST_LEGACY_REFERENCE
        assert(fresh==legacy_book_layout_text_at(page,body,x,y));
#endif
        ++i;
    }
}
static void check_page(size_t page, int px, bool night) {
#ifdef HOST_LEGACY_REFERENCE
    reset_draw(legacy_frame); legacy_book_layout_draw_page(legacy_frame,page,body,px);assert(gray_calls&&!bw_calls);save_geometry();
#endif
    reset_draw(original); book_layout_set_direct_fine(false); book_layout_draw_page(original,page,body,px);
    assert(gray_calls&&!bw_calls);
#ifdef HOST_LEGACY_REFERENCE
    assert(!memcmp(original,legacy_frame,BYTES));check_geometry();
#endif
    save_geometry();snapshot_hits(page,false);
    reset_draw(original);memset(reference,0xff,BYTES);observe_reference=true;book_layout_draw_page(original,page,body,px);
    observe_reference=false;assert(!memcmp(original,reference,BYTES));check_geometry();
    display_prepare_direct_frame(original,WIDTH,HEIGHT,night);
    book_layout_set_direct_fine(true);reset_draw(actual);memset(reference,0xff,BYTES);observe_reference=true;book_layout_draw_page(actual,page,body,px);
    observe_reference=false;assert(bw_calls&&!gray_calls);assert(!memcmp(actual,reference,BYTES));check_geometry();snapshot_hits(page,true);
    uint8_t ink=night?15:0;
    for (size_t p=0;p<PIXELS;++p) {
        uint8_t n=pixel(actual,p), old=pixel(original,p);
        assert(n==0||n==15); ++binary_pixels;
        if (raw_core[p/8] & (1u<<(p%8))) { assert(n==ink); ++core_checks; }
        // 细边可保留被后字低覆盖率覆盖的前字墨芯；独立覆盖率合成已验证其来源。
        // Fine edges may retain preceding ink overwritten by a later low-coverage glyph; the independent coverage oracle verifies its origin.
        if(n!=old){if(old==ink)++removed_edges;else ++restored_overlap;}
    }
    memcpy(reference,actual,BYTES); display_prepare_direct_frame(actual,WIDTH,HEIGHT,night);
    assert(!memcmp(actual,reference,BYTES)); ++cases;
}

// 仅提取产品调用侧策略，绘制与字体均为真实实现。/ Extract only the product caller policy; use real drawing/fonts.
static blk_t* s_blocks;
static size_t s_block_count;
static bool setting_direct, setting_fine, setting_night;
static uint8_t setting_profile;
bool app_settings_book_direct(void) { return setting_direct; }
bool app_settings_book_direct_fine(void) { return setting_fine; }
bool app_settings_book_night(void) { return setting_night; }
uint8_t app_settings_book_night_profile(void) { return setting_profile; }
uint8_t app_settings_book_leading(void) { return 0; }
bool app_settings_book_indent(void) { return true; }
uint8_t app_settings_book_para(void) { return 0; }
uint8_t app_settings_book_guide(void) { return 0; }
uint8_t app_settings_book_align(void) { return 0; }
static EpdRect ui_product_reader_body(bool notice) { (void)notice; return body; }
#include "book_policy.h"
#include "reader_policy.h"

static void check_policy_exclusions(void) {
    const char text[]="Pico 中文正文\nimg\n文字与图像";
    const char heading[]="Pico 中文正文";
    uint8_t image[32*24]; memset(image,117,sizeof(image));
    size_t h=strlen(heading);
    blk_t blocks[]={{.offset=0,.len=h},{.offset=h+1,.len=3,.image=image,.image_width=32,.image_height=24},{.offset=h+5,.len=strlen(text)-h-5}};
    setting_direct=false;setting_fine=true;setting_night=false;setting_profile=BOOK_NIGHT_PROFILE_CURRENT;
    s_blocks=blocks;s_block_count=1;apply_typography();
    assert(book_layout_build(heading,h,body,48));reset_draw(actual);book_layout_draw_page(actual,0,body,48);
    assert(gray_calls&&!bw_calls);
    setting_direct=true;apply_typography();reset_draw(actual);book_layout_draw_page(actual,0,body,48);assert(bw_calls&&!gray_calls);
    setting_fine=false;apply_typography();reset_draw(actual);book_layout_draw_page(actual,0,body,48);assert(gray_calls&&!bw_calls);
    setting_fine=true;s_block_count=3;apply_typography();assert(!reader_direct_enabled());
    assert(book_layout_build_blocks(text,strlen(text),blocks,3,body,48));reset_draw(actual);book_layout_draw_page(actual,0,body,48);
    assert(gray_calls&&!bw_calls);bool middle=false;
    for(size_t p=0;p<PIXELS;++p)if(pixel(actual,p)>0&&pixel(actual,p)<15){middle=true;break;}assert(middle);
    // 加载失败的图片占位保留原绘制函数；细边不得改占位文案字形。
    // Failed-image placeholders retain the original drawing function; fine mode must not change their caption glyphs.
    blocks[1].image=NULL;blocks[1].image_src="missing.jpg";apply_typography();
    assert(reader_direct_enabled()&&book_layout_build_blocks(text,strlen(text),blocks,3,body,48));
    reset_draw(actual);book_layout_draw_page(actual,0,body,48);assert(gray_calls&&bw_calls);
    setting_direct=false;setting_night=true;setting_profile=BOOK_NIGHT_PROFILE_CROSSMUX;s_block_count=1;apply_typography();
    assert(reader_direct_enabled()&&book_layout_build(heading,h,body,48));
    reset_draw(actual);book_layout_draw_page(actual,0,body,48);assert(bw_calls&&!gray_calls);
    setting_profile=BOOK_NIGHT_PROFILE_CURRENT;apply_typography();reset_draw(actual);book_layout_draw_page(actual,0,body,48);
    assert(gray_calls&&!bw_calls);
    book_layout_free();s_blocks=NULL;s_block_count=0;
}
int main(int argc,char** argv) {
    assert(argc==3);char chapter[10000];size_t len=0;blk_t blocks[18];
    const char* para="小纸 Pico 清晰字缘，阅读 gypq 中文与 Latin 0123456789，继续翻页观察。";
    for(int b=0;b<18;++b){size_t start=len;for(int repeat=0;repeat<1+b%2;++repeat){size_t n=strlen(para);memcpy(chapter+len,para,n);len+=n;}
        blocks[b]=(blk_t){.offset=start,.len=len-start,.heading=b%5==0};if(b<17)chapter[len++]='\n';}
    chapter[len]=0;int sizes[]={36,48,64,80},weights[]={300,400,700};
    host_largest_block=64u*1024u*1024u;
    for(int file=1;file<argc;++file){assert(ttf_font_open(argv[file])==ESP_OK);printf("font: %s\n",argv[file]);fflush(stdout);
        for(size_t w=0;w<3;++w){ttf_set_weight(weights[w]);assert(ttf_get_weight()==weights[w]);
            for(size_t z=0;z<4;++z)for(int align=0;align<3;++align)for(int night=0;night<2;++night)for(int guide=0;guide<3;++guide){
                configure(guide,night,align,false);assert(book_layout_build_blocks(chapter,len,blocks,18,body,sizes[z]));
#ifdef HOST_LEGACY_REFERENCE
                assert(legacy_book_layout_build_blocks(chapter,len,blocks,18,body,sizes[z]));
                size_t count=book_layout_page_count();assert(count>1&&count==legacy_book_layout_page_count());
                for(size_t p=0;p<=count;++p)assert(book_layout_page_start_offset(p)==legacy_book_layout_page_start_offset(p));
                for(size_t off=0;off<len;off+=19)assert(book_layout_page_for_offset(off)==legacy_book_layout_page_for_offset(off));
#else
                size_t count=book_layout_page_count();assert(count>1);
#endif
                size_t pages[]={0,count/2,count-1};for(size_t p=0;p<3;++p)check_page(pages[p],sizes[z],night);
                size_t offsets[128];assert(count<128);for(size_t p=0;p<count;++p)offsets[p]=book_layout_page_start_offset(p);
                assert(book_layout_begin_blocks(chapter,len,blocks,18,body,sizes[z]));while(!book_layout_complete())assert(book_layout_extend(2));
                assert(book_layout_page_count()==count);for(size_t p=0;p<count;++p)assert(book_layout_page_start_offset(p)==offsets[p]);
                book_layout_free();
#ifdef HOST_LEGACY_REFERENCE
                legacy_book_layout_free();
#endif
            }
        }
        check_policy_exclusions();ttf_font_unload();
    }
    assert(removed_edges&&core_checks&&binary_pixels);
    printf("fine direct: %zu page cases, %zu edge pixels removed, %zu overlap ink pixels retained, %zu ink-core checks, unchanged geometry/anchors/hits PASS\n",cases,removed_edges,restored_overlap,core_checks);
    puts("fine direct: default gray bytes, real fonts/variable weights, standard/decoded-image/placeholder exclusions and effective Crossmux policy PASS; no optical claim");
}
