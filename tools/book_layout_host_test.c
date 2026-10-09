/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 * 中文：用伪字体验证布局完整性、偏移和测宽复杂度。
 * English: Verify layout completeness, offsets and measurement complexity using a fake font.
 * 冻结：只用于宿主测试。/ Frozen: Host testing only.
 */
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "book_layout.h"
#include "ttf_font.h"
static char drawn[20000];
static size_t measured_codepoints;
static size_t measure_calls;
static int first_draw_px, last_draw_px;
static int first_draw_y, last_draw_y;
static size_t image_pixels;
static EpdRect filled[1024];
static uint8_t fill_colors[1024];
static size_t fill_count;
void epd_fill_rect(EpdRect rect, uint8_t color, uint8_t* fb) {
    (void)fb; assert(color <= 15 || color == 0xB0 || color == 0x50 || color == 0xF0);
    assert(fill_count < sizeof(filled) / sizeof(filled[0]));
    filled[fill_count] = rect; fill_colors[fill_count++] = color;
}
void epd_draw_pixel(int x, int y, uint8_t color, uint8_t* fb) {
    (void)color; (void)fb;
    assert(x >= 0 && y >= 0);
    ++image_pixels;
}
int ttf_text_width_px(int px, const char* text) {
    int n = 0, width = 0;
    for (; *text; text++) if (((unsigned char)*text & 0xc0) != 0x80) {
        n++;
        width += *text == 'i' ? px / 2 : *text == 'W' ? px + px / 2 : px;
    }
    measured_codepoints += (size_t)n;
    measure_calls++;
    return width;
}
int ttf_ascender_px(int px) { return px; }
void ttf_draw_text_px(uint8_t* fb, int x, int y, int px, const char* text,
                      enum EpdFontFlags align, uint8_t fg, uint8_t bg) {
    (void)fb; (void)x; (void)y; (void)px; (void)align;
    assert(fg <= 15 && bg <= 15);
    if (!drawn[0]) { first_draw_px = px; first_draw_y = y; }
    last_draw_px = px;
    last_draw_y = y;
    assert(strlen(drawn) + strlen(text) < sizeof(drawn));
    strcat(drawn, text);
}
// 几何替身共享记录；真实二值覆盖率由细边回归验证。/ Geometry seams share records; fine-edge regressions verify real binary coverage.
void ttf_draw_text_px_bw(uint8_t* fb, int x, int y, int px, const char* text,
                         enum EpdFontFlags align, uint8_t fg, uint8_t bg) {
    ttf_draw_text_px(fb, x, y, px, text, align, fg, bg);
}

int main(void) {
    // 小尺寸几何夹具关闭段首缩进，避免两字符缩进占满整行。/ Disable paragraph indent in tiny geometry fixtures so two ems do not consume the whole row.
    book_layout_set_indent(false);
    EpdRect r = {0, 0, 20, 30};
    const char text[] = "甲乙丙丁戊己庚辛壬癸";
    assert(book_layout_build(text, strlen(text), r, 10));
    assert(book_layout_page_count() == 3);
    assert(book_layout_page_start_offset(1) == 12);
    assert(book_layout_page_for_offset(11) == 0);
    assert(book_layout_page_for_offset(12) == 1);
    assert(book_layout_page_for_offset(999) == 2);
    uint8_t fb = 0;
    for (size_t i = 0; i < book_layout_page_count(); i++) {
        assert(book_layout_page_start_offset(i) % 3 == 0);
        book_layout_draw_page(&fb, i, r, 10);
    }
    assert(strcmp(drawn, text) == 0);
    const char mixed[] = "Wi甲iiW";
    assert(book_layout_build(mixed, strlen(mixed), r, 10));
    assert(book_layout_page_count() == 2);
    assert(book_layout_page_start_offset(1) == 7);
    drawn[0] = 0;
    book_layout_draw_page(&fb, 0, r, 10);
    book_layout_draw_page(&fb, 1, r, 10);
    assert(strcmp(drawn, mixed) == 0);
    assert(book_layout_build("", 0, r, 10));
    assert(book_layout_page_count() == 1);
    assert(book_layout_build(NULL, 0, r, 10));
    assert(!book_layout_build(NULL, 1, r, 10));
    assert(book_layout_page_count() == 0);
    assert(!book_layout_build("\xe7\x94", 2, r, 10));
    assert(!book_layout_build("x\0y", 3, r, 10));
    assert(!book_layout_build("x", 1, (EpdRect){0,0,5,30}, 10));
    assert(!book_layout_build("x", 1, (EpdRect){0,0,20,5}, 10));
    char* many = malloc(4097);
    memset(many, 'x', 4097);
    r = (EpdRect){0,0,10,15};
    assert(book_layout_build(many, 4096, r, 10));
    assert(book_layout_page_count() == 4096);
    assert(!book_layout_build(many, 4097, r, 10));
    assert(book_layout_page_count() == 0);
    free(many);
    assert(book_layout_build("a\r\n\r\nb", 6, (EpdRect){0,0,20,100}, 10));
    drawn[0] = 0;
    book_layout_draw_page(&fb, 0, (EpdRect){0,0,20,100}, 10);
    assert(strcmp(drawn, "ab") == 0);
    char long_line[1024];
    memset(long_line, 'z', sizeof(long_line));
    r = (EpdRect){0,0,10000,30};
    measured_codepoints = measure_calls = 0;
    assert(book_layout_build(long_line, sizeof(long_line), r, 10));
    printf("long-line build: %zu calls, %zu measured codepoints\n", measure_calls, measured_codepoints);
    fflush(stdout);
    assert(measured_codepoints <= sizeof(long_line) * 2);
    assert(book_layout_page_count() == 1);
    drawn[0] = 0;
    book_layout_draw_page(&fb, 0, r, 11);
    assert(drawn[0] == 0);
    book_layout_draw_page(&fb, 99, r, 10);
    assert(drawn[0] == 0);
    measured_codepoints = measure_calls = 0;
    book_layout_draw_page(&fb, 0, r, 10);
    // 对齐多测一次整行，末尾溢出字符仍只计一次。/ Alignment measures the final line once more; the overflow character is still measured only once.
    assert(measured_codepoints <= sizeof(long_line) * 2 + 1);
    assert(strlen(drawn) == sizeof(long_line));
    assert(memcmp(drawn, long_line, sizeof(long_line)) == 0);
    assert(book_layout_page_start_offset(99) == sizeof(long_line));
    assert(!book_layout_build("\xed\xa0\x80", 3, r, 10));
    assert(!book_layout_build("\xf4\x90\x80\x80", 4, r, 10));
    assert(!book_layout_build("\xc0\xaf", 2, r, 10));
    book_layout_free();
    assert(book_layout_page_count() == 0);
    const char styled[] = "Title\nbody";
    blk_t blocks[] = {{.offset=0, .len=5, .heading=true}, {.offset=6, .len=4}};
    r = (EpdRect){0, 0, 100, 45};
    assert(book_layout_build_blocks(styled, strlen(styled), blocks, 2, r, 10));
    assert(book_layout_page_count() == 2);
    assert(book_layout_page_start_offset(1) == 6);
    drawn[0] = 0;
    book_layout_draw_page(&fb, 0, r, 10);
    book_layout_draw_page(&fb, 1, r, 10);
    assert(!strcmp(drawn, "Titlebody") && first_draw_px == 18 && last_draw_px == 10);
    r.width = 20;
    assert(book_layout_build_blocks(styled, strlen(styled), blocks, 2, r, 10));
    assert(book_layout_page_count() == 6);
    assert(book_layout_page_start_offset(5) == 6);
    drawn[0] = 0;
    for (size_t i = 0; i < book_layout_page_count(); ++i) book_layout_draw_page(&fb, i, r, 10);
    assert(!strcmp(drawn, "Titlebody"));
    assert(!book_layout_build_blocks(styled, strlen(styled), blocks, 2, (EpdRect){0,0,100,20}, 10));
    blocks[1].offset = 5;
    assert(!book_layout_build_blocks(styled, strlen(styled), blocks, 2, r, 10));
    assert(book_layout_page_count() == 0);
    blocks[1].offset = 6;
    blocks[1].len = SIZE_MAX;
    assert(!book_layout_build_blocks(styled, strlen(styled), blocks, 2, r, 10));
    assert(!book_layout_build_blocks(styled, strlen(styled), NULL, 2, r, 10));
    blk_t split_utf8[] = {{.offset=0, .len=1, .heading=true}, {.offset=2, .len=2}};
    assert(!book_layout_build_blocks("甲\nx", 5, split_utf8, 2, r, 10));
    assert(book_layout_build("body", 4, r, 10));
    drawn[0] = 0;
    book_layout_draw_page(&fb, 0, r, 10);
    assert(first_draw_px == 10);
    // 分批与一次分页的边界相同，第一页不必测量整章。
    // Incremental and eager pagination agree; the first page never measures the whole chapter.
    char chapter[1201]; memset(chapter, 'x', 1200); chapter[1200] = 0;
    r = (EpdRect){0,0,40,30};
    assert(book_layout_build(chapter, 1200, r, 10));
    size_t pages = book_layout_page_count(), offsets[256];
    assert(pages < 256);
    for (size_t i=0; i<pages; ++i) offsets[i] = book_layout_page_start_offset(i);
    measured_codepoints = 0;
    assert(book_layout_begin_blocks(chapter, 1200, NULL, 0, r, 10));
    assert(!book_layout_complete() && book_layout_page_count() == 2 && measured_codepoints < 40);
    drawn[0] = 0;
    book_layout_draw_page(&fb, 0, r, 10);
    assert(strlen(drawn) == 8);
    while (!book_layout_complete()) {
        size_t before = book_layout_page_count();
        assert(book_layout_extend(2));
        assert(book_layout_page_count() > before && book_layout_page_count() <= before + 2);
    }
    assert(book_layout_page_count() == pages);
    for (size_t i=0; i<pages; ++i) assert(book_layout_page_start_offset(i) == offsets[i]);
    assert(book_layout_page_for_offset(501) == 62);
    // 图片单块换页，文字不丢失；适配后仍在正文边界内。
    // Images cross pages atomically without losing text and fit inside the body.
    uint8_t pixels[2000]; memset(pixels, 128, sizeof(pixels));
    blk_t illustrated[] = {{.offset=0,.len=1}, {.offset=2,.len=3,.image=pixels,.image_width=40,.image_height=50}, {.offset=6,.len=1}};
    r = (EpdRect){0,0,20,30};
    assert(book_layout_build_blocks("a\nimg\nb", 7, illustrated, 3, r, 10));
    assert(book_layout_page_count() == 3);
    assert(book_layout_page_start_offset(1) == 2 && book_layout_page_start_offset(2) == 6);
    drawn[0] = 0; image_pixels = 0;
    for (size_t i=0; i<3; ++i) book_layout_draw_page(&fb,i,r,10);
    assert(!strcmp(drawn,"ab") && image_pixels == 500);
    // 占位命中与绘图边界一致，不读取像素；重复提示不改变文本偏移。
    // Placeholder hits match drawing bounds without loading pixels; repeat hints preserve text offsets.
    illustrated[1].image = NULL; illustrated[1].image_src = "cover.jpg";
    r = (EpdRect){40,24,604,1000};
    assert(book_layout_build_blocks("a\nimg\nb", 7, illustrated, 3, r, 48));
    size_t before_pages = book_layout_page_count();
    size_t before_offset = book_layout_page_start_offset(0);
    EpdRect hit;
    assert(book_layout_image_at(0,r,100,150,&hit)==1);
    assert(hit.x==48 && hit.y>=r.y && hit.y+hit.height<=r.y+r.height);
    assert(book_layout_image_at(0,r,hit.x,hit.y,NULL)==1);
    assert(book_layout_image_at(0,r,hit.x+hit.width-1,hit.y+hit.height-1,NULL)==1);
    assert(book_layout_image_at(0,r,hit.x-1,hit.y,NULL)==SIZE_MAX);
    assert(book_layout_image_at(0,r,hit.x+hit.width,hit.y,NULL)==SIZE_MAX);
    assert(book_layout_image_at(0,r,hit.x,hit.y+hit.height,NULL)==SIZE_MAX);
    assert(book_layout_image_at(1,r,100,150,NULL)==SIZE_MAX);
    assert(book_layout_image_at(0,(EpdRect){40,24,600,1000},100,150,NULL)==SIZE_MAX);
    drawn[0]=0;book_layout_draw_page(&fb,0,r,48);
    assert(strstr(drawn,"点击查看图片"));
    illustrated[1].image_repeated=true; illustrated[1].image_first_chapter=7;
    drawn[0]=0;book_layout_draw_page(&fb,0,r,48);
    assert(strstr(drawn,"重复图片") && strstr(drawn,"第 8 节"));
    illustrated[1].image_title=true;
    drawn[0]=0;book_layout_draw_page(&fb,0,r,48);
    assert(strstr(drawn,"标题图") && !strstr(drawn,"重复") && !strstr(drawn,"第 8 节"));
    assert(book_layout_page_count()==before_pages && book_layout_page_start_offset(0)==before_offset);
    // 字符命中与实际对齐一致；空白、行距和图片不选成文字。/ Glyph hits follow alignment; whitespace, leading and images never select text.
    assert(book_layout_text_at(0,r,342,150)==SIZE_MAX);
    book_layout_set_indent(false);book_layout_set_leading(0);book_layout_set_align(0);
    r=(EpdRect){40,24,80,100};
    assert(book_layout_build("ab cd\nEF",8,r,10));
    assert(book_layout_text_at(0,r,44,25)==0 && book_layout_text_at(0,r,60,25)==SIZE_MAX);
    assert(book_layout_text_at(0,r,44,35)==SIZE_MAX && book_layout_text_at(0,r,44,46)==6);
    book_layout_set_align(1);
    assert(book_layout_text_at(0,r,55,25)==0 && book_layout_text_at(0,r,44,25)==SIZE_MAX);
    book_layout_set_align(2);r.width=85;
    assert(book_layout_build("WiWiWiWiWiWi",12,r,10));
    assert(book_layout_text_at(0,r,55,25)==SIZE_MAX && book_layout_text_at(0,r,56,25)==1);
    assert(book_layout_text_at(0,(EpdRect){INT_MAX,24,85,100},INT_MAX,25)==SIZE_MAX);
    // 辅助线开关改变分页；稳定网格的段距、标题与命中共用相同位置。
    // Guide activation repaginates; grid paragraph gaps, headings and hits share positions.
    book_layout_set_align(0); book_layout_set_guide_origin(24); book_layout_set_paragraph(0);
    r=(EpdRect){40,24,200,240};
    const char rows[]="a\nb\nc\nd";
    assert(book_layout_build(rows,strlen(rows),r,36));
    assert(book_layout_page_count()==2 && book_layout_page_start_offset(1)==6);
    drawn[0]=0; book_layout_draw_page(&fb,0,r,36);
    assert(first_draw_y==60 && last_draw_y==204);
    book_layout_set_guide(1);
    assert(book_layout_build(rows,strlen(rows),r,36));
    assert(book_layout_page_count()==1 && book_layout_page_for_offset(6)==0);
    drawn[0]=0; fill_count=0; book_layout_draw_page(&fb,0,r,36);
    assert(first_draw_y==60 && last_draw_y==222 && fill_count==4);
    for(size_t i=0;i<4;++i) assert(filled[i].y==75+(int)i*54 && filled[i].height==2 && fill_colors[i]==0xB0);
    assert(book_layout_text_at(0,r,44,25)==0 && book_layout_text_at(0,r,44,79)==2);
    assert(book_layout_text_at(0,r,44,75)==SIZE_MAX);
    book_layout_set_paragraph(1);
    assert(book_layout_build(rows,strlen(rows),r,36));
    assert(book_layout_page_count()==2 && book_layout_page_start_offset(1)==4);
    drawn[0]=0; fill_count=0; book_layout_draw_page(&fb,0,r,36);
    assert(first_draw_y==60 && last_draw_y==168 && fill_count==2);
    const char title[]="T\nb";
    blk_t titled[]={{.offset=0,.len=1,.heading=true},{.offset=2,.len=1}};
    book_layout_set_paragraph(0);
    assert(book_layout_build_blocks(title,strlen(title),titled,2,r,36));
    drawn[0]=0; fill_count=0; book_layout_draw_page(&fb,0,r,36);
    assert(first_draw_px==44 && first_draw_y==68 && last_draw_y==168);
    assert(filled[0].y==75 && filled[1].y==183);
    // 缺字提示提高正文顶部，但保留24px原点，跨章规则相位一致。
    // A font notice raises the body top while the 24px origin retains cross-chapter rule phase.
    r=(EpdRect){40,88,200,176};
    assert(book_layout_build(rows,strlen(rows),r,36));
    drawn[0]=0; fill_count=0; book_layout_draw_page(&fb,0,r,36);
    assert(first_draw_y==168 && last_draw_y==222 && filled[0].y==183 && filled[1].y==237);
    assert(book_layout_text_at(0,r,44,100)==SIZE_MAX && book_layout_text_at(0,r,44,133)==0);
    assert(book_layout_text_at(0,r,44,187)==2 && book_layout_text_at(0,r,44,183)==SIZE_MAX);
    assert(book_layout_page_for_offset(6)==1 && book_layout_page_start_offset(1)==4);
    size_t grid_pages=book_layout_page_count(), grid_offset=book_layout_page_start_offset(1);
    book_layout_set_guide(2); book_layout_set_guide_contrast(true);
    drawn[0]=0; fill_count=0; book_layout_draw_page(&fb,0,r,36);
    assert(book_layout_page_count()==grid_pages && book_layout_page_start_offset(1)==grid_offset);
    assert(fill_count && fill_colors[0]==0 && filled[0].y==183);
    book_layout_set_night(true); drawn[0]=0; fill_count=0; book_layout_draw_page(&fb,1,r,36);
    assert(fill_count>1 && fill_colors[0]==0 && filled[0].height==r.height);
    for(size_t i=1;i<fill_count;++i) assert(fill_colors[i]==0xF0 && filled[i].height==2);
    book_layout_set_night(false); book_layout_set_guide_contrast(false); book_layout_set_guide(0);
    drawn[0]=0; book_layout_draw_page(&fb,0,r,36);
    assert(!drawn[0]);
    assert(book_layout_build(rows,strlen(rows),r,36));
    drawn[0]=0; book_layout_draw_page(&fb,0,r,36);
    assert(first_draw_y==124 && last_draw_y==196);
    // 含图章节无网格化：开线前后图片与文字位置一致，命中仍按实际占位。
    // Illustration chapters retain positions across guide activation and hits follow actual placeholders.
    r=(EpdRect){40,24,200,240}; illustrated[1].image=pixels; illustrated[1].image_src=NULL;
    assert(book_layout_build_blocks("a\nimg\nb",7,illustrated,3,r,36));
    drawn[0]=0; book_layout_draw_page(&fb,0,r,36); int image_last=last_draw_y;
    book_layout_set_guide(1);
    assert(book_layout_build_blocks("a\nimg\nb",7,illustrated,3,r,36));
    drawn[0]=0; fill_count=0; book_layout_draw_page(&fb,0,r,36);
    assert(last_draw_y==image_last && filled[0].y==68);
    assert(!book_layout_build("a",1,(EpdRect){40,88,200,54},36));
    // 上一夹具含图，纯文字网格不可放进不足一槽的顶部余量。
    // The previous fixture had images; a text grid cannot fit an incomplete slot after top padding.
    book_layout_set_guide(0);
    book_layout_free();
    puts("book_layout_host_test: PASS");
}
