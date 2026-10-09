/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 *
 * 中文：真实显示出口、高层提交、物理清白循环及厂家表回归，只替换供电和扫描硬件边界。
 * English: Test actual display paths, high-level commits, physical-clear loops and vendor tables, replacing only power and scan hardware seams.
 * 冻结：断言参考与相序，不模拟墨粒、不声称光学改善；失败必须保留目标并恢复未知状态。
 * Frozen: Assert references and phase order without particle simulation or optical claims; failures preserve targets and restore unknown state.
 */
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "display.h"
#include "display_pixels.h"
#include "e0470_epaper_waveform.h"

enum { WIDTH = 64, HEIGHT = 4, PIXELS = WIDTH * HEIGHT, BYTES = PIXELS / 2, CLEAR_PUSHES = 78 };
static EpdiyHighlevelState* current;
static uint8_t expected_front[BYTES], expected_prior[BYTES], clear_prior[BYTES];
static unsigned scans, pushes, ons, offs, gc_scans, fail_scan, start_pushes;
static unsigned every = 3;
static bool night = true, fail_power, powered, expect_clean, hint_present, expect_area;
static enum EpdDrawError failure = EPD_DRAW_FAILED_ALLOC;
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

void epd_push_pixels(EpdRect area, short time, int color) {
    assert(powered && expect_clean);
    assert(area.x == 0 && area.y == 0 && area.width == WIDTH && area.height == HEIGHT);
    assert(time == 15);
    // 每轮必须按原10黑/13白/3保持结束，且直到全部78次真实推屏返回才准修改旧参考。
    // Every round retains 10 black/13 white/3 holds; prior references cannot change before all 78 actual pushes return.
    unsigned phase = pushes - start_pushes;
    assert(phase < CLEAR_PUSHES);
    assert(color == (phase % 26 < 10 ? 0 : phase % 26 < 23 ? 1 : 2));
    assert(!memcmp(current->front_fb, expected_front, BYTES));
    assert(!memcmp(current->back_fb, clear_prior, BYTES));
    ++pushes;
}
void epd_leading_skip_discard(void) { hint_present = false; }
void epd_leading_skip_set_present(const uint8_t* data, const uint8_t present[256]) {
    uint8_t actual[256] = {0};
    for (unsigned p = 0; p < (expect_area ? 32 : PIXELS); ++p) actual[data[p]] = 1;
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
    assert(!memcmp(current->back_fb, expected_prior, BYTES));
    assert(pushes - start_pushes == (expect_clean ? CLEAR_PUSHES : 0));
    assert(waveform == expected_waveform);
    assert(mode == (MODE_PACKING_1PPB_DIFFERENCE | expected_mode));
    assert(hint_present);
    for (int y = 0; y < HEIGHT; ++y) assert(lines[y] == (!expect_area || y == 0));
    for (int x = 0; x < WIDTH / 2; ++x) assert(columns[x] == (!expect_area || x < 16 ? 255 : 0));
    const EpdWaveformPhases* phases = e0470_waveform_phases(waveform, expected_mode);
    assert(phases);
    for (unsigned p = 0; p < PIXELS; ++p) {
        if (expect_area && p >= 32) continue;
        unsigned to = expected_front[p / 2] >> ((p % 2) * 4) & 15;
        unsigned from = expected_prior[p / 2] >> ((p % 2) * 4) & 15;
        bool selective = waveform == &E0470_TEXTTURN_NIGHT_WAVEFORM ||
            waveform == &E0470_DIRECT_WAVEFORM || waveform == &E0470_GRAY_DIRECT_WAVEFORM;
        unsigned selector = to == from && !expect_clean && !expect_area && selective ? 0xee : to << 4 | from;
        assert(data[p] == selector);
        if (expect_clean) {
            assert(from == 15);
            if (expected_mode != MODE_GC16 && to == 15)
                for (int phase = 0; phase < phases->phases; ++phase)
                    assert(e0470_phase_action(phases, phase, to, from) == 0);
            if (expected_mode != MODE_GC16 && to == 0) {
                unsigned blacks = 0;
                for (int phase = 0; phase < phases->phases; ++phase) {
                    int action = e0470_phase_action(phases, phase, to, from);
                    assert(action != 2 && action != 3);
                    blacks += action == 1;
                }
                assert(blacks == 18);
            }
        }
    }
    ++scans; gc_scans += (mode & 15) == MODE_GC16;
    hint_present = false;
    return fail_scan == scans ? failure : EPD_DRAW_SUCCESS;
}

static void targets(EpdiyHighlevelState* hl, unsigned seed) {
    // 单帧覆盖所有256旧/新灰码对，包含空白、纯黑和相同像素。
    // One frame covers all 256 prior/target gray-code pairs, including white, black and equal pixels.
    for (unsigned p = 0; p < PIXELS; ++p) {
        unsigned goal = (p / 16 + seed) % 16;
        unsigned prior = (p + seed) % 16;
        if (!(p % 2)) hl->front_fb[p / 2] = hl->back_fb[p / 2] = 0;
        hl->front_fb[p / 2] |= goal << ((p % 2) * 4);
        hl->back_fb[p / 2] |= prior << ((p % 2) * 4);
    }
}
static void prepare(EpdiyHighlevelState* hl, bool binary, bool clean,
    const EpdWaveform* waveform, enum EpdDrawMode mode) {
    memcpy(expected_front, hl->front_fb, BYTES);
    if (binary) display_prepare_direct_frame(expected_front, WIDTH, HEIGHT, true);
    memcpy(expected_prior, hl->back_fb, BYTES);
    memcpy(clear_prior, hl->back_fb, BYTES);
    if (clean) memset(expected_prior, 255, BYTES);
    expect_clean = clean; start_pushes = pushes;
    expect_area = false;
    expected_waveform = waveform; expected_mode = mode;
}
static void clean_start(EpdiyHighlevelState* hl) {
    // 普通GC只用于重置测试计数，仍验证真实目标提交。/ Normal GC only resets test counters while verifying actual target commits.
    prepare(hl, false, false, &E0470_FULL_WAVEFORM, MODE_GC16);
    assert(update_display_full(hl) == EPD_DRAW_SUCCESS);
}
static void scan(EpdiyHighlevelState* hl, display_crossmux_action_t action, unsigned effect, bool clean) {
    assert(effect < 3);
    const bool direct = effect != 0, gray = effect == 2;
    const EpdWaveform* ordinary = effect == 0 ? &E0470_TEXTTURN_NIGHT_WAVEFORM :
        effect == 1 ? &E0470_DIRECT_WAVEFORM : &E0470_GRAY_DIRECT_WAVEFORM;
    unsigned before = scans, before_pushes = pushes, before_ons = ons, before_offs = offs, before_gc = gc_scans;
    prepare(hl, effect == 1, clean, clean ? &E0470_FULL_WAVEFORM : ordinary,
        clean && effect == 1 ? MODE_DU : MODE_GL16);
    assert(update_display_night_white_repaint(hl, action, direct, gray) == EPD_DRAW_SUCCESS);
    assert(scans == before + 1 && pushes == before_pushes + (clean ? CLEAR_PUSHES : 0));
    assert(ons == before_ons + 1 && offs == before_offs + 1 && !powered);
    assert(gc_scans == before_gc);
    assert(!memcmp(hl->front_fb, expected_front, BYTES) && !memcmp(hl->back_fb, expected_front, BYTES));
    assert(hl->waveform == &E0470_WAVEFORM);
}
static void recover(EpdiyHighlevelState* hl, unsigned effect) {
    unsigned before = scans, before_pushes = pushes, before_gc = gc_scans;
    prepare(hl, effect == 1, true, &E0470_FULL_WAVEFORM, MODE_GC16);
    assert(update_display_night_white_repaint(hl, DISPLAY_CROSSMUX_TURN, effect != 0, effect == 2) == EPD_DRAW_SUCCESS);
    assert(scans == before + 1 && pushes == before_pushes + CLEAR_PUSHES && gc_scans == before_gc + 1);
    assert(!memcmp(hl->front_fb, expected_front, BYTES) && !memcmp(hl->back_fb, expected_front, BYTES));
}

int main(void) {
    uint8_t front[BYTES], back[BYTES], difference[PIXELS], columns[WIDTH / 2]; bool lines[HEIGHT];
    EpdiyHighlevelState hl = {.front_fb = front, .back_fb = back, .difference_fb = difference,
        .dirty_lines = lines, .dirty_columns = columns, .waveform = &E0470_WAVEFORM};
    current = &hl; e0470_waveform_init(); targets(&hl, 0);
    for (unsigned effect = 0; effect < 3; ++effect) {
        clean_start(&hl); targets(&hl, effect);
        scan(&hl, DISPLAY_CROSSMUX_CLEAN, effect, true);
        targets(&hl, effect + 1);
        scan(&hl, DISPLAY_CROSSMUX_ENTRY, effect, true);
        targets(&hl, effect + 2); display_request_navigation_settle();
        scan(&hl, DISPLAY_CROSSMUX_REDRAW, effect, true);
        scan(&hl, DISPLAY_CROSSMUX_REDRAW, effect, false);
    }
    const unsigned periods[] = {1, 3, 5, 10, 30};
    for (unsigned c = 0; c < sizeof(periods)/sizeof(periods[0]); ++c) {
        every = periods[c]; clean_start(&hl);
        for (unsigned turn = 1; turn <= every * 2 + 2; ++turn) {
            targets(&hl, turn);
            scan(&hl, DISPLAY_CROSSMUX_TURN, turn % 3, turn % every == 0);
        }
    }
    every = 3; clean_start(&hl);
    scan(&hl, DISPLAY_CROSSMUX_TURN, 0, false);
    // 灰缘局部控件只提交实际区域，不能消耗正文周期或把其它灰目标偷偷写回参考。
    // Gray-edge controls commit only their actual area, without consuming body intervals or silently advancing other gray targets.
    targets(&hl, 4);
    prepare(&hl, false, false, &E0470_GRAY_DIRECT_WAVEFORM, MODE_GL16);
    expect_area = true;
    unsigned before_area = scans, before_area_pushes = pushes;
    assert(update_display_area_with(&hl, &E0470_GRAY_DIRECT_WAVEFORM, MODE_GL16,
        (EpdRect){0, 0, 32, 1}) == EPD_DRAW_SUCCESS);
    assert(scans == before_area + 1 && pushes == before_area_pushes);
    assert(!memcmp(hl.front_fb, expected_front, BYTES));
    assert(!memcmp(hl.back_fb, expected_front, 16));
    assert(!memcmp(hl.back_fb + 16, expected_prior + 16, BYTES - 16));
    // 同布局重绘、日间正文不推进夜间周期，也不会顺带重新擦白。
    // Same-layout redraws and daytime body updates never advance night cycles or incidentally clear again.
    for (int c = 0; c < 32; ++c) {
        targets(&hl, c); scan(&hl, DISPLAY_CROSSMUX_REDRAW, c % 3, false);
        prepare(&hl, false, false, &E0470_GRAY_DIRECT_WAVEFORM, MODE_GL16);
        assert(update_display_text_gray_direct(&hl, false) == EPD_DRAW_SUCCESS);
    }
    scan(&hl, DISPLAY_CROSSMUX_TURN, 1, false);
    scan(&hl, DISPLAY_CROSSMUX_TURN, 2, true);

    night = false; every = 1;
    // 新API误从日间调用仍保持原一次导航GC或普通正文出口，不触发物理清白。
    // Accidental day calls retain one original navigation GC or ordinary body update, without physical clearing.
    for (unsigned effect = 0; effect < 3; ++effect) for (unsigned action = 0; action < 4; ++action) {
        targets(&hl, effect + action); clean_start(&hl); targets(&hl, effect + action + 1);
        unsigned before = scans, before_pushes = pushes;
        const bool gc = action == DISPLAY_CROSSMUX_ENTRY || action == DISPLAY_CROSSMUX_CLEAN;
        prepare(&hl, effect == 1, false, gc ? &E0470_FULL_WAVEFORM :
            effect == 0 ? &E0470_TEXTTURN_WAVEFORM : effect == 1 ? &E0470_DIRECT_WAVEFORM : &E0470_GRAY_DIRECT_WAVEFORM,
            gc ? MODE_GC16 : MODE_GL16);
        assert(update_display_night_white_repaint(&hl, (display_crossmux_action_t)action, effect != 0, effect == 2) == EPD_DRAW_SUCCESS);
        assert(scans == before + 1 && pushes == before_pushes);
        assert(!memcmp(hl.front_fb, expected_front, BYTES) && !memcmp(hl.back_fb, expected_front, BYTES));
    }
    night = true; every = 0; clean_start(&hl);
    for (unsigned c = 0; c < 64; ++c) scan(&hl, DISPLAY_CROSSMUX_TURN, c % 3, false);
    every = 3;
    scan(&hl, DISPLAY_CROSSMUX_TURN, 0, false);
    scan(&hl, DISPLAY_CROSSMUX_TURN, 1, false);
    scan(&hl, DISPLAY_CROSSMUX_TURN, 2, true);

    const enum EpdDrawError errors[] = {EPD_DRAW_FAILED_ALLOC, EPD_DRAW_NO_PHASES_AVAILABLE, EPD_DRAW_EMPTY_LINE_QUEUE};
    for (unsigned e = 0; e < sizeof(errors)/sizeof(errors[0]); ++e) for (unsigned effect = 0; effect < 3; ++effect) {
        for (unsigned clean = 0; clean < 2; ++clean) {
            clean_start(&hl); targets(&hl, e + effect); display_request_navigation_settle();
            if (!clean) {
                scan(&hl, DISPLAY_CROSSMUX_ENTRY, effect, true);
                targets(&hl, e + effect + 1);
            }
            prepare(&hl, effect == 1, clean, clean ? &E0470_FULL_WAVEFORM :
                effect == 0 ? &E0470_TEXTTURN_NIGHT_WAVEFORM : effect == 1 ? &E0470_DIRECT_WAVEFORM : &E0470_GRAY_DIRECT_WAVEFORM,
                clean && effect == 1 ? MODE_DU : MODE_GL16);
            unsigned before = scans, before_pushes = pushes;
            failure = errors[e]; fail_scan = scans + 1;
            assert(update_display_night_white_repaint(&hl, clean ? DISPLAY_CROSSMUX_CLEAN : DISPLAY_CROSSMUX_TURN,
                effect != 0, effect == 2) == failure);
            assert(scans == before + 1 && pushes == before_pushes + clean * CLEAR_PUSHES && !powered);
            assert(!memcmp(hl.front_fb, expected_front, BYTES) && !memcmp(hl.back_fb, expected_prior, BYTES));
            fail_scan = 0;
            recover(&hl, effect);
            scan(&hl, DISPLAY_CROSSMUX_TURN, effect, false);
            scan(&hl, DISPLAY_CROSSMUX_TURN, effect, false);
            scan(&hl, DISPLAY_CROSSMUX_TURN, effect, true);
        }
    }
    for (unsigned effect = 0; effect < 3; ++effect) {
        clean_start(&hl); targets(&hl, effect);
        prepare(&hl, effect == 1, true, &E0470_FULL_WAVEFORM, effect == 1 ? MODE_DU : MODE_GL16);
        unsigned before = scans, before_pushes = pushes;
        fail_power = true;
        assert(update_display_night_white_repaint(&hl, DISPLAY_CROSSMUX_CLEAN, effect != 0, effect == 2) == EPD_DRAW_POWER_NOT_READY);
        assert(scans == before && pushes == before_pushes && !powered);
        assert(!memcmp(hl.front_fb, expected_front, BYTES) && !memcmp(hl.back_fb, clear_prior, BYTES));
        fail_power = false; recover(&hl, effect);
    }
    for (unsigned effect = 0; effect < 3; ++effect) {
        clean_start(&hl); targets(&hl, effect + 1);
        prepare(&hl, effect == 1, true, &E0470_FULL_WAVEFORM, effect == 1 ? MODE_DU : MODE_GL16);
        failure = EPD_DRAW_EMPTY_LINE_QUEUE; fail_scan = scans + 1;
        enum EpdDrawError result = update_display_night_white_repaint(&hl, DISPLAY_CROSSMUX_CLEAN,
            effect != 0, effect == 2);
        assert(result == failure);
        fail_scan = 0;
        prepare(&hl, false, true, &E0470_FULL_WAVEFORM, MODE_GC16);
        unsigned before = scans, before_pushes = pushes;
        // 实际欠载守卫当轮恢复也必须保留灰/二值目标，并执行原完整清白与GC。
        // Immediate actual underrun-guard recovery retains gray/binary targets and performs original full clear plus GC.
        guard_draw_result(&hl, result);
        assert(scans == before + 1 && pushes == before_pushes + CLEAR_PUSHES);
        assert(!memcmp(hl.front_fb, expected_front, BYTES) && !memcmp(hl.back_fb, expected_front, BYTES));
        scan(&hl, DISPLAY_CROSSMUX_TURN, effect, false);
        scan(&hl, DISPLAY_CROSSMUX_TURN, effect, false);
        scan(&hl, DISPLAY_CROSSMUX_TURN, effect, true);
    }
    // 新灰缘当前档保持真实灰目标与原周期GC，日间不计周期。
    // Current-profile gray edges retain real gray targets and original periodic GC, with daytime excluded.
    every = 3; clean_start(&hl);
    for (unsigned turn = 1; turn <= 6; ++turn) {
        targets(&hl, turn);
        bool clean = turn % every == 0;
        prepare(&hl, false, clean, clean ? &E0470_FULL_WAVEFORM : &E0470_GRAY_DIRECT_WAVEFORM,
            clean ? MODE_GC16 : MODE_GL16);
        assert(update_display_text_gray_direct(&hl, true) == EPD_DRAW_SUCCESS);
        assert(!memcmp(hl.front_fb, expected_front, BYTES) && !memcmp(hl.back_fb, expected_front, BYTES));
    }
    puts("night white repaint: actual 3x10B13W3H clear, retained target/true-white commits, one GL or DU repaint with white hold and B18 black; period1/3/5/10/30/off, day/REDRAW/local-control exclusion, all256 prior-gray selectors, gray-direct cycles, failed-scan/power and original unknown/immediate-underrun GC recovery PASS");
}
