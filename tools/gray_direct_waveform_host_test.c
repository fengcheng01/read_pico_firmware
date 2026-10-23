/*
 * SPDX-FileCopyrightText: 2026 mindreset
 *
 * 核对灰边直刷（DU差值剂量单向直驱+DU端点）与夜间白预驱（EE行14→14微洗+保持行）装配的每一动作、
 * 全部未变保持和重复初始化；不模拟光学灰阶。
 * Check every action, every unchanged hold and repeated initialization in the gray-edge direct
 * assembly (one-way drives dosed from DU mid-gray rows plus DU endpoints) and the night white
 * pre-drive (EE-row 14→14 wash with the 15→15 hold row); no optical gray simulation.
 * 冻结：期望逐字读取未改厂家DU/GL16/灰8源；软件通过不能代表残影、闪动、字缘或真机耗时改善。
 * Frozen: Expectations read the unchanged vendor DU/GL16/gray8 sources verbatim; software success
 * cannot establish ghost, flicker, edge or device-latency improvements.
 */

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#include "e0470_epaper_waveform.h"
#include "du.h"
#include "gl16.h"
#include "gray8_gl16.h"

// 独立按源packed字节解码，不复用被测装配器的lut_get/lut_set。
// Decode source packed bytes independently without reusing the tested assembler's lut_get/lut_set.
static int source_action(const uint8_t* source, int phase, int to, int from) {
    return (source[phase * 64 + to * 4 + from / 4] >> (6 - 2 * (from % 4))) & 3;
}

int main(void) {
    uint8_t saved[E0470_GRAY_DIRECT_FRAMES * 64];
    uint8_t balanced_saved[E0470_GRAY_BALANCED_FRAMES * 64];
    unsigned checked = 0;
    for (int run = 0; run < 3; ++run) {
        e0470_waveform_init();
        const EpdWaveformPhases* balanced = e0470_waveform_phases(&E0470_GRAY_BALANCED_WAVEFORM, 5);
        assert(balanced && balanced->phases == 28 && balanced->phase_times == NULL);
        if (run == 0) memcpy(balanced_saved, balanced->luts, sizeof(balanced_saved));
        else assert(!memcmp(balanced_saved, balanced->luts, sizeof(balanced_saved)));
        for (int to=0;to<16;++to) for(int from=0;from<16;++from) {
            const bool endpoint=to==0||to==15;
            const uint8_t* original=endpoint?(const uint8_t*)e0470_full_du_data:(const uint8_t*)e0470_gray8_gl16_data;
            const int total=endpoint?20:30;
            int first=total,last=-1;
            for(int p=0;p<total;++p) if(source_action(original,p,to,from)) {
                if(first==total) first=p;
                last=p;
            }
            int active[3]={0},expected[3]={0};
            for(int p=0;p<28;++p) {
                int actual=e0470_phase_action(balanced,p,to,from);
                assert(actual>=0&&actual<=2); ++active[actual];
                int src=p-(26-last);
                int value=to!=from&&last>=0&&src>=first&&src<=last?source_action(original,src,to,from):0;
                assert(actual==value);
                if(to==from||p==27) assert(actual==0);
                if(p==26&&to!=from&&last>=0) assert(actual!=0);
            }
            if(to!=from) for(int p=0;p<total;++p) ++expected[source_action(original,p,to,from)];
            assert(active[1]==expected[1]&&active[2]==expected[2]);
        }
        const EpdWaveform* waveform = &E0470_GRAY_DIRECT_WAVEFORM;
        assert(waveform->num_modes == 1 && waveform->num_temp_ranges == 1);
        assert(waveform->temp_intervals[0].min == 0 && waveform->temp_intervals[0].max == 50);
        assert(waveform->mode_data[0]->type == 5 && waveform->mode_data[0]->temp_ranges == 1);
        assert(e0470_waveform_phases(waveform, 1) == NULL);
        assert(e0470_waveform_phases(waveform, 2) == NULL);
        const EpdWaveformPhases* phases = e0470_waveform_phases(waveform, 5);
        assert(phases && phases->luts && phases->phases == 21 && phases->phase_times == NULL);
        // 独立重算剂量：白底降灰用厂家锚点陡降曲线（B10成形位/4脉冲浅区/18满黑），奇数档
        // 3/5/7/9/11主段短一档并在相14补迟到脉冲；旧灰降灰与升向取纯DU行差值，降灰行
        // 并列组最深档加迟到脉冲（加法拆对）；端点逐字复制厂家DU；对角线保持。
        // Recompute doses independently: white-background descent uses the vendor-anchored
        // plunge curve (B10 formation level, 4 pulses still light, 18 full black) with odd
        // targets 3/5/7/9/11 one dose shorter plus a phase-14 late pulse; old-gray descent and
        // climbing take pure DU row differences, with descent adding the late pulse on each
        // tie group's darkest member (additive split); endpoints copy vendor DU verbatim; the
        // diagonal holds.
        static const uint8_t white_descend[14] = {10, 9, 8, 8, 7, 7, 6, 6, 5, 5, 4, 4, 3, 3};
        static const bool white_late[14] = {false, false, true, false, true, false, true, false, true, false, true, false, false, false};
        int count_b[16] = {0}, count_w[16] = {0};
        for (int g = 0; g < 16; ++g)
            for (int phase = 0; phase < 20; ++phase) {
                count_b[g] += !!source_action((const uint8_t*)e0470_full_du_data, phase, 0, g);
                count_w[g] += !!source_action((const uint8_t*)e0470_full_du_data, phase, 15, g);
            }
        const int endpoint_shift = 0;
        for (int to = 0; to < 16; ++to) for (int from = 0; from < 16; ++from) {
            const bool endpoint = to == 0 || to == 15;
            int pulses = 1, action = to < from ? 1 : 2;
            bool late = false;
            if (to != from && endpoint) {
                pulses = -1;
            } else if (to != from && from == 15) {
                pulses = white_descend[to - 1];
                late = white_late[to - 1];
            } else if (to != from) {
                if (to < from) {
                    pulses = count_b[from] - count_b[to];
                    if (pulses < 1) pulses = 1;
                    late = count_b[to - 1] != count_b[to] && to + 1 <= 15 && count_b[to + 1] == count_b[to];
                } else {
                    pulses = count_w[from] - count_w[to];
                    if (pulses < 1) pulses = 1;
                }
            }
            for (int phase = 0; phase < 21; ++phase) {
                const int expected = to == from ? 0 : endpoint
                    ? (phase < 20 ? source_action((const uint8_t*)e0470_full_du_data, phase + endpoint_shift, to, from) : 0)
                    : (phase < pulses ? action : phase == 14 && late ? action : 0);
                const int actual = e0470_phase_action(phases, phase, to, from);
                assert(actual == expected);
                if (to == from) assert(actual == 0);
                if (!endpoint && to != from && phase >= pulses && !(phase == 14 && late)) assert(actual == 0);
                if (!endpoint && to != from && actual) assert(actual == action);
                ++checked;
            }
        }
        // 单向样点：15→8为6个黑脉冲（陡降曲线），15→1为10个（厂家成形位），15→12为4个，15→3为8个+相14迟到脉冲（半档拆对），12→14为4个白脉冲。
        // One-way samples: 15→8 takes 6 black pulses (plunge curve), 15→1 takes 10 (vendor formation level), 15→12 takes 4, 15→3 takes 8 plus the phase-14 late pulse (half-step pair split), and 12→14 takes 4 white pulses.
        int seen12;
        seen12 = 0; for (int phase = 0; phase < 21; ++phase) seen12 += e0470_phase_action(phases, phase, 12, 15) == 1;
        assert(seen12 == 4 && e0470_phase_action(phases, 4, 12, 15) == 0);
        int seen;
        seen = 0; for (int phase = 0; phase < 21; ++phase) seen += e0470_phase_action(phases, phase, 8, 15) == 1;
        assert(seen == 6 && e0470_phase_action(phases, 0, 8, 15) == 1 && e0470_phase_action(phases, 6, 8, 15) == 0);
        seen = 0; for (int phase = 0; phase < 21; ++phase) seen += e0470_phase_action(phases, phase, 1, 15) == 1;
        assert(seen == 10);
        seen = 0; for (int phase = 0; phase < 21; ++phase) seen += e0470_phase_action(phases, phase, 3, 15) == 1;
        assert(seen == 9 && e0470_phase_action(phases, 8, 3, 15) == 0 && e0470_phase_action(phases, 13, 3, 15) == 0
               && e0470_phase_action(phases, 14, 3, 15) == 1 && e0470_phase_action(phases, 15, 3, 15) == 0);
        seen = 0; for (int phase = 0; phase < 21; ++phase) seen += e0470_phase_action(phases, phase, 14, 12) == 2;
        assert(seen == 4 && e0470_phase_action(phases, 4, 14, 12) == 0);
        const EpdWaveformPhases* original = e0470_waveform_phases(&E0470_DIRECT_WAVEFORM, 5);
        assert(e0470_phase_action(original, 6, 4, 0) == 0);
        // 夜间白预驱GL表：15→0逐相等于厂家GL48的0→0对角线；EE行14→14微洗=末2白+完整黑；
        // 0→0与15→15保持零——前者不再承载微洗，后者是保持掩码白位FF的落点。
        // Night pre-drive GL table: 15→0 equals the vendor GL48 0→0 diagonal phase by phase;
        // the EE row 14→14 wash keeps the last two white phases plus the full black block;
        // 0→0 and 15→15 stay zero — the former no longer carries the wash, the latter is where
        // masked-white FF codes land.
        const EpdWaveformPhases* gl = e0470_waveform_phases(&E0470_NIGHT_PREWIPE_GL_WAVEFORM, 5);
        assert(gl && gl->phases == 49);
        for (int phase = 0; phase < 49; ++phase) {
            const int diag = phase < 48 ? source_action((const uint8_t*)e0470_full_gl16_data, phase, 0, 0) : 0;
            assert(e0470_phase_action(gl, phase, 0, 15) == diag);
            assert(e0470_phase_action(gl, phase, 14, 14) == ((diag == 2 && phase < 26) ? 0 : diag));
            assert(e0470_phase_action(gl, phase, 0, 0) == 0);
            assert(e0470_phase_action(gl, phase, 15, 15) == 0);
        }

        // 直刷族预驱：15→0为W18前导+完整18黑动作；EE行14→14微洗W2紧贴同一黑窗；
        // 0→0与15→15保持零；其余格与成品表一致。
        // Direct-family pre-drive: 15→0 is a W18 lead plus the complete 18 black actions;
        // the EE row 14→14 W2 wash hugs the same black window; 0→0 and 15→15 stay zero;
        // every other cell matches its finished table.
        const EpdWaveformPhases* direct_pw = e0470_waveform_phases(&E0470_DIRECT_PREWIPE_WAVEFORM, 5);
        const EpdWaveformPhases* gray_pw = e0470_waveform_phases(&E0470_GRAY_DIRECT_PREWIPE_WAVEFORM, 5);
        assert(direct_pw && direct_pw->phases == 37 && gray_pw && gray_pw->phases == 37);
        const int base_frames[2] = { 21, 21 };
        const EpdWaveformPhases* pw_sources[2] = { original, phases };
        const EpdWaveformPhases* pw_tables[2] = { direct_pw, gray_pw };
        for (unsigned t = 0; t < 2; ++t)
            for (int to = 0; to < 16; ++to) for (int from = 0; from < 16; ++from)
                for (int phase = 0; phase < 37; ++phase) {
                    const int black = phase >= 18 && phase < 36
                        ? source_action((const uint8_t*)e0470_full_du_data, phase - 18, 0, 15) : 0;
                    int expected = phase < base_frames[t]
                        ? e0470_phase_action(pw_sources[t], phase, to, from) : 0;
                    if (to == 0 && from == 15) expected = phase < 18 ? 2 : black;
                    if (to == 14 && from == 14) expected = phase < 16 ? 0 : phase < 18 ? 2 : black;
                    if ((to == 0 && from == 0) || (to == 15 && from == 15)) expected = 0;
                    assert(e0470_phase_action(pw_tables[t], phase, to, from) == expected);
                    ++checked;
                }
        if (run == 0) memcpy(saved, phases->luts, sizeof(saved));
        else assert(memcmp(saved, phases->luts, sizeof(saved)) == 0);
    }
    printf("Gray-edge direct (21 phases, one-way DU-dose gray rows + DU endpoints) and night pre-drive tables (EE-row wash, FF hold row): %u exact actions, diagonals held, repeat init PASS\n", checked);
    return 0;
}
