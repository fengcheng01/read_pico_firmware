/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 *
 * 中文：借鉴阅读演示的逐码点折行，建立 PSRAM 页表并绘制章节。
 * English: Adapt the reading demo's codepoint wrapping into PSRAM chapter pagination.
 *
 * 冻结：原文由调用方持有；字体测量和绘制必须由调用方串行化。
 * 为尽早进入阅读，支持分批分页；正文插图在分页前加载；失败图片绘制可点击占位，不在绘图时读取资源或解码。
 * Frozen: Caller owns source text and serializes all font measurement and drawing.
 * Incremental pagination enables early reading; inline images load before pagination; failed images show retry placeholders; never read or decode resources while drawing.
 */
#include "book_layout.h"
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "esp_heap_caps.h"
#include "ttf_font.h"

#define PAGE_MAX 4096u
#define PSRAM_CAPS (MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)

static const char* s_text;
static size_t s_len;
static size_t* s_pages;
static size_t s_count;
static size_t s_capacity;
static char* s_line;
static EpdRect s_rect;
static int s_px;
static size_t s_scan;
static int64_t s_used;
static bool s_complete;
// 行距加成与夜间反色是模块状态：build 与 draw 共用，调用方在 build 前设置。
// Leading bonus and night inversion are module state shared by build and draw; callers set them before building.
static int s_leading_pct;
static bool s_night;
// 首行缩进/段落间距影响 build 与 draw；辅助线只影响 draw，三者都在 build 前设置。
// Indent and paragraph gap affect builds and draws; the guide rule is draw-only; set all before building.
static bool s_indent_on = true;
static int s_para_tier;
static int s_guide_style;
static int s_align;

static const blk_t* s_blocks;
static size_t s_block_count;

// 块表是有序字节区间，二分查找当前行样式。/ Blocks are ordered byte ranges; binary-search the line style.
static const blk_t* block_at(size_t off) {
    if (!s_block_count) return NULL;
    size_t lo = 0, hi = s_block_count;
    while (lo + 1 < hi) {
        size_t mid = lo + (hi - lo) / 2;
        if (s_blocks[mid].offset <= off) lo = mid;
        else hi = mid;
    }
    return &s_blocks[lo];
}
// 插图等比适配正文区域，不放大；整块换页，不切断图片。
// Fit illustrations proportionally without upscaling; move whole images across page boundaries.
static void image_size(const blk_t* block, int* width, int* height) {
    *width = block->image_width; *height = block->image_height;
    if (*width > s_rect.width) { *height = (int)((int64_t)*height * s_rect.width / *width); *width = s_rect.width; }
    if (*height > s_rect.height) { *width = (int)((int64_t)*width * s_rect.height / *height); *height = s_rect.height; }
    if (*width < 1) *width = 1;
    if (*height < 1) *height = 1;
}
static int row_height(const blk_t* block, int px) {
    if (block && block->image) { int width, height; image_size(block, &width, &height); return height; }
    if (block && block->image_src) return 2 * s_px < s_rect.height ? 2 * s_px : s_rect.height;
    return px + px / 2 + px * s_leading_pct / 100;
}
static EpdRect placeholder_rect(EpdRect body, int top, int height) {
    int pad = body.width > 24 && height > 16 ? 8 : 0;
    return (EpdRect){body.x + pad, body.y + top + pad / 2, body.width - 2 * pad, height - pad};
}
static void draw_placeholder(uint8_t* fb, const blk_t* block, EpdRect rect) {
    uint8_t fg = s_night ? 255 : 0;
    for (int x = rect.x; x < rect.x + rect.width; ++x) {
        epd_draw_pixel(x, rect.y, fg, fb); epd_draw_pixel(x, rect.y + rect.height - 1, fg, fb);
    }
    for (int y = rect.y; y < rect.y + rect.height; ++y) {
        epd_draw_pixel(rect.x, y, fg, fb); epd_draw_pixel(rect.x + rect.width - 1, y, fg, fb);
    }
    int px = s_px < 32 ? s_px : 32;
    bool repeat = block->image_repeated && !block->image_title;
    const char* label = block->image_title ? "标题图 · 点击查看" : repeat ? "重复图片 · 点击查看" : "点击查看图片";
    if (ttf_text_width_px(px, label) > rect.width - 16) return;
    int lines = repeat ? 2 : 1;
    int top = rect.y + (rect.height - lines * (px + 4)) / 2;
    ttf_draw_text_px(fb, rect.x + rect.width / 2, top + ttf_ascender_px(px), px,
                     label, EPD_DRAW_ALIGN_CENTER, s_night ? 15 : 0, s_night ? 0 : 15);
    if (repeat) {
        char origin[64]; snprintf(origin, sizeof(origin), "已读最早：第 %u 节", (unsigned)block->image_first_chapter + 1);
        int small = px < 24 ? px : 24;
        if (ttf_text_width_px(small, origin) <= rect.width - 16)
            ttf_draw_text_px(fb, rect.x + rect.width / 2, top + px + 4 + ttf_ascender_px(small), small,
                             origin, EPD_DRAW_ALIGN_CENTER, s_night ? 15 : 0, s_night ? 0 : 15);
    }
}
// 拒绝截断、过长编码、代理项和嵌入零字节。/ Reject truncation, overlong encodings, surrogates and embedded NUL.
static size_t codepoint_size(const char* text, size_t remaining) {
    if (!remaining) return 0;
    const unsigned char* p = (const unsigned char*)text;
    if (p[0] > 0 && p[0] < 0x80) return 1;
    size_t n = p[0] >= 0xc2 && p[0] <= 0xdf ? 2 :
               p[0] >= 0xe0 && p[0] <= 0xef ? 3 :
               p[0] >= 0xf0 && p[0] <= 0xf4 ? 4 : 0;
    if (!n || n > remaining) return 0;
    for (size_t i = 1; i < n; i++) if ((p[i] & 0xc0) != 0x80) return 0;
    if ((p[0] == 0xe0 && p[1] < 0xa0) || (p[0] == 0xed && p[1] >= 0xa0) ||
        (p[0] == 0xf0 && p[1] < 0x90) || (p[0] == 0xf4 && p[1] >= 0x90)) return 0;
    return n;
}

void book_layout_free(void) {
    free(s_pages);
    free(s_line);
    s_pages = NULL;
    s_line = NULL;
    s_text = NULL;
    s_count = s_capacity = s_len = 0;
    s_px = 0;
    s_scan = 0;
    s_used = 0;
    s_complete = false;
    s_blocks = NULL;
    s_block_count = 0;
}

static bool append_page(size_t off) {
    if (s_count == PAGE_MAX) return false;
    if (s_count == s_capacity) {
        size_t cap = s_capacity ? s_capacity * 2 : 16;
        size_t* pages = heap_caps_realloc(s_pages, cap * sizeof(*pages), PSRAM_CAPS);
        if (!pages) return false;
        s_pages = pages;
        s_capacity = cap;
    }
    s_pages[s_count++] = off;
    return true;
}

// 段首（前一字节是换行或章节开头）缩进两字符，标题不缩进。/ Paragraph starts (preceded by a newline or chapter head) indent two ems; headings do not.
static int line_indent(size_t off, bool heading) {
    if (!s_indent_on || heading) return 0;
    if (!off) return 2 * s_px;
    char prev = s_text[off - 1];
    return prev == '\n' || prev == '\r' ? 2 * s_px : 0;
}
// 段落间距：标题恒为半行，正文标准三分之一行、加大二分之一行。/ Paragraph gap: half a line for headings; body thirds standard and halves relaxed.
static int para_gap(int line_height, bool heading) {
    return line_height / (heading ? 2 : s_para_tier ? 2 : 3);
}
// 折行时保留原文字节位置；CRLF 算一个段落边界。/ Preserve source offsets while wrapping; CRLF is one paragraph boundary.
static bool take_line(size_t off, size_t* next, bool* paragraph_end, int* px, bool* heading) {
    const blk_t* block = block_at(off);
    *heading = block && block->heading;
    *px = s_px + (*heading ? 8 : 0);
    int64_t budget = s_rect.width - line_indent(off, *heading);
    size_t limit = block ? block->offset + block->len : s_len;
    size_t end = off;
    int64_t width = 0;
    s_line[0] = 0;
    *paragraph_end = false;
    if (block && (block->image || block->image_src)) {
        *heading = false;
        *next = limit + (limit < s_len && s_text[limit] == '\n');
        return *next > off;
    }
    while (end < limit && s_text[end] != '\r' && s_text[end] != '\n') {
        size_t n = codepoint_size(s_text + end, s_len - end);
        if (!n) return false;
        // 字体逐字取整后累加 advance；单字测量避免反复扫描整行前缀。
        // Font advances are rounded per glyph and summed; measure each glyph once instead of every prefix.
        char glyph[5];
        memcpy(glyph, s_text + end, n);
        glyph[n] = 0;
        int64_t candidate = width + ttf_text_width_px(*px, glyph);
        if (candidate < 0) return false;
        if (candidate > budget) {
            if (end == off) return false;
            break;
        }
        width = candidate;
        memcpy(s_line + end - off, glyph, n);
        s_line[end - off + n] = 0;
        end += n;
    }
    *next = end;
    if (end < s_len && (s_text[end] == '\r' || s_text[end] == '\n')) {
        *next = end + 1;
        if (s_text[end] == '\r' && *next < s_len && s_text[*next] == '\n') (*next)++;
        *paragraph_end = true;
    }
    return *next > off;
}

bool book_layout_build(const char* utf8, size_t len, EpdRect rect, int px) {
    return book_layout_build_blocks(utf8, len, NULL, 0, rect, px);
}

bool book_layout_build_blocks(const char* utf8, size_t len, const blk_t* blocks, size_t count, EpdRect rect, int px) {
    return book_layout_begin_blocks(utf8, len, blocks, count, rect, px) && book_layout_extend(PAGE_MAX);
}

bool book_layout_begin_blocks(const char* utf8, size_t len, const blk_t* blocks, size_t count, EpdRect rect, int px) {
    book_layout_free();
    if ((!utf8 && len) || len == SIZE_MAX || px <= 0 || px > INT_MAX / 3 ||
        rect.width <= 0 || rect.height <= 0 || rect.x < 0 || rect.y < 0 ||
        rect.x > INT_MAX - rect.width || rect.y > INT_MAX - rect.height) return false;
    int line_height = px + px / 2;
    if (line_height > rect.height) return false;
    for (size_t off = 0; off < len;) {
        size_t n = codepoint_size(utf8 + off, len - off);
        if (!n) return false;
        off += n;
    }
    if (count) {
        if (!blocks || count > HTML_TEXT_MAX_BLOCKS) return false;
        size_t expected = 0;
        for (size_t i = 0; i < count; ++i) {
            const blk_t* b = &blocks[i];
            if (b->image && (!b->image_width || !b->image_height)) return false;
            if (b->offset != expected || b->offset >= len || !b->len || b->len > len - b->offset ||
                ((unsigned char)utf8[b->offset] & 0xc0) == 0x80) return false;
            size_t end = b->offset + b->len;
            if (i + 1 < count) {
                if (end >= len || utf8[end] != '\n') return false;
                expected = end + 1;
            } else if (end != len) return false;
        }
    }
    s_blocks = blocks;
    s_block_count = count;
    s_text = utf8;
    s_len = len;
    s_px = px;
    s_rect = rect;
    s_line = heap_caps_malloc(len + 1, PSRAM_CAPS);
    if (!s_line || !append_page(0)) goto fail;
    return book_layout_extend(2);
fail:
    book_layout_free();
    return false;
}

bool book_layout_extend(size_t pages) {
    if (!s_count) return false;
    if (!pages || s_complete) return true;
    size_t target = pages > PAGE_MAX - book_layout_page_count() ? PAGE_MAX : book_layout_page_count() + pages;
    while (s_scan < s_len) {
        size_t next;
        bool paragraph_end, heading;
        int line_px;
        if (!take_line(s_scan, &next, &paragraph_end, &line_px, &heading)) goto fail;
        const blk_t* block = block_at(s_scan);
        int line_height = row_height(block, line_px);
        if (line_height > s_rect.height) goto fail;
        if (s_used + line_height > s_rect.height) {
            if (!append_page(s_scan)) goto fail;
            s_used = 0;
            if (book_layout_page_count() >= target) return true;
        }
        s_used += line_height;
        if (paragraph_end) s_used += para_gap(line_height, heading);
        s_scan = next;
    }
    s_complete = true;
    return true;
fail:
    book_layout_free();
    return false;
}

size_t book_layout_page_count(void) { return s_count ? s_count - !s_complete : 0; }
bool book_layout_complete(void) { return s_complete; }

size_t book_layout_page_start_offset(size_t page) {
    return page < s_count ? s_pages[page] : s_len;
}

size_t book_layout_page_for_offset(size_t off) {
    size_t lo = 0, hi = book_layout_page_count();
    while (lo + 1 < hi) {
        size_t mid = lo + (hi - lo) / 2;
        if (s_pages[mid] <= off) lo = mid;
        else hi = mid;
    }
    return lo;
}

void book_layout_draw_page(uint8_t* fb, size_t page, EpdRect rect, int px) {
    if (!fb || page >= book_layout_page_count() || px != s_px || rect.width != s_rect.width ||
        rect.height != s_rect.height || rect.x < 0 || rect.y < 0 ||
        rect.x > INT_MAX - rect.width || rect.y > INT_MAX - rect.height) return;
    // 夜间模式整块反色：黑底、白字、图片灰度翻转。/ Night inverts the whole body: black ground, white text, flipped image grays.
    uint8_t fg = s_night ? 15 : 0, bg = s_night ? 0 : 15;
    if (s_night) epd_fill_rect(rect, 0, fb);
    size_t off = s_pages[page];
    size_t end = page + 1 < s_count ? s_pages[page + 1] : s_len;
    int64_t used = 0;
    while (off < end) {
        size_t next;
        bool paragraph_end, heading;
        int line_px;
        if (!take_line(off, &next, &paragraph_end, &line_px, &heading)) return;
        const blk_t* block = block_at(off);
        int line_height = row_height(block, line_px);
        int image_width = 0;
        if (block && block->image) image_size(block, &image_width, &line_height);
        if (used + line_height > rect.height) return;
        if (image_width) {
            int left = rect.x + (rect.width - image_width) / 2;
            for (int y = 0; y < line_height; ++y) for (int x = 0; x < image_width; ++x) {
                size_t src = (size_t)((int64_t)y * block->image_height / line_height) * block->image_width +
                             (size_t)((int64_t)x * block->image_width / image_width);
                epd_draw_pixel(left + x, rect.y + (int)used + y, s_night ? 255 - block->image[src] : block->image[src], fb);
            }
        } else if (block && block->image_src) {
            draw_placeholder(fb, block, placeholder_rect(rect, (int)used, line_height));
        } else if (s_line[0]) {
            int indent = line_indent(off, heading);
            int64_t width = ttf_text_width_px(line_px, s_line);
            // 对齐：居中整行居中；两端对齐只作用于段中行（段末行、标题与图片行保持左对齐）。
            // Alignment: center centers the whole line; justification covers mid-paragraph lines only.
            if (s_align == 1) {
                ttf_draw_text_px(fb, rect.x + indent + (rect.width - indent - (int)width) / 2,
                                 rect.y + (int)used + ttf_ascender_px(line_px), line_px,
                                 s_line, EPD_DRAW_ALIGN_LEFT, fg, bg);
            } else if (s_align == 2 && !paragraph_end && !heading && width < rect.width - indent) {
                // 两端对齐：逐字绘制并在字隙均摊余量；余量过小走整行绘制。
                // Justified: draw glyph by glyph spreading the slack; tiny slack keeps whole-line draws.
                int slack = rect.width - indent - (int)width;
                if (slack < line_px / 2) {
                    ttf_draw_text_px(fb, rect.x + indent, rect.y + (int)used + ttf_ascender_px(line_px),
                                     line_px, s_line, EPD_DRAW_ALIGN_LEFT, fg, bg);
                } else {
                    size_t glyphs = 0;
                    for (const char* q = s_line; *q; ++glyphs) {
                        size_t n = codepoint_size(q, strlen(q));
                        if (!n) break;
                        q += n;
                    }
                    int per_gap = glyphs > 1 ? slack / (int)(glyphs - 1) : 0;
                    int extra = glyphs > 1 ? slack % (int)(glyphs - 1) : 0;
                    int x = rect.x + indent;
                    for (const char* q = s_line; *q;) {
                        size_t n = codepoint_size(q, strlen(q));
                        if (!n) break;
                        char glyph[5];
                        memcpy(glyph, q, n);
                        glyph[n] = 0;
                        ttf_draw_text_px(fb, x, rect.y + (int)used + ttf_ascender_px(line_px), line_px,
                                         glyph, EPD_DRAW_ALIGN_LEFT, fg, bg);
                        x += (int)ttf_text_width_px(line_px, glyph) + per_gap + (extra-- > 0 ? 1 : 0);
                        q += n;
                    }
                }
            } else {
                ttf_draw_text_px(fb, rect.x + indent, rect.y + (int)used + ttf_ascender_px(line_px), line_px,
                                 s_line, EPD_DRAW_ALIGN_LEFT, fg, bg);
            }
            // 行辅助线：每行文字下方一条横线，实线整条、虚线 8/6 分段，随夜间换灰。
            // Guide rule: one rule under each text line, solid whole or dashed 8/6 segments, graying with night.
            if (s_guide_style) {
                int y = rect.y + (int)used + line_px + (line_height - line_px) / 2 - 1;
                uint8_t gray = s_night ? 0x50 : 0xB0;
                if (s_guide_style == 1) {
                    epd_fill_rect((EpdRect){rect.x, y, rect.width, 2}, gray, fb);
                } else {
                    for (int x = 0; x + 8 <= rect.width; x += 14)
                        epd_fill_rect((EpdRect){rect.x + x, y, 8, 2}, gray, fb);
                }
            }
        }
        used += line_height;
        if (paragraph_end) used += para_gap(line_height, heading);
        off = next;
    }
}
size_t book_layout_image_at(size_t page, EpdRect rect, int x, int y, EpdRect* hit) {
    if (page >= book_layout_page_count() || rect.width != s_rect.width || rect.height != s_rect.height ||
        rect.x < 0 || rect.y < 0 || rect.x > INT_MAX - rect.width || rect.y > INT_MAX - rect.height ||
        x < rect.x || y < rect.y || x >= rect.x + rect.width || y >= rect.y + rect.height) return SIZE_MAX;
    size_t off = s_pages[page], end = page + 1 < s_count ? s_pages[page + 1] : s_len;
    int64_t used = 0;
    while (off < end) {
        size_t next; bool paragraph_end, heading; int px;
        if (!take_line(off, &next, &paragraph_end, &px, &heading)) return SIZE_MAX;
        const blk_t* block = block_at(off);
        int height = row_height(block, px);
        if (used + height > rect.height) return SIZE_MAX;
        if (block && block->image_src && !block->image) {
            EpdRect area = placeholder_rect(rect, (int)used, height);
            if (x >= area.x && x < area.x + area.width && y >= area.y && y < area.y + area.height) {
                if (hit) *hit = area;
                return (size_t)(block - s_blocks);
            }
        }
        used += height;
        if (paragraph_end) used += para_gap(height, heading);
        off = next;
    }
    return SIZE_MAX;
}

// 行距加成作用于后续 build 与 draw；夜间反色只影响 draw，二者都须在 build 前设置。
// The leading bonus applies to later builds and draws; night inversion affects draws only; set both before building.
void book_layout_set_leading(int percent) {
    s_leading_pct = percent < 0 || percent > 60 ? 0 : percent;
}

void book_layout_set_night(bool on) {
    s_night = on;
}

void book_layout_set_indent(bool on) {
    s_indent_on = on;
}

void book_layout_set_paragraph(int tier) {
    s_para_tier = tier == 1 ? 1 : 0;
}

void book_layout_set_guide(int style) {
    s_guide_style = style == 1 || style == 2 ? style : 0;
}

void book_layout_set_align(int align) {
    s_align = align == 1 || align == 2 ? align : 0;
}
