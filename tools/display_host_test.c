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
#include "display.h"
#include "display_pixels.h"
#include "e0470_epaper_waveform.h"
#include "e0470_page_turn.h"
#include "esp_heap_caps.h"

#define FB_BYTES 128
const EpdWaveform E0470_WAVEFORM = {0}, E0470_FOLLOW_WAVEFORM = {1}, E0470_FULL_WAVEFORM = {2}, E0470_TEXTTURN_WAVEFORM = {3}, E0470_NAVIGATION_WAVEFORM = {4}, E0470_NAVIGATION_ENTRY_WAVEFORM = {7};
const EpdWaveform E0470_DIRECT_WAVEFORM = {5}, E0470_WHITE_CLEANUP_WAVEFORM = {6}, E0470_TEXTTURN_NIGHT_WAVEFORM = {8};
const EpdWaveform E0470_GRAY_DIRECT_WAVEFORM = {10};
const EpdWaveform E0470_GRAY_BALANCED_WAVEFORM = {15};
const EpdWaveform E0470_NIGHT_PREWIPE_GL_WAVEFORM = {11}, E0470_DIRECT_PREWIPE_WAVEFORM = {12}, E0470_GRAY_DIRECT_PREWIPE_WAVEFORM = {13};
static uint8_t target[FB_BYTES], presented[FB_BYTES];
static uint8_t expected_old[FB_BYTES];
static bool check_old_at_draw;
static EpdiyHighlevelState* clear_hl;
static uint8_t clear_target[FB_BYTES], clear_prior[FB_BYTES];
static bool check_clean_at_clear;
static int clocks, powerons, clears, draws, full_draws, safe_clock, prefill, poweroffs;
static bool white_baseline, correct_target_at_draw, fail_power, fail_draw;
static bool hv_on;
static read_pico_epd_scan_t last_scan;
bool read_pico_rails_on(void) { return hv_on && !fail_power; }
static unsigned cleanup_every = 3;
static enum EpdDrawMode last_mode;
static bool last_area;
static const EpdWaveform* last_waveform;
int epd_width(void) { return 16; }
int epd_height(void) { return 16; }
int epd_rotated_display_width(void) { return 16; }
int epd_rotated_display_height(void) { return 16; }
uint8_t app_settings_gc_every(void) { return (uint8_t)cleanup_every; }
bool app_settings_book_night(void) { return true; }
// 夜间白预驱开关替身：默认关，检查函数按需翻转。/ Night white pre-drive switch shim: off by default, flipped by checks.
static bool night_prewipe;
bool app_settings_book_night_prewipe(void) { return night_prewipe; }
// 仅测试保留的旧符号：撤回后真实显示路径不得读取旧实验档位。
// Test-only legacy symbol: the withdrawn production display path must never read old experiment tiers.
static uint8_t legacy_night_cleanup;
static unsigned legacy_cleanup_reads;
uint8_t app_settings_book_night_cleanup(void) { ++legacy_cleanup_reads; return legacy_night_cleanup; }

// 错相揭页替身：记录方向并可注入失败；成功按引擎契约提交基准。/ Staggered-turn shim: record direction, inject failures; success commits the baseline per the engine contract.
static int turn_calls;
static enum EpdDrawError turn_result = EPD_DRAW_SUCCESS;
static bool turn_forward[8];
static bool turn_busy;
static uint8_t animation_style;
static unsigned soft_gets, soft_releases;
static const EpdWaveform soft_waveform={14};
uint8_t app_settings_book_turn_style(void) { return animation_style; }
const EpdWaveform* display_animation_waveform(void) { ++soft_gets;return fail_draw?&E0470_TEXTTURN_WAVEFORM:&soft_waveform; }
void display_animation_release(void) { assert(!turn_busy);++soft_releases; }
void read_pico_epd_set_pclk(int mhz) { ++clocks; safe_clock = mhz; }
// FAST 现有两条合法路径（跟随 DU、错相揭页），替身无法区分调用者，只拒绝其他值。
// FAST now has two sanctioned callers (follow DU, staggered page turn); the stub cannot tell them apart and rejects other values only.
void read_pico_epd_use_scan(read_pico_epd_scan_t scan) {
    assert(scan == READ_PICO_EPD_SCAN_FULL || scan == READ_PICO_EPD_SCAN_FAST);
    last_scan=scan;
}
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
static unsigned masked_draws;  // 携带夜间保持掩码的选择性扫描 / Selective scans carrying the night hold mask
static bool fail_mask, fail_alloc;
static unsigned allocations;
void* heap_caps_calloc(size_t n, size_t size, unsigned caps) {
    ++allocations; assert(caps == 3 && !hv_on); return fail_alloc ? NULL : calloc(n, size);
}
void* heap_caps_malloc(size_t size, unsigned caps) {
    // 掩码与固件同寿命不释放；替身用静态槽避免泄漏检测误报。
    // The mask lives as long as the firmware and is never freed; the stub uses a static slot to keep leak checks quiet.
    static uint8_t slot[256];
    ++allocations; assert(caps == MALLOC_CAP_SPIRAM);
    return fail_alloc || size > sizeof(slot) ? NULL : slot;
}
void epd_leading_skip_discard(void) {}
// 掩码仅在预驱表下出现，且每位都等于“目标非纯黑”；其余选择性扫描保持无掩码。
// The mask appears only under pre-drive tables with every bit meaning "target not pure black";
// all other selective scans stay unmasked.
static void check_night_mask_content(const EpdiyHighlevelState* hl, const uint8_t* mask) {
    if (!mask) return;
    ++masked_draws;
    const bool prewipe = hl->waveform == &E0470_NIGHT_PREWIPE_GL_WAVEFORM ||
        hl->waveform == &E0470_DIRECT_PREWIPE_WAVEFORM || hl->waveform == &E0470_GRAY_DIRECT_PREWIPE_WAVEFORM;
    assert(prewipe);
    for (int y = 0; y < epd_height(); ++y) for (int x = 0; x < epd_width(); ++x) {
        const size_t pixel = (size_t)y * epd_width() + x;
        const unsigned goal = hl->front_fb[pixel / 2] >> ((pixel % 2) * 4) & 15;
        assert(((mask[pixel / 8] >> (pixel % 8)) & 1) == (goal != 0));
    }
}
enum EpdDrawError epd_hl_update_screen_selective(EpdiyHighlevelState* hl, enum EpdDrawMode mode, int temperature, const uint8_t* mask) {
    // 产品页不得再按历史字形补擦白底；夜间预驱例外是保持掩码，不是字形补擦。/ Product pages must never re-erase
    // white backgrounds in historical glyph shapes; the night pre-drive exception is a hold mask, not glyph cleanup.
    check_night_mask_content(hl, mask);
    ++mask_draws;
    if (fail_mask) return EPD_DRAW_OTHER_ERROR;
    return epd_hl_update_screen_full(hl, mode, temperature);
}

// 高层扫描已由上面的替身承接；直接底层补扫代表已撤回的实验意外复活。
// High-level scans are handled above; a direct low-level post scan means the withdrawn experiment has returned accidentally.
enum EpdDrawError epd_draw_base(EpdRect area, const uint8_t* data, EpdRect crop,
    enum EpdDrawMode mode, int temperature, const bool* lines, const uint8_t* columns,
    const EpdWaveform* waveform) {
    (void)area; (void)data; (void)crop; (void)mode; (void)temperature;
    (void)lines; (void)columns; (void)waveform;
    assert(!"withdrawn night post scan must remain disabled");
    return EPD_DRAW_OTHER_ERROR;
}

static int turn_tick=E0470_TURN_FAST_TICK_US;
static unsigned turn_releases;
void e0470_page_turn_set_tick_us(int us) { assert(us == 0 || us == E0470_TURN_FAST_TICK_US || us == E0470_TURN_DEFAULT_TICK_US);turn_tick=us; }
int e0470_page_turn_tick_us(void) { return turn_tick; }
void e0470_page_turn_release(void) { assert(!turn_busy);++turn_releases; }
enum EpdDrawError e0470_page_turn(EpdiyHighlevelState* hl, EpdRect area, e0470_turn_dir_t dir) {
    return e0470_page_turn_with_waveform_compact(hl, area, dir, &E0470_TEXTTURN_WAVEFORM, 16);
}
enum EpdDrawError e0470_page_turn_with_waveform(
    EpdiyHighlevelState* hl, EpdRect area, e0470_turn_dir_t dir,
    const EpdWaveform* waveform, unsigned bands) {
    return e0470_page_turn_with_waveform_compact(hl, area, dir, waveform, bands);
}
enum EpdDrawError e0470_page_turn_with_waveform_compact(
    EpdiyHighlevelState* hl, EpdRect area, e0470_turn_dir_t dir,
    const EpdWaveform* waveform, unsigned bands) {
    (void)area; (void)waveform;
    if(waveform==&E0470_GRAY_BALANCED_WAVEFORM) {
        assert(bands==8 && dir==E0470_TURN_RTL && turn_tick==0);
        assert(area.x==0 && area.y==0 && area.width==16 && area.height==16);
        assert(last_scan==READ_PICO_EPD_SCAN_FULL && prefill==127);
    } else assert(bands == 16 && (waveform == &E0470_TEXTTURN_WAVEFORM || (animation_style==2&&waveform==&soft_waveform)));
    if (turn_calls < 8) turn_forward[turn_calls] = dir != E0470_TURN_LTR;
    ++turn_calls;
    turn_busy = true;
    // 成功即按引擎契约提交基准；失败不回写，留给原出口恢复。
    // Success commits the baseline per the engine contract; failure keeps it for the original exit.
    enum EpdDrawError result = turn_result;
    if (result == EPD_DRAW_SUCCESS) memcpy(hl->back_fb, hl->front_fb, FB_BYTES);
    turn_busy = false;
    return result;
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
    // 所有推屏统一闲置保活：推完保持上电，8 秒到期检查才断（电源时序决策变更）。
    // Every update shares the idle keepalive: rails stay up after the push; only the
    // 8-second deadline check cuts them.
    assert(draws == drawn_before + 1 && powerons == on_before + 1 && poweroffs == off_before);
    rails_idle_check(esp_timer_get_time() / 1000 + 9000);
    assert(poweroffs == off_before + 1);
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

// 白预驱开时三档夜间普通翻页换预驱波形且仍走选择性整页；到期/入口清理与日间完全不变。
// With white pre-drive on, the three ordinary night profiles swap to pre-drive waveforms and keep selective
// whole-page scans; due/entry cleaning and day turns stay exactly as before.
static void check_night_prewipe(EpdiyHighlevelState* hl) {
    cleanup_every = 3;
    night_prewipe = true;
    assert(update_display_full(hl) == EPD_DRAW_SUCCESS);
    for (int turn = 1; turn <= 6; ++turn) {
        memset(hl->front_fb, turn & 1 ? 0xf0 : 0x0f, FB_BYTES);
        const bool direct = turn % 2 == 0, gray = turn % 3 == 0;
        int before_draws = draws, before_clears = clears;
        unsigned before_selected = mask_draws;
        memcpy(expected_old, hl->back_fb, FB_BYTES);
        const bool due = turn % 3 == 0;
        if (due) memset(expected_old, 255, FB_BYTES);
        check_old_at_draw = true;
        enum EpdDrawError err = gray ? update_display_text_gray_direct(hl, true)
            : direct ? update_display_text_direct(hl, true) : update_display_text_turn(hl, true);
        check_old_at_draw = false;
        assert(err == EPD_DRAW_SUCCESS && draws == before_draws + 1);
        assert(clears == before_clears + due);
        if (due) assert(last_mode == MODE_GC16 && last_waveform == &E0470_FULL_WAVEFORM);
        else {
            assert(last_mode == MODE_GL16 && mask_draws == before_selected + 1);
            assert(last_waveform == (gray ? &E0470_GRAY_DIRECT_PREWIPE_WAVEFORM :
                direct ? &E0470_DIRECT_PREWIPE_WAVEFORM : &E0470_NIGHT_PREWIPE_GL_WAVEFORM));
        }
        assert(!memcmp(hl->front_fb, hl->back_fb, FB_BYTES) && hl->waveform == &E0470_WAVEFORM);
    }
    // 白重画普通夜间翻页同样换预驱表；日间调用不受开关影响。
    // Night white-repaint ordinary turns swap likewise, while day calls ignore the switch.
    memset(hl->front_fb, 0x0f, FB_BYTES);
    int before_draws2 = draws;
    assert(update_display_night_white_repaint(hl, DISPLAY_NIGHT_TURN, true, true) == EPD_DRAW_SUCCESS);
    assert(draws == before_draws2 + 1 && last_waveform == &E0470_GRAY_DIRECT_PREWIPE_WAVEFORM && last_mode == MODE_GL16);
    memset(hl->front_fb, 0x37, FB_BYTES);
    assert(update_display_night_white_repaint(hl, DISPLAY_NIGHT_TURN, false, false) == EPD_DRAW_SUCCESS);
    assert(last_waveform == &E0470_NIGHT_PREWIPE_GL_WAVEFORM && last_mode == MODE_GL16);
    assert(update_display_text_turn(hl, false) == EPD_DRAW_SUCCESS && last_waveform == &E0470_TEXTTURN_WAVEFORM);
    assert(update_display_text_direct(hl, false) == EPD_DRAW_SUCCESS && last_waveform == &E0470_DIRECT_WAVEFORM);
    assert(update_display_text_gray_direct(hl, false) == EPD_DRAW_SUCCESS && last_waveform == &E0470_GRAY_DIRECT_WAVEFORM);
    // 入口清理在预驱开时仍先物理清白再厂家GC。/ Entry cleaning still physically clears white before vendor GC with pre-drive on.
    memset(hl->front_fb, 0x5a, FB_BYTES);
    display_request_navigation_settle();
    int before_clears2 = clears;
    assert(update_display_text_turn(hl, true) == EPD_DRAW_SUCCESS);
    assert(clears == before_clears2 + 1 && last_mode == MODE_GC16 && last_waveform == &E0470_FULL_WAVEFORM);
    // 六次预驱选择性扫描全部携带保持掩码且内容逐位核对；非预驱与清理扫描不带掩码。
    // All six pre-drive selective scans carry the hold mask verified bit by bit;
    // non-pre-drive and cleaning scans carry none.
    assert(masked_draws == 6);
    night_prewipe = false;
    memset(hl->front_fb, 0x0f, FB_BYTES);
    assert(update_display_text_turn(hl, true) == EPD_DRAW_SUCCESS && last_waveform == &E0470_TEXTTURN_NIGHT_WAVEFORM);
    assert(update_display_text_gray_direct(hl, true) == EPD_DRAW_SUCCESS && last_waveform == &E0470_GRAY_DIRECT_WAVEFORM);
    assert(masked_draws == 6);
    memcpy(hl->front_fb, target, FB_BYTES);
    assert(update_display_full(hl) == EPD_DRAW_SUCCESS);
}

static void check_withdrawn_night_tiers(EpdiyHighlevelState* hl) {    // 升级残留1/2不能唤起补扫或分配；交替两种夜间效果仍只在第3/6页清理。
    // Legacy upgrade tiers 1/2 cannot activate post scans or allocation; alternating night profiles still clean only on turns three and six.
    cleanup_every = 3;
    unsigned before_alloc = allocations;
    fail_alloc = true;
    for (legacy_night_cleanup = 1; legacy_night_cleanup <= 2; ++legacy_night_cleanup) {
        assert(update_display_full(hl) == EPD_DRAW_SUCCESS);
        for (unsigned turn = 1; turn <= 7; ++turn) {
            memset(hl->front_fb, turn & 1 ? 0xf0 : 0x0f, FB_BYTES);
            assert_night_scan(hl, turn & 1, turn % 3 == 0);
        }
    }
    fail_alloc = false;
    assert(legacy_cleanup_reads == 0 && allocations == before_alloc);
}

// 产品日间入口统一按实际成功翻页计数；旧低层API仍供控件和兼容路径使用。
// The product day entry counts successful body turns; legacy primitives remain for controls/compatibility.
static void check_day_product_cycles(EpdiyHighlevelState* hl) {
    const unsigned intervals[] = {1, 3, 5, 30};
    EpdRect area = {0, 0, 16, 16};
    for (unsigned i = 0; i < sizeof(intervals) / sizeof(intervals[0]); ++i) {
        cleanup_every = intervals[i];
        for (int profile = DISPLAY_READER_STANDARD; profile <= DISPLAY_READER_GRAY_COMPACT; ++profile) {
            assert(update_display_full(hl) == EPD_DRAW_SUCCESS);
            int old_clears = clears;
            for (unsigned turn = 1; turn <= intervals[i] * 2 + 1; ++turn) {
                memset(hl->front_fb, (int)(turn * 17), FB_BYTES);
                last_mode = MODE_DU;
                int previous_draws = draws;
                assert(update_display_reader_turn(hl, (display_reader_mode_t)profile, area, turn % 2) == EPD_DRAW_SUCCESS);
                if (turn % intervals[i] == 0) {
                    assert(last_mode == MODE_GC16 && last_waveform == &E0470_FULL_WAVEFORM);
                    assert(draws == previous_draws + 1);
                } else {
                    assert(last_mode != MODE_GC16);
                    if(profile>=DISPLAY_READER_GRAY_BALANCED) {
                        assert(last_waveform==&E0470_GRAY_BALANCED_WAVEFORM);
                        assert(draws==previous_draws+1);
                        assert(last_scan==READ_PICO_EPD_SCAN_FULL && prefill==127);
                    }
                }
                assert(clears == old_clears);
                assert(!memcmp(hl->front_fb, hl->back_fb, FB_BYTES));
                if (profile == DISPLAY_READER_BINARY)
                    for (size_t j = 0; j < FB_BYTES; ++j) assert((hl->front_fb[j] & 15) == 0 || (hl->front_fb[j] & 15) == 15);
            }
        }
    }
    cleanup_every = 3;
    assert(update_display_full(hl) == EPD_DRAW_SUCCESS);
    assert(update_display_reader_turn(hl, DISPLAY_READER_STANDARD, area, true) == EPD_DRAW_SUCCESS);
    for (int i = 0; i < 20; ++i)
        assert(update_display_area_with(hl, &E0470_WAVEFORM, MODE_GL16, (EpdRect){1,2,3,4}) == EPD_DRAW_SUCCESS);
    assert(update_display_reader_turn(hl, DISPLAY_READER_GRAY, area, true) == EPD_DRAW_SUCCESS && last_mode == MODE_GL16);
    fail_power = true;
    assert(update_display_reader_turn(hl, DISPLAY_READER_BINARY, area, true) == EPD_DRAW_POWER_NOT_READY);
    fail_power = false;
    assert(update_display_reader_turn(hl, DISPLAY_READER_BINARY, area, true) == EPD_DRAW_SUCCESS && last_mode == MODE_GC16);
    assert(update_display_reader_turn(hl, DISPLAY_READER_STANDARD, area, true) == EPD_DRAW_SUCCESS && last_mode == MODE_GL16);
    assert(update_display_full(hl) == EPD_DRAW_SUCCESS);
    assert(update_display_reader_turn(hl, DISPLAY_READER_STANDARD, area, true) == EPD_DRAW_SUCCESS && last_mode == MODE_GL16);
    cleanup_every = 0;
    fail_draw=true;
    assert(update_display_reader_turn(hl,DISPLAY_READER_GRAY_BALANCED,area,true)!=EPD_DRAW_SUCCESS);
    fail_draw=false;
    assert(update_display_reader_turn(hl,DISPLAY_READER_GRAY_BALANCED,area,true)==EPD_DRAW_SUCCESS && last_mode==MODE_GC16);
    int balanced_before=draws;
    assert(update_display_reader_turn(hl,DISPLAY_READER_GRAY_BALANCED,area,true)==EPD_DRAW_SUCCESS);
    assert(last_waveform==&E0470_GRAY_BALANCED_WAVEFORM && draws==balanced_before+1);
    int old_turns=turn_calls,old_draws=draws;
    assert(update_display_reader_turn(hl,DISPLAY_READER_GRAY_STAGGERED,area,true)==EPD_DRAW_SUCCESS);
    assert(turn_calls==old_turns && draws==old_draws+1 && last_waveform==&E0470_GRAY_BALANCED_WAVEFORM);
    assert(update_display_reader_turn(hl,DISPLAY_READER_GRAY_SYNC,area,true)==EPD_DRAW_SUCCESS);
    assert(turn_calls==old_turns && draws==old_draws+2 && last_waveform==&E0470_GRAY_BALANCED_WAVEFORM);
    assert(update_display_reader_turn(hl,DISPLAY_READER_GRAY_MILD,area,true)==EPD_DRAW_SUCCESS);
    assert(turn_calls==old_turns && draws==old_draws+3 && last_waveform==&E0470_GRAY_BALANCED_WAVEFORM);
    assert(update_display_reader_turn(hl,DISPLAY_READER_GRAY_COMPACT,area,true)==EPD_DRAW_SUCCESS);
    assert(turn_calls==old_turns && draws==old_draws+4 && last_waveform==&E0470_GRAY_BALANCED_WAVEFORM);
    fail_draw=true;
    assert(update_display_reader_turn(hl,DISPLAY_READER_GRAY_COMPACT,area,true)!=EPD_DRAW_SUCCESS);
    fail_draw=false;
    assert(update_display_reader_turn(hl,DISPLAY_READER_GRAY_COMPACT,area,true)==EPD_DRAW_SUCCESS && last_mode==MODE_GC16);
    assert(turn_calls==old_turns && !turn_busy);
    for (int i = 0; i < 64; ++i)
        assert(update_display_reader_turn(hl, DISPLAY_READER_STANDARD, area, true) == EPD_DRAW_SUCCESS && last_mode == MODE_GL16);
    int previous_draws = draws;
    assert(update_display_reader_turn(hl, (display_reader_mode_t)99, area, true) == EPD_DRAW_INVALID_CROP && draws == previous_draws);
    cleanup_every = 5;
    puts("product day cleanup: standard/binary/gray/animation intervals1/3/5/30/off, one GC target/no preclear, shared mixed modes, controls excluded, failed power retry and manual reset PASS");
}

int main(void) {
    check_history_reference();
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
    // 统一闲置保活：普通刷新不再立即下电，到期检查只断一次且不重复。
    // Unified idle keepalive: ordinary pushes no longer power off immediately; the deadline
    // check cuts once and never repeats.
    assert(draws == 7 && full_draws == 2 && !clears && poweroffs == 0);
    rails_idle_check(esp_timer_get_time() / 1000 + 9000);
    assert(poweroffs == 1);
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
            // 直刷正文翻页同样走闲置保活，不逐页上下电。
            // Direct body turns share the idle keepalive instead of cycling rails per page.
            assert(powerons == on_before + 1 && poweroffs == off_before);
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
    check_withdrawn_night_tiers(&hl);
    check_night_prewipe(&hl);
    // 错相揭页：成功提交基准并交闲置保活；到期断电；失败按未知基准整屏恢复；入口补偿不进引擎。
    // Staggered page turns: success commits the baseline and defers power-off to the idle deadline;
    // failure recovers whole-screen from an unknown baseline; entry compensation bypasses the engine entirely.
    {
        int before_turns = turn_calls, before_draws = draws, before_clears = clears, before_offs = poweroffs;
        memset(front, 0x5A, FB_BYTES);
        memset(back, 0xA5, FB_BYTES);
        assert(update_display_page_turn(&hl, (EpdRect){0, 0, 16, 16}, true) == EPD_DRAW_SUCCESS);
        assert(turn_calls == before_turns + 1 && turn_forward[turn_calls - 1]);
        assert(!memcmp(front, back, FB_BYTES) && draws == before_draws && clears == before_clears);
        assert(poweroffs == before_offs);
        rails_idle_check(esp_timer_get_time() / 1000 + 9000);
        assert(poweroffs == before_offs + 1);
        assert(update_display_page_turn(&hl, (EpdRect){0, 0, 16, 16}, false) == EPD_DRAW_SUCCESS);
        assert(turn_calls == before_turns + 2 && !turn_forward[turn_calls - 1]);
        // 引擎失败：不提交基准，标未知后由原出口物理清白整屏重放。
        // Engine failure: keep the baseline, mark unknown and let the original exit physically clear and replay.
        memset(front, 0x33, FB_BYTES);
        memset(back, 0xCC, FB_BYTES);
        turn_result = EPD_DRAW_OTHER_ERROR;
        int clears_at_fail = clears;
        assert(update_display_page_turn(&hl, (EpdRect){0, 0, 16, 16}, true) == EPD_DRAW_SUCCESS);
        assert(turn_calls == before_turns + 3 && clears == clears_at_fail + 1);
        assert(!memcmp(front, back, FB_BYTES));
        turn_result = EPD_DRAW_SUCCESS;
        // 入口补偿在场时直接走原文字翻页，不调度条带。
        // With entry compensation pending, take the original text turn without scheduling bands.
        display_request_navigation_settle();
        int turns_at_entry = turn_calls;
        memset(back, 0x77, FB_BYTES);
        assert(update_display_page_turn(&hl, (EpdRect){0, 0, 16, 16}, true) == EPD_DRAW_SUCCESS);
        assert(turn_calls == turns_at_entry && last_waveform == &E0470_FULL_WAVEFORM);
    }
    {
        cleanup_every=0;animation_style=2;
        unsigned before=soft_gets, released=soft_releases;
        assert(update_display_page_turn(&hl,(EpdRect){0,0,16,16},true)==EPD_DRAW_SUCCESS);
        assert(soft_gets==before+1&&soft_releases==released+1);
        turn_result=EPD_DRAW_OTHER_ERROR;
        assert(update_display_page_turn(&hl,(EpdRect){0,0,16,16},false)==EPD_DRAW_SUCCESS);
        assert(soft_gets==before+2&&soft_releases==released+2&&!memcmp(front,back,FB_BYTES));
        turn_result=EPD_DRAW_SUCCESS;animation_style=1;
        assert(update_display_page_turn(&hl,(EpdRect){0,0,16,16},true)==EPD_DRAW_SUCCESS&&soft_gets==before+2);
        animation_style=0;
    }
    check_day_product_cycles(&hl);
    puts("display: physical night cleanup preserves targets and establishes actual white, shared cycles 1/3/5/30/off, excluded day/controls/footer, successful resets, failed retry and underrun recovery passed");
    puts("display: legacy night experiment tiers 1/2 remain unread; no post scans or allocation, both night profiles retain their shared cleanup interval");
    puts("display: white pre-drive swaps only the three ordinary night turn waveforms, keeps selective scans, due/entry cleaning and day profiles unchanged");
}
