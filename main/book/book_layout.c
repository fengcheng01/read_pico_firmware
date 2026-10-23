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
 * 用户确认旧线仅在转场穿字，修订辅助线只绘制的决定：纯文字开线时统一行网格、段距按整槽，关闭恢复原排版。
 * The user confirmed turn-only rule crossings, revising draw-only guides: text-only guides use one row grid and whole-slot paragraph gaps; disabling restores original layout.
 * 用户修订：TXT 连续换行只算一个段落边界，保留原始字节与进度锚点；EPUB 块排版不受影响。
 * User revision: consecutive TXT line breaks count as one paragraph boundary while preserving source bytes and progress anchors; EPUB block layout is unchanged.
 * 冻结：构建、绘制与命中共用同一网格；不得只固定辅助线而让文字跨线。图片章节保留原图文布局，不承诺跨页线位兼容。
 * Frozen: Build, drawing and hits share the grid; never fix rules while letting text cross them. Image chapters retain their original layout without cross-page rule compatibility.
 * 实机修订：槽底辅助线离本行文字过远（整段行距悬空），改为统一贴近文字——行底下方四分之一
 * 行距处；网格与图文回退同式，文字带依旧不与任何页的线位相交。
 * Device revision: slot-bottom rules sat a full leading away from their line; both grid
 * and illustration fallback now place rules a quarter of the leading below the text,
 * still outside every page's text band.
 * 实机修订：0.5.29细边阈值没有可见抗锯齿改善，撤回绘制选择，恢复原灰阶字形再由显示层生成黑白目标。
 * Device revision: The 0.5.29 fine-edge threshold gives no visible antialiasing benefit; withdraw its drawing switch and restore gray glyphs with display-owned binary targets.
 * 标点挤压（0.6.16）：禁则悬挂前先让行尾右标点串各收半个字宽（全角句读右半空白，
 * 只收紧步进不动字形位图）；断行、绘制与命中三端共用同一计数。
 * Punctuation compression (0.6.16): before hanging, the trailing closer run each gives up
 * half an em (full-width closers carry blank right halves; advances tighten, bitmaps stay);
 * wrapping, drawing and hits share the one count.
 */
#include "book_layout.h"
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "esp_heap_caps.h"
#include "book_hyphen.h"
#include "ttf_font.h"
#include "ui_dither.h"

#define PAGE_MAX 4096u
#define PSRAM_CAPS (MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)

static const char* s_text;
static size_t s_len;
static size_t* s_pages;
static size_t s_count;
static size_t s_capacity;
static char* s_line;
static size_t s_line_capacity;
// 最近一次 take_line 折行结果的行尾压半宽右标点数；绘制与命中紧随其后读取。
// Trailing closers halved by the latest take_line wrap; drawing and hits read it right after.
static int s_line_compressed;
// 行尾连字符是折行补画的合成字形（软连字符/TeX 断点），不属于原文；命中跳过。
// A line-end hyphen painted by the wrapper (soft-hyphen or TeX break) is synthetic;
// hit testing skips it.
static bool s_line_hyphen;
static uint32_t s_generation;
static EpdRect s_rect;
static int s_px;
static size_t s_scan;
static int64_t s_used;
static bool s_complete;
// 行距加成与夜间反色是模块状态：build 与 draw 共用，调用方在 build 前设置。
// Leading bonus and night inversion are module state shared by build and draw; callers set them before building.
static int s_leading_pct;
static bool s_night;
// 缩进、段距和辅助线开关影响分页，在建立布局前设置；实虚线及线色只影响绘制。
// Indent, paragraph gaps and guide activation affect pagination; set before building; rule style and color are draw-only.
static bool s_indent_on = true;
static int s_para_tier;
static int s_guide_style;
static int s_guide_origin;
static bool s_guide_binary;
static bool s_guide_grid;
static int s_grid_height;
static int s_grid_padding;
static int s_align;

static const blk_t* s_blocks;
static size_t s_block_count;

// 图片或失败占位保持原图文排版；纯文字章节才能共用跨页规则位置。
// Images and failed placeholders retain original illustration layout; only text-only chapters share rule positions across pages.
static bool guide_grid_requested(void) {
    if (!s_guide_style) return false;
    for (size_t i = 0; i < s_block_count; ++i)
        if (s_blocks[i].image || s_blocks[i].image_src) return false;
    return true;
}
static int grid_padding(EpdRect rect) {
    if (!s_guide_grid) return 0;
    int phase = (int)(((int64_t)rect.y - s_guide_origin) % s_grid_height);
    if (phase < 0) phase += s_grid_height;
    return phase ? s_grid_height - phase : 0;
}

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
    if (s_guide_grid) return s_grid_height;
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

// UTF-8 序列已通过长度校验，此处解出码点供禁则判断。/ Sequence lengths are validated; decode the codepoint for kinsoku.
static uint32_t codepoint_value(const char* text, size_t n) {
    const unsigned char* p = (const unsigned char*)text;
    uint32_t cp = p[0] & (n == 1 ? 0x7f : n == 2 ? 0x1f : n == 3 ? 0x0f : 0x07);
    for (size_t i = 1; i < n; i++) cp = (cp << 6) | (p[i] & 0x3f);
    return cp;
}

// 拉丁词成员：字母/数字/撇号/连字符/软连字符；折行把整词当作不可分单元。
// Latin word members: letters, digits, apostrophes, hyphens and soft hyphens; the
// wrapper treats a whole word as one unbreakable unit.
static bool latin_word_char(uint32_t cp) {
    return (cp >= 'a' && cp <= 'z') || (cp >= 'A' && cp <= 'Z') || (cp >= '0' && cp <= '9') ||
           cp == '\'' || cp == '-' || cp == 0x00AD;
}

// 中西文边界间隙：CJK 与 ASCII 字母数字相邻时各让 1/4 em；测量、绘制与命中同式。
// CJK-Latin boundary spacing: a quarter em between CJK and ASCII alphanumerics,
// shared by measurement, drawing and hit testing.
static bool script_cjk(uint32_t cp) {
    return (cp >= 0x2E80 && cp <= 0x9FFF) || (cp >= 0xAC00 && cp <= 0xD7AF) ||
           (cp >= 0xF900 && cp <= 0xFAFF) || (cp >= 0xFF00 && cp <= 0xFFEF) ||
           (cp >= 0x3000 && cp <= 0x30FF);
}
static bool script_latin(uint32_t cp) {
    return (cp >= '0' && cp <= '9') || (cp >= 'A' && cp <= 'Z') || (cp >= 'a' && cp <= 'z');
}
static int script_gap(uint32_t a, uint32_t b, int px) {
    return (script_cjk(a) && script_latin(b)) || (script_latin(a) && script_cjk(b)) ? px / 4 : 0;
}
// 行内边界间隙总和；与 take_line 的累计同式。/ Total boundary spacing on the line, matching take_line's accumulation.
static int64_t line_script_gaps(const char* line, size_t len, int px) {
    int64_t total = 0;
    uint32_t prev = 0;
    for (size_t i = 0; i < len;) {
        size_t n = codepoint_size(line + i, len - i);
        if (!n) break;
        uint32_t cp = codepoint_value(line + i, n);
        total += script_gap(prev, cp, px);
        prev = cp;
        i += n;
    }
    return total;
}

// 中文排版禁则：右标点不得出现在行首，左标点不得停留在行尾。
// CJK kinsoku: closing punctuation may not start a line; opening punctuation may not end one.
static bool prohibited_line_start(uint32_t cp) {
    switch (cp) {
        case 0x0021: case 0x0025: case 0x0029: case 0x002c: case 0x002e: case 0x003a:
        case 0x003b: case 0x003f: case 0x005d: case 0x007d:
        case 0x2019: case 0x201d: case 0x2026: case 0x3001: case 0x3002: case 0x3009:
        case 0x300b: case 0x300d: case 0x300f: case 0x3011: case 0x3015: case 0x3017:
        case 0x3019: case 0x301b: case 0xff01: case 0xff05: case 0xff09: case 0xff0c:
        case 0xff0e: case 0xff1a: case 0xff1b: case 0xff1f: case 0xff3d: case 0xff5d:
            return true;
        default: return false;
    }
}

static bool prohibited_line_end(uint32_t cp) {
    switch (cp) {
        case 0x0028: case 0x005b: case 0x007b: case 0x2018: case 0x201c: case 0x3008:
        case 0x300a: case 0x300c: case 0x300e: case 0x3010: case 0x3014: case 0x3016:
        case 0x3018: case 0x301a: case 0xff08: case 0xff3b: case 0xff5b:
            return true;
        default: return false;
    }
}

// 禁则悬挂行的字隙压缩量：向上取整、每隙最多 3px，与断行时的 squeeze 容许一致。
// Per-gap squeeze for hung lines: ceil division capped at 3px, matching the squeeze allowed while wrapping.
static int hang_compress(int overflow, int gaps) {
    if (overflow <= 0 || gaps <= 0) return 0;
    int per = overflow / gaps;
    if (per * gaps < overflow) ++per;
    return per > 3 ? 3 : per;
}

// 回退目标前行尾不得残留左标点：把 target 前紧邻的左标点串一并退到下一行；串首即行首时回退会清空行，宣告无效。
// A rollback must not leave openers at the line end: walk the opener run before target down too; a line-initial run head invalidates the rollback.
static size_t back_past_openers(size_t off, size_t target) {
    while (target > off) {
        // 从 target 向左走完一个码点，p 停在该字的起始字节。/ Walk one codepoint left of target; p lands on that glyph's lead byte.
        size_t p = target;
        while (p > off) {
            --p;
            if (((unsigned char)s_text[p] & 0xc0) != 0x80) break;
        }
        size_t n = codepoint_size(s_text + p, target - p);
        if (!n || !prohibited_line_end(codepoint_value(s_text + p, n))) break;
        if (p == off) return off;
        target = p;
    }
    return target;
}

void book_layout_free(void) {
    free(s_pages);
    free(s_line);
    s_pages = NULL;
    s_line = NULL; s_line_capacity = 0;
    s_line_compressed = 0;
    s_line_hyphen = false;
    if (!++s_generation) ++s_generation;
    s_text = NULL;
    s_count = s_capacity = s_len = 0;
    s_px = 0;
    s_scan = 0;
    s_used = 0;
    s_complete = false;
    s_guide_grid = false;
    s_grid_height = s_grid_padding = 0;
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
// 网格标题和加大段距空一槽，标准不额外空槽；无网格时保留原半行/三分之一行。
// Grid headings and relaxed paragraphs leave one slot, standard leaves none; nongrid spacing retains original halves/thirds.
static int para_gap(int line_height, bool heading) {
    if (s_guide_grid) return heading || s_para_tier ? s_grid_height : 0;
    return line_height / (heading ? 2 : s_para_tier ? 2 : 3);
}
// 折行时保留原文字节位置；CRLF 算一个段落边界。/ Preserve source offsets while wrapping; CRLF is one paragraph boundary.
// 行尾的右标点串先尝试悬挂进右缘留白并压缩字隙，放不下就把标点串前一个正文字拉到下一行，
// 保证下一行永远不以右标点开头；行尾左标点串整体移到下一行，极窄行才允许成组越界。
// A trailing run of closers first hangs into the margin with squeezed gaps; when it cannot fit, the
// body glyph before the run moves down so a line never starts with a closer. A trailing run of
// openers moves down as a whole; only one-glyph-wide lines may keep the pair beyond the budget.
// 按实际行字节增长；即使零宽字符连续出现也不截断，扩容失败保留旧缓冲。
// Grow by actual line bytes, including zero-width runs; failed growth retains the old allocation.
static bool line_append(size_t at, const char* text, size_t bytes) {
    if (at > s_len || bytes > s_len - at) return false;
    size_t need = at + bytes + 1;
    if (need > s_line_capacity) {
        size_t capacity = s_line_capacity;
        while (capacity < need) {
            if (capacity > (s_len + 1) / 2) { capacity = s_len + 1; break; }
            capacity *= 2;
        }
        char* grown = heap_caps_realloc(s_line, capacity, PSRAM_CAPS);
        if (!grown) return false;
        s_line = grown; s_line_capacity = capacity;
    }
    memcpy(s_line + at, text, bytes);
    s_line[at + bytes] = 0;
    return true;
}
static bool take_line(size_t off, size_t* next, bool* paragraph_end, int* px, bool* heading) {
    const blk_t* block = block_at(off);
    *heading = block && block->heading;
    *px = s_px + (*heading ? 8 : 0);
    int64_t budget = s_rect.width - line_indent(off, *heading);
    size_t limit = block ? block->offset + block->len : s_len;
    size_t end = off;
    size_t last_start = off;
    int64_t width = 0;
    int glyphs = 0;
    // trail 记录行尾同类标点串：kind 1=右标点 2=左标点；start 是串首字节，anchor 是串前一个正文字的字节。
    // trail tracks the trailing same-class run: kind 1=closers 2=openers; start is its first byte, anchor the body glyph before it.
    int trail_kind = 0;
    size_t trail_start = 0, trail_anchor = 0;
    uint32_t prev_cp = 0;
    // 词内断点生效后的下一行起点；词基宽在追加时跟踪，供断点前缀免重复测量。
    // Where the next line resumes after an in-word break; the word's base width is
    // tracked while appending so break prefixes need no repeated measurement.
    size_t word_resume = 0;
    bool word_broken = false;
    size_t track_word_start = SIZE_MAX;
    int64_t track_word_base = 0;
    s_line[0] = 0;
    s_line_compressed = 0;
    s_line_hyphen = false;
    *paragraph_end = false;
    if (block && (block->image || block->image_src)) {
        *heading = false;
        *next = limit + (limit < s_len && s_text[limit] == '\n');
        return *next > off;
    }
    while (end < limit && s_text[end] != '\r' && s_text[end] != '\n') {
        size_t n = codepoint_size(s_text + end, s_len - end);
        if (!n) return false;
        uint32_t cp = codepoint_value(s_text + end, n);
        // 字体逐字取整后累加 advance（含中西文边界 1/4 em）；单字测量避免反复扫描整行前缀。
        // Font advances (with the quarter-em CJK-Latin spacing) are rounded per glyph
        // and summed; measure each glyph once instead of every prefix.
        char glyph[5];
        memcpy(glyph, s_text + end, n);
        glyph[n] = 0;
        int64_t candidate = width + script_gap(prev_cp, cp, *px) + ttf_text_width_px(*px, glyph);
        if (candidate < 0) return false;
        if (candidate > budget) {
            if (end == off) return false;
            // 英文整词换行：词内溢出时整词下移；已在行首的长词按断点（软连字符/连字符/
            // TeX 模式）断开并补画连字符，找不到可断前缀才退回逐字断开。
            // English word wrapping: an in-word overflow moves the whole word down; a
            // long word already starting the line breaks at a hyphenation point (soft
            // hyphen, explicit hyphen or TeX pattern) with a painted hyphen, and only
            // falls back to per-glyph breaking without a fitting prefix.
            if (latin_word_char(cp) && track_word_start != SIZE_MAX) {
                size_t word_start = track_word_start;
                int64_t word_base = track_word_base;
                size_t word_end = end;
                while (word_end < limit) {
                    size_t wn = codepoint_size(s_text + word_end, s_len - word_end);
                    if (!wn || !latin_word_char(codepoint_value(s_text + word_end, wn))) break;
                    word_end += wn;
                }
                size_t picks[12];
                int picks_n = book_hyphen_candidates(s_text + word_start, word_end - word_start, picks, 12);
                if (picks_n) {
                    // 词内累计宽度一次测量（截到最大候选），断点二分免重复测宽。
                    // One measurement pass over the word (up to the largest candidate)
                    // feeds a binary search over break points.
                    size_t far = word_start + picks[0];
                    if (far + 1 < s_len && (unsigned char)s_text[far] == 0xC2) far += 2;
                    if (far > end) far = end;
                    int64_t cum[64];
                    size_t cut_at[64];
                    int cuts = 0;
                    int64_t run = 0;
                    for (size_t w = word_start; w < far && cuts < 64;) {
                        size_t wn = codepoint_size(s_text + w, s_len - w);
                        if (!wn) break;
                        // 软连字符记零宽：断点前缀不包含它，绘制端另补合成连字符。
                        // Soft hyphens count zero: break prefixes exclude them and the
                        // draw side paints the synthetic hyphen instead.
                        if (wn == 2 && (unsigned char)s_text[w] == 0xC2) {
                            w += wn;
                            continue;
                        }
                        char wg[5];
                        memcpy(wg, s_text + w, wn);
                        wg[wn] = 0;
                        run += ttf_text_width_px(*px, wg);
                        w += wn;
                        // 候选落在该字之后即收录（含软连字符位置）。/ A candidate right
                        // after this glyph is recorded, soft-hyphen positions included.
                        for (int pick = 0; pick < picks_n; ++pick) {
                            size_t cut = word_start + picks[pick];
                            if (cut == w || (cut + 1 < s_len && (unsigned char)s_text[cut] == 0xC2 && cut + 2 <= w)) {
                                if (cuts && cut_at[cuts - 1] == cut) break;
                                cut_at[cuts] = cut;
                                cum[cuts] = run;
                                ++cuts;
                                break;
                            }
                        }
                    }
                    (void)cuts;
                    int64_t hyphen_w = ttf_text_width_px(*px, "-");
                    // 候选降序：从最长前缀起找首个可放下者即最长可断前缀。
                    // Candidates run longest-first: the first fitting prefix while
                    // scanning downward is the longest breakable one.
                    size_t chosen = 0;
                    for (int pick = 0; pick < picks_n; ++pick) {
                        size_t cut = word_start + picks[pick];
                        if (cut <= off || cut > end) continue;
                        // 该断点前缀宽＝词基宽＋词内累计（在累计表中查）。
                        // The prefix width at a break is the word base plus the
                        // in-word cumulative at that cut.
                        int64_t prefix = -1;
                        for (int c = 0; c < cuts; ++c)
                            if (cut_at[c] == cut) { prefix = word_base + cum[c]; break; }
                        if (prefix < 0) continue;
                        if (prefix + hyphen_w > budget) continue;
                        chosen = cut;
                        break;
                    }
                    if (chosen) {
                        // 断点生效：行尾补画合成连字符；软连字符本身被断点消费。
                        // The break lands: paint the synthetic hyphen at the line
                        // end; a soft hyphen at the cut is consumed by the break.
                        bool soft = (unsigned char)s_text[chosen] == 0xC2;
                        word_resume = chosen + (soft ? 2u : 0u);
                        end = chosen;
                        s_line[end - off] = 0;
                        s_line_compressed = 0;
                        if (!line_append(end - off, "-", 1)) return false;
                        s_line_hyphen = true;
                        word_broken = true;
                        break;
                    }
                }
                if (word_broken) break;
                if (word_start > off) {
                    // 整词下移；行尾空格一并退去，但至少保留一个字节避免空行失败。
                    // Move the whole word down, trailing spaces included, always
                    // keeping one byte so the line cannot empty out.
                    end = word_start;
                    while (end > off + 1 && s_text[end - 1] == ' ') --end;
                    s_line[end - off] = 0;
                    s_line_compressed = 0;
                    break;
                }
                // 词在行首仍放不下且无可断前缀：保持逐字断开（下方通用路径）。
                // The word starts the line with no fitting prefix: keep the generic
                // per-glyph path below.
            }
            // 右标点禁排：优先悬挂；放不下时把标点串连同串前正文字一起下移，串前无正文才允许成组越界。
            // Closer: hang first; else the run moves down together with the body glyph before it; overhang only when no body glyph precedes.
            if (prohibited_line_start(cp)) {
                int hang = s_rect.x > 4 ? *px / 3 : 0;
                if (hang > s_rect.x - 4) hang = s_rect.x - 4;
                int squeeze = glyphs > 1 ? (glyphs - 1) * 3 : 0;
                // 标点挤压先于悬挂与字距压缩：行尾右标点串（含本字）各让出半个字宽，
                // 全角句读右半本就空白，视觉上比全行压字距自然。
                // Punctuation compression outranks hanging and gap squeeze: the trailing
                // closer run (this glyph included) each gives up half an em — full-width
                // closers carry blank right halves, which reads far more natural than
                // squeezing every gap on the line.
                int run = 1;
                for (size_t p = trail_kind == 1 ? trail_start : end; p < end;) {
                    size_t cn = codepoint_size(s_text + p, end - p);
                    if (!cn) return false;
                    ++run;
                    p += cn;
                }
                int reclaim = run * (*px / 2);
                if (candidate <= budget + hang + squeeze + reclaim) {
                    if (trail_kind != 1) { trail_kind = 1; trail_start = end; trail_anchor = last_start; }
                    if (!line_append(end - off, glyph, n)) return false;
                    s_line_compressed = run;
                    width = candidate;
                    prev_cp = cp;
                    track_word_start = SIZE_MAX;
                    track_word_base = 0;
                    end += n;
                    continue;
                }
                // 行尾已有右标点串时只能退到串前正文（last_start 会落在串内）；否则退最后一个字。
                // With a trailing closer run, roll back only to the body glyph before it (last_start sits inside the run); otherwise to the last glyph.
                size_t target = 0;
                if (trail_kind == 1 && trail_anchor > off) target = trail_anchor;
                else if (trail_kind != 1 && last_start > off) target = last_start;
                target = back_past_openers(off, target);
                if (target > off) {
                    end = target;
                    s_line[end - off] = 0;
                    s_line_compressed = 0;
                    break;
                }
                // 回退会清空整行（行首即标点串）：保持原行成组越界，不再截断。
                // Rolling back would empty the line (it starts with the mark run): keep it and overhang as a group.
                if (candidate <= budget + (int64_t)*px * 2) {
                    if (!line_append(end - off, glyph, n)) return false;
                    s_line_compressed = 0;
                    end += n;
                }
                break;
            }
            // 左标点禁排：串首在行内时整串移到下一行；串从行首开始则允许成组越界。
            // Opener: the whole run moves down when it starts inside the line; a line-initial run may overhang.
            if (trail_kind == 2 && trail_start > off) {
                end = trail_start;
                s_line[end - off] = 0;
                s_line_compressed = 0;
                break;
            }
            if (trail_kind == 2 && candidate <= budget + (int64_t)*px * 2) {
                if (!line_append(end - off, glyph, n)) return false;
                s_line_compressed = 0;
                end += n;
            }
            break;
        }
        if (prohibited_line_start(cp)) {
            if (trail_kind != 1) { trail_kind = 1; trail_start = end; trail_anchor = last_start; }
        } else if (prohibited_line_end(cp)) {
            if (trail_kind != 2) { trail_kind = 2; trail_start = end; trail_anchor = last_start; }
        } else {
            trail_kind = 0;
        }
        last_start = end;
        if (latin_word_char(cp) && !latin_word_char(prev_cp)) {
            track_word_start = end;
            track_word_base = width;
        } else if (!latin_word_char(cp)) {
            track_word_start = SIZE_MAX;
            track_word_base = 0;
        }
        width = candidate;
        ++glyphs;
        if (!line_append(end - off, glyph, n)) return false;
        // 压半宽只对最终的行尾标点串成立；后续再接正文字即取消。
        // Halving holds only for the run that finally ends the line; appending body glyphs after it cancels.
        s_line_compressed = 0;
        prev_cp = cp;
        end += n;
    }
    *next = word_broken ? word_resume : end;
    if (!word_broken && end < s_len && (s_text[end] == '\r' || s_text[end] == '\n')) {
        *next = end + 1;
        if (s_text[end] == '\r' && *next < s_len && s_text[*next] == '\n') (*next)++;
        // TXT 导出常用空行分段；把连续 CR/LF 归成一个段落边界，避免空行高度和段距重复叠加。
        // TXT exports often separate paragraphs with a blank line; collapse consecutive
        // CR/LF boundaries so an empty row is not added on top of paragraph spacing.
        if (!s_block_count) while (*next < s_len && (s_text[*next] == '\r' || s_text[*next] == '\n')) {
            char newline = s_text[(*next)++];
            if (newline == '\r' && *next < s_len && s_text[*next] == '\n') (*next)++;
        }
        *paragraph_end = true;
    }
    return *next > off;
}

// 标题从块首开始时，测出“完整标题 + 下一段首行”的高度；只有整组能在空白页放下才要求同页。
// At the start of a heading block, measure the whole heading plus the next paragraph's first
// line; keep them together only when the group fits on an otherwise empty page.
static bool heading_keep_height(size_t off, int64_t* height) {
    const blk_t* heading_block = block_at(off);
    if (!height || !heading_block || !heading_block->heading || off != heading_block->offset) return false;
    size_t at = off;
    int64_t total = 0;
    while (at < s_len) {
        const blk_t* block = block_at(at);
        if (block != heading_block && (block->heading || block->image || block->image_src)) return false;
        size_t next;
        bool paragraph_end, heading;
        int line_px;
        if (!take_line(at, &next, &paragraph_end, &line_px, &heading)) return false;
        int line_height = row_height(block, line_px);
        if (line_height > s_rect.height || total > s_rect.height - line_height) return false;
        total += line_height;
        if (paragraph_end) {
            int gap = para_gap(line_height, heading);
            if (gap > s_rect.height || total > s_rect.height - gap) return false;
            total += gap;
        }
        at = next;
        if (block != heading_block) {
            *height = total;
            return true;
        }
    }
    return false;
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
    s_guide_grid = guide_grid_requested();
    int64_t grid_height = (int64_t)px + px / 2 + (int64_t)px * s_leading_pct / 100;
    if (s_guide_grid && (grid_height < 3 || grid_height > rect.height)) goto fail;
    s_grid_height = s_guide_grid ? (int)grid_height : 0;
    s_grid_padding = grid_padding(rect);
    if (s_guide_grid && (int64_t)s_grid_padding + s_grid_height > rect.height) goto fail;
    s_used = s_grid_padding;
    s_line_capacity = len < 255 ? len + 1 : 256;
    s_line = heap_caps_malloc(s_line_capacity, PSRAM_CAPS);
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
        int64_t keep_height = 0;
        if (s_used > s_grid_padding && heading_keep_height(s_scan, &keep_height) &&
            keep_height <= s_rect.height - s_grid_padding && s_used + keep_height > s_rect.height) {
            if (!append_page(s_scan)) goto fail;
            s_used = s_grid_padding;
            if (book_layout_page_count() >= target) return true;
        }
        size_t next;
        bool paragraph_end, heading;
        int line_px;
        if (!take_line(s_scan, &next, &paragraph_end, &line_px, &heading)) goto fail;
        const blk_t* block = block_at(s_scan);
        int line_height = row_height(block, line_px);
        if (line_height > s_rect.height) goto fail;
        if (s_used + line_height > s_rect.height) {
            if (!append_page(s_scan)) goto fail;
            s_used = s_grid_padding;
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

// 逐字推进一行并绘制：advance = 字宽 + 中西文边界 1/4 em + 每隙调整；trailing_half
// 行尾右标点压半宽；justified 时开括号后/右标点前的隙不加拉；hung 时空格先收半宽。
// 字形位图保持原宽，只调步进。spread_extra 从首个受调隙起每隙再 +1。
// Advance a line glyph by glyph and draw: width plus the quarter-em CJK-Latin spacing
// plus the per-gap adjustment; trailing_half halves line-end closers; justified lines
// never widen right after openers or right before closers; hung lines halve spaces
// first. Glyph bitmaps keep full width — only steps move. spread_extra adds one more
// pixel to adjusted gaps from the first onward.
static void draw_line_glyphs(uint8_t* fb, int x, int y, int px, int gap, int extra, int trailing_half,
                             bool justified, bool hung, uint8_t fg, uint8_t bg) {
    size_t len = strlen(s_line), glyphs = 0;
    for (size_t i = 0; i < len;) {
        size_t n = codepoint_size(s_line + i, len - i);
        if (!n) return;
        ++glyphs;
        i += n;
    }
    int half = px / 2, spread = extra;
    int baseline = y + ttf_ascender_px(px);
    uint32_t prev = 0;
    size_t i = 0;
    while (i < len) {
        size_t n = codepoint_size(s_line + i, len - i);
        if (!n) return;
        char glyph[5];
        memcpy(glyph, s_line + i, n);
        glyph[n] = 0;
        ttf_draw_text_px(fb, x, baseline, px, glyph, EPD_DRAW_ALIGN_LEFT, fg, bg);
        uint32_t cp = codepoint_value(s_line + i, n);
        int w = (int)ttf_text_width_px(px, glyph) + script_gap(prev, cp, px);
        --glyphs;
        if (trailing_half && glyphs < (size_t)trailing_half) w -= half;
        else if (hung && n == 1 && glyph[0] == ' ') w -= w / 2;
        // 隙调整只落在该隙两端都不是开/右标点的位置；悬挂压缩不受此限。
        // Gap adjustments land only where neither neighbour is an opener/closer;
        // the hung squeeze is exempt.
        int step = 0;
        if (gap || spread) {
            size_t next = i + n;
            bool eligible = true;
            if (justified) {
                if (prohibited_line_end(cp)) eligible = false;
                if (eligible && next < len) {
                    size_t nn = codepoint_size(s_line + next, len - next);
                    if (nn && prohibited_line_start(codepoint_value(s_line + next, nn))) eligible = false;
                }
            }
            if (eligible) step = gap + (spread-- > 0 ? 1 : 0);
        }
        x += w + step;
        prev = cp;
        i += n;
    }
}
// 行内可加拉隙数与空间先收总量（与绘制端同规则）。/ Eligible widening gaps and the
// space-reclaim total, sharing the draw-side rules.
static int line_elastic_gaps(const char* line, size_t len, int px, int* space_reclaim) {
    int gaps = 0;
    if (space_reclaim) *space_reclaim = 0;
    uint32_t prev = 0;
    for (size_t i = 0; i < len;) {
        size_t n = codepoint_size(line + i, len - i);
        if (!n) break;
        uint32_t cp = codepoint_value(line + i, n);
        if (n == 1 && line[i] == ' ' && space_reclaim) *space_reclaim += (int)ttf_text_width_px(px, " ") / 2;
        if (i + n < len) {
            size_t nn = codepoint_size(line + i + n, len - i - n);
            uint32_t next = nn ? codepoint_value(line + i + n, nn) : 0;
            if (!prohibited_line_end(cp) && !(nn && prohibited_line_start(next))) ++gaps;
        }
        prev = cp;
        i += n;
    }
    (void)prev;
    return gaps;
}

// 正文与短预览共用折行、对齐和字形；不改变页表或扫描游标。
// Body and short previews share wrapping, alignment and glyphs without changing pages or the scan cursor.
static bool draw_range(uint8_t* fb, size_t off, size_t end, EpdRect rect, int padding, bool text_only, book_layout_cursor_t* cursor) {
    // 夜间模式整块反色：黑底、白字、图片灰度翻转。/ Night inverts the whole body: black ground, white text, flipped image grays.
    uint8_t fg = s_night ? 15 : 0, bg = s_night ? 0 : 15;
    if (s_night && (!cursor || !cursor->started)) epd_fill_rect(rect, 0, fb);
    bool drawn = false;
    int64_t used = padding;
    if (cursor && cursor->started) { off = cursor->offset; used = cursor->used; }
    while (off < end) {
        size_t next;
        bool paragraph_end, heading;
        int line_px;
        if (!take_line(off, &next, &paragraph_end, &line_px, &heading)) return drawn;
        const blk_t* block = block_at(off);
        // 预览只读文字，插图跳到后面的正文，不加载资源。
        // Preview borrows text only, skipping images without loading resources.
        if (text_only && block && (block->image || block->image_src)) { off = next; continue; }
        int line_height = row_height(block, line_px);
        int image_width = 0;
        if (block && block->image) image_size(block, &image_width, &line_height);
        if (used + line_height > rect.height) return drawn;
        if (image_width) {
            // 插图与查看器同走有序抖动进 16 级面板灰；夜间先翻灰再抖。
            // Illustrations reach the panel's 16 levels through the same ordered dither
            // as the viewer; night flips gray before dithering.
            int left = rect.x + (rect.width - image_width) / 2;
            for (int y = 0; y < line_height; ++y) for (int x = 0; x < image_width; ++x) {
                size_t src = (size_t)((int64_t)y * block->image_height / line_height) * block->image_width +
                             (size_t)((int64_t)x * block->image_width / image_width);
                uint8_t gray = s_night ? (uint8_t)(255 - block->image[src]) : block->image[src];
                epd_draw_pixel(left + x, rect.y + (int)used + y, ui_image_dither(gray, left + x, rect.y + (int)used + y), fb);
            }
        } else if (block && block->image_src) {
            draw_placeholder(fb, block, placeholder_rect(rect, (int)used, line_height));
        } else if (s_line[0]) {
            drawn = true;
            // 标题假粗体：临时提升覆盖率微调（1/16px 步进），只影响光栅缓存，不改步进与分页。
            // Heading faux bold: temporarily raise the coverage stroke (1/16-px steps);
            // raster cache only, never steps or pagination.
            int stroke_saved = ttf_get_stroke();
            if (heading) ttf_set_stroke(stroke_saved + 8 > 16 ? 16 : stroke_saved + 8);
            int indent = line_indent(off, heading);
            size_t line_len = strlen(s_line);
            int64_t width = ttf_text_width_px(line_px, s_line);
            int available = rect.width - indent;
            // 实际占宽＝测宽＋中西文边界间隙－行尾压半宽；无挤压时两后项自然为零/常量。
            // Effective width: measured width plus boundary spacing minus line-end halving.
            int64_t script_total = line_script_gaps(s_line, line_len, line_px);
            int64_t laid = width + script_total - (int64_t)s_line_compressed * (line_px / 2);
            int baseline = rect.y + (int)used + ttf_ascender_px(line_px);
            if (laid > available) {
                // 禁则悬挂行：空格先收半宽，再按 ≤3px/隙压缩拉回，残余量留在右缘留白。
                // Kinsoku hung line: halve spaces first, then squeeze gaps ≤3px each and leave the rest in the margin.
                int space_reclaim = 0;
                (void)line_elastic_gaps(s_line, line_len, line_px, &space_reclaim);
                int64_t overflow = laid - available - space_reclaim;
                size_t glyphs = 0;
                for (const char* q = s_line; *q; ++glyphs) {
                    size_t n = codepoint_size(q, strlen(q));
                    if (!n) break;
                    q += n;
                }
                int per = overflow > 0 ? hang_compress((int)overflow, glyphs > 1 ? (int)glyphs - 1 : 0) : 0;
                draw_line_glyphs(fb, rect.x + indent, rect.y + (int)used, line_px, -per, 0, s_line_compressed, false, true, fg, bg);
            } else if (s_align == 1 && !s_line_compressed && !script_total) {
                // 对齐：居中整行居中；两端对齐只作用于段中行（段末行、标题与图片行保持左对齐）。
                // Alignment: center centers the whole line; justification covers mid-paragraph lines only.
                ttf_draw_text_px(fb, rect.x + indent + (rect.width - indent - (int)width) / 2,
                                      baseline, line_px,
                                      s_line, EPD_DRAW_ALIGN_LEFT, fg, bg);
            } else if (s_align == 1) {
                draw_line_glyphs(fb, rect.x + indent + (available - (int)laid) / 2, rect.y + (int)used,
                                 line_px, 0, 0, s_line_compressed, false, false, fg, bg);
            } else if (s_align == 2 && !paragraph_end && !heading && laid < available) {
                // 两端对齐：余量摊到可加拉的隙（开括号后/右标点前不拉）；余量过小且无挤压走整行绘制。
                // Justified: spread the slack over elastic gaps (never right after openers or
                // before closers); tiny slack without compression keeps whole-line draws.
                int slack = available - (int)laid;
                int elastic = line_elastic_gaps(s_line, line_len, line_px, NULL);
                if ((slack < line_px / 2 && !s_line_compressed && !script_total) || !elastic) {
                    ttf_draw_text_px(fb, rect.x + indent, baseline, line_px,
                                          s_line, EPD_DRAW_ALIGN_LEFT, fg, bg);
                } else {
                    int per_gap = slack / elastic;
                    int extra = slack % elastic;
                    draw_line_glyphs(fb, rect.x + indent, rect.y + (int)used, line_px, per_gap, extra, s_line_compressed, true, false, fg, bg);
                }
            } else if (s_line_compressed || script_total) {
                draw_line_glyphs(fb, rect.x + indent, rect.y + (int)used, line_px, 0, 0, s_line_compressed, false, false, fg, bg);
            } else {
                ttf_draw_text_px(fb, rect.x + indent, baseline, line_px,
                                      s_line, EPD_DRAW_ALIGN_LEFT, fg, bg);
            }
            if (heading) ttf_set_stroke(stroke_saved);
            // 辅助线统一贴近文字：行底下方四分之一行距；网格槽对齐保证不进任何页的文字带。
            // Rules hug their line: a quarter of the leading below the text; the aligned
            // grid keeps them outside every page's text band.
            if (s_guide_style) {
                int y = rect.y + (int)used + line_px + (line_height - line_px) / 4 - 1;
                uint8_t gray = s_guide_binary ? (s_night ? 0xF0 : 0x00) : (s_night ? 0x50 : 0xB0);
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
        if (cursor) {
            cursor->offset = off; cursor->used = used; cursor->started = true;
            break;
        }
    }
    if (cursor) cursor->done = off >= end;
    return drawn;
}
void book_layout_draw_page(uint8_t* fb, size_t page, EpdRect rect, int px) {
    if (!fb || page >= book_layout_page_count() || px != s_px || rect.width != s_rect.width ||
        rect.height != s_rect.height || rect.x < 0 || rect.y < 0 ||
        rect.x > INT_MAX - rect.width || rect.y > INT_MAX - rect.height ||
        guide_grid_requested() != s_guide_grid || grid_padding(rect) != s_grid_padding) return;
    (void)draw_range(fb, s_pages[page], page + 1 < s_count ? s_pages[page + 1] : s_len, rect, s_grid_padding, false, NULL);
}

uint32_t book_layout_generation(void) { return s_generation; }

bool book_layout_page_has_images(size_t page) {
    if (page >= book_layout_page_count()) return true;
    size_t start = s_pages[page], end = book_layout_page_start_offset(page + 1);
    for (size_t i = 0; i < s_block_count; ++i)
        if (s_blocks[i].offset < end && s_blocks[i].offset + s_blocks[i].len > start &&
            (s_blocks[i].image || s_blocks[i].image_src)) return true;
    return false;
}

int book_layout_draw_slice(uint8_t* fb, size_t page, EpdRect rect, int px, book_layout_cursor_t* cursor) {
    if (!cursor || !fb || page >= book_layout_page_count() || px != s_px ||
        memcmp(&rect, &s_rect, sizeof(rect)) || (cursor->generation &&
        (cursor->generation != s_generation || cursor->page != page))) return -1;
    if (cursor->done) return 1;
    cursor->generation = s_generation; cursor->page = page;
    size_t end = book_layout_page_start_offset(page + 1);
    size_t before = cursor->started ? cursor->offset : s_pages[page];
    (void)draw_range(fb, s_pages[page], end, rect, s_grid_padding, false, cursor);
    if (cursor->done) return 1;
    return cursor->started && cursor->offset > before ? 0 : -1;
}

bool book_layout_draw_preview(uint8_t* fb, size_t offset, EpdRect rect, int px) {
    if (!fb || !s_line || !s_text || !s_count || offset >= s_len || px != s_px ||
        rect.width != s_rect.width || rect.height <= 0 || rect.height > s_rect.height ||
        rect.x < 0 || rect.y < 0 || rect.x > INT_MAX - rect.width || rect.y > INT_MAX - rect.height ||
        ((unsigned char)s_text[offset] & 0xC0) == 0x80 || guide_grid_requested() != s_guide_grid) return false;
    return draw_range(fb, offset, s_len, rect, grid_padding(rect), true, NULL);
}
size_t book_layout_image_at(size_t page, EpdRect rect, int x, int y, EpdRect* hit) {
    if (page >= book_layout_page_count() || rect.width != s_rect.width || rect.height != s_rect.height ||
        rect.x < 0 || rect.y < 0 || rect.x > INT_MAX - rect.width || rect.y > INT_MAX - rect.height ||
        x < rect.x || y < rect.y || x >= rect.x + rect.width || y >= rect.y + rect.height ||
        guide_grid_requested() != s_guide_grid || grid_padding(rect) != s_grid_padding) return SIZE_MAX;
    size_t off = s_pages[page], end = page + 1 < s_count ? s_pages[page + 1] : s_len;
    int64_t used = s_grid_padding;
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

// 字符命中沿用绘制的行高、缩进与对齐。/ Glyph hits share drawing heights, indentation and alignment.
size_t book_layout_text_at(size_t page, EpdRect rect, int x, int y) {
    if (page >= book_layout_page_count() || rect.width != s_rect.width || rect.height != s_rect.height ||
        rect.x < 0 || rect.y < 0 || rect.x > INT_MAX - rect.width || rect.y > INT_MAX - rect.height ||
        x < rect.x || y < rect.y || x >= rect.x + rect.width || y >= rect.y + rect.height ||
        guide_grid_requested() != s_guide_grid || grid_padding(rect) != s_grid_padding) return SIZE_MAX;
    size_t off = s_pages[page], end = page + 1 < s_count ? s_pages[page + 1] : s_len;
    int used = s_grid_padding;
    while (off < end) {
        size_t next; bool paragraph_end, heading; int px;
        if (!take_line(off, &next, &paragraph_end, &px, &heading)) return SIZE_MAX;
        const blk_t* block = block_at(off); int height = row_height(block, px);
        if (used + height > rect.height) return SIZE_MAX;
        if (y >= rect.y + used && y < rect.y + used + px && s_line[0] && !(block && (block->image || block->image_src))) {
            int indent = line_indent(off, heading);
            size_t len = strlen(s_line);
            int64_t script_total = line_script_gaps(s_line, len, px);
            int width = ttf_text_width_px(px, s_line);
            int left = rect.x + indent;
            // 与绘制端一致：占宽含边界间隙与压半宽；悬挂先收空格半宽。/ Mirror the
            // draw side: effective width carries boundary spacing and halving; hung
            // lines halve spaces first.
            int64_t laid = width + script_total - (int64_t)s_line_compressed * (px / 2);
            int slack = rect.width - indent - (int)laid;
            if (s_align == 1 && slack >= 0) left += slack / 2;
            size_t glyphs = 0;
            for (size_t i=0;i<len;) { size_t n=codepoint_size(s_line+i,len-i); if(!n)return SIZE_MAX; i+=n; ++glyphs; }
            int elastic = line_elastic_gaps(s_line, len, px, NULL);
            int space_reclaim = 0;
            (void)line_elastic_gaps(s_line, len, px, &space_reclaim);
            bool justified = s_align == 2 && !paragraph_end && !heading && glyphs > 1 && elastic &&
                             (slack >= px / 2 || s_line_compressed);
            // 悬挂行的字隙压缩与绘制端一致，命中位置才不漂移。/ Hung lines share the draw-side squeeze so hit positions do not drift.
            bool hung = slack < 0;
            int64_t overflow = -slack - space_reclaim;
            int gap = justified ? slack / elastic :
                      hung && glyphs > 1 && overflow > 0 ? -hang_compress((int)overflow, (int)glyphs - 1) : 0;
            int extra = justified ? slack % elastic : 0;
            int half = px / 2;
            int pen = left;
            uint32_t prev = 0;
            for (size_t i=0;i<len;) {
                size_t n=codepoint_size(s_line+i,len-i); if(!n)return SIZE_MAX;
                char glyph[5]; memcpy(glyph,s_line+i,n); glyph[n]=0;
                uint32_t cp = codepoint_value(s_line+i,n);
                int w=ttf_text_width_px(px,glyph)+script_gap(prev,cp,px);
                --glyphs;
                if (s_line_compressed && glyphs < (size_t)s_line_compressed) w -= half;
                else if (hung && n==1 && glyph[0]==' ') w -= w / 2;
                int step_extra = extra-- > 0 ? 1 : 0;
                int step = 0;
                if (gap || step_extra) {
                    // 拉伸隙与绘制端同一资格判定；悬挂压缩不限。/ Widening shares
                    // the draw-side eligibility; the hung squeeze is exempt.
                    bool eligible = true;
                    if (justified) {
                        if (prohibited_line_end(cp)) eligible = false;
                        if (eligible && i + n < len) {
                            size_t nn = codepoint_size(s_line+i+n, len-i-n);
                            if (nn && prohibited_line_start(codepoint_value(s_line+i+n, nn))) eligible = false;
                        }
                    }
                    if (eligible) step = gap + step_extra;
                }
                int advance = w + step;
                if (!justified && !hung && !s_line_compressed && !script_total) {
                    char keep=s_line[i]; s_line[i]=0; pen=left+ttf_text_width_px(px,s_line); s_line[i]=keep;
                    char after=s_line[i+n]; s_line[i+n]=0; w=left+ttf_text_width_px(px,s_line)-pen; s_line[i+n]=after;
                } else if (glyphs && advance < w) {
                    // 压缩/悬挂行的步进小于字宽，命中槽随步进收窄，相邻命中不重叠。
                    // Squeezed or hung steps run narrower than glyph widths; hit slots follow the step so neighbours never overlap.
                    w = advance > 0 ? advance : 1;
                }
                // 合成连字符不属于原文，不产生命中。/ The synthetic hyphen is not
                // source text and yields no hit.
                bool synthetic = s_line_hyphen && i + n >= len;
                if(!synthetic && x>=pen && x<pen+w && !(n==1 && (glyph[0]==' '||glyph[0]=='\t'))) return off+i;
                pen+=advance; prev=cp; i+=n;
            }
            return SIZE_MAX;
        }
        used += height; if(paragraph_end) used += para_gap(height,heading); off=next;
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
void book_layout_set_guide_origin(int y) {
    s_guide_origin = y >= 0 ? y : 0;
}
void book_layout_set_guide_contrast(bool binary) {
    s_guide_binary = binary;
}

void book_layout_set_align(int align) {
    s_align = align == 1 || align == 2 ? align : 0;
}
