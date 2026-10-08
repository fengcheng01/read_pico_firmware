/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 *
 * 真实夜间实验波形的作用域与厂家相序回归；不模拟面板光学效果。
 * Scope and vendor phase-order regression for actual night experimental waveforms; panel optics are not simulated.
 * 冻结：256选择码仅00允许推动，EE始终保持；原厂家表和原刷新档位不变。
 * Frozen: Only 00 may drive across all 256 selectors, EE always holds, and original vendor tables and refresh profiles remain unchanged.
 */
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "e0470_epaper_waveform.h"
#include "du.h"
#include "gc16.h"
#include "gl16.h"

enum { TEST_DU = 1, TEST_GC16 = 2, TEST_GL16 = 5 };

static int source_action(const uint8_t (*data)[16][4], int phase, int to, int from) {
    return (data[phase][to][from / 4] >> (6 - 2 * (from % 4))) & 3;
}

static void check_scope(const EpdWaveform* waveform, int frames, const int* expected) {
    const EpdWaveformPhases* phases = e0470_waveform_phases(waveform, TEST_GL16);
    assert(phases && phases->phases == frames && phases->phase_times == NULL);
    assert(waveform->num_modes == 1 && waveform->num_temp_ranges == 1);
    assert(waveform->mode_data[0]->type == TEST_GL16);
    assert(waveform->mode_data[0]->temp_ranges == 1);
    assert(waveform->temp_intervals[0].min == 0 && waveform->temp_intervals[0].max == 50);
    assert(e0470_waveform_phases(waveform, TEST_DU) == NULL);
    assert(e0470_waveform_phases(waveform, TEST_GC16) == NULL);
    for (int phase = 0; phase < frames; ++phase) {
        for (int code = 0; code < 256; ++code) {
            const int action = e0470_phase_action(phases, phase, code >> 4, code & 15);
            assert(action == (code == 0 ? expected[phase] : 0));
        }
        assert(e0470_phase_action(phases, phase, 14, 14) == 0);
    }
}

static void check_original_profiles(void) {
    const EpdWaveformPhases* du = e0470_waveform_phases(&E0470_FULL_WAVEFORM, TEST_DU);
    const EpdWaveformPhases* gc = e0470_waveform_phases(&E0470_FULL_WAVEFORM, TEST_GC16);
    const EpdWaveformPhases* gl = e0470_waveform_phases(&E0470_FULL_WAVEFORM, TEST_GL16);
    const EpdWaveformPhases* direct = e0470_waveform_phases(&E0470_DIRECT_WAVEFORM, TEST_GL16);
    const EpdWaveformPhases* day = e0470_waveform_phases(&E0470_TEXTTURN_WAVEFORM, TEST_GL16);
    const EpdWaveformPhases* night = e0470_waveform_phases(&E0470_TEXTTURN_NIGHT_WAVEFORM, TEST_GL16);
    assert(du && du->phases == E0470_FULL_DU_FRAMES);
    assert(gc && gc->phases == E0470_FULL_GC16_FRAMES);
    assert(gl && gl->phases == E0470_FULL_GL16_FRAMES);
    assert(direct && direct->phases == E0470_DIRECT_FRAMES);
    assert(day && night && day->phases == E0470_PAGE_GL16_FRAMES && night->phases == day->phases);
    assert(!memcmp(du->luts, e0470_full_du_data, sizeof(e0470_full_du_data)));
    assert(!memcmp(gc->luts, e0470_full_gc16_data, sizeof(e0470_full_gc16_data)));
    assert(!memcmp(gl->luts, e0470_full_gl16_data, sizeof(e0470_full_gl16_data)));
    for (int phase = 0; phase < E0470_PAGE_GL16_FRAMES; ++phase) {
        for (int to = 0; to < 16; ++to) for (int from = 0; from < 16; ++from) {
            const int vendor = phase < E0470_FULL_GL16_FRAMES ? source_action(e0470_full_gl16_data, phase, to, from) : 0;
            assert(e0470_phase_action(day, phase, to, from) == vendor);
            assert(e0470_phase_action(night, phase, to, from) == (to == from ? 0 : vendor));
        }
    }
    for (int phase = 0; phase < E0470_DIRECT_FRAMES; ++phase) {
        for (int to = 0; to < 16; ++to) for (int from = 0; from < 16; ++from) {
            const int vendor = phase < E0470_FULL_DU_FRAMES ? source_action(e0470_full_du_data, phase, to, from) : 0;
            const int expected = (to == 0 || to == 15) && to != from ? vendor : 0;
            assert(e0470_phase_action(direct, phase, to, from) == expected);
        }
    }
}

int main(void) {
    int boost[E0470_NIGHT_BLACK_BOOST_FRAMES] = {0};
    int local[E0470_NIGHT_LOCAL_CLEAN_FRAMES] = {0};
    int phase = E0470_NIGHT_BLACK_BOOST_ACTIVE_FRAMES - 1;
    for (int f = E0470_FULL_DU_FRAMES - 1; f >= 0 && phase >= 0; --f) {
        const int action = source_action(e0470_full_du_data, f, 0, 15);
        if (action == 1) boost[phase--] = action;
    }
    assert(phase == -1 && boost[0] == 1 && boost[1] == 1);
    int black = 0, white = 0;
    for (int f = 0; f < E0470_NIGHT_LOCAL_CLEAN_FRAMES; ++f) {
        local[f] = source_action(e0470_full_gc16_data, f, 0, 0);
        black += local[f] == 1; white += local[f] == 2;
    }
    assert(black > 0 && white > 0);
    e0470_waveform_init();
    check_scope(&E0470_NIGHT_BLACK_BOOST_WAVEFORM, E0470_NIGHT_BLACK_BOOST_FRAMES, boost);
    check_scope(&E0470_NIGHT_LOCAL_CLEAN_WAVEFORM, E0470_NIGHT_LOCAL_CLEAN_FRAMES, local);
    check_original_profiles();
    // 重复初始化仍逐相一致，没有把局部动作累加进原表。/ Reinitialization remains phase-identical without accumulating local actions into original tables.
    e0470_waveform_init();
    check_scope(&E0470_NIGHT_BLACK_BOOST_WAVEFORM, E0470_NIGHT_BLACK_BOOST_FRAMES, boost);
    check_scope(&E0470_NIGHT_LOCAL_CLEAN_WAVEFORM, E0470_NIGHT_LOCAL_CLEAN_FRAMES, local);
    check_original_profiles();
    puts("night cleanup waveform: 256 selectors, vendor actions/order, neutral tails, held EE and unchanged profiles PASS (no optical claim)");
    return 0;
}
