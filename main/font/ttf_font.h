/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 *
 * 可变 TTF 字形缓存：卡上字体或内置子集，按字重光栅化后画到 framebuffer。
 *
 * Variable TTF glyph cache: SD fonts or the built-in subset, rasterized
 * at the current weight onto the framebuffer.
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "epdiy.h"
#include "esp_err.h"

#define TTF_SIZE_SMALL 0
#define TTF_SIZE_LARGE 1

#define TTF_FONT_MAX 24
#define TTF_FONT_NAME_MAX 64
#define TTF_FONT_PATH_MAX 160

typedef struct {
    char name[TTF_FONT_NAME_MAX];
    char path[TTF_FONT_PATH_MAX];
} ttf_font_item_t;

typedef struct {
    uint32_t glyphs;
    uint32_t hits;
    uint32_t misses;
    int64_t read_us;
    int64_t raster_us;
    int64_t total_us;
} ttf_bench_stats_t;

#define TTF_FONT_BUILTIN "builtin"

/// 灰阶字覆盖率伽马。小于 1 抬中间覆盖率，抗锯齿边缘更深；满墨仍是 0。
/// Coverage gamma for gray glyphs. Below 1 lifts mid coverage so AA edges are darker; full ink stays 0.
#ifndef TTF_COVER_GAMMA
#define TTF_COVER_GAMMA 0.6f
#endif

esp_err_t ttf_font_init(void);
esp_err_t ttf_font_open(const char* path);
esp_err_t ttf_font_open_builtin(void);
/// 切换内置成功才暂停SD打开；失败保留原字体并拒绝传输，停服恢复所选字体。/ Suspend SD opens only after a successful built-in switch; failures retain the original font and gate transfer, restoring the saved font after shutdown.
esp_err_t ttf_font_suspend_sd(bool suspend);
/// HTTP停服后解除暂停并同步恢复所选字体，失败安全回退。/ After HTTP stops, resume and restore the saved font synchronously with safe failure fallback.
esp_err_t ttf_font_resume_sd(void);
bool ttf_font_is_builtin(void);
bool ttf_font_path_is_builtin(const char* path);
void ttf_font_unload(void);
/// 同任务空闲边界回收字形与可选映射，保留可用字库和设置。/ Reclaim glyphs/optional maps at an owner-task idle boundary, retaining the usable source and preferences.
void ttf_font_reclaim(void);
int ttf_font_scan(void);
int ttf_font_count(void);
const ttf_font_item_t* ttf_font_item(int index);
const char* ttf_font_path(void);
const char* ttf_font_display_name(void);
bool ttf_font_ready(void);
/// 仅检查驻留 cmap，不读字形轮廓；输入长度有界。/ Check resident cmap only, without outline IO, over bounded input.
/// 原字优先；缺失的 ⋯ 可借同字体 …，与测量、预热和绘制一致，不改输入文本。
/// Prefer native glyphs; absent ⋯ may borrow the same font's … consistently with metrics, prewarm and drawing, without rewriting input.
bool ttf_font_supports_text(const char* text, size_t len);
/// 关键控件绘制前实际准备字形，失败可改画无字库图形。/ Prepare actual glyphs for critical controls so failures can use font-independent graphics.
bool ttf_font_prepare_text_px(int pixel_height, const char* text);
int ttf_ascender(int size);
int ttf_ascender_px(int pixel_height);

void ttf_draw_text(
    uint8_t* framebuffer, int x, int y, int size, const char* text,
    enum EpdFontFlags align, uint8_t fg, uint8_t bg
);

void ttf_draw_text_px(
    uint8_t* framebuffer, int x, int y, int pixel_height, const char* text,
    enum EpdFontFlags align, uint8_t fg, uint8_t bg
);
/// 覆盖率过半才落墨，像素只有 fg/bg。/ Ink only when coverage is over half; pixels are fg/bg only.
void ttf_draw_text_px_bw(
    uint8_t* framebuffer, int x, int y, int pixel_height, const char* text,
    enum EpdFontFlags align, uint8_t fg, uint8_t bg
);

void ttf_measure_line(int size, const char* text, int* above, int* below);
void ttf_measure_line_px(int pixel_height, const char* text, int* above, int* below);
int ttf_text_width_px(int pixel_height, const char* text);
/// 独立覆盖率微调，每步1/16像素，-16..16；不改原字体字重或字宽，调用方持有绘图锁。
/// Independent coverage tuning in 1/16-pixel steps, -16..16; preserve font weight/advances; hold the draw lock.
void ttf_set_stroke(int steps);
int ttf_get_stroke(void);
/// 控制曲线展平精度，仅影响新光栅与缓存身份，不改度量。/ Select curve flattening precision, affecting raster/cache identity only, never metrics.
void ttf_set_precise_curves(bool precise);
bool ttf_get_precise_curves(void);
void ttf_set_weight(int wght);
int ttf_get_weight(void);

void ttf_font_cache_clear(void);
void ttf_bench_begin(void);
void ttf_bench_end(ttf_bench_stats_t* out);
