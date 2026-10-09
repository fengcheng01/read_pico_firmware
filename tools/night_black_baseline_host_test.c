/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 *
 * 中文：真实显示、高层提交和厂家波形的双GC回归，只替换供电与扫描硬件边界。
 * English: Test actual display, high-level commits and vendor waveforms for dual GC, replacing only power and scan hardware seams.
 * 冻结：验证参考和相序，不模拟墨粒、不声称光学改善；两阶段失败都必须恢复未知状态。
 * Frozen: Verify references and phase order without particle simulation or optical claims; either stage failing must recover an unknown state.
 */
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "display.h"
#include "display_pixels.h"
#include "e0470_epaper_waveform.h"

enum { WIDTH = 64, HEIGHT = 4, PIXELS = WIDTH * HEIGHT, BYTES = PIXELS / 2 };
static EpdiyHighlevelState* current;
static uint8_t expected_front[BYTES], expected_prior[BYTES];
static unsigned scans, clears, ons, offs, gc_scans, hints_discarded;
static unsigned every = 3, fail_scan;
static bool night = true;
static bool fail_power, powered, expect_dual, hint_present;
static enum EpdDrawError failure = EPD_DRAW_FAILED_ALLOC;
static unsigned dual_stage;
static const EpdWaveform* expected_waveform;
static enum EpdDrawMode expected_mode;
static const EpdDisplay_t display = {.width = WIDTH, .height = HEIGHT, .default_waveform = &E0470_WAVEFORM};

int epd_width(void) { return WIDTH; }
int epd_height(void) { return HEIGHT; }
const EpdDisplay_t* epd_get_display(void) { return &display; }
EpdRect epd_full_screen(void) { return (EpdRect){0, 0, WIDTH, HEIGHT}; }
enum EpdRotation epd_get_rotation(void) { return EPD_ROT_LANDSCAPE; }
uint8_t app_settings_gc_every(void) { return every; }
bool app_settings_book_night(void) { return night; }
void read_pico_epd_set_pclk(int mhz) { (void)mhz; }
void read_pico_epd_use_scan(read_pico_epd_scan_t scan) { assert(scan == READ_PICO_EPD_SCAN_FULL); }
void epd_lcd_set_prefill_lines(int lines) { assert(lines == 127); }
void epd_poweron(void) { ++ons; powered = !fail_power; }
void epd_poweroff(void) { ++offs; powered = false; }
bool read_pico_rails_on(void) { return powered; }
void epd_clear(void) { assert(powered); ++clears; }
void epd_leading_skip_discard(void) { ++hints_discarded; hint_present = false; }
void epd_leading_skip_set_present(const uint8_t* data, const uint8_t present[256]) {
    uint8_t actual[256] = {0};
    for (unsigned p = 0; p < PIXELS; ++p) actual[data[p]] = 1;
    assert(!memcmp(present, actual, sizeof(actual)));
    hint_present = true;
}
void epd_difference_column_range(EpdRect area, int* first, int* end) {
    *first = area.x & ~31; *end = (area.x + area.width + 31) & ~31;
}
EpdRect epd_difference_image_cropped(const uint8_t* to, const uint8_t* from, EpdRect area,
    uint8_t* data, bool* lines, uint8_t* columns) {
    int first, end; epd_difference_column_range(area, &first, &end);
    memset(lines, 0, HEIGHT * sizeof(*lines)); memset(columns, 0, WIDTH / 2);
    bool changed = false;
    uint8_t present[256] = {0};
    for (int y = area.y; y < area.y + area.height; ++y) for (int x = first; x < end; ++x) {
        unsigned p = y * WIDTH + x, shift = (p % 2) * 4;
        unsigned a = to[p / 2] >> shift & 15, b = from[p / 2] >> shift & 15;
        data[p] = a << 4 | b; present[data[p]] = 1;
        if (a != b) { changed = lines[y] = true; columns[x / 2] = 255; }
    }
    epd_leading_skip_set_present(data, present);
    return changed ? area : (EpdRect){0};
}

enum EpdDrawError epd_draw_base(EpdRect area, const uint8_t* data, EpdRect crop,
    enum EpdDrawMode mode, int temperature, const bool* lines, const uint8_t* columns,
    const EpdWaveform* waveform) {
    assert(powered && temperature == 25);
    assert(area.x == 0 && area.y == 0 && area.width == WIDTH && area.height == HEIGHT);
    assert(crop.x == 0 && crop.y == 0 && crop.width == WIDTH && crop.height == HEIGHT);
    assert(!memcmp(current->front_fb, expected_front, BYTES));
    ++scans; gc_scans += (mode & 15) == MODE_GC16;
    if (expect_dual) {
        assert(waveform == &E0470_FULL_WAVEFORM);
        assert(mode == (MODE_PACKING_1PPB_DIFFERENCE | MODE_GC16));
        for (int y = 0; y < HEIGHT; ++y) assert(lines[y]);
        for (int x = 0; x < WIDTH / 2; ++x) assert(columns[x] == 255);
        const EpdWaveformPhases* gc = e0470_waveform_phases(waveform, MODE_GC16);
        assert(gc && gc->phases == 48);
        if (dual_stage == 0) {
            assert(!hint_present);
            assert(!memcmp(current->back_fb, expected_prior, BYTES));
            for (unsigned p = 0; p < PIXELS; ++p)
                assert(data[p] == (expected_prior[p / 2] >> ((p % 2) * 4) & 15));
        } else {
            assert(dual_stage == 1);
            for (unsigned b = 0; b < BYTES; ++b) assert(current->back_fb[b] == 0);
            for (unsigned p = 0; p < PIXELS; ++p) {
                unsigned goal = expected_front[p / 2] >> ((p % 2) * 4) & 15;
                assert(data[p] == goal << 4);
                if (goal == 0) for (int f = 0; f < 48; ++f)
                    assert(e0470_phase_action(gc, f, 0, 0) == (f < 10 || f >= 45 ? 0 : f < 28 ? 2 : 1));
            }
        }
        ++dual_stage;
    } else {
        assert(waveform == expected_waveform);
        assert(mode == (MODE_PACKING_1PPB_DIFFERENCE | expected_mode));
        assert(!memcmp(current->back_fb, expected_prior, BYTES));
    }
    hint_present = false;
    return fail_scan == scans ? failure : EPD_DRAW_SUCCESS;
}

static void targets(EpdiyHighlevelState* hl, unsigned seed) {
    for (unsigned p = 0; p < PIXELS; ++p) {
        unsigned goal = (p / 16 + seed) % 16;
        unsigned prior = (p + seed) % 16;
        if (!(p % 2)) hl->front_fb[p / 2] = hl->back_fb[p / 2] = 0;
        hl->front_fb[p / 2] |= goal << ((p % 2) * 4);
        hl->back_fb[p / 2] |= prior << ((p % 2) * 4);
    }
}
static void prepare(EpdiyHighlevelState* hl, bool direct, bool dual,
    const EpdWaveform* waveform, enum EpdDrawMode mode) {
    memcpy(expected_front, hl->front_fb, BYTES);
    if (direct) display_prepare_direct_frame(expected_front, WIDTH, HEIGHT, true);
    memcpy(expected_prior, hl->back_fb, BYTES);
    expect_dual = dual; dual_stage = 0;
    expected_waveform = waveform; expected_mode = mode;
    hint_present = true;
}
static void clean_start(EpdiyHighlevelState* hl) {
    // 正常GC仅用于重置测试计数，仍验证真实目标提交。/ Normal GC resets test counters while checking actual target commits.
    prepare(hl, false, false, &E0470_FULL_WAVEFORM, MODE_GC16);
    assert(update_display_full(hl) == EPD_DRAW_SUCCESS);
}
static void scan(EpdiyHighlevelState* hl, display_black_baseline_action_t action, bool direct, bool dual) {
    unsigned before = scans, before_clears = clears, before_ons = ons, before_offs = offs;
    prepare(hl, direct, dual, direct ? &E0470_DIRECT_WAVEFORM : &E0470_TEXTTURN_NIGHT_WAVEFORM, MODE_GL16);
    assert(update_display_night_black_baseline(hl, action, direct) == EPD_DRAW_SUCCESS);
    assert(scans == before + (dual ? 2 : 1) && clears == before_clears);
    assert(ons == before_ons + 1 && offs == before_offs + 1 && !powered);
    assert(!memcmp(hl->front_fb, expected_front, BYTES) && !memcmp(hl->back_fb, expected_front, BYTES));
    assert(hl->waveform == &E0470_WAVEFORM);
    if (dual) assert(dual_stage == 2);
}
static void recover(EpdiyHighlevelState* hl, bool direct) {
    unsigned before = scans, before_clears = clears;
    prepare(hl, direct, false, &E0470_FULL_WAVEFORM, MODE_GC16);
    // 旧恢复先物理清白，实际扫描时旧帧必须白。/ Existing recovery clears physically first, so the draw reference must be white.
    memset(expected_prior, 255, BYTES);
    assert(update_display_night_black_baseline(hl, DISPLAY_BLACK_BASELINE_TURN, direct) == EPD_DRAW_SUCCESS);
    assert(scans == before + 1 && clears == before_clears + 1);
    assert(!memcmp(hl->front_fb, expected_front, BYTES) && !memcmp(hl->back_fb, expected_front, BYTES));
}

int main(void) {
    uint8_t front[BYTES], back[BYTES], difference[PIXELS], columns[WIDTH / 2]; bool lines[HEIGHT];
    EpdiyHighlevelState hl = {.front_fb = front, .back_fb = back, .difference_fb = difference,
        .dirty_lines = lines, .dirty_columns = columns, .waveform = &E0470_WAVEFORM};
    current = &hl; e0470_waveform_init(); targets(&hl, 0);
    // 全部256迁移与灰阶/二值目标：前缓冲两次扫描都必须保留。/ All 256 source/target pairs and gray/binary targets preserve front through both scans.
    for (int direct = 0; direct < 2; ++direct) {
        targets(&hl, direct); clean_start(&hl); targets(&hl, direct);
        scan(&hl, DISPLAY_BLACK_BASELINE_CLEAN, direct, true);
        targets(&hl, direct + 1);
        scan(&hl, DISPLAY_BLACK_BASELINE_ENTRY, direct, true);
        targets(&hl, direct + 2); display_request_navigation_settle();
        scan(&hl, DISPLAY_BLACK_BASELINE_REDRAW, direct, true);
        scan(&hl, DISPLAY_BLACK_BASELINE_REDRAW, direct, false);
    }
    const unsigned periods[] = {1, 3, 5, 10, 30};
    for (unsigned c = 0; c < sizeof(periods)/sizeof(periods[0]); ++c) {
        every = periods[c]; clean_start(&hl);
        for (unsigned turn = 1; turn <= every * 2 + 2; ++turn) {
            targets(&hl, turn);
            scan(&hl, DISPLAY_BLACK_BASELINE_TURN, turn % 2, turn % every == 0);
        }
    }
    every = 3; clean_start(&hl);
    scan(&hl, DISPLAY_BLACK_BASELINE_TURN, false, false);
    // 同布局重绘和日间不推进夜间周期、不触发新清理。/ Same-layout redraws and daytime do not advance night cycles or invoke new cleaning.
    for (int c = 0; c < 32; ++c) {
        targets(&hl, c); scan(&hl, DISPLAY_BLACK_BASELINE_REDRAW, c % 2, false);
        unsigned before = scans, before_clears = clears;
        prepare(&hl, false, false, &E0470_TEXTTURN_WAVEFORM, MODE_GL16);
        assert(update_display_text_turn(&hl, false) == EPD_DRAW_SUCCESS);
        assert(scans == before + 1 && clears == before_clears);
        prepare(&hl, true, false, &E0470_DIRECT_WAVEFORM, MODE_GL16);
        assert(update_display_text_direct(&hl, false) == EPD_DRAW_SUCCESS);
        assert(scans == before + 2 && clears == before_clears);
    }
    scan(&hl, DISPLAY_BLACK_BASELINE_TURN, true, false);
    scan(&hl, DISPLAY_BLACK_BASELINE_TURN, false, true);
    every = 3; clean_start(&hl);
    scan(&hl, DISPLAY_BLACK_BASELINE_TURN, false, false);
    night = false;
    // 即使误从日间调用新入口，清理/入口动作也不得发出黑目标首扫。
    // Even accidental day calls to the new API must not issue a black-target stage for clean/entry actions.
    for (int direct = 0; direct < 2; ++direct) for (int action = 0; action < 4; ++action) {
        targets(&hl, direct + action + 1);
        unsigned before = scans, before_clears = clears, before_gc = gc_scans;
        prepare(&hl, direct, false, direct ? &E0470_DIRECT_WAVEFORM : &E0470_TEXTTURN_WAVEFORM, MODE_GL16);
        assert(update_display_night_black_baseline(&hl, (display_black_baseline_action_t)action, direct) == EPD_DRAW_SUCCESS);
        assert(scans == before + 1 && clears == before_clears && gc_scans == before_gc);
        assert(!memcmp(hl.front_fb, expected_front, BYTES) && !memcmp(hl.back_fb, expected_front, BYTES));
    }
    night = true;
    scan(&hl, DISPLAY_BLACK_BASELINE_TURN, true, false);
    scan(&hl, DISPLAY_BLACK_BASELINE_TURN, false, true);
    night = false;
    for (int direct = 0; direct < 2; ++direct) {
        targets(&hl, direct + 1);
        unsigned before = scans, before_clears = clears;
        prepare(&hl, direct, false, direct ? &E0470_DIRECT_WAVEFORM : &E0470_TEXTTURN_WAVEFORM, MODE_GL16);
        assert(update_display_night_black_baseline(&hl, DISPLAY_BLACK_BASELINE_CLEAN, direct) == EPD_DRAW_SUCCESS);
        assert(scans == before + 1 && clears == before_clears);
        assert(!memcmp(hl.front_fb, expected_front, BYTES) && !memcmp(hl.back_fb, expected_front, BYTES));
        targets(&hl, direct + 2); display_request_navigation_settle();
        before = scans;
        prepare(&hl, direct, false, &E0470_FULL_WAVEFORM, MODE_GC16);
        assert(update_display_night_black_baseline(&hl, DISPLAY_BLACK_BASELINE_ENTRY, direct) == EPD_DRAW_SUCCESS);
        assert(scans == before + 1 && clears == before_clears);
        assert(!memcmp(hl.front_fb, expected_front, BYTES) && !memcmp(hl.back_fb, expected_front, BYTES));
    }
    night = true;
    // 日间布局GC按原语义重置周期；新黑基准只有下一轮第三页才触发。
    // Original day-layout GC resets the interval; the new black cleaning triggers only on the third subsequent night turn.
    scan(&hl, DISPLAY_BLACK_BASELINE_TURN, false, false);
    scan(&hl, DISPLAY_BLACK_BASELINE_TURN, true, false);
    scan(&hl, DISPLAY_BLACK_BASELINE_TURN, false, true);
    every = 0;
    for (int c = 0; c < 64; ++c) scan(&hl, DISPLAY_BLACK_BASELINE_TURN, c % 2, false);
    every = 3;
    scan(&hl, DISPLAY_BLACK_BASELINE_TURN, false, false);
    scan(&hl, DISPLAY_BLACK_BASELINE_TURN, true, false);
    scan(&hl, DISPLAY_BLACK_BASELINE_TURN, false, true);

    // 内存/扫描两段失败、供电失败：已完成黑段可保留真实黑参考，其余不能伪造提交。
    // Allocation/scan failures in either stage and power failures retain only completed true references, without fictional commits.
    const enum EpdDrawError errors[] = {EPD_DRAW_FAILED_ALLOC, EPD_DRAW_NO_PHASES_AVAILABLE, EPD_DRAW_EMPTY_LINE_QUEUE};
    for (unsigned e = 0; e < sizeof(errors)/sizeof(errors[0]); ++e) for (unsigned stage = 1; stage <= 2; ++stage) {
        clean_start(&hl); targets(&hl, stage + e); display_request_navigation_settle();
        prepare(&hl, false, true, NULL, MODE_GC16); failure = errors[e];
        fail_scan = scans + stage; unsigned before = scans, before_clears = clears;
        assert(update_display_night_black_baseline(&hl, DISPLAY_BLACK_BASELINE_ENTRY, false) == failure);
        assert(scans == before + stage && clears == before_clears);
        assert(!memcmp(hl.front_fb, expected_front, BYTES));
        if (stage == 1) assert(!memcmp(hl.back_fb, expected_prior, BYTES));
        else for (unsigned b = 0; b < BYTES; ++b) assert(hl.back_fb[b] == 0);
        assert(hl.waveform == &E0470_WAVEFORM && !powered);
        fail_scan = 0; recover(&hl, false);
        scan(&hl, DISPLAY_BLACK_BASELINE_TURN, false, false);
        scan(&hl, DISPLAY_BLACK_BASELINE_TURN, true, false);
        scan(&hl, DISPLAY_BLACK_BASELINE_TURN, false, true);
    }
    clean_start(&hl); targets(&hl, 7); display_request_navigation_settle();
    prepare(&hl, true, true, NULL, MODE_GC16); fail_power = true;
    unsigned before = scans, before_clears = clears;
    assert(update_display_night_black_baseline(&hl, DISPLAY_BLACK_BASELINE_ENTRY, true) == EPD_DRAW_POWER_NOT_READY);
    assert(scans == before && clears == before_clears && !memcmp(hl.back_fb, expected_prior, BYTES));
    assert(!memcmp(hl.front_fb, expected_front, BYTES));
    fail_power = false; recover(&hl, true);
    scan(&hl, DISPLAY_BLACK_BASELINE_REDRAW, true, false);
    // 普通翻页扫描失败也沿用未知白恢复，不把黑档当作错误恢复波形。/ Failed ordinary turns still use unknown-white recovery, never treating black cleaning as fault recovery.
    clean_start(&hl); prepare(&hl, false, false, &E0470_TEXTTURN_NIGHT_WAVEFORM, MODE_GL16);
    fail_scan = scans + 1; failure = EPD_DRAW_FAILED_ALLOC;
    assert(update_display_night_black_baseline(&hl, DISPLAY_BLACK_BASELINE_TURN, false) == failure);
    assert(!memcmp(hl.back_fb, expected_prior, BYTES)); fail_scan = 0;
    recover(&hl, false);
    assert(hints_discarded && gc_scans);
    puts("black baseline: actual display+highlevel+vendor GC48 paths, retained targets, true two-stage commits, 00 reconditioning, saved cycles and day/redraw exclusion PASS");
    puts("black baseline: power/allocation/first/second/ordinary scan failures preserve true references, restore white on retry, reset cycles only after success PASS (no optical claim)");
}
