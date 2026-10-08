/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 *
 * 导出真实运行时波形的全部字节，供独立SDK指纹和原档位基线比较；不模拟面板。
 * Export every actual runtime waveform byte for independent SDK fingerprints and original-profile baseline comparison; no panel simulation.
 * 冻结：导出真实waveform.c的初始化产物，不能用测试替身或派生期望表替换。
 * Frozen: Export actual waveform.c initialization products, without substituting test shims or derived expected tables.
 */
#include <assert.h>
#include <stdio.h>

#include "e0470_epaper_waveform.h"

typedef struct {
    const char* name;
    const EpdWaveform* waveform;
} wave_case_t;

static void dump_waveform(const wave_case_t* test, int run) {
    const EpdWaveform* waveform = test->waveform;
    assert(waveform && waveform->num_modes && waveform->num_temp_ranges == 1);
    for (int i = 0; i < waveform->num_modes; ++i) {
        const EpdWaveformMode* mode = waveform->mode_data[i];
        assert(mode && mode->temp_ranges == 1);
        const EpdWaveformPhases* phases = mode->range_data[0];
        assert(phases && phases->luts && phases->phases > 0);
        printf("{\"name\":\"%s\",\"run\":%d,\"mode\":%u,\"frames\":%d,\"min\":%d,\"max\":%d,\"phase_times_null\":%s,\"data\":\"",
               test->name, run, mode->type, phases->phases,
               waveform->temp_intervals[0].min, waveform->temp_intervals[0].max,
               phases->phase_times == NULL ? "true" : "false");
        for (int n = 0; n < phases->phases * 64; ++n) printf("%02x", phases->luts[n]);
        puts("\"}");
    }
}

int main(void) {
    static const wave_case_t tests[] = {
#ifndef CROSSMUX_BASELINE_ONLY
        {"crossmux", &E0470_CROSSMUX_WAVEFORM},
#endif
        {"local-default", &E0470_WAVEFORM},
        {"local-full", &E0470_FULL_WAVEFORM},
        {"local-day", &E0470_TEXTTURN_WAVEFORM},
        {"local-night", &E0470_TEXTTURN_NIGHT_WAVEFORM},
        {"local-direct", &E0470_DIRECT_WAVEFORM},
        {"local-navigation", &E0470_NAVIGATION_WAVEFORM},
        {"local-entry-diagnostic", &E0470_NAVIGATION_ENTRY_WAVEFORM},
        {"local-white-diagnostic", &E0470_WHITE_CLEANUP_WAVEFORM},
        {"local-gray8", &E0470_GRAY8_WAVEFORM},
        {"local-follow", &E0470_FOLLOW_WAVEFORM},
    };
    for (int run = 0; run < 2; ++run) {
        e0470_waveform_init();
        for (unsigned i = 0; i < sizeof(tests) / sizeof(tests[0]); ++i) dump_waveform(&tests[i], run);
    }
    return 0;
}
