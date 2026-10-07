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

#define FB_BYTES 128
const EpdWaveform E0470_WAVEFORM = {0}, E0470_FOLLOW_WAVEFORM = {1}, E0470_FULL_WAVEFORM = {2}, E0470_TEXTTURN_WAVEFORM = {3}, E0470_NAVIGATION_WAVEFORM = {4}, E0470_NAVIGATION_ENTRY_WAVEFORM = {7};
const EpdWaveform E0470_DIRECT_WAVEFORM = {5}, E0470_WHITE_CLEANUP_WAVEFORM = {6}, E0470_TEXTTURN_NIGHT_WAVEFORM = {8};
static uint8_t target[FB_BYTES], presented[FB_BYTES];
static uint8_t expected_old[FB_BYTES];
static bool check_old_at_draw;
static int clocks, powerons, clears, draws, full_draws, safe_clock, prefill, poweroffs;
static bool white_baseline, correct_target_at_draw, fail_power, fail_draw;
static bool hv_on;
bool read_pico_rails_on(void) { return hv_on && !fail_power; }
static unsigned cleanup_every = 3;
static enum EpdDrawMode last_mode;
static bool last_area;
static const EpdWaveform* last_waveform;
int epd_width(void) { return 16; }
int epd_height(void) { return 16; }
uint8_t app_settings_gc_every(void) { return (uint8_t)cleanup_every; }

void read_pico_epd_set_pclk(int mhz) { ++clocks; safe_clock = mhz; }
void read_pico_epd_use_scan(read_pico_epd_scan_t scan) { assert(scan == READ_PICO_EPD_SCAN_FULL); }
void epd_lcd_set_prefill_lines(int lines) { prefill = lines; }
void epd_poweron(void) { ++powerons; hv_on = !fail_power; }
void epd_poweroff(void) { ++poweroffs; hv_on = false; }
void epd_clear(void) { assert(powerons > 0); ++clears; }
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
void* heap_caps_calloc(size_t n, size_t size, unsigned caps) {
    ++allocations; assert(caps == 3 && !hv_on); return fail_alloc ? NULL : calloc(n, size);
}
void epd_leading_skip_discard(void) {}
enum EpdDrawError epd_hl_update_screen_selective(EpdiyHighlevelState* hl, enum EpdDrawMode mode, int temperature, const uint8_t* mask) {
    // 产品页不得再按历史字形补擦白底。/ Product pages must never re-erase white backgrounds in historical glyph shapes.
    assert(mask == NULL);
    ++mask_draws;
    if (fail_mask) return EPD_DRAW_OTHER_ERROR;
    return epd_hl_update_screen_full(hl, mode, temperature);
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

int main(void) {
    check_history_reference();
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
    // 夜间标准仍用原选择性保持，黑背景不走厂家0→0擦写，也不触发周期清理。
    // Standard night retains selective hold, keeping black backgrounds off vendor 0→0 erasure and scheduled cleaning.
    for (unsigned n = 0; n < sizeof(intervals) / sizeof(intervals[0]); ++n) {
        cleanup_every = intervals[n];
        for (int turn = 0; turn < 32; ++turn) {
            memset(back, 0, FB_BYTES); memset(front, 0, FB_BYTES);
            unsigned selected_before = mask_draws; int drawn_before = draws;
            assert(update_display_text_turn(&hl, true) == EPD_DRAW_SUCCESS);
            assert(last_mode == MODE_GL16 && last_waveform == &E0470_TEXTTURN_NIGHT_WAVEFORM);
            assert(mask_draws == selected_before + 1 && draws == drawn_before + 1 && clears == clears_before_reading);
        }
    }
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
            assert(update_display_text_direct(&hl, (page & 1) != 0) == EPD_DRAW_SUCCESS);
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
    puts("display underrun: front retained, dual-buffer diff, full GC16 recovery and bulk prefill passed");
}
