/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 * 中文：核对厂家DU、日间完整GL定稿与夜间未变保持；正文白白保持，诊断入口单相差异不参与产品刷新。
 * English: Compare vendor DU, complete day GL settling and night unchanged holds; body white holds and the diagnostic single-phase entry difference stays outside product updates.
 */
#include "e0470_epaper_waveform.h"
#include "gc16.h"
#include "gl16.h"
#include "du.h"
#include "gray8_gl16.h"
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

int main(void) {
    uint8_t saved_direct[E0470_DIRECT_FRAMES * 16 * 4];
    uint8_t saved_entry[E0470_GL16_FRAMES * 16 * 4];
    uint8_t saved_text[E0470_PAGE_GL16_FRAMES * 16 * 4];
    uint8_t saved_night[E0470_PAGE_GL16_FRAMES * 16 * 4];
    for (int run = 0; run < 2; run++) {
        e0470_waveform_init();
        const EpdWaveform* profiles[] = {
            &E0470_WAVEFORM, &E0470_FULL_WAVEFORM,
            &E0470_NAVIGATION_WAVEFORM, &E0470_TEXTTURN_WAVEFORM,
            &E0470_TEXTTURN_NIGHT_WAVEFORM,
        };
        for (unsigned i = 0; i < sizeof(profiles) / sizeof(profiles[0]); ++i) {
            const EpdWaveformPhases* gl = e0470_waveform_phases(profiles[i], 5);
            assert(gl && gl->phases == (profiles[i] == &E0470_FULL_WAVEFORM ? 48 : 49) && gl->phase_times == NULL);
            unsigned black_diagonal = 0, gray_diagonal = 0;
            for (int to = 0; to < 16; ++to) for (int from = 0; from < 16; ++from)
                for (int f = 0; f < gl->phases; ++f) {
                    int expected = f < 48 ? (e0470_full_gl16_data[f][to][from / 4] >> (6 - 2 * (from % 4))) & 3 : 0;
                    if (profiles[i] == &E0470_TEXTTURN_NIGHT_WAVEFORM && to == from) expected = 0;
                    int actual = e0470_phase_action(gl, f, to, from);
                    assert(actual == expected);
                    if (profiles[i] != &E0470_FULL_WAVEFORM && f >= gl->phases - 3)
                        assert(expected == 0);
                    if (to == 15 && from == 15) assert(expected == 0);
                    if (to == from && to == 0) black_diagonal += expected != 0;
                    if (to == from && to > 0 && to < 15) gray_diagonal += expected != 0;
                }
            if (profiles[i] == &E0470_TEXTTURN_NIGHT_WAVEFORM) assert(black_diagonal == 0 && gray_diagonal == 0);
            else assert(black_diagonal > 0 && gray_diagonal > 0);
        }
        // 黑芯逻辑未变也走厂家擦除/重写；14→14同样有效，禁止把EE当中性选择码。
        // Logically unchanged black cores retain vendor erase/rewrite; 14→14 is also active, so EE must never be a neutral selector here.
        const EpdWaveformPhases* text = e0470_waveform_phases(&E0470_TEXTTURN_WAVEFORM, 5);
        unsigned text_black = 0, text_white = 0, text_gray = 0;
        for (int f = 0; f < text->phases; ++f) {
            int black_action = e0470_phase_action(text, f, 0, 0);
            text_black += black_action == 1;
            text_white += black_action == 2;
            text_gray += e0470_phase_action(text, f, 14, 14) != 0;
            assert(black_action == (f >= 10 && f < 28 ? 2 : f >= 28 && f < 46 ? 1 : 0));
            assert(e0470_phase_action(text, f, 15, 15) == 0);
        }
        assert(text_black == 18 && text_white == 18 && text_gray > 0);
        if (run == 0) memcpy(saved_text, text->luts, sizeof(saved_text));
        else assert(!memcmp(saved_text, text->luts, sizeof(saved_text)));
        // 夜间黑底和未变白芯都保持；不得误用日间0→0定稿，EE仍是安全保持码。
        // Night black backgrounds and unchanged white cores hold; never apply day 0→0 settling, and EE remains a safe hold code.
        const EpdWaveformPhases* night = e0470_waveform_phases(&E0470_TEXTTURN_NIGHT_WAVEFORM, 5);
        assert(text->luts == e0470_waveform_phases(&E0470_WAVEFORM, 5)->luts);
        for (int f = 0; f < night->phases; ++f) for (int g = 0; g < 16; ++g)
            assert(e0470_phase_action(night, f, g, g) == 0);
        if (run == 0) memcpy(saved_night, night->luts, sizeof(saved_night));
        else assert(!memcmp(saved_night, night->luts, sizeof(saved_night)));
        const EpdWaveformPhases* direct = e0470_waveform_phases(&E0470_DIRECT_WAVEFORM, 5);
        assert(direct && direct->phases == 21 && direct->phase_times == NULL);
        for (int to = 0; to < 16; ++to) for (int from = 0; from < 16; ++from)
            for (int f = 0; f < direct->phases; ++f) {
                int actual = e0470_phase_action(direct, f, to, from);
                int expected;
                if (to == 0 || to == 15)
                    expected = f < E0470_FULL_DU_FRAMES ? e0470_phase_action(&e0470_full_du_phases, f, to, from) : 0;
                else expected = 0;
                if (to == from) expected = 0;
                assert(actual == expected);
                if (f >= 18) assert(actual == 0);
                if (to == 0) assert(actual != 2);
                if (to == 15) assert(actual != 1);
                if (to == from) assert(actual == 0);
            }
        // 不把旧灰当成白或重排厂家相位：浅灰擦白延后、近黑收尾推动仍逐相保留。
        // Never pretend old grays are white or reorder vendor phases: delayed light-gray erasure and near-black settling pushes remain exact.
        assert(e0470_phase_action(direct, 0, 15, 14) == 0);
        assert(e0470_phase_action(direct, 11, 15, 14) == 2);
        assert(e0470_phase_action(direct, 0, 0, 2) == 1);
        assert(e0470_phase_action(direct, 2, 0, 2) == 0);
        assert(e0470_phase_action(direct, 17, 0, 2) == 1);
        for (int f = 0; f < direct->phases; ++f) {
            assert(e0470_phase_action(direct, f, 14, 14) == 0);
            assert(e0470_phase_action(direct, f, 15, 15) == 0);
            assert(e0470_phase_action(direct, f, 0, 0) == 0);
        }
        if (run == 0) memcpy(saved_direct, direct->luts, sizeof(saved_direct));
        else assert(!memcmp(saved_direct, direct->luts, sizeof(saved_direct)));
        // 旧字消失后多页保持白底，不能再按旧字形状输出白推动。
        // Once an old glyph disappears, subsequent white pages must not emit more white drive in its shape.
        const EpdWaveform* turns[] = { &E0470_TEXTTURN_WAVEFORM, &E0470_TEXTTURN_NIGHT_WAVEFORM, &E0470_DIRECT_WAVEFORM, &E0470_NAVIGATION_WAVEFORM };
        for (unsigned p = 0; p < sizeof(turns) / sizeof(turns[0]); ++p) {
            const EpdWaveformPhases* phases = e0470_waveform_phases(turns[p], 5);
            int after_erase_white = 0, after_erase_dark = 0;
            for (int turn = 0; turn < 100; ++turn) for (int f = 0; f < phases->phases; ++f) {
                int action = e0470_phase_action(phases, f, 15, 15);
                after_erase_white += action == 2; after_erase_dark += action == 1;
                if (turns[p] == &E0470_DIRECT_WAVEFORM || turns[p] == &E0470_TEXTTURN_NIGHT_WAVEFORM)
                    assert(e0470_phase_action(phases, f, 14, 14) == 0);
            }
            assert(after_erase_white == 0 && after_erase_dark == 0);
        }
        // 导航必须与完整厂家GL16一致，白底的所有相都保持，不能周期压黑。
        // Navigation must match full vendor GL16 with held white in every phase and no periodic black drive.
        const EpdWaveformPhases* nav = e0470_waveform_phases(&E0470_NAVIGATION_WAVEFORM, 5);
        for (int f = 0; f < nav->phases; ++f) assert(e0470_phase_action(nav,f,15,15)==0);
        // 仅诊断对照：边界表与NAV相差最后白相一个真实FF动作，未增加扫描或黑推动；产品不调用。
        // Diagnostic only: the entry table differs from NAV at one actual FF action in the last white phase without extra scans or black drive; product does not use it.
        const EpdWaveformPhases* entry = e0470_waveform_phases(&E0470_NAVIGATION_ENTRY_WAVEFORM, 5);
        assert(entry && entry->phases == nav->phases && entry->phase_times == NULL);
        int last_white_phase = -1;
        for (int f = 0; f < E0470_FULL_GL16_FRAMES; ++f) for (int from = 0; from < 15; ++from)
            if (e0470_phase_action(&e0470_full_gl16_phases, f, 15, from) == 2) last_white_phase = f;
        assert(last_white_phase == 45);
        unsigned differences = 0, entry_white_ticks = 0;
        for (int to = 0; to < 16; ++to) for (int from = 0; from < 16; ++from)
            for (int f = 0; f < entry->phases; ++f) {
                int baseline = e0470_phase_action(nav, f, to, from);
                int action = e0470_phase_action(entry, f, to, from);
                bool tick = to == 15 && from == 15 && f == last_white_phase;
                assert(action == (tick ? 2 : baseline));
                differences += action != baseline;
                if (to == 15 && from == 15) {
                    assert(action != 1 && action != 3);
                    entry_white_ticks += action == 2;
                }
                if (f >= entry->phases - 3) assert(action == 0);
            }
        assert(differences == 1 && entry_white_ticks == 1);
        if (run == 0) memcpy(saved_entry, entry->luts, sizeof(saved_entry));
        else assert(!memcmp(saved_entry, entry->luts, sizeof(saved_entry)));
        assert(e0470_waveform_phases(&E0470_NAVIGATION_ENTRY_WAVEFORM, 2) == NULL);
        const EpdWaveformPhases* erase = e0470_waveform_phases(&E0470_WHITE_CLEANUP_WAVEFORM, 1);
        assert(erase && erase->phases == 6);
        for (int to = 0; to < 16; ++to) for (int from = 0; from < 16; ++from)
            for (int f = 0; f < 6; ++f)
                assert(e0470_phase_action(erase, f, to, from) == (to == 15 && from == 15 && f < 3 ? 2 : 0));
        const EpdWaveform* gc_profiles[] = { &E0470_WAVEFORM, &E0470_FULL_WAVEFORM };
        for (unsigned i = 0; i < 2; ++i) {
            const EpdWaveformPhases* gc = e0470_waveform_phases(gc_profiles[i], 2);
            assert(gc && gc->phases == 48);
            assert(!memcmp(gc->luts, e0470_full_gc16_data, sizeof(e0470_full_gc16_data)));
        }
        const EpdWaveformPhases* raw_gray = e0470_waveform_phases(&E0470_GRAY8_WAVEFORM, 5);
        assert(raw_gray && raw_gray->phases == 30 && !memcmp(raw_gray->luts, e0470_gray8_gl16_data, sizeof(e0470_gray8_gl16_data)));
    }
    puts("waveforms: day all 256 vendor GL transitions/diagonals exact, night vendor changes and all diagonals held, white holds; direct21 DU endpoint phases unchanged; diagnostic single FF tick, three neutral tails, repeat init stable");
}
