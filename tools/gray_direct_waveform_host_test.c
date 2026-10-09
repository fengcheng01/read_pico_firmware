/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 *
 * 核对真实灰边直刷装配的每一厂家动作、全部未变保持和重复初始化；不模拟光学灰阶。
 * Check every vendor action, every unchanged hold and repeated initialization in actual gray-edge direct assembly; no optical gray simulation.
 * 冻结：期望直接读取未改厂家源；软件通过不能代表残影、闪动、字缘或真机耗时改善。
 * Frozen: Read expectations directly from unchanged vendor sources; software success cannot establish ghost, flicker, edge or device-latency improvements.
 */

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "e0470_epaper_waveform.h"
#include "du.h"
#include "gray8_gl16.h"

// 独立按源packed字节解码，不复用被测装配器的lut_get/lut_set。
// Decode source packed bytes independently without reusing the tested assembler's lut_get/lut_set.
static int source_action(const uint8_t* source, int phase, int to, int from) {
    return (source[phase * 64 + to * 4 + from / 4] >> (6 - 2 * (from % 4))) & 3;
}

int main(void) {
    uint8_t saved[E0470_GRAY_DIRECT_FRAMES * 64];
    unsigned checked = 0;
    for (int run = 0; run < 3; ++run) {
        e0470_waveform_init();
        const EpdWaveform* waveform = &E0470_GRAY_DIRECT_WAVEFORM;
        assert(waveform->num_modes == 1 && waveform->num_temp_ranges == 1);
        assert(waveform->temp_intervals[0].min == 0 && waveform->temp_intervals[0].max == 50);
        assert(waveform->mode_data[0]->type == 5 && waveform->mode_data[0]->temp_ranges == 1);
        assert(e0470_waveform_phases(waveform, 1) == NULL);
        assert(e0470_waveform_phases(waveform, 2) == NULL);
        const EpdWaveformPhases* phases = e0470_waveform_phases(waveform, 5);
        assert(phases && phases->luts && phases->phases == 31 && phases->phase_times == NULL);
        for (int to = 0; to < 16; ++to) for (int from = 0; from < 16; ++from) {
            const bool endpoint = to == 0 || to == 15;
            const uint8_t* source = endpoint ? (const uint8_t*)e0470_full_du_data
                                             : (const uint8_t*)e0470_gray8_gl16_data;
            const int source_frames = endpoint ? 20 : 30;
            for (int phase = 0; phase < 31; ++phase) {
                const int expected = to == from || phase >= source_frames ? 0
                                     : source_action(source, phase, to, from);
                const int actual = e0470_phase_action(phases, phase, to, from);
                assert(actual == expected);
                if (to == from || phase >= 28) assert(actual == 0);
                if (to == 0) assert(actual != 2);
                if (to == 15) assert(actual != 1);
                ++checked;
            }
        }
        // 4/10确有灰尾且保留完整反向迁移；旧直刷中间目标全保持，不能通过这组检查。
        // Targets 4/10 contain gray tails and complete reverse transitions; original direct holds intermediate targets and cannot pass these checks.
        assert(e0470_phase_action(phases, 6, 4, 0) == 2);
        assert(e0470_phase_action(phases, 16, 4, 0) == 1);
        assert(e0470_phase_action(phases, 26, 4, 0) == 2);
        assert(e0470_phase_action(phases, 27, 4, 0) == 2);
        assert(e0470_phase_action(phases, 14, 10, 15) == 1);
        assert(e0470_phase_action(phases, 24, 10, 15) == 2);
        assert(e0470_phase_action(phases, 27, 10, 15) == 2);
        const EpdWaveformPhases* original = e0470_waveform_phases(&E0470_DIRECT_WAVEFORM, 5);
        assert(e0470_phase_action(original, 6, 4, 0) == 0);
        assert(e0470_phase_action(phases, 6, 4, 0) != e0470_phase_action(original, 6, 4, 0));
        if (run == 0) memcpy(saved, phases->luts, sizeof(saved));
        else assert(memcmp(saved, phases->luts, sizeof(saved)) == 0);
    }
    printf("Gray-edge DU20 + complete 8-gray GL30: %u exact source actions, every diagonal/EE hold, neutral tails, real gray targets and repeated initialization PASS\n", checked);
    return 0;
}
