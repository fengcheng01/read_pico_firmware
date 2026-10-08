/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 *
 * 真实 display.c 欠载恢复回归；仅模拟硬件与高层 framebuffer 边界。
 * Regression for real display.c underrun recovery, mocking hardware and high-level framebuffer boundaries only.
 * 冻结：目标画面不得丢失；白色基准只作用于后缓冲。
 * Frozen: Preserve the requested picture; the white baseline affects only the back buffer.
 */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <limits.h>
#include "display.h"
#include "display_pixels.h"
#include "e0470_epaper_waveform.h"
#include "settings.h"

#define FB_BYTES 128
const EpdWaveform E0470_WAVEFORM = {0}, E0470_FOLLOW_WAVEFORM = {1}, E0470_FULL_WAVEFORM = {2}, E0470_TEXTTURN_WAVEFORM = {3}, E0470_NAVIGATION_WAVEFORM = {4}, E0470_NAVIGATION_ENTRY_WAVEFORM = {7};
const EpdWaveform E0470_DIRECT_WAVEFORM = {5}, E0470_WHITE_CLEANUP_WAVEFORM = {6}, E0470_TEXTTURN_NIGHT_WAVEFORM = {8};
const EpdWaveform E0470_NIGHT_BLACK_BOOST_WAVEFORM = {9}, E0470_NIGHT_LOCAL_CLEAN_WAVEFORM = {10};
static uint8_t target[FB_BYTES], presented[FB_BYTES];
static uint8_t expected_old[FB_BYTES];
static bool check_old_at_draw;
static EpdiyHighlevelState* clear_hl;
static uint8_t clear_target[FB_BYTES], clear_prior[FB_BYTES];
static bool check_clean_at_clear;
static int clocks, powerons, clears, draws, full_draws, safe_clock, prefill, poweroffs;
static bool white_baseline, correct_target_at_draw, fail_power, fail_draw;
static bool hv_on;
bool read_pico_rails_on(void) { return hv_on && !fail_power; }
static unsigned cleanup_every = 3;
static uint8_t night_cleanup;
static enum EpdDrawMode last_mode;
static bool last_area;
static const EpdWaveform* last_waveform;
int epd_width(void) { return 16; }
int epd_height(void) { return 16; }
uint8_t app_settings_gc_every(void) { return (uint8_t)cleanup_every; }
uint8_t app_settings_book_night_cleanup(void) { return night_cleanup; }

void read_pico_epd_set_pclk(int mhz) { ++clocks; safe_clock = mhz; }
void read_pico_epd_use_scan(read_pico_epd_scan_t scan) { assert(scan == READ_PICO_EPD_SCAN_FULL); }
void epd_lcd_set_prefill_lines(int lines) { prefill = lines; }
void epd_poweron(void) { ++powerons; hv_on = !fail_power; }
void epd_poweroff(void) { ++poweroffs; hv_on = false; }
void epd_clear(void) {
    assert(hv_on && !fail_power && prefill == 127);
    if (check_clean_at_clear) {
        assert(clear_hl && !memcmp(clear_hl->front_fb, clear_target, FB_BYTES));
        assert(!memcmp(clear_hl->back_fb, clear_prior, FB_BYTES));
    }
    ++clears;
}
int64_t esp_timer_get_time(void) { return 1000000; }

// 与 highlevel.c:307 相同：该 API 清前缓冲，而不是参考后缓冲。
// Match highlevel.c:307: this API clears the front buffer, not the reference back buffer.
void epd_hl_set_all_white(EpdiyHighlevelState* hl) { memset(hl->front_fb, 255, FB_BYTES); }
void epd_hl_waveform(EpdiyHighlevelState* hl, const EpdWaveform* waveform) { hl->waveform = waveform; }
static enum EpdDrawError draw(EpdiyHighlevelState* hl, enum EpdDrawMode mode, int temperature, bool full) {
    assert(temperature == 25);
    if (check_old_at_draw) assert(!memcmp(hl->back_fb, expected_old, FB_BYTES));
    last_mode = mode; last_waveform = hl->waveform;
    ++draws;
    full_draws += full;
    white_baseline = true;
    for (size_t i = 0; i < FB_BYTES; ++i) if (hl->back_fb[i] != 255) white_baseline = false;
    correct_target_at_draw = memcmp(hl->front_fb, target, FB_BYTES) == 0;
    if (fail_draw) return EPD_DRAW_OTHER_ERROR;
    memcpy(presented, hl->front_fb, FB_BYTES);
    memcpy(hl->back_fb, hl->front_fb, FB_BYTES);
    return EPD_DRAW_SUCCESS;
}
enum EpdDrawError epd_hl_update_screen(EpdiyHighlevelState* hl, enum EpdDrawMode mode, int temperature) {
    last_area = false; return draw(hl, mode, temperature, false);
}
enum EpdDrawError epd_hl_update_screen_full(EpdiyHighlevelState* hl, enum EpdDrawMode mode, int temperature) {
    last_area = false; return draw(hl, mode, temperature, true);
}
// 与 highlevel.c:142 相同：白后缓冲后，强制整屏推目标前缓冲。
// Match highlevel.c:142: whiten the back buffer, then force a full update from the target front buffer.
enum EpdDrawError epd_hl_update_screen_from_white(EpdiyHighlevelState* hl, enum EpdDrawMode mode, int temperature) {
    memset(hl->back_fb, 255, FB_BYTES);
    return epd_hl_update_screen_full(hl, mode, temperature);
}

enum EpdDrawError epd_hl_update_area(EpdiyHighlevelState* hl, enum EpdDrawMode mode, int temperature, EpdRect area) {
    (void)area; last_area = true; return draw(hl, mode, temperature, false);
}
enum EpdDrawError epd_hl_update_area_full(EpdiyHighlevelState* hl, enum EpdDrawMode mode, int temperature, EpdRect area) {
    (void)area; last_area = true; return draw(hl, mode, temperature, true);
}
static unsigned mask_draws;
static bool fail_mask, fail_alloc;
static unsigned allocations;
static unsigned post_draws;
static bool fail_post, check_post_selectors;
static uint8_t post_prior[FB_BYTES], post_target[FB_BYTES];
static unsigned post_selected;
static unsigned post_presence, post_discard;
void* heap_caps_calloc(size_t n, size_t size, unsigned caps) {
    ++allocations;
    assert(n == 1 && size == (size_t)epd_width() * (size_t)epd_height() / 8 && caps == 3 && !hv_on);
    return fail_alloc ? NULL : calloc(n, size);
}
void epd_leading_skip_discard(void) { ++post_discard; }
void epd_leading_skip_set_present(const uint8_t* data, const uint8_t present[256]) {
    assert(check_post_selectors && post_discard == post_presence + 1);
    uint8_t observed[256] = {0};
    for (unsigned p = 0; p < FB_BYTES * 2; ++p) observed[data[p]] = 1;
    for (unsigned code = 0; code < 256; ++code) {
        assert(!observed[code] || present[code]);
        if (code != 0 && code != 0xee) assert(!present[code]);
    }
    ++post_presence;
}
enum EpdDrawError epd_hl_update_screen_selective(EpdiyHighlevelState* hl, enum EpdDrawMode mode, int temperature, const uint8_t* mask) {
    // 产品页不得再按历史字形补擦白底。/ Product pages must never re-erase white backgrounds in historical glyph shapes.
    assert(mask == NULL);
    ++mask_draws;
    if (fail_mask) return EPD_DRAW_OTHER_ERROR;
    return epd_hl_update_screen_full(hl, mode, temperature);
}

enum EpdDrawError epd_draw_base(EpdRect area, const uint8_t* data, EpdRect crop,
    enum EpdDrawMode mode, int temperature, const bool* lines, const uint8_t* columns,
    const EpdWaveform* waveform) {
    assert(lines == NULL && columns == NULL);
    assert(check_post_selectors && hv_on && temperature == 25);
    assert(post_presence == post_draws + 1);
    assert(area.x == 0 && area.y == 0 && area.width == epd_width() && area.height == epd_height());
    assert(crop.x == 0 && crop.y == 0 && crop.width == epd_width() && crop.height == epd_height());
    assert(mode == (MODE_GL16 | MODE_PACKING_1PPB_DIFFERENCE));
    assert(waveform == (night_cleanup == 1 ? &E0470_NIGHT_BLACK_BOOST_WAVEFORM : &E0470_NIGHT_LOCAL_CLEAN_WAVEFORM));
    assert(clear_hl && !memcmp(clear_hl->front_fb, post_target, FB_BYTES));
    assert(!memcmp(clear_hl->back_fb, post_target, FB_BYTES));
    unsigned selected = 0;
    for (unsigned p = 0; p < FB_BYTES * 2; ++p) {
        unsigned shift = (p % 2) * 4;
        unsigned old = post_prior[p / 2] >> shift & 15;
        unsigned goal = post_target[p / 2] >> shift & 15;
        bool chosen = old > 0 && goal == 0;
        assert(data[p] == (chosen ? 0x00 : 0xee));
        selected += chosen;
    }
    assert(selected && selected == post_selected);
    ++post_draws;
    return fail_post ? EPD_DRAW_OTHER_ERROR : EPD_DRAW_SUCCESS;
}

// 随机帧与朴素邻域实现对照，覆盖四像素组边界及非整除宽度。
// Compare random frames against a naive neighborhood implementation across packed-group and odd-group boundaries.
static void check_history_reference(void) {
    uint32_t random = 7;
    for (int w = 2; w <= 32; w += 2) {
        uint8_t fb[32 * 9 / 2], ages[32 * 9 / 4 + 1], expected[32 * 9], mask[32 * 9], packed[36], expanded[288];
        memset(ages, 0, sizeof(ages)); memset(expected, 0, sizeof(expected));
        display_ghost_history_t h = {ages, w, 9};
        for (int turn = 0; turn < 12; ++turn) {
            memset(fb, 255, sizeof(fb));
            for (int i = 0; i < w * 9; ++i) {
                random = random * 1664525u + 1013904223u;
                if ((random >> 28) < 3) fb[i / 2] &= (uint8_t)~(15u << (4 * (i % 2)));
            }
            display_ghost_history_observe(&h, fb);
            for (int y = 0; y < 9; ++y) for (int x = 0; x < w; ++x) {
                int i = y * w + x;
                if ((fb[i / 2] >> (4 * (i % 2)) & 15) == 15) continue;
                for (int dy = -1; dy <= 1; ++dy) for (int dx = -1; dx <= 1; ++dx)
                    if (x + dx >= 0 && x + dx < w && y + dy >= 0 && y + dy < 9)
                        expected[(y + dy) * w + x + dx] = 3;
            }
            size_t count = display_ghost_history_mask(&h, fb, mask);
            assert(display_ghost_history_pack(&h, fb, packed) == count);
            display_ghost_history_expand(packed, expanded, (size_t)w * 9);
            assert(!memcmp(mask, expanded, (size_t)w * 9));
            for (int i = 0; i < w * 9; ++i) {
                bool white = (fb[i / 2] >> (4 * (i % 2)) & 15) == 15;
                assert(mask[i] == (expected[i] && white ? 255 : 0));
                if (expected[i] && white) --expected[i];
            }
            display_ghost_history_commit(&h, fb);
            for (int i = 0; i < w * 9; ++i) assert((ages[i / 4] >> (2 * (i % 4)) & 3) == expected[i]);
        }
    }
}

static enum EpdDrawError night_turn(EpdiyHighlevelState* hl, bool direct) {
    return direct ? update_display_text_direct(hl, true) : update_display_text_turn(hl, true);
}

static void assert_night_scan(EpdiyHighlevelState* hl, bool direct, bool clean) {
    int drawn_before = draws, on_before = powerons, off_before = poweroffs, clears_before = clears;
    unsigned selected_before = mask_draws;
    memcpy(expected_old, hl->back_fb, FB_BYTES);
    memcpy(clear_prior, hl->back_fb, FB_BYTES);
    memcpy(clear_target, hl->front_fb, FB_BYTES);
    if (direct) display_prepare_direct_frame(clear_target, epd_width(), epd_height(), true);
    if (clean) memset(expected_old, 255, FB_BYTES);
    check_clean_at_clear = clean;
    check_old_at_draw = true;
    assert(night_turn(hl, direct) == EPD_DRAW_SUCCESS);
    check_old_at_draw = false;
    check_clean_at_clear = false;
    assert(draws == drawn_before + 1 && powerons == on_before + 1 && poweroffs == off_before + 1);
    assert(last_mode == (clean ? MODE_GC16 : MODE_GL16));
    assert(last_waveform == (clean ? &E0470_FULL_WAVEFORM : direct ? &E0470_DIRECT_WAVEFORM : &E0470_TEXTTURN_NIGHT_WAVEFORM));
    assert(mask_draws == selected_before + (clean ? 0 : 1) && clears == clears_before + clean);
    assert(!memcmp(hl->front_fb, hl->back_fb, FB_BYTES) && hl->waveform == &E0470_WAVEFORM);
}

static void assert_fresh_night_cycle(EpdiyHighlevelState* hl) {
    assert_night_scan(hl, false, false);
    assert_night_scan(hl, true, false);
    assert_night_scan(hl, false, true);
}

static enum EpdDrawError explicit_cleanup(EpdiyHighlevelState* hl, int kind) {
    if (!kind) return update_display_clean(hl);
    if (kind == 1) return update_display_with(hl, &E0470_TEXTTURN_NIGHT_WAVEFORM, MODE_GL16);
    return night_turn(hl, kind == 3);
}

static void expect_clean(EpdiyHighlevelState* hl, bool direct) {
    memcpy(clear_prior, hl->back_fb, FB_BYTES);
    memcpy(clear_target, hl->front_fb, FB_BYTES);
    if (direct) display_prepare_direct_frame(clear_target, epd_width(), epd_height(), true);
    memset(expected_old, 255, FB_BYTES);
    check_clean_at_clear = check_old_at_draw = true;
}

static void check_physical_cleanup(EpdiyHighlevelState* hl) {
    cleanup_every = 3;
    // 手动和夜间入口都先实际清白，目标不动，白参考仅在清白之后建立，随后一次GC提交。
    // Manual and night entries physically clear first without altering targets, establish white afterward, then commit one GC draw.
    for (int kind = 0; kind < 4; ++kind) {
        assert(update_display_full(hl) == EPD_DRAW_SUCCESS);
        assert_night_scan(hl, false, false);
        assert_night_scan(hl, true, false);
        memset(hl->front_fb, kind == 3 ? 0x78 : 0x37, FB_BYTES);
        memset(hl->back_fb, 0x66, FB_BYTES);
        display_request_navigation_settle();
        int before_clears = clears, before_draws = draws, before_full = full_draws;
        unsigned before_selected = mask_draws;
        expect_clean(hl, kind == 3);
        assert(explicit_cleanup(hl, kind) == EPD_DRAW_SUCCESS);
        check_clean_at_clear = check_old_at_draw = false;
        assert(clears == before_clears + 1 && draws == before_draws + 1 && full_draws == before_full + 1);
        assert(mask_draws == before_selected && last_mode == MODE_GC16 && last_waveform == &E0470_FULL_WAVEFORM && !last_area);
        assert(!memcmp(hl->front_fb, clear_target, FB_BYTES) && !memcmp(hl->back_fb, clear_target, FB_BYTES));
        assert(hl->waveform == &E0470_WAVEFORM);
        before_clears = clears;
        assert(update_display_with(hl, &E0470_TEXTTURN_NIGHT_WAVEFORM, MODE_GL16) == EPD_DRAW_SUCCESS);
        assert(clears == before_clears && last_mode == MODE_GL16);
        assert_fresh_night_cycle(hl);
    }

    // 清白之后画失败只能保留真实白参考及未知状态；上电失败不清白、不改参考，重试只清一次。
    // A draw failure after clearing retains the real white reference and unknown state; power failure changes neither, and retries clear only once.
    for (int kind = 0; kind < 4; ++kind) for (int power_failure = 0; power_failure < 2; ++power_failure) {
        assert(update_display_full(hl) == EPD_DRAW_SUCCESS);
        memset(hl->front_fb, 0xf0, FB_BYTES);
        memset(hl->back_fb, 0x66, FB_BYTES);
        display_request_navigation_settle();
        int before_clears = clears, before_draws = draws;
        fail_power = power_failure != 0;
        fail_draw = !power_failure;
        if (!power_failure) expect_clean(hl, kind == 3);
        assert(explicit_cleanup(hl, kind) != EPD_DRAW_SUCCESS);
        check_clean_at_clear = check_old_at_draw = false;
        assert(clears == before_clears + !power_failure && draws == before_draws + !power_failure);
        for (size_t b = 0; b < FB_BYTES; ++b) assert(hl->back_fb[b] == (power_failure ? 0x66 : 255));
        for (size_t b = 0; b < FB_BYTES; ++b) assert(hl->front_fb[b] == 0xf0);
        fail_power = fail_draw = false;
        before_clears = clears; before_draws = draws;
        expect_clean(hl, kind == 3);
        assert(explicit_cleanup(hl, kind) == EPD_DRAW_SUCCESS);
        check_clean_at_clear = check_old_at_draw = false;
        assert(clears == before_clears + 1 && draws == before_draws + 1 && last_mode == MODE_GC16);
        before_clears = clears;
        assert(update_display_with(hl, &E0470_TEXTTURN_NIGHT_WAVEFORM, MODE_GL16) == EPD_DRAW_SUCCESS);
        assert(last_mode == MODE_GL16 && clears == before_clears);
        assert_fresh_night_cycle(hl);
    }
    memcpy(hl->front_fb, target, FB_BYTES);
    assert(update_display_full(hl) == EPD_DRAW_SUCCESS);
}

static void check_night_cycles(EpdiyHighlevelState* hl) {
    const unsigned intervals[] = {1, 3, 5, 30};
    // 两夜间效果单独或交替使用，都在第N次成功翻页清理一次。
    // Both night effects alone or mixed clean once on every Nth successful turn.
    for (unsigned n = 0; n < sizeof(intervals) / sizeof(intervals[0]); ++n) {
        cleanup_every = intervals[n];
        for (int profile = 0; profile < 3; ++profile) {
            assert(update_display_full(hl) == EPD_DRAW_SUCCESS);
            for (unsigned turn = 1; turn <= intervals[n] * 2 + 2; ++turn) {
                memset(hl->front_fb, turn & 1 ? 0xf0 : 0x0f, FB_BYTES);
                bool direct = profile == 1 || (profile == 2 && (turn & 1));
                assert_night_scan(hl, direct, turn % intervals[n] == 0);
            }
        }
    }
    // 关闭期间不积累欠账，重新开启从下一次成功夜间翻页起算。
    // Disabled cleaning accumulates no debt; reenabling starts with the next successful night turn.
    cleanup_every = 3;
    assert(update_display_full(hl) == EPD_DRAW_SUCCESS);
    assert_night_scan(hl, false, false);
    assert_night_scan(hl, true, false);
    cleanup_every = 0;
    for (int turn = 0; turn < 64; ++turn) assert_night_scan(hl, turn & 1, false);
    cleanup_every = 3;
    assert_fresh_night_cycle(hl);

    // 日间两效果、普通导航、按钮和分钟局推不改变夜间周期。
    // Day effects, ordinary navigation, controls and minute bands leave the night interval untouched.
    assert(update_display_full(hl) == EPD_DRAW_SUCCESS);
    assert_night_scan(hl, false, false);
    int clears_before_controls = clears;
    for (int i = 0; i < 32; ++i) {
        assert(update_display_text_turn(hl, false) == EPD_DRAW_SUCCESS && last_mode == MODE_GL16);
        assert(update_display_text_direct(hl, false) == EPD_DRAW_SUCCESS && last_mode == MODE_GL16);
        assert(update_display_area_with(hl, &E0470_WAVEFORM, MODE_GL16, (EpdRect){1,2,3,4}) == EPD_DRAW_SUCCESS && last_mode == MODE_GL16);
        assert(update_display_area_quiet(hl, (EpdRect){1,2,3,4}) == EPD_DRAW_SUCCESS && last_mode == MODE_GL16);
        assert(update_display_area_with(hl, &E0470_TEXTTURN_NIGHT_WAVEFORM, MODE_GL16, (EpdRect){1,2,3,4}) == EPD_DRAW_SUCCESS && last_mode == MODE_GL16);
        assert(update_display_area_with(hl, &E0470_TEXTTURN_WAVEFORM, MODE_GL16, (EpdRect){0,0,epd_width(),epd_height()}) == EPD_DRAW_SUCCESS && last_mode == MODE_GL16);
        assert(update_display_area_with(hl, &E0470_TEXTTURN_NIGHT_WAVEFORM, MODE_GL16, (EpdRect){0,0,epd_width(),epd_height()}) == EPD_DRAW_SUCCESS && last_mode == MODE_GL16);
        assert(update_display_with(hl, &E0470_TEXTTURN_NIGHT_WAVEFORM, MODE_GL16) == EPD_DRAW_SUCCESS && last_mode == MODE_GL16);
        assert(update_display_with(hl, &E0470_NAVIGATION_WAVEFORM, MODE_GL16) == EPD_DRAW_SUCCESS && last_mode == MODE_GL16);
        assert(update_display_mode(hl, MODE_GL16) == EPD_DRAW_SUCCESS && last_mode == MODE_GL16);
    }
    assert(clears == clears_before_controls);
    assert_night_scan(hl, true, false);
    assert_night_scan(hl, false, true);

    // 布局、手动、开机及两类故障的成功全屏清理都重置未完成周期。
    // Successful whole-screen layout, manual, boot and both fault cleanups reset an unfinished interval.
    for (int reset_kind = 0; reset_kind < 5; ++reset_kind) {
        assert(update_display_full(hl) == EPD_DRAW_SUCCESS);
        assert_night_scan(hl, false, false);
        assert_night_scan(hl, true, false);
        if (reset_kind == 0) {
            display_request_navigation_settle();
            assert(update_display_mode(hl, MODE_GL16) == EPD_DRAW_SUCCESS && last_mode == MODE_GC16);
        } else if (reset_kind == 1) {
            assert(update_display_clean(hl) == EPD_DRAW_SUCCESS);
        } else if (reset_kind == 2) {
            assert(display_boot_white(hl) == EPD_DRAW_SUCCESS);
        } else if (reset_kind == 3) {
            guard_draw_result(hl, EPD_DRAW_OTHER_ERROR);
            assert(update_display_area_quiet(hl, (EpdRect){1,2,3,4}) == EPD_DRAW_SUCCESS && last_mode == MODE_GC16);
        } else {
            guard_draw_result(hl, EPD_DRAW_EMPTY_LINE_QUEUE);
            assert(last_mode == MODE_GC16);
        }
        assert_fresh_night_cycle(hl);
    }

    // 普通页或到期清理失败均不提交旧参考；恢复GC不能被再次计作第1页。
    // Failed ordinary or due turns never commit references; recovery GC must not be counted again as turn one.
    for (int due = 0; due < 2; ++due) for (int power_failure = 0; power_failure < 2; ++power_failure) {
        assert(update_display_full(hl) == EPD_DRAW_SUCCESS);
        assert_night_scan(hl, false, false);
        if (due) assert_night_scan(hl, true, false);
        memcpy(expected_old, hl->back_fb, FB_BYTES);
        memset(hl->front_fb, 0x0f, FB_BYTES);
        int drawn_before = draws, clears_before_failure = clears;
        fail_power = power_failure != 0; fail_draw = !power_failure;
        assert(night_turn(hl, due != 0) != EPD_DRAW_SUCCESS);
        if (due && !power_failure) {
            for (size_t b = 0; b < FB_BYTES; ++b) assert(hl->back_fb[b] == 255);
            assert(memcmp(hl->front_fb, hl->back_fb, FB_BYTES));
        } else assert(!memcmp(hl->back_fb, expected_old, FB_BYTES));
        if (power_failure) assert(draws == drawn_before);
        else assert(last_mode == (due ? MODE_GC16 : MODE_GL16));
        assert(clears == clears_before_failure + (due && !power_failure));
        fail_power = fail_draw = false;
        int clears_before = clears;
        expect_clean(hl, due == 0);
        assert(night_turn(hl, due == 0) == EPD_DRAW_SUCCESS && last_mode == MODE_GC16);
        check_clean_at_clear = check_old_at_draw = false;
        assert(clears == clears_before + 1 && !memcmp(hl->front_fb, hl->back_fb, FB_BYTES));
        assert_fresh_night_cycle(hl);
    }
    memcpy(hl->front_fb, target, FB_BYTES);
    assert(update_display_full(hl) == EPD_DRAW_SUCCESS);
}

// 独立按真实旧灰码和已量化目标计算选择器，避免把待测掩码实现复制到测试。
// Derive expected selectors independently from real old grays and quantized targets instead of copying the mask implementation.
static void prepare_post_expectation(EpdiyHighlevelState* hl) {
    memcpy(post_prior, hl->back_fb, FB_BYTES);
    memcpy(post_target, hl->front_fb, FB_BYTES);
    display_prepare_direct_frame(post_target, epd_width(), epd_height(), true);
    post_selected = 0;
    for (unsigned p = 0; p < FB_BYTES * 2; ++p) {
        unsigned shift = (p % 2) * 4;
        post_selected += (post_prior[p / 2] >> shift & 15) > 0 &&
            (post_target[p / 2] >> shift & 15) == 0;
    }
}

static void set_post_picture(EpdiyHighlevelState* hl, unsigned page) {
    static const uint8_t prior[] = {0xff, 0x0f, 0xf0, 0x00, 0x87, 0x78, 0x11, 0xee, 0x55};
    static const uint8_t goal[] = {0x78, 0xff, 0x00, 0x87, 0xf0, 0x0f};
    for (unsigned b = 0; b < FB_BYTES; ++b) {
        hl->back_fb[b] = prior[(b + page) % sizeof(prior)];
        hl->front_fb[b] = goal[(b + page) % sizeof(goal)];
    }
}

static void assert_experimental_turn(EpdiyHighlevelState* hl, bool post, bool clean) {
    prepare_post_expectation(hl);
    unsigned before_post = post_draws;
    unsigned before_alloc = allocations;
    check_post_selectors = true;
    assert_night_scan(hl, true, clean);
    check_post_selectors = false;
    assert(post_draws == before_post + post);
    assert(allocations == before_alloc);
    assert(!memcmp(hl->front_fb, post_target, FB_BYTES) && !memcmp(hl->back_fb, post_target, FB_BYTES));
}

static void check_night_experiment(EpdiyHighlevelState* hl) {
    // 首次分配失败不得假成功或扫描；成功重试复用真实旧帧，存储只分配一次。
    // Initial allocation failure must not report success or scan; retry uses the real old frame and allocates reusable storage once.
    cleanup_every = 0;
    night_cleanup = BOOK_NIGHT_CLEAN_BLACK;
    assert(update_display_full(hl) == EPD_DRAW_SUCCESS);
    set_post_picture(hl, 0);
    prepare_post_expectation(hl);
    unsigned before_post = post_draws, before_alloc = allocations;
    int before_draws = draws, before_on = powerons, before_off = poweroffs;
    fail_alloc = true;
    assert(update_display_text_direct(hl, true) == EPD_DRAW_FAILED_ALLOC);
    fail_alloc = false;
    assert(allocations == before_alloc + 1 && post_draws == before_post && draws == before_draws);
    assert(powerons == before_on && poweroffs == before_off && !memcmp(hl->back_fb, post_prior, FB_BYTES));
    assert(!memcmp(hl->front_fb, post_target, FB_BYTES));
    prepare_post_expectation(hl);
    check_post_selectors = true;
    assert_night_scan(hl, true, false);
    check_post_selectors = false;
    assert(post_draws == before_post + 1 && allocations == before_alloc + 2);

    // 所有灰旧码、相邻半字节、组/行边界都覆盖；新白字和原黑底始终EE保持。
    // Cover all old gray codes, adjacent nibbles and group/row boundaries; new white glyphs and existing black backgrounds always hold with EE.
    for (uint8_t mode = BOOK_NIGHT_CLEAN_BLACK; mode <= BOOK_NIGHT_CLEAN_LOCAL; ++mode) {
        night_cleanup = mode;
        for (unsigned page = 0; page < 16; ++page) {
            for (unsigned b = 0; b < FB_BYTES; ++b) {
                hl->back_fb[b] = (uint8_t)(b + page * 16);
                hl->front_fb[b] = (uint8_t)((b * 53u + page * 31u) & 255);
            }
            fail_alloc = true;
            assert_experimental_turn(hl, true, false);
            fail_alloc = false;
        }
        // 全黑页及完全重叠的白字没有旧亮转黑，不应执行空补扫。
        // All-black pages and fully overlapping white glyphs have no old-light-to-black pixels and must not run an empty post scan.
        memset(hl->front_fb, 0, FB_BYTES); memset(hl->back_fb, 0, FB_BYTES);
        assert_experimental_turn(hl, false, false);
        memset(hl->front_fb, 255, FB_BYTES); memset(hl->back_fb, 255, FB_BYTES);
        assert_experimental_turn(hl, false, false);
        memset(hl->front_fb, 255, FB_BYTES); memset(hl->back_fb, 0, FB_BYTES);
        assert_experimental_turn(hl, false, false);
        memset(hl->front_fb, 0, FB_BYTES); memset(hl->back_fb, 255, FB_BYTES);
        assert_experimental_turn(hl, true, false);
    }

    // 关闭、标准灰阶、日间、独立控件/页脚和普通导航不补扫、不改变实验分配状态。
    // Off, standard gray, day, independent controls/footer and ordinary navigation never post-scan or alter experiment allocation state.
    before_post = post_draws; before_alloc = allocations;
    night_cleanup = BOOK_NIGHT_CLEAN_OFF;
    set_post_picture(hl, 0);
    assert_experimental_turn(hl, false, false);
    for (uint8_t mode = BOOK_NIGHT_CLEAN_BLACK; mode <= BOOK_NIGHT_CLEAN_LOCAL; ++mode) {
        night_cleanup = mode;
        set_post_picture(hl, mode);
        assert(update_display_text_turn(hl, true) == EPD_DRAW_SUCCESS);
        assert(update_display_text_turn(hl, false) == EPD_DRAW_SUCCESS);
        assert(update_display_text_direct(hl, false) == EPD_DRAW_SUCCESS);
        assert(update_display_area_with(hl, &E0470_DIRECT_WAVEFORM, MODE_GL16, (EpdRect){1,2,3,4}) == EPD_DRAW_SUCCESS);
        assert(update_display_area_with(hl, &E0470_DIRECT_WAVEFORM, MODE_GL16, epd_full_screen()) == EPD_DRAW_SUCCESS);
        assert(update_display_area_quiet(hl, (EpdRect){1,2,3,4}) == EPD_DRAW_SUCCESS);
        assert(update_display_with(hl, &E0470_DIRECT_WAVEFORM, MODE_GL16) == EPD_DRAW_SUCCESS);
        assert(update_display_with(hl, &E0470_TEXTTURN_NIGHT_WAVEFORM, MODE_GL16) == EPD_DRAW_SUCCESS);
        assert(update_display_with(hl, &E0470_NAVIGATION_WAVEFORM, MODE_GL16) == EPD_DRAW_SUCCESS);
    }
    assert(post_draws == before_post && allocations == before_alloc);

    // 周期第N页、入口和未知恢复以一次完整清理替代实验；计数以整个翻页成功为准。
    // Due turn N, entries and unknown recovery replace the experiment with one full cleanup; count only a wholly successful turn.
    for (uint8_t mode = BOOK_NIGHT_CLEAN_BLACK; mode <= BOOK_NIGHT_CLEAN_LOCAL; ++mode) {
        night_cleanup = mode;
        cleanup_every = 3;
        assert(update_display_full(hl) == EPD_DRAW_SUCCESS);
        for (unsigned turn = 1; turn <= 8; ++turn) {
            set_post_picture(hl, turn);
            assert_experimental_turn(hl, turn % 3 != 0, turn % 3 == 0);
        }
        set_post_picture(hl, 1);
        display_request_navigation_settle();
        assert_experimental_turn(hl, false, true);
        for (unsigned turn = 1; turn <= 3; ++turn) {
            set_post_picture(hl, turn);
            assert_experimental_turn(hl, turn != 3, turn == 3);
        }
        cleanup_every = 1;
        set_post_picture(hl, 0);
        assert_experimental_turn(hl, false, true);

        // 基础DU失败和上电失败不允许补扫；成功恢复先清白，随后从0重新累计。
        // Failed base DU and failed power never permit a post scan; successful recovery clears first and restarts counting from zero.
        for (int power_failure = 0; power_failure < 2; ++power_failure) {
            cleanup_every = 3;
            assert(update_display_full(hl) == EPD_DRAW_SUCCESS);
            set_post_picture(hl, 0);
            assert_experimental_turn(hl, true, false);
            set_post_picture(hl, 1);
            prepare_post_expectation(hl);
            before_post = post_draws;
            fail_power = power_failure != 0; fail_mask = !power_failure;
            assert(update_display_text_direct(hl, true) == (power_failure ? EPD_DRAW_POWER_NOT_READY : EPD_DRAW_OTHER_ERROR));
            fail_power = fail_mask = false;
            assert(post_draws == before_post && !memcmp(hl->back_fb, post_prior, FB_BYTES));
            assert_experimental_turn(hl, false, true);
            for (unsigned turn = 1; turn <= 3; ++turn) {
                set_post_picture(hl, turn);
                assert_experimental_turn(hl, turn != 3, turn == 3);
            }
        }

        // 补扫失败保留实际DU新帧，但物理基准未知；下一次局推也必须完整恢复。
        // Failed post scans retain the actual new DU frame but invalidate the physical baseline; even the next local update must fully recover.
        assert(update_display_full(hl) == EPD_DRAW_SUCCESS);
        set_post_picture(hl, 0);
        assert_experimental_turn(hl, true, false);
        set_post_picture(hl, 1);
        prepare_post_expectation(hl);
        before_post = post_draws;
        before_draws = draws; before_on = powerons; before_off = poweroffs;
        fail_post = check_post_selectors = true;
        assert(update_display_text_direct(hl, true) == EPD_DRAW_OTHER_ERROR);
        fail_post = check_post_selectors = false;
        assert(post_draws == before_post + 1 && draws == before_draws + 1);
        assert(powerons == before_on + 1 && poweroffs == before_off + 1 && !hv_on);
        assert(!memcmp(hl->front_fb, post_target, FB_BYTES) && !memcmp(hl->back_fb, post_target, FB_BYTES));
        int before_clears = clears;
        before_post = post_draws;
        assert(update_display_area_quiet(hl, (EpdRect){1,2,3,4}) == EPD_DRAW_SUCCESS);
        assert(last_mode == MODE_GC16 && !last_area && clears == before_clears + 1 && post_draws == before_post);
        for (unsigned turn = 1; turn <= 3; ++turn) {
            set_post_picture(hl, turn);
            assert_experimental_turn(hl, turn != 3, turn == 3);
        }
    }
    night_cleanup = BOOK_NIGHT_CLEAN_OFF;
}

static unsigned night_mask_checks;

static void check_night_mask_case(const uint8_t* old, const uint8_t* goal, int width, int height) {
    enum { GUARD = 16, MAX_PIXELS = 288, MASK_BYTES = (MAX_PIXELS + 7) / 8 };
    const size_t pixels = (size_t)width * (size_t)height, frame_bytes = pixels / 2;
    assert(pixels <= MAX_PIXELS);
    uint8_t prior[GUARD + MAX_PIXELS / 2 + GUARD], target_copy[sizeof(prior)];
    uint8_t expected_prior[sizeof(prior)], expected_target[sizeof(prior)];
    uint8_t packed[GUARD + MASK_BYTES + GUARD], expected_packed[sizeof(packed)];
    uint8_t selectors[GUARD + MAX_PIXELS + GUARD], expected_selectors[sizeof(selectors)];
    memset(prior, 0xa5, sizeof(prior)); memset(target_copy, 0xa5, sizeof(target_copy));
    memcpy(prior + GUARD, old, frame_bytes); memcpy(target_copy + GUARD, goal, frame_bytes);
    memcpy(expected_prior, prior, sizeof(prior)); memcpy(expected_target, target_copy, sizeof(prior));
    memset(packed, 0xa5, sizeof(packed)); memcpy(expected_packed, packed, sizeof(packed));
    memset(expected_packed + GUARD, 0, (pixels + 7) / 8);
    memset(selectors, 0xa5, sizeof(selectors)); memcpy(expected_selectors, selectors, sizeof(selectors));
    size_t count = 0;
    for (int y = 0; y < height; ++y) for (int x = 0; x < width; ++x) {
        size_t p = (size_t)y * (size_t)width + (size_t)x;
        unsigned before = old[(size_t)y * (size_t)width / 2 + (unsigned)x / 2] >> ((x & 1) * 4) & 15;
        unsigned after = goal[(size_t)y * (size_t)width / 2 + (unsigned)x / 2] >> ((x & 1) * 4) & 15;
        bool select = before != 0 && after == 0;
        if (select) expected_packed[GUARD + p / 8] |= (uint8_t)(1u << (p % 8));
        expected_selectors[GUARD + p] = select ? 0 : 0xee;
        count += select;
    }
    assert(display_night_erased_mask(prior + GUARD, target_copy + GUARD, packed + GUARD, width, height) == count);
    assert(!memcmp(packed, expected_packed, sizeof(packed)));
    assert(display_night_cleanup_selectors(packed + GUARD, target_copy + GUARD, selectors + GUARD, width, height) == count);
    assert(!memcmp(selectors, expected_selectors, sizeof(selectors)));
    assert(!memcmp(prior, expected_prior, sizeof(prior)) && !memcmp(target_copy, expected_target, sizeof(prior)));
    assert(!memcmp(packed, expected_packed, sizeof(packed)));

    // 即便选择图被篡改为全选，所有当前非黑像素仍须保持，包括新白字和任何灰码。
    // Even an all-selected tampered mask must hold every current nonblack pixel, including new white glyphs and all gray codes.
    memset(packed + GUARD, 255, (pixels + 7) / 8);
    memcpy(expected_packed, packed, sizeof(packed));
    count = 0;
    for (size_t p = 0; p < pixels; ++p) {
        bool black = (goal[p / 2] >> ((p & 1) * 4) & 15) == 0;
        expected_selectors[GUARD + p] = black ? 0 : 0xee;
        count += black;
    }
    assert(display_night_cleanup_selectors(packed + GUARD, target_copy + GUARD, selectors + GUARD, width, height) == count);
    assert(!memcmp(selectors, expected_selectors, sizeof(selectors)) && !memcmp(packed, expected_packed, sizeof(packed)));
    assert(!memcmp(prior, expected_prior, sizeof(prior)) && !memcmp(target_copy, expected_target, sizeof(prior)));
    ++night_mask_checks;
}

static void check_night_masks(void) {
    // 穷举前后两个打包像素，覆盖每种灰码、半字节顺序以及目标纯黑判定。
    // Exhaust pairs of packed pixels to cover every gray code, nibble ordering and exact-black target classification.
    for (unsigned old = 0; old < 256; ++old) for (unsigned goal = 0; goal < 256; ++goal) {
        uint8_t prior = (uint8_t)old, target_copy = (uint8_t)goal;
        check_night_mask_case(&prior, &target_copy, 2, 1);
    }
    uint32_t random = 41;
    for (int width = 2; width <= 32; width += 2) for (int height = 1; height <= 9; ++height) {
        uint8_t prior[144], target_copy[144];
        for (size_t b = 0; b < (size_t)width * (size_t)height / 2; ++b) {
            random = random * 1664525u + 1013904223u; prior[b] = (uint8_t)(random >> 16);
            random = random * 1664525u + 1013904223u; target_copy[b] = (uint8_t)(random >> 16);
        }
        check_night_mask_case(prior, target_copy, width, height);
    }
    // 无效尺寸或指针不写输出，尾部未使用位及输出护栏亦由上面的全数组对照覆盖。
    // Invalid dimensions or pointers leave output untouched; full-array comparisons above also cover unused tail bits and guards.
    uint8_t prior[16] = {0}, target_copy[16] = {0}, packed[16], selectors[16], expected[16];
    memset(packed, 0xa5, sizeof(packed)); memset(selectors, 0xa5, sizeof(selectors)); memset(expected, 0xa5, sizeof(expected));
    const int dims[][2] = {{0, 2}, {-2, 2}, {2, 0}, {2, -1}, {3, 2}, {INT_MAX, INT_MAX}};
    for (size_t n = 0; n < sizeof(dims) / sizeof(*dims); ++n) {
        assert(!display_night_erased_mask(prior, target_copy, packed, dims[n][0], dims[n][1]));
        assert(!display_night_cleanup_selectors(packed, target_copy, selectors, dims[n][0], dims[n][1]));
    }
    assert(!display_night_erased_mask(NULL, target_copy, packed, 2, 2));
    assert(!display_night_erased_mask(prior, NULL, packed, 2, 2));
    assert(!display_night_erased_mask(prior, target_copy, NULL, 2, 2));
    assert(!display_night_cleanup_selectors(NULL, target_copy, selectors, 2, 2));
    assert(!display_night_cleanup_selectors(packed, NULL, selectors, 2, 2));
    assert(!display_night_cleanup_selectors(packed, target_copy, NULL, 2, 2));
    assert(!memcmp(packed, expected, sizeof(packed)) && !memcmp(selectors, expected, sizeof(selectors)));
    printf("night masks: %u independent checks passed; packed gray pairs, random dimensions, tail bits, guarded outputs, read-only frames and malicious selections\n", night_mask_checks);
}

int main(void) {
    check_history_reference();
    check_night_masks();
    // 整帧反色覆盖全部打包灰码，并验证往返与无效尺寸不写内存。
    // Whole-frame inversion covers every packed gray code, including round trips and untouched invalid dimensions.
    uint8_t inverse[256];
    for (unsigned i = 0; i < sizeof(inverse); ++i) inverse[i] = (uint8_t)i;
    display_invert_frame(inverse, 32, 16);
    for (unsigned i = 0; i < sizeof(inverse); ++i) assert(inverse[i] == (uint8_t)(i ^ 255));
    display_invert_frame(inverse, 32, 16);
    display_invert_frame(inverse, 31, 16);
    display_invert_frame(inverse, 0, 16);
    display_invert_frame(inverse, 32, -1);
    display_invert_frame(NULL, 32, 16);
    for (unsigned i = 0; i < sizeof(inverse); ++i) assert(inverse[i] == i);
    // 日夜均按真实灰码阈值转为黑白；穷举打包像素与边界，重复转换不变。
    // Threshold actual gray codes to black/white in both day and night; exhaust packed pixels and thresholds and verify idempotence.
    for (int night = 0; night < 2; ++night) {
        uint8_t samples[256], converted[256];
        for (int b = 0; b < 256; ++b) samples[b] = (uint8_t)b;
        display_prepare_direct_frame(samples, 32, 16, night != 0);
        for (int b = 0; b < 256; ++b) {
            unsigned low = (b & 15) < 8 ? 0 : 15, high = (b >> 4) < 8 ? 0 : 15;
            assert(samples[b] == (uint8_t)(low | high << 4));
        }
        assert(samples[0] == 0 && samples[255] == 255 && samples[0xbb] == 255 && samples[0x55] == 0);
        assert(samples[0x77] == 0 && samples[0x88] == 255 && samples[0x87] == 0xf0 && samples[0x78] == 0x0f);
        memcpy(converted, samples, sizeof(samples));
        display_prepare_direct_frame(samples, 32, 16, night != 0);
        assert(!memcmp(samples, converted, sizeof(samples)));
        display_prepare_direct_frame(samples, 31, 16, night != 0);
        assert(!memcmp(samples, converted, sizeof(samples)));
    }
    display_prepare_direct_frame(NULL, 32, 16, false);
    uint8_t ages[8] = {0}, old[16], goal[16], mask[32];
    display_ghost_history_t history = {ages, 8, 4};
    memset(old, 255, sizeof(old)); old[0] = 0xf0;
    memset(goal, 255, sizeof(goal));
    display_ghost_history_observe(&history, old);
    assert(display_ghost_history_mask(&history, goal, mask) == 4);
    assert(mask[0] && mask[1] && mask[8] && mask[9] && !mask[7] && !mask[15]);
    goal[0] = 0x80;
    assert(display_ghost_history_mask(&history, goal, mask) == 2 && !mask[0] && !mask[1]);
    memset(goal, 255, sizeof(goal));
    for (int pass = 0; pass < 3; ++pass) {
        assert(display_ghost_history_mask(&history, goal, mask) == 4);
        display_ghost_history_commit(&history, goal);
    }
    assert(display_ghost_history_mask(&history, goal, mask) == 0);
    display_ghost_history_observe(&history, old);
    display_ghost_history_reset(&history);
    assert(display_ghost_history_mask(&history, goal, mask) == 0);
    assert(display_ghost_history_bytes(1216, 684) == 207936 && !display_ghost_history_bytes(3, 4));
    uint8_t front[FB_BYTES], back[FB_BYTES], difference[FB_BYTES * 2];
    for (size_t i = 0; i < FB_BYTES; ++i) target[i] = (uint8_t)(i * 37U + 3U);
    memcpy(front, target, FB_BYTES);
    memset(back, 0x55, FB_BYTES);
    EpdiyHighlevelState hl = {.front_fb = front, .back_fb = back, .difference_fb = difference, .waveform = &E0470_WAVEFORM};
    clear_hl = &hl;
    guard_draw_result(&hl, EPD_DRAW_SUCCESS);
    assert(!clocks && !clears && !draws && !memcmp(front, target, FB_BYTES));
    for (int i = 0; i < 7; ++i) {
        memset(back, 0x55, FB_BYTES);
        guard_draw_result(&hl, update_display_mode(&hl, MODE_GL16));
        assert(last_mode == (i % 3 == 2 ? MODE_GC16 : MODE_GL16));
        // GL16 页面使用完整表；档位到期升 GC16 时才整屏清理。
        // GL16 pages use complete tables; only tier promotion cleans the whole panel.
        assert(last_waveform == (i % 3 == 2 ? &E0470_FULL_WAVEFORM : &E0470_WAVEFORM));
        assert(hl.waveform == &E0470_WAVEFORM);
    }
    // 双遍扫描是白屏事故根因，禁止回归。/ The double scan caused the white-screen regression; keep it out.
    // 普通刷新立即下电，空闲检查不能重复下电。/ Ordinary pushes power off immediately; idle checks must not repeat it.
    assert(draws == 7 && full_draws == 2 && !clears && poweroffs == 7);
    rails_idle_check(esp_timer_get_time() / 1000 + 4000);
    assert(poweroffs == 7);
    update_display_full(&hl);
    assert(last_mode == MODE_GC16 && last_waveform == &E0470_FULL_WAVEFORM && !last_area);
    // 按钮不能触发整屏清理，也不能消耗页级周期。/ Buttons never clean the full screen or consume the page cleanup interval.
    int before_turn = draws, full_before_turn = full_draws;
    fail_alloc = true;
    update_display_text_turn(&hl, false);
    assert(mask_draws == 0 && allocations == 0);
    fail_alloc = false;
    assert(draws == before_turn + 1 && full_draws == full_before_turn + 1);
    assert(last_mode == MODE_GL16 && last_waveform == &E0470_TEXTTURN_WAVEFORM && !last_area);
    for (int i = 0; i < 30; ++i) {
        update_display_area_with(&hl, &E0470_WAVEFORM, MODE_DU, (EpdRect){1,2,3,4});
        assert(last_mode == MODE_DU && last_area);
        update_display_area_with(&hl, &E0470_WAVEFORM, MODE_GL16, (EpdRect){1,2,3,4});
        assert(last_mode == MODE_GL16 && last_area);
        assert(last_waveform == &E0470_WAVEFORM && hl.waveform == &E0470_WAVEFORM);
    }
    // 页面只返回 PAGE 的按钮也不应触发周期清理。/ Buttons returning PAGE also stay outside periodic cleaning.
    for (int i = 0; i < 30; ++i) {
        memcpy(back, target, FB_BYTES); back[17] ^= 0x11;
        update_display_mode(&hl, MODE_GL16);
        assert(last_mode == MODE_GL16);
        memcpy(back, target, FB_BYTES); back[17] ^= 0x11;
        update_display_with(&hl, &E0470_NAVIGATION_WAVEFORM, MODE_GL16);
        assert(last_mode == MODE_GL16);
    }
    // 即使保存周期=1，正常正文也始终 GL16；翻页不消耗通用页面计数。
    // Even with saved interval=1, normal body turns always use GL16 and never consume generic-page counts.
    const unsigned intervals[] = {1, 3, 5, 30};
    int clears_before_reading = clears;
    for (unsigned n = 0; n < sizeof(intervals) / sizeof(intervals[0]); ++n) {
        cleanup_every = intervals[n];
        for (int turn = 0; turn < 64; ++turn) {
            memset(back, (unsigned char)(turn + 0x33), FB_BYTES);
            int previous_draws = draws;
            update_display_text_turn(&hl, false);
            assert(last_mode == MODE_GL16 && last_waveform == &E0470_TEXTTURN_WAVEFORM);
            assert(draws == previous_draws + 1 && clears == clears_before_reading);
        }
    }
    check_night_cycles(&hl);
    check_physical_cleanup(&hl);
    memcpy(front, target, FB_BYTES);
    cleanup_every = 3;
    update_display_full(&hl);
    assert(last_mode == MODE_GC16 && last_waveform == &E0470_FULL_WAVEFORM);
    assert(hl.waveform == &E0470_WAVEFORM);
    for (int i = 0; i < 2; ++i) {
        memset(back, 0x55, FB_BYTES);
        update_display_with(&hl, &E0470_WAVEFORM, MODE_GL16);
        assert(last_mode == MODE_GL16);
    }
    for (int i = 0; i < 64; ++i) {
        update_display_text_turn(&hl, false);
        assert(last_mode == MODE_GL16 && last_waveform == &E0470_TEXTTURN_WAVEFORM);
    }
    memset(back, 0x55, FB_BYTES);
    update_display_with(&hl, &E0470_WAVEFORM, MODE_GL16);
    assert(last_mode == MODE_GC16);
    // 显式整屏 AREA 同样参与页级周期。/ Explicit full-screen AREA also counts toward page cleanup.
    for (int i = 0; i < 3; ++i) {
        update_display_area_with(&hl, &E0470_WAVEFORM, MODE_GL16, (EpdRect){0,0,16,16});
        assert(last_mode == (i == 2 ? MODE_GC16 : MODE_GL16) && !last_area);
    }
    cleanup_every = 0;
    for (int i = 0; i < 20; ++i) {
        update_display_mode(&hl, MODE_GL16); assert(last_mode == MODE_GL16);
    }
    // 冷启动铺白：物理清屏一次，双缓冲归白，不推任何差分帧。
    // Cold-boot white: one physical clear, both buffers white, no diff frame pushed.
    memset(front, 0x11, FB_BYTES); memset(back, 0x22, FB_BYTES);
    clocks = powerons = clears = draws = full_draws = 0;
    display_boot_white(&hl);
    assert(clears == 1 && powerons == 1 && draws == 0);
    for (size_t i = 0; i < FB_BYTES; ++i) assert(front[i] == 255 && back[i] == 255);
    for (int bulk = 0; bulk < 2; ++bulk) {
        memcpy(front, target, FB_BYTES);
        memset(back, 0x55, FB_BYTES);
        clocks = powerons = clears = draws = full_draws = 0;
        display_set_bulk_io(bulk != 0);
        guard_draw_result(&hl, EPD_DRAW_EMPTY_LINE_QUEUE | EPD_DRAW_OTHER_ERROR);
        if (memcmp(front, target, FB_BYTES)) {
            fputs("FAIL: underrun recovery erased target front_fb (white screen regression)\n", stderr);
            return 1;
        }
        assert(correct_target_at_draw && !memcmp(presented, target, FB_BYTES));
        assert(white_baseline && full_draws == 1 && draws == 1 && last_mode == MODE_GC16);
        assert(!memcmp(back, target, FB_BYTES));
        assert(clocks == 1 && safe_clock == DISPLAY_PCLK_SAFE_MHZ && display_pclk_mhz() == DISPLAY_PCLK_SAFE_MHZ);
        assert(powerons == 1 && clears == 1 && prefill == 127);
    }
    // 根页一次全像素推送，旧帧不能伪装成白底。/ Root navigation presents every pixel once without pretending the old frame is white.
    cleanup_every = 0;
    memcpy(front, target, FB_BYTES); memset(back, 0x66, FB_BYTES);
    int before_draws = draws, before_full = full_draws;
    update_display_with(&hl, &E0470_NAVIGATION_WAVEFORM, MODE_GL16);
    assert(draws == before_draws + 1 && full_draws == before_full + 1);
    assert(last_waveform == &E0470_NAVIGATION_WAVEFORM && last_mode == MODE_GL16);
    assert(!white_baseline && correct_target_at_draw && hl.waveform == &E0470_WAVEFORM);
    assert(!memcmp(front, back, FB_BYTES));
    unsigned mask_before=mask_draws;
    for (unsigned interval=1;interval<=5;interval+=2) {
        cleanup_every=interval;
        for (int i=0;i<32;++i) {
            memset(back,0x77,FB_BYTES);
            int before=draws;
            update_display_with(&hl,&E0470_NAVIGATION_WAVEFORM,MODE_GL16);
            assert(last_mode==MODE_GL16&&draws==before+1&&mask_draws==mask_before);
        }
    }
    // 布局入口一次厂家GC16，普通导航/局推/正文不得重复；失败上电保留资格。
    // Layout entries use one vendor GC16; ordinary navigation/local/body updates never repeat it, and failed power retains eligibility.
    for (unsigned interval=1;interval<=5;interval+=2) {
        cleanup_every=interval;
        memcpy(front,target,FB_BYTES);memset(back,0x66,FB_BYTES);
        display_request_navigation_settle();
        before_draws=draws;mask_before=mask_draws;
        assert(update_display_with(&hl,&E0470_NAVIGATION_WAVEFORM,MODE_GL16)==EPD_DRAW_SUCCESS);
        assert(draws==before_draws+1&&last_waveform==&E0470_FULL_WAVEFORM&&last_mode==MODE_GC16);
        assert(!white_baseline&&correct_target_at_draw&&mask_draws==mask_before&&!memcmp(front,back,FB_BYTES));
        update_display_with(&hl,&E0470_NAVIGATION_WAVEFORM,MODE_GL16);
        assert(last_waveform==&E0470_NAVIGATION_WAVEFORM&&last_mode==MODE_GL16);
        update_display_text_turn(&hl, false);
        assert(last_waveform==&E0470_TEXTTURN_WAVEFORM&&last_mode==MODE_GL16);
    }
    // 未声明clean_page的子页也消费入口；下一次同页GL不再清理。
    // Subpages without clean_page also consume their entry; the next same-page GL never cleans again.
    // 夜间菜单关闭仍尊重全局入口清理，不能被同视图保持路径吞掉。
    // Night menu closing honors the global entry cleanup instead of swallowing it in a same-view hold path.
    const EpdWaveform* reader_entries[] = {&E0470_TEXTTURN_NIGHT_WAVEFORM, &E0470_TEXTTURN_WAVEFORM, &E0470_DIRECT_WAVEFORM};
    cleanup_every = 0;
    for (unsigned i = 0; i < sizeof(reader_entries) / sizeof(reader_entries[0]); ++i) {
        display_request_navigation_settle();
        before_draws = draws;
        int clears_before = clears;
        assert(update_display_with(&hl, reader_entries[i], MODE_GL16) == EPD_DRAW_SUCCESS);
        assert(draws == before_draws + 1 && last_waveform == &E0470_FULL_WAVEFORM && last_mode == MODE_GC16);
        assert(clears == clears_before + (reader_entries[i] == &E0470_TEXTTURN_NIGHT_WAVEFORM));
        assert(update_display_with(&hl, reader_entries[i], MODE_GL16) == EPD_DRAW_SUCCESS);
        assert(last_waveform == reader_entries[i] && last_mode == MODE_GL16);
        assert(clears == clears_before + (reader_entries[i] == &E0470_TEXTTURN_NIGHT_WAVEFORM));
    }
    memcpy(front,target,FB_BYTES);memset(back,0x66,FB_BYTES);
    display_request_navigation_settle();before_draws=draws;
    assert(update_display_mode(&hl,MODE_GL16)==EPD_DRAW_SUCCESS);
    assert(draws==before_draws+1&&last_waveform==&E0470_FULL_WAVEFORM&&last_mode==MODE_GC16);
    assert(!white_baseline&&correct_target_at_draw&&!memcmp(front,back,FB_BYTES));
    assert(update_display_mode(&hl,MODE_GL16)==EPD_DRAW_SUCCESS);
    assert(last_waveform==&E0470_WAVEFORM&&last_mode==MODE_GL16);
    // 扫描失败不消费入口、不提交旧参考；重试沿用已有未知基准恢复。
    // Scan failure never consumes an entry or commits a new baseline; retries use existing unknown-baseline recovery.
    memset(back,0x66,FB_BYTES);memcpy(expected_old,back,FB_BYTES);
    display_request_navigation_settle();fail_draw=true;
    assert(update_display_mode(&hl,MODE_GL16)==EPD_DRAW_OTHER_ERROR);
    assert(!memcmp(back,expected_old,FB_BYTES));
    fail_draw=false;
    assert(update_display_mode(&hl,MODE_GL16)==EPD_DRAW_SUCCESS);
    assert(last_waveform==&E0470_FULL_WAVEFORM&&last_mode==MODE_GC16);
    update_display_mode(&hl,MODE_GL16);
    assert(last_waveform==&E0470_WAVEFORM&&last_mode==MODE_GL16);
    display_request_navigation_settle();fail_power=true;
    before_draws=draws;
    assert(update_display_with(&hl,&E0470_NAVIGATION_WAVEFORM,MODE_GL16)==EPD_DRAW_POWER_NOT_READY&&draws==before_draws);
    fail_power=false;
    assert(update_display_with(&hl,&E0470_NAVIGATION_WAVEFORM,MODE_GL16)==EPD_DRAW_SUCCESS);
    assert(last_waveform==&E0470_FULL_WAVEFORM&&last_mode==MODE_GC16);
    update_display_with(&hl,&E0470_NAVIGATION_WAVEFORM,MODE_GL16);
    assert(last_waveform==&E0470_NAVIGATION_WAVEFORM);
    // 失败入口后若分钟/页脚局推先恢复整屏，下一次普通NAV不能遗留额外白推动。
    // If a minute/footer local update recovers a failed entry first, later ordinary NAV must not retain an extra white tick.
    display_request_navigation_settle();fail_power=true;
    assert(update_display_with(&hl,&E0470_NAVIGATION_WAVEFORM,MODE_GL16)==EPD_DRAW_POWER_NOT_READY);
    fail_power=false;
    update_display_area_quiet(&hl,(EpdRect){1,2,3,4});
    assert(last_waveform==&E0470_FULL_WAVEFORM&&last_mode==MODE_GC16&&!last_area);
    update_display_with(&hl,&E0470_NAVIGATION_WAVEFORM,MODE_GL16);
    assert(last_waveform==&E0470_NAVIGATION_WAVEFORM&&last_mode==MODE_GL16);
    // 非欠载错误也不能相信失败基准；下一次局推必须先清白并保留整页目标。
    // Non-underrun errors invalidate the baseline too; the next local update clears first and retains the whole target.
    before_draws = draws;
    int before_clears = clears;
    guard_draw_result(&hl, EPD_DRAW_OTHER_ERROR);
    assert(draws == before_draws && clears == before_clears);
    update_display_area_with(&hl, &E0470_WAVEFORM, MODE_GL16, (EpdRect){1,2,3,4});
    assert(last_mode == MODE_GC16 && !last_area && clears == before_clears + 1);
    assert(draws == before_draws + 1 && correct_target_at_draw && !memcmp(front, target, FB_BYTES));
    // 上电失败覆盖所有出口：不画、不清白、不回写；成功重试先恢复未知基准。
    // Power failures cover every entry: no draw, clear or commit; successful retry first recovers the unknown baseline.
    memset(back, 0x66, FB_BYTES);
    before_draws = draws; before_clears = clears;
    fail_power = true;
    assert(update_display_mode(&hl, MODE_DU) == EPD_DRAW_POWER_NOT_READY);
    assert(update_display_mode(&hl, MODE_GL16) == EPD_DRAW_POWER_NOT_READY);
    assert(update_display_from_white(&hl) == EPD_DRAW_POWER_NOT_READY);
    assert(update_display_clean(&hl) == EPD_DRAW_POWER_NOT_READY);
    assert(display_boot_white(&hl) == EPD_DRAW_POWER_NOT_READY);
    assert(update_display_area_quiet(&hl, (EpdRect){1,2,3,4}) == EPD_DRAW_POWER_NOT_READY);
    assert(update_display_area_with(&hl, &E0470_WAVEFORM, MODE_GL16, (EpdRect){1,2,3,4}) == EPD_DRAW_POWER_NOT_READY);
    guard_draw_result(&hl, EPD_DRAW_EMPTY_LINE_QUEUE);
    assert(draws == before_draws && clears == before_clears && !memcmp(front, target, FB_BYTES));
    for (size_t i = 0; i < FB_BYTES; ++i) assert(back[i] == 0x66);
    fail_power = false;
    assert(update_display_text_turn(&hl, false) == EPD_DRAW_SUCCESS);
    assert(last_mode == MODE_GC16 && clears == before_clears + 1 && correct_target_at_draw);
    // 两种正文效果互切：直刷只转换目标，完整旧灰仍交给扫描；成功提交实际黑白，标准恢复真实灰阶。
    // Switch body profiles: direct converts only targets while scanning from complete old grays, commits actual black/white on success, and standard restores actual grays.
    for (unsigned i = 0; i < 4; ++i) {
        cleanup_every = intervals[i];
        update_display_full(&hl);
        before_clears = clears;
        for (int page = 0; page < 64; ++page) {
            uint8_t expected_direct[FB_BYTES];
            for (int b = 0; b < FB_BYTES; ++b) {
                unsigned source = (unsigned)(b + page) % 256;
                front[b] = (uint8_t)source;
                expected_direct[b] = (uint8_t)(((source & 15) < 8 ? 0 : 15) | ((source >> 4) < 8 ? 0 : 15) << 4);
            }
            memcpy(expected_old, back, FB_BYTES);
            check_old_at_draw = true;
            before_draws = draws; before_full = full_draws;
            int on_before = powerons, off_before = poweroffs;
            assert(update_display_text_direct(&hl, false) == EPD_DRAW_SUCCESS);
            check_old_at_draw = false;
            assert(draws == before_draws + 1 && full_draws == before_full + 1);
            assert(powerons == on_before + 1 && poweroffs == off_before + 1);
            assert(last_waveform == &E0470_DIRECT_WAVEFORM && last_mode == MODE_GL16 && clears == before_clears);
            assert(hl.waveform == &E0470_WAVEFORM && !memcmp(front, back, FB_BYTES));
            assert(!memcmp(presented, expected_direct, FB_BYTES) && !memcmp(back, expected_direct, FB_BYTES));
            for (int b = 0; b < FB_BYTES; ++b) assert(front[b] == 0 || front[b] == 0x0f || front[b] == 0xf0 || front[b] == 255);
        }
        memcpy(front, target, FB_BYTES);
        update_display_text_turn(&hl, false);
        assert(last_mode == MODE_GL16 && !memcmp(front, target, FB_BYTES) && !memcmp(back, target, FB_BYTES));
    }
    // 直刷失败不得提交黑白参考；成功重试只恢复真实黑白目标，不回到伪灰帧。
    // A failed direct turn must not commit a black/white baseline; successful retry recovers the actual binary target instead of a fabricated gray frame.
    memcpy(expected_old, back, FB_BYTES);
    memset(front, 0x78, FB_BYTES);
    fail_mask = true;
    assert(update_display_text_direct(&hl, false) == EPD_DRAW_OTHER_ERROR);
    assert(!memcmp(back, expected_old, FB_BYTES));
    for (int b = 0; b < FB_BYTES; ++b) assert(front[b] == 0x0f);
    fail_mask = false;
    before_clears = clears;
    assert(update_display_text_direct(&hl, false) == EPD_DRAW_SUCCESS && last_mode == MODE_GC16 && clears == before_clears + 1);
    assert(!memcmp(front, back, FB_BYTES) && !memcmp(presented, back, FB_BYTES));
    assert(mask_draws > 0);
    memset(front, 255, FB_BYTES);
    fail_draw = true;
    assert(update_display_text_turn(&hl, false) == EPD_DRAW_OTHER_ERROR);
    assert(memcmp(front, back, FB_BYTES));
    fail_draw = false; before_clears = clears;
    assert(update_display_text_turn(&hl, false) == EPD_DRAW_SUCCESS && last_mode == MODE_GC16 && clears == before_clears + 1);
    unsigned clean_count = mask_draws;
    update_display_area_quiet(&hl, (EpdRect){1,2,3,4});
    assert(mask_draws == clean_count);
    assert(allocations == 0);
    check_night_experiment(&hl);
    puts("display: physical night cleanup preserves targets and establishes actual white, shared cycles 1/3/5/30/off, excluded day/controls/footer, successful resets, failed retry and underrun recovery passed");
    puts("display: targeted night experiments protect new-white/old-black pixels, preserve real baselines, exclude off/day/gray/controls, replace due/entry/recovery, reuse allocation and recover both scan failures");
}
