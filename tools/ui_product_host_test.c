/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 * 中文：产品文字宽度、UTF-8 省略和能力降级命中，不运行硬件。
 * English: Product text widths, UTF-8 ellipsizing and capability-aware hit testing, without hardware.
 */
#include "epdiy.h"
#include "ui_product.h"
#include "book_store.h"
#include "app_registry.h"
#include "ttf_font.h"
#include "book_entry.h"
#include "ui_gesture.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static app_desc_t s_library, s_transfer;
static app_desc_t s_roots[3];
static bool s_has_transfer = true;
static unsigned s_lines;
static char s_line[260];
static int s_width;
static bool s_footer_capture;
static struct { int x, y, px, width; enum EpdFontFlags align; char text[260]; } s_footer[4];
static unsigned s_footer_count;
const app_desc_t* app_by_id(os_app_id_t id) {
    if (id == OS_APP_LIBRARY) return &s_library;
    if (id == OS_APP_TRANSFER && s_has_transfer) return &s_transfer;
    if (id == OS_APP_HOME) return &s_roots[0];
    if (id == OS_APP_TODAY) return &s_roots[1];
    if (id == OS_APP_SETTINGS) return &s_roots[2];
    return NULL;
}
bool ttf_font_prepare_text_px(int px,const char* text) {(void)px;(void)text;return true;}
void epd_draw_line(int x0,int y0,int x1,int y1,uint8_t color,uint8_t* fb) {(void)x0;(void)y0;(void)x1;(void)y1;(void)color;(void)fb;}
void ui_draw_selected_round_rect(uint8_t* fb,EpdRect rect,int radius) {(void)fb;(void)rect;(void)radius;}
int ttf_text_width_px(int px, const char* text) {
    int count = 0;
    for (const unsigned char* p = (const unsigned char*)text; *p; ++p)
        if ((*p & 0xc0) != 0x80) ++count;
    return count * px;
}
static bool s_lock_capture, s_bad_stamp;
static struct {int x,y,px;char text[5];} s_quote_glyphs[384];
static unsigned s_quote_glyph_count;
// 来源列走灰色墨 4；单独捕获以断言“出自-书名·章节名”确实落在侧栏。/ The source column
// uses gray ink 4; capture it separately to assert “出自-书名·章节名” lands in the side strip.
static struct {int x,y;char text[5];} s_source_glyphs[48];
static unsigned s_source_glyph_count;
void ui_text(uint8_t* fb, int x, int y, int px, const char* text, enum EpdFontFlags align, bool inverted) {
    (void)fb; (void)x; (void)y; (void)align; (void)inverted;
    if (s_lock_capture && !strcmp(text,"页")) s_bad_stamp = true;
    assert(ttf_text_width_px(px, text) <= s_width);
    ++s_lines; snprintf(s_line, sizeof(s_line), "%s", text);
    if (s_footer_capture && y < 1192) {
        assert(s_footer_count < 4);
        s_footer[s_footer_count].x = x; s_footer[s_footer_count].y = y;
        s_footer[s_footer_count].px = px; s_footer[s_footer_count].align = align;
        s_footer[s_footer_count].width = ttf_text_width_px(px, text);
        snprintf(s_footer[s_footer_count++].text, sizeof(s_footer[0].text), "%s", text);
    }
    // 省略必须保留完整的中文字符。/ Ellipsizing must retain complete Chinese characters.
    for (const unsigned char* p = (const unsigned char*)text; *p; ++p) {
        if (*p >= 0xe0 && *p <= 0xef) { assert((p[1] & 0xc0) == 0x80 && (p[2] & 0xc0) == 0x80); p += 2; }
    }
}
EpdRect ui_bar_rect(int col, int cols) { int w = (508 - (cols - 1) * 12) / cols; return (EpdRect){40 + col * (w + 12), 1096, w, 96}; }
int ui_content_width(void) { return 604; }
int ui_content_right(void) { return 644; }
EpdRect ui_row_rect(int i, int n, int y, int h) { int w=(604-(n-1)*12)/n; return (EpdRect){40+i*(w+12),y,w,h}; }
EpdRect ui_grid_rect(int i, int n, int row, int y, int h) { EpdRect r = ui_row_rect(i, n, y + row * (h + 12), h); return r; }
void ui_clear_rect_fast(uint8_t* fb, EpdRect r) { (void)fb; (void)r; }
void ui_hairline(uint8_t* fb, int y, int x, int w, uint8_t c) { (void)fb; (void)y; (void)x; (void)w; (void)c; }
void epd_fill_rect(EpdRect r, uint8_t c, uint8_t* fb) { (void)fb; (void)r; (void)c; }
void epd_draw_rect(EpdRect r, uint8_t c, uint8_t* fb) { (void)fb; (void)r; (void)c; }
void ui_draw_pressed_round_rect(uint8_t* fb, EpdRect r, int radius) { (void)fb; (void)r; (void)radius; }
void ui_draw_round_rect(uint8_t* fb, EpdRect r, int radius, uint8_t c) { (void)fb; (void)r; (void)radius; (void)c; }
int ttf_ascender_px(int px) { return px; }
static unsigned vertical_glyphs;
void ttf_draw_text_px(uint8_t* fb,int x,int y,int px,const char* text,enum EpdFontFlags align,uint8_t fg,uint8_t bg) {
    (void)fb;(void)align;(void)bg;
    assert(x>=0&&x<684&&y>=px&&y<1216&&fg<=15&&bg==UI_INK_WHITE);++vertical_glyphs;
    if (s_lock_capture && fg == UI_INK_BLACK) {
        assert(s_quote_glyph_count < 384);
        unsigned i = s_quote_glyph_count++;
        s_quote_glyphs[i].x=x;s_quote_glyphs[i].y=y;s_quote_glyphs[i].px=px;
        snprintf(s_quote_glyphs[i].text,sizeof(s_quote_glyphs[i].text),"%s",text);
    }
    if (s_lock_capture && fg == 4) {
        assert(s_source_glyph_count < 48);
        unsigned i = s_source_glyph_count++;
        s_source_glyphs[i].x=x;s_source_glyphs[i].y=y;
        snprintf(s_source_glyphs[i].text,sizeof(s_source_glyphs[i].text),"%s",text);
    }
}
void ui_text_vc(uint8_t* fb, int x, int y, int px, const char* t, enum EpdFontFlags a, bool inv) { ui_text(fb, x, y, px, t, a, inv); }
bool ui_rect_hit(EpdRect r, uint16_t x, uint16_t y) { return x >= r.x && x < r.x + r.width && y >= r.y && y < r.y + r.height; }
int ui_bar_hit(uint16_t x, uint16_t y, int cols) {
    for (int i = 0; i < cols; ++i) if (ui_rect_hit(ui_bar_rect(i, cols), x, y)) return i;
    return -1;
}
int book_store_read_roots(book_store_root_t out[2], int* n) { (void)out; *n = 0; return 0; }

void ui_draw_button(uint8_t* fb, EpdRect r, const char* text, bool on) { (void)fb; (void)r; (void)text; (void)on; }
void ui_clear_page(uint8_t* fb) { (void)fb; }
void ui_fill_round_rect(uint8_t* fb, EpdRect r, int radius, uint8_t c) { (void)fb; (void)r; (void)radius; (void)c; }
void epd_draw_pixel(int x, int y, uint8_t c, uint8_t* fb) { (void)x; (void)y; (void)c; (void)fb; }
void ui_draw_menu_handle(uint8_t* fb, bool open) { (void)fb; (void)open; }
int main(void) {
    s_width = 120;
    ui_product_title(NULL, (EpdRect){0, 0, s_width, 100}, "中文长标题测试", 30, 2);
    assert(s_lines == 2 && !strcmp(s_line, "题测试"));
    s_lines = 0;
    ui_product_title(NULL, (EpdRect){0, 0, s_width, 50}, "中文长标题测试", 30, 1);
    assert(s_lines == 1 && !strcmp(s_line, "中文长…"));
    s_lines = 0;
    ui_product_title(NULL, (EpdRect){0, 0, 120, 100}, "", 30, 2);
    ui_product_title(NULL, (EpdRect){0, 0, 120, 10}, "标题", 30, 2);
    assert(!s_lines);
    assert(ui_product_home_bar_hit(100, 1140, true) == OS_APP_LIBRARY);
    assert(ui_product_home_bar_hit(400, 1140, true) == OS_APP_TRANSFER);
    assert(ui_product_home_bar_hit(400, 1140, false) == OS_APP_NONE);
    s_has_transfer = false;
    assert(ui_product_home_bar_hit(400, 1140, true) == OS_APP_NONE);
    assert(ui_product_home_bar_hit(600, 1140, true) == OS_APP_NONE);
    assert(ui_product_home_bar_hit(100, 100, true) == OS_APP_NONE);
    ui_product_home_bar(NULL, false);
    assert(ui_product_root_hit(90, 1140) == OS_APP_HOME);
    assert(ui_product_root_hit(220, 1140) == OS_APP_LIBRARY);
    assert(ui_product_root_hit(350, 1140) == OS_APP_TODAY);
    assert(ui_product_root_hit(480, 1140) == OS_APP_SETTINGS);
    assert(ui_product_root_hit(612, 1140) == OS_APP_NONE);
    app_ctx_t ctx = {0};
    const os_app_id_t roots[] = {OS_APP_HOME, OS_APP_LIBRARY, OS_APP_TODAY, OS_APP_SETTINGS};
    for (int i=0;i<4;++i) {
        ui_gesture_event_t press={.type=UI_GESTURE_PRESS,.x=90+130*i,.y=1140};
        ctx.request_app=NULL;
        assert(ui_product_root_press(&ctx,&press) && ctx.request_app==app_by_id(roots[i]));
    }
    assert(ui_product_navigate(&ctx, OS_APP_LIBRARY) && ctx.request_app == &s_library);
    // 排队中的请求被新请求覆盖，导航不再拒绝。/ A queued request is overwritten; navigation no longer bounces.
    assert(ui_product_navigate(&ctx, OS_APP_LIBRARY) && ctx.request_app == &s_library);
    book_entry_request_t entry;
    assert(book_entry_take(&entry) && entry.kind == BOOK_ENTRY_SHELF);
    book_entry_finish(BOOK_ENTRY_SHELF_READY);
    assert(ui_product_shelf_rect(2).y + ui_product_shelf_rect(2).height < ui_product_shelf_nav_rect(0).y);
    for(int i=0;i<6;++i){
        EpdRect r=ui_product_shelf_grid_rect(i);
        assert(r.width>=180 && r.height>=334 && r.y+r.height<ui_product_shelf_nav_rect(0).y);
        assert(r.x>=40 && r.x+r.width<=644);
    }

    s_width = 508; s_footer_capture = true;
    ui_product_reader_chrome(NULL, "这是一个很长很长的中文书名用于检查省略", 125, 2048, 67, false, "10:13 · 78%", true, NULL, 0);
    assert(s_footer_count == 3);
    assert(s_footer[0].y == 1104 && s_footer[0].px == 28 && strstr(s_footer[0].text, "…"));
    assert(s_footer[1].y == 1150 && s_footer[2].y == 1150);
    assert(s_footer[1].x + s_footer[1].width + 20 <= s_footer[2].x - s_footer[2].width);
    assert(!strcmp(s_footer[2].text, "125/2048 · 67%"));
    s_footer_count = 0;
    ui_product_reader_chrome(NULL, "书名", 5, 0, 0, false, "", false, NULL, 0);
    assert(s_footer_count == 2 && !strcmp(s_footer[1].text, "5/…"));
    s_footer_count = 0;
    ui_product_reader_chrome(NULL, "书名", 99999, 99999, 100, false, "10:13 · 100%", true, NULL, 0);
    // 章节刻度：千分比位置画到轨道上，越界跳过。/ Chapter ticks draw at permille positions; out-of-range entries skip.
    static const uint16_t ticks[3] = {0, 500, 999};
    ui_product_reader_chrome(NULL, "书名", 5, 100, 50, false, "", true, ticks, 3);
    assert(s_footer[s_footer_count - 1].x == 548);
    s_footer_capture = false;
    // 实际竖排路径保持字形在屏内，并使用真实农历换算；真字体另由原生预览验证。
    // The real vertical path keeps glyphs in bounds and uses actual lunar conversion; native previews verify real fonts separately.
    os_time_info_t poster_time={.state=OS_TIME_VALID,.year=2026,.month=10,.day=15,.hour=14,.minute=45};
    ui_product_excerpt_lock(NULL,"满地都是便士，他却抬头看见了月亮。","/flash/books/月亮与六便士.txt",NULL,"短按开机",&poster_time,42,18,true,false);
    assert(vertical_glyphs>20);
    s_width=604;s_lock_capture=true;s_bad_stamp=false;s_quote_glyph_count=0;s_source_glyph_count=0;
    ui_product_excerpt_lock(NULL,"满地都是便士，他却抬头看见了月亮。","/flash/books/月亮与六便士.txt",NULL,"短按开机",&poster_time,42,18,true,false);
    assert(!s_bad_stamp && s_quote_glyph_count==17);
    assert(!strcmp(s_quote_glyphs[7].text,"他"));
    assert(s_quote_glyphs[7].x<s_quote_glyphs[0].x && s_quote_glyphs[7].y==s_quote_glyphs[0].y);
    assert(s_quote_glyphs[0].y<490);
    for(int preview=0;preview<2;++preview) {
        s_quote_glyph_count=0;s_source_glyph_count=0;
        ui_product_excerpt_lock(NULL,"雨过天晴。","/flash/books/短篇.txt","第三章 雨夜","短按开机",&poster_time,42,18,true,preview);
        assert(s_quote_glyph_count==5);
        assert(s_quote_glyphs[0].x>=340 && s_quote_glyphs[0].x<=405);
        assert(s_quote_glyphs[0].px==60);
    }
    char maximum[382];for(int i=0;i<127;++i)memcpy(maximum+i*3,"字",3);maximum[381]=0;
    for(int preview=0;preview<2;++preview){
        s_quote_glyph_count=0;s_source_glyph_count=0;
        ui_product_excerpt_lock(NULL,maximum,"/flash/books/长书名长书名长书名长书名长书名.txt",NULL,"短按开机",&poster_time,42,18,true,preview);
        assert(s_quote_glyph_count==127);
        for(unsigned i=0;i<s_quote_glyph_count;++i)assert(s_quote_glyphs[i].x>=40&&s_quote_glyphs[i].y<1036);
    }
    // 章节来源：短句留出侧栏时，“出自-书名·章节名”按竖排列落在屏内；列数随正文高度收缩。
    // Chapter sources: with a short quote leaving the side column, “出自-书名·章节名” lands
    // on-screen vertically, its column count shrinking with the body height.
    {
        s_quote_glyph_count=0;s_source_glyph_count=0;
        ui_product_excerpt_lock(NULL,"雨过天晴。","/flash/books/短篇.txt","很长的章节标题来验证来源列边界","短按开机",&poster_time,42,18,true,false);
        for(unsigned i=0;i<s_quote_glyph_count;++i)assert(s_quote_glyphs[i].x>=40&&s_quote_glyphs[i].y<1036);
        assert(s_source_glyph_count>=6 && s_source_glyph_count<=21);
        assert(!strcmp(s_source_glyphs[0].text,"出")&&!strcmp(s_source_glyphs[1].text,"自")&&!strcmp(s_source_glyphs[2].text,"-"));
        assert(!strcmp(s_source_glyphs[3].text,"短")&&!strcmp(s_source_glyphs[5].text,"·"));
        for(unsigned i=0;i<s_source_glyph_count;++i)assert(s_source_glyphs[i].x>=40&&s_source_glyphs[i].y>=224&&s_source_glyphs[i].y<1036);
    }
    s_lock_capture=false;
    puts("excerpt typography: no stamp, punctuation-aware balanced columns, attached source and 127-glyph preview/device bounds PASS");

    puts("ui_product: bounded UTF-8 text and disabled/missing navigation passed");
    return 0;
}
