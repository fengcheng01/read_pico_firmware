#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 mindreset
# SPDX-License-Identifier: Apache-2.0
"""中文：真实 TTF 管线的缺标点回退、度量、预热、字重和缓存回归。
English: Missing-punctuation fallback, metrics, prewarm, weight and cache through the real TTF pipeline.
"""
from pathlib import Path
import subprocess
import tempfile

from fontTools.fontBuilder import FontBuilder
from fontTools.pens.ttGlyphPen import TTGlyphPen
from fontTools.ttLib import TTFont

ROOT = Path(__file__).resolve().parents[1]

STUBS = {
    "epdiy.h": """
#pragma once
#include <stdint.h>
enum EpdFontFlags { EPD_DRAW_ALIGN_LEFT = 0, EPD_DRAW_ALIGN_CENTER = 1, EPD_DRAW_ALIGN_RIGHT = 2 };
void epd_draw_pixel(int x, int y, uint8_t color, uint8_t* framebuffer);
""",
    "esp_err.h": """
#pragma once
typedef int esp_err_t;
#define ESP_OK 0
#define ESP_FAIL -1
#define ESP_ERR_NO_MEM 0x101
#define ESP_ERR_INVALID_STATE 0x103
#define ESP_ERR_INVALID_SIZE 0x104
#define ESP_ERR_NOT_FOUND 0x105
#define ESP_ERR_INVALID_RESPONSE 0x108
""",
    "esp_heap_caps.h": """
#pragma once
#include <stdlib.h>
#define MALLOC_CAP_SPIRAM 1
#define MALLOC_CAP_8BIT 2
extern size_t host_largest_block;
static inline void* heap_caps_malloc(size_t n, unsigned caps) { (void)caps; return malloc(n); }
static inline void* heap_caps_calloc(size_t n, size_t size, unsigned caps) { (void)caps; return calloc(n, size); }
static inline void heap_caps_free(void* p) { free(p); }
static inline size_t heap_caps_get_largest_free_block(unsigned caps) { (void)caps; return host_largest_block; }
""",
    "esp_log.h": """
#pragma once
#include <stdio.h>
#define ESP_LOGI(tag, ...) do { (void)(tag); if (0) fprintf(stderr, __VA_ARGS__); } while (0)
#define ESP_LOGW(tag, ...) ESP_LOGI(tag, __VA_ARGS__)
#define ESP_LOGE(tag, ...) ESP_LOGI(tag, __VA_ARGS__)
""",
    "esp_timer.h": """
#pragma once
#include <stdint.h>
static inline int64_t esp_timer_get_time(void) { static int64_t tick; return ++tick; }
""",
}

UNIT = r'''
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
#include "main/font/ttf_font.c"

const uint8_t builtin_pack_start[] asm("_binary_builtin_pack_start") = {0};
const uint8_t builtin_pack_end[] asm("_binary_builtin_pack_end") = {0};
const char* app_settings_font_path(void) { return TTF_FONT_BUILTIN; }
size_t asset_pack_size(const uint8_t* data, size_t size) { (void)data; (void)size; return 0; }
bool asset_pack_unpack(const uint8_t* data, size_t size, uint8_t* out, size_t cap) {
    (void)data; (void)size; (void)out; (void)cap; return false;
}

#define FB_WIDTH 256
#define FB_HEIGHT 160
#define FB_BYTES (FB_WIDTH * FB_HEIGHT / 2)
static uint8_t actual[FB_BYTES], expected[FB_BYTES], light[FB_BYTES];
void epd_draw_pixel(int x, int y, uint8_t color, uint8_t* fb) {
    if (x < 0 || x >= FB_WIDTH || y < 0 || y >= FB_HEIGHT) return;
    size_t index = ((size_t)y * FB_WIDTH + (unsigned)x) / 2;
    if (x & 1) fb[index] = (fb[index] & 0x0f) | color;
    else fb[index] = (fb[index] & 0xf0) | (color >> 4);
}
static void draw(uint8_t* fb, const char* text, int px, int align, bool bw) {
    memset(fb, 0xff, FB_BYTES);
    if (bw) ttf_draw_text_px_bw(fb, 120, 100, px, text, align, 0, 15);
    else ttf_draw_text_px(fb, 120, 100, px, text, align, 0, 15);
}
static void check_resident_metrics(const char* text, int gid, int px) {
    int advance, lsb;
    stbtt_GetGlyphHMetrics(&font_info, gid, &advance, &lsb);
    int width = (int)lroundf(advance * stbtt_ScaleForPixelHeight(&font_info, (float)px));
    int glyf = font_info.glyf, loca = font_info.loca;
    font_info.glyf = font_info.loca = INT_MAX;
    assert(ttf_text_width_px(px, text) == width);
    assert(ttf_font_supports_text(text, strlen(text)) == (gid != 0));
    font_info.glyf = glyf; font_info.loca = loca;
}
static void check_alias_draw(int px, int align, bool bw, bool equal) {
    draw(actual, "A\xe2\x8b\xaf" "A", px, align, bw);
    draw(expected, "A\xe2\x80\xa6" "A", px, align, bw);
    assert((memcmp(actual, expected, FB_BYTES) == 0) == equal);
}
static void check_stream_prewarm(int gid) {
    assert(!glyf_ram);
    ttf_font_cache_clear();
    warm_text_io(43, "\xe2\x8b\xaf");
    uint32_t base = (file_glyf_off + file_glyph_off(gid)) & ~(uint32_t)(TTF_IO_BLOCK - 1);
    assert(io_find(base) >= 0);
    if (gid != 0) {
        uint32_t missing = (file_glyf_off + file_glyph_off(0)) & ~(uint32_t)(TTF_IO_BLOCK - 1);
        assert(base != missing);
        assert(io_find(missing) < 0);
    }
    assert(cache_lookup(0x22ef, 43) == NULL);
}
static void check_fixture(const char* path, int mode) {
    host_largest_block = 0;
    assert(ttf_font_open(path) == ESP_OK);
    int native = stbtt_FindGlyphIndex(&font_info, 0x22ef);
    int alias = stbtt_FindGlyphIndex(&font_info, 0x2026);
    assert((native != 0) == (mode == 0 || mode == 3));
    assert((alias != 0) == (mode == 0 || mode == 1));
    int gid = native ? native : alias;
    assert(resolve_glyph_index(0x22ef) == gid);
    assert(resolve_glyph_index(0x2e3a) == 0);
    assert(!ttf_font_supports_text("\xe2\xb8\xba", 3));
    check_stream_prewarm(gid);
    for (int px = 12; px <= 80; px += 4) {
        check_resident_metrics("\xe2\x8b\xaf", gid, px);
        if (alias) {
            for (int align = 0; align <= 2; align++) {
                check_alias_draw(px, align, false, !native);
                check_alias_draw(px, align, true, !native);
            }
            int above, below, alias_above, alias_below;
            ttf_measure_line_px(px, "\xe2\x8b\xaf", &above, &below);
            ttf_measure_line_px(px, "\xe2\x80\xa6", &alias_above, &alias_below);
            if (!native) assert(above == alias_above && below == alias_below);
        } else if (!native) {
            for (int align = 0; align <= 2; align++) {
                for (int bw = 0; bw <= 1; bw++) {
                    draw(actual, "A\xe2\x8b\xaf" "A", px, align, bw);
                    draw(expected, "A\xf4\x8f\xbf\xbf" "A", px, align, bw);
                    assert(memcmp(actual, expected, FB_BYTES) == 0);
                }
            }
        }
    }
    ttf_font_cache_clear();
    ttf_bench_stats_t first, repeat;
    ttf_bench_begin(); draw(actual, "A\xe2\x8b\xaf" "A", 43, 0, false); ttf_bench_end(&first);
    ttf_bench_begin(); draw(expected, "A\xe2\x8b\xaf" "A", 43, 0, false); ttf_bench_end(&repeat);
    assert(first.misses == 2 && repeat.misses == 0 && repeat.hits == 3);
    assert(memcmp(actual, expected, FB_BYTES) == 0);
    const glyph_entry_t* entry = cache_lookup(0x22ef, 43);
    assert(entry && entry->advance_x == ttf_text_width_px(43, "\xe2\x8b\xaf"));
    assert(entry->codepoint == 0x22ef);
    assert(entry->weight == ttf_get_weight());
    ttf_font_unload();
}
static void check_variable_font(const char* path) {
    host_largest_block = 64u * 1024u * 1024u;
    assert(ttf_font_open(path) == ESP_OK && gvar_ready);
    assert(stbtt_FindGlyphIndex(&font_info, 0x22ef) == 0);
    assert(resolve_glyph_index(0x22ef) == stbtt_FindGlyphIndex(&font_info, 0x2026));
    ttf_set_weight(300);
    check_alias_draw(67, 0, false, true);
    memcpy(light, actual, FB_BYTES);
    const glyph_entry_t* normal = cache_lookup(0x22ef, 67);
    assert(normal && normal->weight == 300);
    ttf_set_weight(700);
    check_alias_draw(67, 0, false, true);
    const glyph_entry_t* heavy = cache_lookup(0x22ef, 67);
    assert(heavy && heavy != normal && heavy->weight == 700);
    assert(memcmp(light, actual, FB_BYTES) != 0);
    ttf_set_weight(300);
    assert(cache_lookup(0x22ef, 67) == normal);
    check_alias_draw(67, 0, true, true);
    io_unmap();
    ttf_set_weight(700);
    int gid = resolve_glyph_index(0x22ef);
    check_stream_prewarm(gid);
    uint32_t a = gvar_glyph_off[gid], b = gvar_glyph_off[gid + 1];
    assert(b > a);
    uint32_t base = (file_gvar_off + gvar_data_array_off + a) & ~(uint32_t)(TTF_IO_BLOCK - 1);
    assert(io_find(base) >= 0);
    check_alias_draw(67, 0, false, true);
    ttf_font_unload();
}
int main(int argc, char** argv) {
    assert(argc == 6);
    for (int mode = 0; mode < 4; mode++) check_fixture(argv[mode + 1], mode);
    check_variable_font(argv[5]);
    puts("font punctuation: native/alias/missing, streamed prewarm, 18 sizes, AA/BW and all alignments, real LRU and gvar passed");
}
'''


def rectangle(x0, y0, x1, y1):
    pen = TTGlyphPen(None)
    pen.moveTo((x0, y0))
    pen.lineTo((x1, y0))
    pen.lineTo((x1, y1))
    pen.lineTo((x0, y1))
    pen.closePath()
    return pen.glyph()


def fixture(path, native, alias):
    # 把目标字形放在缺字所在扇区之外，确认真实预热选择了替代字形。
    # Place target glyphs beyond the missing-glyph sector to verify real prewarm selection.
    order = [".notdef", "A"] + [f"pad{i}" for i in range(240)] + ["ellipsis", "midline"]
    font = FontBuilder(1000, isTTF=True)
    font.setupGlyphOrder(order)
    cmap = {0x41: "A"}
    if native:
        cmap[0x22EF] = "midline"
    if alias:
        cmap[0x2026] = "ellipsis"
    font.setupCharacterMap(cmap)
    glyphs = {name: rectangle(80, 100, 180, 300) for name in order}
    glyphs[".notdef"] = rectangle(30, 0, 450, 700)
    glyphs["A"] = rectangle(50, 0, 500, 700)
    glyphs["ellipsis"] = rectangle(40, 0, 300, 100)
    glyphs["midline"] = rectangle(80, 350, 750, 450)
    font.setupGlyf(glyphs)
    metrics = {name: (600, 0) for name in order}
    metrics.update({"A": (650, 0), "ellipsis": (400, 0), "midline": (900, 0)})
    font.setupHorizontalMetrics(metrics)
    font.setupHorizontalHeader(ascent=800, descent=-200)
    font.setupOS2(sTypoAscender=800, sTypoDescender=-200, usWinAscent=800, usWinDescent=200)
    font.setupNameTable({"familyName": "Pico punctuation test", "styleName": "Regular"})
    font.setupPost()
    font.save(path)


def main():
    with tempfile.TemporaryDirectory(prefix="font-punctuation-") as temp:
        work = Path(temp)
        for name, content in STUBS.items():
            (work / name).write_text(content, encoding="utf-8")
        paths = []
        for mode, (native, alias) in enumerate(((True, True), (False, True), (False, False), (True, False))):
            path = work / f"case{mode}.ttf"
            fixture(path, native, alias)
            paths.append(path)
        variable = TTFont(ROOT / "sdcard/fonts/ChillDuanSansVF.ttf")
        assert 0x2026 in variable.getBestCmap()
        for table in variable["cmap"].tables:
            if table.isUnicode():
                table.cmap.pop(0x22EF, None)
        variable_path = work / "alias-variable.ttf"
        variable.save(variable_path)
        variable.close()
        c = work / "test.c"
        c.write_text(UNIT, encoding="utf-8")
        exe = work / "test"
        subprocess.run(["cc", "-std=gnu11", "-Wall", "-Wextra", "-Werror",
                        "-fsanitize=address,undefined", "-fno-omit-frame-pointer",
                        "-I" + str(work), "-I" + str(ROOT), "-I" + str(ROOT / "main"),
                        str(c), "-lm", "-o", str(exe)], cwd=ROOT, check=True)
        subprocess.run([str(exe), *map(str, paths), str(variable_path)], check=True)


if __name__ == "__main__":
    main()
