/* SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 * 中文：独立适配本地49相；借鉴if-ink/kiikoread的灰阶反向脉冲交错思路。
 * English: Independently adapt local49-phase tables using the if-ink/kiikoread gray-pulse interleaving idea.
 * 参考：yiifanrecyclable/if-ink-OS@efb6cb50/main/display_profiles.c（Apache-2.0）。
 * Reference: yiifanrecyclable/if-ink-OS@efb6cb50/main/display_profiles.c (Apache-2.0).
 * 冻结：不移植去擦除、对角线保持或37相表；只在显式选择且前后纯文字时使用。
 * Frozen: Never port erase removal, held diagonals or37-phase tables; explicit text-to-text selection only.
 */
#include "display_animation.h"
#include "e0470_epaper_waveform.h"
#include "esp_heap_caps.h"
#include <stdlib.h>
#include <string.h>

static unsigned action(const uint8_t* data, unsigned phase, unsigned to, unsigned from) {
    return (data[phase * 64 + to * 4 + from / 4] >> (6 - 2 * (from % 4))) & 3u;
}
static void replace(uint8_t* data, unsigned phase, unsigned to, unsigned value) {
    size_t at = phase * 64 + to * 4 + 3;
    data[at] = (uint8_t)((data[at] & ~3u) | value);
}
bool display_animation_build(uint8_t* dst, size_t capacity, const uint8_t* source, unsigned phases) {
    if (!dst || !source || !phases || phases > 64 || capacity < phases * 64u) return false;
    uintptr_t a = (uintptr_t)dst, b = (uintptr_t)source;
    if ((a >= b ? a - b : b - a) < phases * 64u) return false;
    // 厂家表仅允许保持/黑/白；未知动作不能参与重排。/ Vendor entries must be hold/black/white; unknown actions never enter a derived table.
    for (unsigned phase = 0; phase < phases; ++phase)
        for (unsigned to = 0; to < 16; ++to)
            for (unsigned from = 0; from < 16; ++from)
                if (action(source, phase, to, from) == 3) return false;
    memcpy(dst, source, phases * 64u);
    for (unsigned to = 1; to < 15; ++to) {
        unsigned first = phases, last = 0, first_white = phases, black = 0;
        for (unsigned phase = 0; phase < phases; ++phase) {
            unsigned a = action(source, phase, to, 15);
            if (a) { if (first == phases) first = phase; last = phase; }
            if (a == 2 && first_white == phases) first_white = phase;
            if (a == 1 && first_white == phases) ++black;
        }
        if (first == phases || action(source, first, to, 15) != 1 || black < 5 || first_white >= last) continue;
        unsigned pairs = 0, white = first_white;
        // 最多两对且隔一相取样；首末主动相和所有中性槽原位不变。
        // Swap at most two spaced pairs; retain first/final active phases and every neutral slot in place.
        for (unsigned phase = first_white; phase > first + 1 && pairs < 2;) {
            --phase;
            if (action(source, phase, to, 15) != 1) continue;
            while (white < last && action(source, white, to, 15) != 2) ++white;
            if (white >= last) break;
            replace(dst, phase, to, 2); replace(dst, white++, to, 1); ++pairs;
            if (phase > first + 1) --phase;
        }
    }
    return true;
}

static uint8_t* s_data;
static EpdWaveformPhases s_phases;
static const EpdWaveformPhases* s_ranges[] = {&s_phases};
static const EpdWaveformMode s_mode = {.type=MODE_GL16, .temp_ranges=1, .range_data=s_ranges};
static const EpdWaveformMode* s_modes[] = {&s_mode};
static EpdWaveform s_waveform = {.num_modes=1, .num_temp_ranges=1, .mode_data=s_modes};
const EpdWaveform* display_animation_waveform(void) {
    if (s_data) return &s_waveform;
    const EpdWaveformPhases* base = e0470_waveform_phases(&E0470_TEXTTURN_WAVEFORM, MODE_GL16);
    if (!base || !base->luts || base->phases != E0470_GL16_FRAMES || base->phase_times) return &E0470_TEXTTURN_WAVEFORM;
    size_t bytes = (size_t)base->phases * 64;
    s_data = heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!s_data) return &E0470_TEXTTURN_WAVEFORM;
    if (!display_animation_build(s_data, bytes, base->luts, (unsigned)base->phases)) {
        display_animation_release(); return &E0470_TEXTTURN_WAVEFORM;
    }
    s_phases = *base; s_phases.luts = s_data;
    s_waveform.temp_intervals = E0470_TEXTTURN_WAVEFORM.temp_intervals;
    return &s_waveform;
}
void display_animation_release(void) { free(s_data); s_data = NULL; s_phases.luts = NULL; }
