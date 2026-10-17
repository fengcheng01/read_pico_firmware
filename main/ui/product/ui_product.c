/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 * 中文：基于现有绘图库的产品排版，文字缓冲固定大小。
 * English: Product layout atop the existing drawing kit, using fixed text buffers.
 * 冻结修订：密码数字和清空/退格在字库失败时仍有无分配图形，不改变密码校验与命中区域。
 * Frozen revision: PIN digits and clear/backspace retain allocation-free graphics on font failure, without changing verification or hit regions.
 */
#include "ui_product.h"
#include "ui_gesture.h"
#include "ui_dither.h"
#include "ttf_font.h"
#include "app_registry.h"
#include "ui_menu.h"
#include "book_entry.h"
#include "os_lunar.h"
#include <stdio.h>
#include <string.h>

static size_t character_bytes(const char* p) {
    unsigned char c = (unsigned char)*p;
    size_t n = c < 0x80 ? 1 : (c & 0xe0) == 0xc0 ? 2 : (c & 0xf0) == 0xe0 ? 3 : (c & 0xf8) == 0xf0 ? 4 : 1;
    for (size_t i = 1; i < n; ++i) if (!p[i] || ((unsigned char)p[i] & 0xc0) != 0x80) return 1;
    return n;
}
void ui_product_title(uint8_t* fb, EpdRect rect, const char* title, int px, int max_lines) {
    if (!title || px <= 0 || rect.width <= 0) return;
    const char* p = title;
    for (int row = 0; row < max_lines && *p && (row + 1) * (px + 8) <= rect.height; ++row) {
        char line[260] = {0};
        size_t used = 0;
        bool last = row + 1 == max_lines || (row + 2) * (px + 8) > rect.height;
        int reserve = last ? ttf_text_width_px(px, "…") : 0;
        while (*p) {
            size_t n = character_bytes(p);
            if (used + n + 4 >= sizeof(line)) break;
            memcpy(line + used, p, n); line[used + n] = 0;
            if (ttf_text_width_px(px, line) > rect.width - reserve) { line[used] = 0; break; }
            used += n; p += n;
        }
        if (!used) break;
        if (last && *p) memcpy(line + used, "…", sizeof("…"));
        ui_text(fb, rect.x, rect.y + row * (px + 8), px, line, EPD_DRAW_ALIGN_LEFT, false);
    }
}

/* ---- 摘录与三键提示 / Excerpts and key hints ---- */
void ui_product_key_hints(uint8_t* fb, const char* left, const char* middle, const char* right) {
    const char* labels[] = {left, middle, right};
    ui_clear_rect_fast(fb, (EpdRect){0, UI_LOCK_HEIGHT - 24, UI_LOCK_WIDTH, 24});
    for (int i = 0; i < 3; ++i) {
        char label[64]; snprintf(label, sizeof(label), "%d  %s", i + 1, labels[i]);
        ui_text_vc(fb, (2 * i + 1) * UI_LOCK_WIDTH / 6, UI_LOCK_HEIGHT - 12, 20,
                   label, EPD_DRAW_ALIGN_CENTER, false);
    }
}
// 逐字估算卡片高度，预留省略号宽度；长句先缩字号，普通短句不占满阅读页。
// Estimate card height per glyph with ellipsis room; long excerpts shrink first and short sentences leave the body visible.
static int excerpt_lines(const char* text, int px, int width) {
    int lines = 1, used = 0;
    int limit = width - ttf_text_width_px(px, "…");
    for (const char* p = text; p && *p;) {
        size_t n = character_bytes(p); char glyph[5] = {0}; memcpy(glyph, p, n);
        int w = ttf_text_width_px(px, glyph);
        if (used && used + w > limit) { ++lines; used = 0; }
        used += w; p += n;
    }
    return lines;
}
EpdRect ui_product_excerpt_button(int index) { return ui_row_rect(index, 2, 944, 70); }
void ui_product_excerpt_confirm(uint8_t* fb, const char* text, const char* source, const char* notice) {
    int px = 32, width = ui_content_width() - 24;
    while (px > 24 && excerpt_lines(text, px, width) * (px + 8) > 450) px -= 2;
    int lines = excerpt_lines(text, px, width), height = 314 + lines * (px + 8);
    if (height > 800) height = 800;
    EpdRect panel = {UI_MARGIN - 12, 1038 - height, ui_content_width() + 24, height};
    ui_fill_round_rect(fb, panel, UI_BTN_RADIUS, UI_GRAY_WHITE);
    ui_draw_round_rect(fb, panel, UI_BTN_RADIUS, UI_GRAY_BLACK);
    ui_text(fb, UI_MARGIN + 12, panel.y + 20, 34, "保存这段摘录？", EPD_DRAW_ALIGN_LEFT, false);
    ui_product_title(fb, (EpdRect){UI_MARGIN + 12, panel.y + 72, width, 36}, source, 24, 1);
    ui_hairline(fb, panel.y + 112, UI_MARGIN + 12, width, UI_GRAY_LIGHT);
    ui_product_title(fb, (EpdRect){UI_MARGIN + 12, panel.y + 132, width, height - 314}, text, px, lines);
    ui_product_title(fb, (EpdRect){UI_MARGIN + 12, 858, width, 66},
                     notice && notice[0] ? notice : "存入手帐的金句便签，可回到原文。最多保留16条，满后替换最旧摘录。", 22, 2);
    ui_draw_button(fb, ui_product_excerpt_button(0), "取消", false);
    ui_draw_button(fb, ui_product_excerpt_button(1), notice && notice[0] ? "重试保存" : "保存摘录", true);
}
// 竖排均衡列长，标点不独占列首；返回实际文字边界供来源跟随，长英文仍横排。
// Balance vertical columns without orphaned punctuation; return ink bounds for the source, leaving Latin passages horizontal.
static bool excerpt_closer(const char* p) {
    size_t n = character_bytes(p); char glyph[5] = {0}; memcpy(glyph, p, n);
    return strstr("，。、；：？！）》」』】…", glyph) != NULL;
}
static bool excerpt_opener(const char* p) {
    size_t n = character_bytes(p); char glyph[5] = {0}; memcpy(glyph, p, n);
    return strstr("（《「『【", glyph) != NULL;
}
static EpdRect excerpt_vertical(uint8_t* fb, EpdRect r, const char* text, int px, uint8_t ink, int source_space) {
    const char* glyphs[384]; unsigned ends[385];
    unsigned count = 0;
    for (const char* p = text; *p && count < 384; p += character_bytes(p)) glyphs[count++] = p;
    if (!count) return (EpdRect){r.x, r.y, 0, 0};
    unsigned columns = 0, drawn = 0, longest = 0;
    int max_rows, max_columns, cell;
    for (;;) {
        cell = px + 12; max_rows = r.height / cell; max_columns = (r.width + 24) / (px + 24);
        if (max_rows < 1 || max_columns < 1) return (EpdRect){r.x, r.y, 0, 0};
        unsigned columns_needed = (count + max_rows - 1) / max_rows;
        unsigned rows = (count + columns_needed - 1) / columns_needed;
        drawn = columns = longest = 0;
        ends[0] = 0;
        while (drawn < count && columns < (unsigned)max_columns) {
            unsigned end = drawn + rows;
            if (end > count) end = count;
            unsigned remaining_columns = columns_needed > columns ? columns_needed - columns - 1 : 0;
            if (!remaining_columns && count - drawn <= (unsigned)max_rows) end = count;
            else {
                // 能在剩余列容下时，优先在靠近列尾的句读处分列，不切断“半途而废”等短语。
                // Prefer punctuation near the column end when the remainder still fits, retaining short phrases.
                for (unsigned cut = end; cut > drawn + rows * 2 / 3; --cut)
                    if (excerpt_closer(glyphs[cut - 1]) && count - cut <= remaining_columns * (unsigned)max_rows) { end = cut; break; }
            }
            while (end < count && end - drawn < (unsigned)max_rows && excerpt_closer(glyphs[end])) ++end;
            if (end < count && excerpt_closer(glyphs[end]) && end > drawn + 1) --end;
            while (end < count && end > drawn + 1 && excerpt_opener(glyphs[end - 1])) --end;
            if (end - drawn > longest) longest = end - drawn;
            ends[++columns] = end; drawn = end;
        }
        if (drawn == count || px <= 24) break;
        px -= 2;
    }
    int height = (int)longest * cell - 12;
    int top = r.y + (r.height - height) / 2;
    int width = (int)(columns - 1) * (px + 24) + px;
    if (width + source_space > r.width) source_space = 0;
    // 正文与来源作为一组居中，短句不再贴右留下大片空白。
    // Center the quote and source together so short passages do not cling to the right edge.
    int left = r.x + (r.width - width - source_space) / 2 + source_space;
    int right = left + width - px / 2;
    for (unsigned col = 0; col < columns; ++col) {
        for (unsigned i = ends[col]; i < ends[col + 1]; ++i) {
            size_t n = character_bytes(glyphs[i]); char glyph[5] = {0}; memcpy(glyph, glyphs[i], n);
            if (i + 1 == drawn && drawn < count) snprintf(glyph, sizeof(glyph), "…");
            int x = right - (int)col * (px + 24);
            int y = top + (int)(i - ends[col]) * cell;
            if (!strcmp(glyph, "。") || !strcmp(glyph, "、") || !strcmp(glyph, "，")) { x += px / 4; y -= px / 4; }
            ttf_draw_text_px(fb, x, y + ttf_ascender_px(px), px, glyph, EPD_DRAW_ALIGN_CENTER, ink, UI_INK_WHITE);
        }
    }
    return (EpdRect){left, top, width, height};
}
void ui_product_excerpt_lock(uint8_t* fb, const char* text, const char* path, const char* chapter, const char* wake,
                             const os_time_info_t* time, unsigned minutes, unsigned active_days, bool live_clock, bool preview) {
    ui_clear_page(fb);
    bool valid = time && time->state == OS_TIME_VALID;
    char date[96] = "时间未校准", clock[20] = "--:--";
    if (valid) {
        os_lunar_date_t lunar;
        if (os_lunar_from_solar(time->year,time->month,time->day,&lunar)) os_lunar_format(date,sizeof(date),&lunar);
        else snprintf(date,sizeof(date),"%u 年 %u 月 %u 日",time->year,time->month,time->day);
        snprintf(clock,sizeof(clock),"%02u:%02u",time->hour,time->minute);
    }
    ui_product_title(fb,(EpdRect){UI_MARGIN,preview ? 142 : 48,ui_content_width(),50},date,32,1);
    ui_hairline(fb,preview ? 194 : 112,UI_MARGIN,ui_content_width(),UI_GRAY_BLACK);
    // 来源组成为「出自-书名·章节名」；章节为空（含旧记录）退回「出自-书名」。
    // The source reads “出自-书名·章节名”; without a chapter (legacy records) it falls back to “出自-书名”.
    char source[384]; const char* base = strrchr(path,'/');
    snprintf(source,sizeof(source),"出自-%s",base?base+1:path);
    char* ext = strrchr(source,'.'); if (ext && ext != source) *ext=0;
    if (chapter && chapter[0]) {
        size_t used = strlen(source);
        snprintf(source+used,sizeof(source)-used,"·%s",chapter);
    }
    size_t count = 0, wide = 0;
    for (const char* p = text; *p;) { size_t n = character_bytes(p); ++count; if (n >= 3) ++wide; p += n; }
    if (wide * 2 >= count) {
        EpdRect body = {UI_MARGIN, preview ? 300 : 224, ui_content_width(), preview ? 676 : 752};
        int quote_px = count <= 8 ? 60 : count <= 24 ? 52 : 48;
        EpdRect ink = excerpt_vertical(fb, body, text, quote_px, UI_INK_BLACK, 88);
        if (ink.x >= UI_MARGIN + 88) {
            // 来源靠近正文左侧，两列竖排；过长明确省略，不制造作者或装饰印章。
            // Keep the source beside the quote in up to two vertical columns, explicitly ellipsizing without inventing authors or stamps.
            char* tail = source; unsigned letters = 0;
            while (*tail && letters++ < 20) tail += character_bytes(tail);
            if (*tail) memcpy(tail, "…", sizeof("…"));
            int source_height = ink.height < 128 ? 128 : ink.height > 480 ? 480 : ink.height;
            excerpt_vertical(fb, (EpdRect){ink.x - 88, ink.y + (ink.height - source_height) / 2, 88, source_height}, source, 26, 4, 0);
        } else ui_product_title(fb, (EpdRect){UI_MARGIN, preview ? 232 : 158, ui_content_width(), 44}, source, 26, 1);
    } else {
        EpdRect body = {UI_MARGIN + 40, preview ? 328 : 270, ui_content_width() - 80, preview ? 620 : 680};
        ui_product_title(fb, (EpdRect){body.x, preview ? 250 : 192, body.width, 44}, source, 26, 1);
        int px = 44; while (px > 24 && excerpt_lines(text, px, body.width) * (px + 8) > body.height) px -= 2;
        ui_product_title(fb, body, text, px, 20);
    }
    ui_hairline(fb,1036,UI_MARGIN,ui_content_width(),UI_GRAY_BLACK);
    ui_text(fb,UI_MARGIN,1064,100,clock,EPD_DRAW_ALIGN_LEFT,false);
    char reading[64],days[64];
    if (valid) {
        snprintf(reading,sizeof(reading),preview ? "预览时刻 · 今日阅读 %u 分钟" : live_clock ? "今日阅读 %u 分钟" : "关机前 · 今日阅读 %u 分钟",minutes);
        snprintf(days,sizeof(days),"近30天阅读 %u 天",active_days);
    } else {
        snprintf(reading,sizeof(reading),"未校时 · 阅读数据暂不可归日");
        snprintf(days,sizeof(days),"已固定所选摘录");
    }
    ui_text(fb,UI_MARGIN,1180,20,reading,EPD_DRAW_ALIGN_LEFT,false);
    ui_draw_round_rect(fb,(EpdRect){386,1106,258,56},8,UI_GRAY_BLACK);
    ui_text_vc(fb,515,1134,22,days,EPD_DRAW_ALIGN_CENTER,false);
    ui_product_title(fb,(EpdRect){386,1178,258,30},wake,18,1);
}
static bool available(os_app_id_t id, bool transfer_enabled) {
    return app_by_id(id) && (id != OS_APP_TRANSFER || transfer_enabled);
}
void ui_product_home_bar(uint8_t* fb, bool transfer_enabled) {
    const os_app_id_t ids[] = {OS_APP_LIBRARY, OS_APP_TRANSFER};
    const char* labels[] = {"书架", "导入图书"};
    for (int i = 0; i < 2; ++i)
        ui_draw_button(fb, ui_bar_rect(i, 2), labels[i], available(ids[i], transfer_enabled));
    ui_draw_menu_handle(fb, false);
}
os_app_id_t ui_product_home_bar_hit(uint16_t x, uint16_t y, bool transfer_enabled) {
    int hit = ui_bar_hit(x, y, 2);
    os_app_id_t id = hit == 0 ? OS_APP_LIBRARY : hit == 1 ? OS_APP_TRANSFER : OS_APP_NONE;
    return available(id, transfer_enabled) ? id : OS_APP_NONE;
}

void ui_product_header(uint8_t* fb, const char* title, const char* detail) {
    ui_text(fb, UI_MARGIN, 24, 30, "小纸 Pico  /  READ PICO", EPD_DRAW_ALIGN_LEFT, false);
    ui_text(fb, UI_MARGIN, 68, 64, title, EPD_DRAW_ALIGN_LEFT, false);
    ui_product_title(fb, (EpdRect){UI_MARGIN, 142, ui_content_width(), 44}, detail, 32, 1);
}
EpdRect ui_product_back_rect(void) { return (EpdRect){470, 60, 174, 80}; }
void ui_product_back(uint8_t* fb, const char* label) { ui_draw_button(fb, ui_product_back_rect(), label, false); }
static const os_app_id_t roots[] = {OS_APP_HOME, OS_APP_LIBRARY, OS_APP_TODAY, OS_APP_SETTINGS};
static const char* root_labels[] = {"正在读", "书架", "手帐", "设置"};

/* ---- 四根导航位图图标 / Root navigation bitmap icons ---- */
// 32×32 1bpp 位图，每行 4 字节；1 = 黑。/ 32×32 1bpp, 4 bytes per row; 1 = ink.
typedef struct { float x0, y0, x1, y1; } icon_stroke_t;
static const icon_stroke_t root_strokes[4][16] = {
    {{3,6,10,5},{10,5,16,8},{16,8,22,5},{22,5,29,6},{3,6,3,25},{3,25,10,24},{10,24,16,27},{16,27,22,24},{22,24,29,25},{29,25,29,6},{16,8,16,27},{7,10,12,11},{7,15,12,16},{20,11,25,10},{20,16,25,15}},
    {{3,27,29,27},{5,26,5,6},{5,6,11,6},{11,6,11,26},{7,10,9,10},{14,26,14,3},{14,3,20,3},{20,3,20,26},{16,8,18,8},{23,8,29,25},{23,8,27,7},{27,7,32,24},{29,25,32,24}},
    {{5,6,5,26},{5,26,16,24},{16,24,27,26},{27,26,27,6},{27,6,16,8},{16,8,5,6},{16,8,16,24},{8,11,13,12},{8,16,13,17},{8,21,13,21},{19,12,24,11},{19,17,24,16},{19,21,24,21}},
    {{3,7,9,7},{17,7,29,7},{3,16,18,16},{26,16,29,16},{3,25,6,25},{14,25,29,25},{10,4,16,4},{16,4,16,10},{16,10,10,10},{10,10,10,4},{19,13,25,13},{25,13,25,19},{25,19,19,19},{19,19,19,13},{7,22,13,22},{7,28,13,28}}
};
static const unsigned root_stroke_count[] = {15,13,13,16};
static bool icon_ink(int icon, float x, float y) {
    float radius = 0.95f;
    for (unsigned i = 0; i < root_stroke_count[icon]; ++i) {
        icon_stroke_t s = root_strokes[icon][i];
        float dx = s.x1 - s.x0, dy = s.y1 - s.y0;
        float t = ((x - s.x0) * dx + (y - s.y0) * dy) / (dx * dx + dy * dy);
        if (t < 0) t = 0;
        if (t > 1) t = 1;
        float ex = x - s.x0 - t * dx, ey = y - s.y0 - t * dy;
        if (ex * ex + ey * ey <= radius * radius) return true;
    }
    // 第三条滑杆的竖边。/ Vertical edges of the third slider.
    return icon == 3 && ((x >= 6 && x <= 8) || (x >= 12 && x <= 14)) && y >= 22 && y <= 28;
}
static uint8_t root_icon_pixels[4][44 * 44];
static bool root_icon_ready[4];
static void draw_root_icon(uint8_t* fb, int cx, int cy, int icon) {
    // 覆盖率只算一次，切根页复用像素。/ Compute coverage once and reuse pixels across roots.
    if (!root_icon_ready[icon]) {
        for (int y = 0; y < 44; ++y) for (int x = 0; x < 44; ++x) {
            unsigned coverage = 0;
            for (int sy = 0; sy < 4; ++sy) for (int sx = 0; sx < 4; ++sx)
                coverage += icon_ink(icon, (x + (sx + 0.5f) / 4) * 34 / 44, (y + (sy + 0.5f) / 4) * 34 / 44);
            root_icon_pixels[icon][y * 44 + x] = 255 - coverage * 255 / 16;
        }
        root_icon_ready[icon] = true;
    }
    for (int y = 0; y < 44; ++y) for (int x = 0; x < 44; ++x) {
        uint8_t gray = root_icon_pixels[icon][y * 44 + x];
        if (gray != 255) epd_draw_pixel(cx - 22 + x, cy - 22 + y, gray, fb);
    }
}

void ui_product_root_bar(uint8_t* fb, os_app_id_t active) {
    ui_clear_rect_fast(fb, (EpdRect){0, UI_BAR_TOP, UI_LOCK_WIDTH, UI_BAR_H});
    ui_hairline(fb, UI_BAR_TOP, UI_MARGIN, ui_content_width(), UI_GRAY_BLACK);
    for (int i = 0; i < 4; ++i) {
        EpdRect r = ui_bar_rect(i, 4);
        bool on = active == roots[i];
        int cx = r.x + r.width / 2, cy = r.y + 26;
        draw_root_icon(fb, cx, cy, i);
        // 图标下方保留小字标签（选中加粗），确保语义清楚。/ Small label below
        // the icon (bold when active) keeps the meaning unambiguous.
        // 标签恒为黑色；选中态由下划线表达，不再用反白把文字变淡。
        // Labels stay black; selection shows via the underline, never washed-out text.
        ui_text_vc(fb, cx, r.y + 70, 26, root_labels[i], EPD_DRAW_ALIGN_CENTER, false);
        if (on) epd_fill_rect((EpdRect){r.x + 14, r.y + r.height - 10, r.width - 28, 4}, UI_GRAY_BLACK, fb);
    }
    ui_draw_menu_handle(fb, false);
}
os_app_id_t ui_product_root_hit(uint16_t x, uint16_t y) {
    int i = ui_bar_hit(x, y, 4);
    return i >= 0 && i < 4 && app_by_id(roots[i]) ? roots[i] : OS_APP_NONE;
}
// 按下即导航（跨面板参考固件的列表按压行为）：底栏 tab 按下立即切换，不等抬起。
// Navigate on press (the cross-panel reference behavior for lists): bottom-bar tabs
// switch the moment they are pressed instead of waiting for the release.
bool ui_product_root_press(app_ctx_t* ctx, const ui_gesture_event_t* ev) {
    if (ev == NULL || ev->type != UI_GESTURE_PRESS) return false;
    os_app_id_t id = ui_product_root_hit(ev->x, ev->y);
    if (id == OS_APP_NONE) return false;
    ctx->request_app_rearm_touch = true;
    return ui_product_navigate(ctx, id);
}
bool ui_product_navigate(app_ctx_t* ctx, os_app_id_t id) {
    const app_desc_t* next = app_by_id(id);
    if (!next) return false;
    // 书架入口被上一条目加载占用时也照常切页：页面会呈现自己的加载态，
    // 静默吞掉 tab 按下只会变成“要点两次才切换”。
    // Switch even when a previous book entry is still loading: the page shows its
    // own loading state; silently swallowing the tab press just becomes "tap twice".
    if (id == OS_APP_LIBRARY) (void)book_entry_request(BOOK_ENTRY_SHELF, NULL);
    ctx->request_app = next;
    return true;
}
void ui_product_cover_bitmap(uint8_t* fb, EpdRect r, const uint8_t* gray) {
    if (!gray || r.width < 3 || r.height < 3) return;
    int w = r.width - 2, h = w * BOOK_COVER_H / BOOK_COVER_W;
    if (h > r.height - 2) { h = r.height - 2; w = h * BOOK_COVER_W / BOOK_COVER_H; }
    int left = r.x + (r.width - w) / 2, top = r.y + (r.height - h) / 2;
    epd_fill_rect(r, UI_GRAY_WHITE, fb);
    // 封面缩略与插图同走 16 级有序抖动。/ Cover thumbnails share the 16-level ordered dither.
    for (int y = 0; y < h; ++y) for (int x = 0; x < w; ++x)
        epd_draw_pixel(left + x, top + y,
                       ui_image_dither(gray[(size_t)(y * BOOK_COVER_H / h) * BOOK_COVER_W + x * BOOK_COVER_W / w], left + x, top + y), fb);
    ui_draw_round_rect(fb, r, 0, UI_GRAY_BLACK);
}
void ui_product_cover(uint8_t* fb, EpdRect r, const char* title, int px) {
    epd_fill_rect(r, UI_GRAY_WHITE, fb);
    epd_draw_rect(r, UI_GRAY_BLACK, fb);
    epd_fill_rect((EpdRect){r.x + 8, r.y + 8, 5, r.height - 16}, UI_GRAY_BLACK, fb);
    ui_product_title(fb, (EpdRect){r.x + 24, r.y + 30, r.width - 40, r.height - 74}, title, px, 5);
    ui_hairline(fb, r.y + r.height - 40, r.x + 24, r.width - 40, UI_GRAY_BLACK);
    ui_text(fb, r.x + 24, r.y + r.height - 30, 18, "READ PICO", EPD_DRAW_ALIGN_LEFT, false);
}
EpdRect ui_product_shelf_rect(int row) { return (EpdRect){UI_MARGIN, 308 + row * 228, ui_content_width(), 208}; }
EpdRect ui_product_shelf_control_rect(int index) { return ui_row_rect(index, 4, 176, 64); }
void ui_product_shelf_layout(uint8_t* fb, bool grid) {
    EpdRect r = ui_product_shelf_control_rect(3);
    ui_fill_round_rect(fb, r, UI_BTN_RADIUS, UI_GRAY_WHITE);
    EpdRect selected = {r.x + (grid ? r.width / 2 : 0) + 4, r.y + 4, r.width / 2 - 8, r.height - 8};
    ui_fill_round_rect(fb, selected, UI_BTN_RADIUS - 4, UI_GRAY_LIGHT);
    ui_draw_round_rect(fb, r, UI_BTN_RADIUS, UI_GRAY_BLACK);
    int list_x = r.x + r.width / 4 - 16, top = r.y + 18;
    for (int i = 0; i < 3; ++i) {
        epd_fill_rect((EpdRect){list_x, top + 10 * i, 4, 4}, UI_GRAY_BLACK, fb);
        epd_fill_rect((EpdRect){list_x + 9, top + 10 * i, 23, 3}, UI_GRAY_BLACK, fb);
    }
    int grid_x = r.x + r.width * 3 / 4 - 17;
    for (int row = 0; row < 2; ++row) for (int col = 0; col < 3; ++col)
        epd_draw_rect((EpdRect){grid_x + col * 13, top + row * 16, 9, 12}, UI_GRAY_BLACK, fb);
}
EpdRect ui_product_shelf_grid_rect(int index) { return ui_row_rect(index % 3, 3, 292 + index / 3 * 354, 342); }
void ui_product_shelf_grid_card(uint8_t* fb, EpdRect r, const char* title, const char* meta,
                                 const char* format, unsigned percent, bool has_progress,
                                 bool pressed, const uint8_t* cover) {
    ui_clear_rect_fast(fb, r);
    if (pressed) epd_fill_rect(r, 0xE0, fb);
    EpdRect slot = {r.x + (r.width - BOOK_COVER_W) / 2, r.y, BOOK_COVER_W, BOOK_COVER_H};
    if (cover) ui_product_cover_bitmap(fb, slot, cover);
    else {
        epd_draw_rect(slot, UI_GRAY_BLACK, fb);
        ui_text_vc(fb, slot.x + slot.width / 2, slot.y + slot.height / 2, 22, format, EPD_DRAW_ALIGN_CENTER, false);
    }
    ui_product_title(fb, (EpdRect){r.x + 4, r.y + 242, r.width - 8, 60}, title, 22, 2);
    ui_product_title(fb, (EpdRect){r.x + 4, r.y + 308, r.width - 8, 26}, meta, 18, 1);
    if (has_progress) {
        epd_fill_rect((EpdRect){r.x + 4, r.y + 340, r.width - 8, 2}, UI_GRAY_LIGHT, fb);
        epd_fill_rect((EpdRect){r.x + 4, r.y + 340, (r.width - 8) * (int)(percent > 100 ? 100 : percent) / 100, 2}, UI_GRAY_BLACK, fb);
    }
}
EpdRect ui_product_shelf_nav_rect(int index) { return ui_row_rect(index, 3, 1006, 80); }
void ui_product_shelf_card(uint8_t* fb, EpdRect r, const char* title, const char* meta,
                           unsigned percent, bool has_progress, bool pressed, const uint8_t* cover) {
    ui_clear_rect_fast(fb, r);
    if (pressed) epd_fill_rect(r, 0xE0, fb);
    EpdRect slot = {r.x, r.y + 6, 146, 188};
    if (cover) ui_product_cover_bitmap(fb, slot, cover);
    else ui_product_cover(fb, slot, title, 34);
    int x = r.x + 174, width = r.width - 174;
    ui_product_title(fb, (EpdRect){x, r.y + 16, width, 144}, title, 44, 2);
    ui_product_title(fb, (EpdRect){x, r.y + 158, width, 42}, meta, 32, 1);
    if (has_progress) {
        epd_fill_rect((EpdRect){x, r.y + 194, width, 3}, UI_GRAY_LIGHT, fb);
        epd_fill_rect((EpdRect){x, r.y + 194, width * (int)(percent > 100 ? 100 : percent) / 100, 3}, UI_GRAY_BLACK, fb);
    }
}
EpdRect ui_product_tool_rect(int index) {
    return ui_grid_rect(index % 3, 3, index / 3, UI_BAR_TOP - 2 * (UI_BTN_H + UI_GAP), UI_BTN_H);
}
static void reader_tools(uint8_t* fb, const char* title, int px, bool sizes, int pressed) {
    EpdRect panel = {UI_MARGIN - 16, 782, ui_content_width() + 32, UI_BAR_TOP - 782};
    ui_clear_rect_fast(fb, panel);
    ui_hairline(fb, 782, panel.x, panel.width, UI_GRAY_BLACK);
    ui_product_title(fb, (EpdRect){UI_MARGIN, 810, ui_content_width() - 180, 46}, title, 34, 1);
    char size[32]; snprintf(size, sizeof(size), "%d px", px);
    ui_text(fb, ui_content_right(), 816, 30, size, EPD_DRAW_ALIGN_RIGHT, false);
    ui_text(fb, UI_MARGIN, 864, 24, sizes ? "当前正文预览 · 调整保持同一位置" : "阅读工具 · 中键长按打开导航", EPD_DRAW_ALIGN_LEFT, false);
    const char* labels[] = {"目录 / 书签", "添加书签", "字号", "更多设置", "清除残影", "书架"};
    const char* size_labels[] = {"返回工具", "字号 −", "字号 +", "正文字体", "更多设置", "书架"};
    for (int i = 0; i < 6; ++i) {
        EpdRect r = ui_product_tool_rect(i);
        if (pressed == 200 + i) ui_draw_pressed_round_rect(fb, r, 4);
        else ui_draw_round_rect(fb, r, 4, UI_GRAY_BLACK);
        ui_text_vc(fb, r.x + r.width / 2, r.y + r.height / 2, 32, sizes ? size_labels[i] : labels[i], EPD_DRAW_ALIGN_CENTER, false);
    }
    EpdRect back = ui_product_reader_return_rect(1);
    ui_clear_rect_fast(fb, (EpdRect){UI_MARGIN, UI_BAR_TOP, ui_content_width() - 80, UI_BAR_H});
    if (!sizes) {
        EpdRect remote = ui_product_reader_return_rect(0);
        if (pressed == 207) ui_draw_pressed_round_rect(fb, remote, UI_BTN_RADIUS);
        ui_draw_button(fb, remote, "蓝牙翻页器", false);
    }
    if (pressed == 206) ui_draw_pressed_round_rect(fb, back, UI_BTN_RADIUS);
    ui_draw_button(fb, back, "返回阅读", false);
}
EpdRect ui_product_reader_preview_rect(void) { return (EpdRect){UI_MARGIN, 112, ui_content_width(), 650}; }
EpdRect ui_product_reader_return_rect(int index) { return ui_bar_rect(index, 2); }
void ui_product_reader_tools(uint8_t* fb, const char* title, int px, bool night, int pressed) {
    (void)night; reader_tools(fb, title, px, false, pressed);
}
void ui_product_reader_sizes(uint8_t* fb, const char* title, int px, int pressed) {
    reader_tools(fb, title, px, true, pressed);
}
EpdRect ui_product_reader_body(bool missing_glyphs) {
    int top = missing_glyphs ? 88 : 24;
    return (EpdRect){UI_MARGIN, top, ui_content_width(), UI_BAR_TOP - 8 - top};
}
void ui_product_reader_chrome(uint8_t* fb, const char* title, unsigned page, unsigned pages,
                             unsigned percent, bool missing_glyphs, const char* status, bool bar,
                             const uint16_t* ticks, int tick_count) {
    if (missing_glyphs) {
        ui_text(fb, UI_MARGIN, 24, 26, "本章有缺字 · 点此选择完整字体", EPD_DRAW_ALIGN_LEFT, false);
        ui_hairline(fb, 76, UI_MARGIN, ui_content_width(), UI_GRAY_LIGHT);
    }
    EpdRect track = ui_bar_rect(0, 1);
    // 标题独占首行；次行左侧时钟/电量，右侧页码/全书百分比，按实测宽度留间隙。
    // Title owns the first row; clock/battery sit below left, page/book percent right, with measured spacing.
    ui_product_title(fb, (EpdRect){track.x, track.y + 8, track.width, 36}, title, 28, 1);
    char label[48];
    if (!bar && pages) snprintf(label, sizeof(label), "%u/%u", page, pages);
    else if (!bar) snprintf(label, sizeof(label), "%u/…", page);
    else if (pages) snprintf(label, sizeof(label), "%u/%u · %u%%", page, pages, percent);
    else snprintf(label, sizeof(label), "%u/… · %u%%", page, percent);
    int label_px = 26;
    while (label_px > 20 && ttf_text_width_px(label_px, label) > track.width) label_px -= 2;
    int status_width = track.width - ttf_text_width_px(label_px, label) - 20;
    if (status && status[0] && status_width > 26)
        ui_product_title(fb, (EpdRect){track.x, track.y + 54, status_width, 36}, status, 26, 1);
    ui_text(fb, track.x + track.width, track.y + 54, label_px, label, EPD_DRAW_ALIGN_RIGHT, false);
    // 进度轨道（开百分比时）：浅灰底线 + 黑色已读段 + 章节起点刻度（千分比位置）。
    // Progress track (with percent on): a light baseline, the black read segment and
    // chapter-start ticks at permille positions.
    if (bar) {
        int y = track.y + 47;
        epd_fill_rect((EpdRect){track.x, y, track.width, 3}, UI_GRAY_LIGHT, fb);
        int fill = (int)((int64_t)track.width * (percent > 100 ? 100 : percent) / 100);
        if (fill > 0) epd_fill_rect((EpdRect){track.x, y, fill, 3}, UI_GRAY_BLACK, fb);
        for (int i = 0; ticks && i < tick_count; ++i) {
            if (ticks[i] >= 1000) continue;
            int x = track.x + (int)((int64_t)track.width * ticks[i] / 1000);
            epd_fill_rect((EpdRect){x, y - 3, 2, 9}, UI_GRAY_BLACK, fb);
        }
    }
    ui_draw_menu_handle(fb, false);
}

/* ---- 锁屏密码键盘 / Lock PIN keypad ---- */
// 1..9 三行、0/清空/退格一行；绘制与命中共用几何。/ 1..9 in three rows plus 0/clear/backspace; drawing and hits share geometry.
static EpdRect lock_key_rect(int index) {
    if (index <= 8) return ui_grid_rect(index % 3, 3, index / 3, 372, 104);
    if (index == 9) return ui_grid_rect(1, 3, 3, 372, 104);
    if (index == 10) return ui_grid_rect(0, 3, 3, 372, 104);
    return ui_grid_rect(2, 3, 3, 372, 104);
}
static void pin_digit(uint8_t* fb, EpdRect rect, int digit) {
    char label[2] = {(char)('0' + digit), 0};
    if (ttf_font_prepare_text_px(44, label)) {
        ui_text_vc(fb, rect.x + rect.width / 2, rect.y + rect.height / 2, 44, label, EPD_DRAW_ALIGN_CENTER, false);
        return;
    }
    // 应急点阵只用栈和常量，不依赖字体、缓存或外部存储。
    // Emergency dots use stack/constants only, without fonts, caches or external storage.
    static const uint8_t columns[10][5] = {
        {62,81,73,69,62},{0,66,127,64,0},{66,97,81,73,70},{33,65,69,75,49},{24,20,18,127,16},
        {39,69,69,69,57},{60,74,73,73,48},{1,113,9,5,3},{54,73,73,73,54},{6,73,73,41,30}
    };
    int x0 = rect.x + (rect.width - 25) / 2, y0 = rect.y + (rect.height - 35) / 2;
    for (int x = 0; x < 5; ++x) for (int y = 0; y < 7; ++y)
        if (columns[digit][x] & (1u << y)) epd_fill_rect((EpdRect){x0+x*5,y0+y*5,5,5}, UI_GRAY_BLACK, fb);
}
static void pin_edit_button(uint8_t* fb, EpdRect rect, const char* label, bool backspace) {
    if (ttf_font_prepare_text_px(UI_PX_BTN, label)) {
        ui_draw_button(fb, rect, label, true);
        return;
    }
    ui_draw_selected_round_rect(fb, rect, UI_BTN_RADIUS);
    int cx = rect.x + rect.width / 2, cy = rect.y + rect.height / 2;
    if (backspace) {
        epd_draw_line(cx-22,cy,cx-8,cy-14,UI_GRAY_BLACK,fb);
        epd_draw_line(cx-22,cy,cx-8,cy+14,UI_GRAY_BLACK,fb);
        epd_draw_line(cx-22,cy,cx+22,cy,UI_GRAY_BLACK,fb);
    } else {
        epd_draw_line(cx-14,cy-14,cx+14,cy+14,UI_GRAY_BLACK,fb);
        epd_draw_line(cx-14,cy+14,cx+14,cy-14,UI_GRAY_BLACK,fb);
    }
}
void ui_product_lock_keypad_body(uint8_t* fb, const char* message, unsigned digits) {
    // 先清圆点、键位与提示带再重画：盖在按下灰底上调用时也能完整恢复常态。
    // Clear the dots, keys and hint band first: restores normal state even when
    // called over a pressed gray bed.
    ui_clear_rect_fast(fb, (EpdRect){0, 210, UI_LOCK_WIDTH, 830});
    for (int i = 0; i < 4; ++i) {
        EpdRect dot = {(UI_LOCK_WIDTH - 336) / 2 + i * 100, 224, 36, 36};
        if ((unsigned)i < digits) ui_fill_round_rect(fb, dot, 18, UI_GRAY_BLACK);
        else ui_draw_round_rect(fb, dot, 18, UI_GRAY_BLACK);
    }
    for (int digit = 1; digit <= 9; ++digit) {
        EpdRect r = lock_key_rect(digit - 1);
        ui_draw_round_rect(fb, r, UI_BTN_RADIUS, UI_GRAY_BLACK);
        pin_digit(fb, r, digit);
    }
    EpdRect zero = lock_key_rect(9);
    ui_draw_round_rect(fb, zero, UI_BTN_RADIUS, UI_GRAY_BLACK);
    pin_digit(fb, zero, 0);
    pin_edit_button(fb, lock_key_rect(10), "清空", false);
    pin_edit_button(fb, lock_key_rect(11), "退格", true);
    if (message && message[0])
        ui_text(fb, UI_LOCK_WIDTH / 2, 986, 30, message, EPD_DRAW_ALIGN_CENTER, false);
    else
        ui_text(fb, UI_LOCK_WIDTH / 2, 986, 26, "4 位数字 · 输完自动校验", EPD_DRAW_ALIGN_CENTER, false);
}
void ui_product_lock_keypad(uint8_t* fb, const char* title, const char* message, unsigned digits, bool back) {
    ui_clear_page(fb);
    ui_text(fb, UI_LOCK_WIDTH / 2, 128, 44, title, EPD_DRAW_ALIGN_CENTER, false);
    ui_product_lock_keypad_body(fb, message, digits);
    if (back) ui_draw_button(fb, (EpdRect){470, 68, 174, 68}, "返回", false);
}
int ui_product_lock_keypad_hit(uint16_t x, uint16_t y) {
    for (int digit = 1; digit <= 9; ++digit)
        if (ui_rect_hit(lock_key_rect(digit - 1), x, y)) return digit;
    if (ui_rect_hit(lock_key_rect(9), x, y)) return 0;
    if (ui_rect_hit(lock_key_rect(10), x, y)) return 10;
    if (ui_rect_hit(lock_key_rect(11), x, y)) return 11;
    return -1;
}
// 命中码转几何下标：1..9→0..8，0→9，10/11 原样。/ Hit code to geometry index: 1..9→0..8, 0→9, 10/11 as-is.
static int lock_key_index(int key) {
    if (key >= 1 && key <= 9) return key - 1;
    if (key == 0) return 9;
    return key;
}
EpdRect ui_product_lock_key_rect(int key) {
    return lock_key_rect(lock_key_index(key));
}
void ui_product_lock_key(uint8_t* fb, int key, bool pressed) {
    if (key < 0 || key > 11) return;
    EpdRect r = lock_key_rect(lock_key_index(key));
    ui_clear_rect_fast(fb, r);
    if (key < 10) {
        if (pressed) ui_draw_pressed_round_rect(fb, r, UI_BTN_RADIUS);
        else ui_draw_round_rect(fb, r, UI_BTN_RADIUS, UI_GRAY_BLACK);
        char label[2] = {(char)('0' + key), 0};
        ui_text_vc(fb, r.x + r.width / 2, r.y + r.height / 2, 44, label, EPD_DRAW_ALIGN_CENTER, false);
    } else if (pressed) {
        // 清空/退格常态是实底按钮，按下改灰底粗边保持可辨。/ Action keys invert to the gray pressed bed for contrast.
        ui_draw_pressed_round_rect(fb, r, UI_BTN_RADIUS);
        ui_text_vc(fb, r.x + r.width / 2, r.y + r.height / 2, UI_PX_BTN, key == 10 ? "清空" : "退格",
                   EPD_DRAW_ALIGN_CENTER, false);
    } else {
        ui_draw_button(fb, r, key == 10 ? "清空" : "退格", true);
    }
}
