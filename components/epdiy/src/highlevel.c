/**
 * High-level API implementation for epdiy.
 */

#include <assert.h>
#include <esp_heap_caps.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <esp_types.h>
#include <string.h>

#include "epd_highlevel.h"
#include "epdiy.h"

#ifndef _swap_int
#define _swap_int(a, b) \
    {                   \
        int t = a;      \
        a = b;          \
        b = t;          \
    }
#endif

static bool already_initialized = 0;

EpdiyHighlevelState epd_hl_init(const EpdWaveform* waveform) {
    assert(!already_initialized);
    if (waveform == NULL) {
        waveform = epd_get_display()->default_waveform;
    }

    int fb_size = epd_width() / 2 * epd_height();

#if !(                                                                             \
    defined(CONFIG_ESP32_SPIRAM_SUPPORT) || defined(CONFIG_ESP32S3_SPIRAM_SUPPORT) \
    || defined(CONFIG_SPIRAM)                                                      \
)
    ESP_LOGW(
        "EPDiy", "Please enable PSRAM for the ESP32 (menuconfig→ Component config→ ESP32-specific)"
    );
#endif
    EpdiyHighlevelState state;
    state.back_fb = heap_caps_aligned_alloc(16, fb_size, MALLOC_CAP_SPIRAM);
    assert(state.back_fb != NULL);
    state.front_fb = heap_caps_aligned_alloc(16, fb_size, MALLOC_CAP_SPIRAM);
    assert(state.front_fb != NULL);
    state.difference_fb = heap_caps_aligned_alloc(16, 2 * fb_size, MALLOC_CAP_SPIRAM);
    assert(state.difference_fb != NULL);
    state.dirty_lines = malloc(epd_height() * sizeof(bool));
    assert(state.dirty_lines != NULL);
    state.dirty_columns
        = heap_caps_aligned_alloc(16, epd_width() / 2, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    assert(state.dirty_columns != NULL);
    state.waveform = waveform;

    memset(state.front_fb, 0xFF, fb_size);
    memset(state.back_fb, 0xFF, fb_size);

    already_initialized = true;
    return state;
}

uint8_t* epd_hl_get_framebuffer(EpdiyHighlevelState* state) {
    assert(state != NULL);
    return state->front_fb;
}

// 最近一次更新的耗时分解，给上层做性能分析用。
static int s_last_diff_ms, s_last_draw_ms, s_last_copy_ms;

static void hl_record_timing(int diff_ms, int draw_ms, int copy_ms) {
    s_last_diff_ms = diff_ms;
    s_last_draw_ms = draw_ms;
    s_last_copy_ms = copy_ms;
}

void epd_hl_last_timing(int* diff_ms, int* draw_ms, int* copy_ms) {
    if (diff_ms) *diff_ms = s_last_diff_ms;
    if (draw_ms) *draw_ms = s_last_draw_ms;
    if (copy_ms) *copy_ms = s_last_copy_ms;
}

enum EpdDrawError epd_hl_update_screen(EpdiyHighlevelState* state, enum EpdDrawMode mode, int temperature) {
    return epd_hl_update_area(state, mode, temperature, epd_full_screen());
}

// 在源帧还在缓存时构造动作码，避免写出PSRAM差分图后再读回修改。
// Build selectors while source frames are cached, avoiding a PSRAM diff write/read/modify pass.
#if defined(__GNUC__) && !defined(__clang__)
__attribute__((optimize("O3")))
#endif
static void hl_prepare_selective(EpdiyHighlevelState* state, const uint8_t* white_mask) {
    size_t pixels = (size_t)epd_width() * epd_height();
    uint8_t present[256] = {0};
    uint32_t previous = 0;
    bool have_previous = false;
    static const uint32_t white_codes[16] = {
        0,0x11,0x1100,0x1111,0x110000,0x110011,0x111100,0x111111,
        0x11000000,0x11000011,0x11001100,0x11001111,0x11110000,0x11110011,0x11111100,0x11111111
    };
    size_t i = 0;
    for (; i + 4 <= pixels; i += 4) {
        unsigned a = state->front_fb[i / 2], b = state->front_fb[i / 2 + 1];
        unsigned c = state->back_fb[i / 2], d = state->back_fb[i / 2 + 1];
        unsigned mask = white_mask ? (white_mask[i / 8] >> (i % 8)) & 15 : 0;
        uint32_t codes;
        // 产品不补擦时，整组相同的字、细线和白底都直接保持。
        // Without product cleanup, identical glyph/guide/white groups all take the hold fast path.
        if (a == c && b == d && mask == 0) codes = 0xeeeeeeeeu;
        else if (a == 255 && b == 255 && c == 255 && d == 255) codes = 0xeeeeeeeeu | white_codes[mask];
        else {
            codes = 0;
            unsigned to = a | b << 8, from = c | d << 8;
            for (int j = 0; j < 4; ++j) {
                unsigned goal = to >> (j * 4) & 15, prior = from >> (j * 4) & 15;
                unsigned code = goal == prior ? (goal == 15 && (mask & (1u << j)) ? 255 : 0xee) : goal << 4 | prior;
                codes |= code << (j * 8);
            }
        }
        memcpy(state->difference_fb + i, &codes, 4);
        if (!have_previous || codes != previous) {
            for (int j = 0; j < 4; ++j) present[codes >> (j * 8) & 255] = 1;
            previous = codes; have_previous = true;
        }
    }
    for (; i < pixels; ++i) {
        unsigned goal = state->front_fb[i / 2] >> ((i % 2) * 4) & 15;
        unsigned prior = state->back_fb[i / 2] >> ((i % 2) * 4) & 15;
        bool erase = goal == 15 && white_mask && (white_mask[i / 8] & (1u << (i % 8)));
        unsigned code = goal == prior ? (erase ? 255 : 0xee) : goal << 4 | prior;
        state->difference_fb[i] = code; present[code] = 1;
    }
    epd_leading_skip_set_present(state->difference_fb, present);
}

static enum EpdDrawError hl_update_screen_full(
    EpdiyHighlevelState* state, enum EpdDrawMode mode, int temperature,
    bool selective, const uint8_t* white_mask
) {
    assert(state != NULL);

    EpdRect area = epd_full_screen();
    uint32_t ts = esp_timer_get_time() / 1000;

    if (selective) hl_prepare_selective(state, white_mask);
    else epd_difference_image_cropped(
        state->front_fb, state->back_fb, area, state->difference_fb,
        state->dirty_lines, state->dirty_columns
    );

    int fb_height = epd_height();
    int col_bytes = epd_width() / 2;
    for (int y = 0; y < fb_height; y++) {
        state->dirty_lines[y] = true;
    }
    memset(state->dirty_columns, 0xFF, col_bytes);

    uint32_t t1 = esp_timer_get_time() / 1000;

    EpdRect full = epd_full_screen();
    enum EpdDrawError err = epd_draw_base(
        full,
        state->difference_fb,
        full,
        MODE_PACKING_1PPB_DIFFERENCE | mode,
        temperature,
        state->dirty_lines,
        state->dirty_columns,
        state->waveform
    );

    uint32_t t2 = esp_timer_get_time() / 1000;

    // 失败帧不是显示参考帧，保留成功的旧画面供上层恢复。/ Failed frames are not display baselines; retain the successful old frame for recovery.
    if (err == EPD_DRAW_SUCCESS)
        memcpy(state->back_fb, state->front_fb, (size_t)col_bytes * fb_height);

    uint32_t t3 = esp_timer_get_time() / 1000;
    hl_record_timing(t1 - ts, t2 - t1, t3 - t2);
    ESP_LOGI(
        "epdiy",
        "full diff: %dms, draw: %dms, buffer update: %dms, total: %dms",
        t1 - ts,
        t2 - t1,
        t3 - t2,
        t3 - ts
    );
    return err;
}

enum EpdDrawError epd_hl_update_screen_full(
    EpdiyHighlevelState* state, enum EpdDrawMode mode, int temperature
) {
    return hl_update_screen_full(state, mode, temperature, false, NULL);
}

enum EpdDrawError epd_hl_update_screen_selective(
    EpdiyHighlevelState* state, enum EpdDrawMode mode, int temperature, const uint8_t* white_mask
) {
    return hl_update_screen_full(state, mode, temperature, true, white_mask);
}

enum EpdDrawError epd_hl_update_screen_from_white(
    EpdiyHighlevelState* state, enum EpdDrawMode mode, int temperature
) {
    assert(state != NULL);
    memset(state->back_fb, 0xFF, (size_t)epd_width() / 2 * epd_height());
    return epd_hl_update_screen_full(state, mode, temperature);
}

EpdRect _inverse_rotated_area(uint16_t x, uint16_t y, uint16_t w, uint16_t h) {
    // If partial update uses full screen do not rotate anything
    if (!(x == 0 && y == 0 && epd_width() == w && epd_height() == h)) {
        // invert the current display rotation
        switch (epd_get_rotation()) {
            // 0 landscape: Leave it as is
            case EPD_ROT_LANDSCAPE:
                break;
            // 1 90 ° clockwise
            case EPD_ROT_PORTRAIT:
                _swap_int(x, y);
                _swap_int(w, h);
                x = epd_width() - x - w;
                break;

            case EPD_ROT_INVERTED_LANDSCAPE:
                // 3 180°
                x = epd_width() - x - w;
                y = epd_height() - y - h;
                break;

            case EPD_ROT_INVERTED_PORTRAIT:
                // 3 270 °
                _swap_int(x, y);
                _swap_int(w, h);
                y = epd_height() - y - h;
                break;
        }
    }

    EpdRect rotated = { x, y, w, h };
    return rotated;
}

static enum EpdDrawError hl_update_area(
    EpdiyHighlevelState* state, enum EpdDrawMode mode, int temperature, EpdRect area, bool force_full
) {
    assert(state != NULL);
    // Not right to rotate here since this copies part of buffer directly

    // Check rotation FIX
    EpdRect rotated_area = _inverse_rotated_area(area.x, area.y, area.width, area.height);
    area.x = rotated_area.x;
    area.y = rotated_area.y;
    area.width = rotated_area.width;
    area.height = rotated_area.height;

    uint32_t ts = esp_timer_get_time() / 1000;

    // FIXME: use crop information here, if available
    EpdRect diff_area = epd_difference_image_cropped(
        state->front_fb,
        state->back_fb,
        area,
        state->difference_fb,
        state->dirty_lines,
        state->dirty_columns
    );

    if (force_full) {
        int x_start, x_stop;
        epd_difference_column_range(area, &x_start, &x_stop);
        const int fb_h = epd_height();
        const int y0 = area.y < 0 ? 0 : area.y;
        const int y1 = area.y + area.height > fb_h ? fb_h : area.y + area.height;
        for (int y = y0; y < y1; y++) state->dirty_lines[y] = true;
        if (x_stop > x_start) {
            memset(state->dirty_columns + x_start / 2, 0xFF, (size_t)((x_stop - x_start) / 2));
        }
        diff_area.width = x_stop - x_start;
        diff_area.height = y1 - y0;
    }

    if (diff_area.height == 0 || diff_area.width == 0) {
        epd_leading_skip_discard();
        return EPD_DRAW_SUCCESS;
    }

    uint32_t t1 = esp_timer_get_time() / 1000;

    diff_area.x = 0;
    diff_area.y = 0;
    diff_area.width = epd_width();
    diff_area.height = epd_height();

    enum EpdDrawError err = EPD_DRAW_SUCCESS;
    err = epd_draw_base(
        epd_full_screen(),
        state->difference_fb,
        diff_area,
        MODE_PACKING_1PPB_DIFFERENCE | mode,
        temperature,
        state->dirty_lines,
        state->dirty_columns,
        state->waveform
    );

    uint32_t t2 = esp_timer_get_time() / 1000;

    // 失败时不回写参考帧。/ Do not advance the baseline after a failed draw.
    if (err != EPD_DRAW_SUCCESS) {
        hl_record_timing(t1 - ts, t2 - t1, 0);
        return err;
    }

    // 回写范围和差分实际算过的列段一致：段外的像素没被驱动，back_fb 不能跟着改。
    int x_start, x_stop;
    epd_difference_column_range(area, &x_start, &x_stop);
    diff_area.x = x_start;
    diff_area.y = 0;
    diff_area.width = x_stop - x_start;
    diff_area.height = epd_height();

    int buf_width = epd_width();

    for (int l = diff_area.y; diff_area.width > 0 && l < diff_area.y + diff_area.height; l++) {
        if (state->dirty_lines[l] > 0) {
            uint8_t* lfb = state->front_fb + buf_width / 2 * l;
            uint8_t* lbb = state->back_fb + buf_width / 2 * l;

            int x = diff_area.x;
            int x_last = diff_area.x + diff_area.width - 1;

            if (x % 2) {
                *(lbb + x / 2) = (*(lfb + x / 2) & 0xF0) | (*(lbb + x / 2) & 0x0F);
                x += 1;
            }

            if (!(x_last % 2)) {
                *(lbb + x_last / 2) = (*(lfb + x_last / 2) & 0x0F) | (*(lbb + x_last / 2) & 0xF0);
                x_last -= 1;
            }

            memcpy(lbb + (x / 2), lfb + (x / 2), (x_last - x + 1) / 2);
        }
    }

    uint32_t t3 = esp_timer_get_time() / 1000;
    hl_record_timing(t1 - ts, t2 - t1, t3 - t2);

    ESP_LOGI(
        "epdiy",
        "diff: %dms, draw: %dms, buffer update: %dms, total: %dms",
        t1 - ts,
        t2 - t1,
        t3 - t2,
        t3 - ts
    );
    return err;
}

enum EpdDrawError epd_hl_update_area(
    EpdiyHighlevelState* state, enum EpdDrawMode mode, int temperature, EpdRect area
) {
    return hl_update_area(state, mode, temperature, area, false);
}

enum EpdDrawError epd_hl_update_area_full(
    EpdiyHighlevelState* state, enum EpdDrawMode mode, int temperature, EpdRect area
) {
    return hl_update_area(state, mode, temperature, area, true);
}

void epd_hl_set_all_white(EpdiyHighlevelState* state) {
    assert(state != NULL);
    int fb_size = epd_width() / 2 * epd_height();
    memset(state->front_fb, 0xFF, fb_size);
}

void epd_fullclear(EpdiyHighlevelState* state, int temperature) {
    assert(state != NULL);
    epd_hl_set_all_white(state);
    enum EpdDrawError err = epd_hl_update_screen(state, MODE_GC16, temperature);
    assert(err == EPD_DRAW_SUCCESS);
    epd_clear();
}

void epd_hl_waveform(EpdiyHighlevelState* state, const EpdWaveform* waveform) {
    if (waveform == NULL) {
        waveform = epd_get_display()->default_waveform;
    }
    state->waveform = waveform;
}
