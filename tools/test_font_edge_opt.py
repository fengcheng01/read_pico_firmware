#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 mindreset
# SPDX-License-Identifier: Apache-2.0
"""灰缘落点预矫正取码回归：单调、端点精确、落点反演偏差与并列码的确定性。/ Gray-edge landing inversion regression: monotonicity, exact endpoints, inversion error and deterministic ties."""
from pathlib import Path
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
SOURCE = (ROOT / 'main/font/ttf_font.c').read_text(encoding='utf-8')

def function(name):
    match = re.search(r'^(?:static )?[^\n]+\b' + name + r'\([^\n]*\) \{', SOURCE, re.M)
    assert match
    at, depth = match.end(), 1
    while depth:
        depth += (SOURCE[at] == '{') - (SOURCE[at] == '}')
        at += 1
    return SOURCE[match.start():at]

def table(name):
    match = re.search(r'static const uint16_t ' + name + r'\[16\] = \{([^}]*)\};', SOURCE, re.M)
    assert match
    return 'static const uint16_t ' + name + '[16] = {' + match.group(1) + '};'

unit = r'''
#include <assert.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#define TTF_COVER_GAMMA 0.6f
static bool current_edge_optimize;
static void ttf_set_edge_optimize(bool on) { current_edge_optimize = on; }
static bool ttf_get_edge_optimize(void) { return current_edge_optimize; }
static uint8_t s_cover[256];
'''
unit += table('s_land') + '\n'
unit += function('ttf_cover_lut_init') + '\n' + function('mix_ink') + '\n' + function('mix_ink_edge') + '\n' + function('mix_ink_smooth')
unit += r'''
int main(void) {
    ttf_cover_lut_init();
    // 空间对照穷举覆盖、奇偶坐标、单调性、端点、可分码、平均误差与重复稳定性。
    // Exhaust spatial coverage/parities, monotonicity, endpoints, distinct codes, mean error and repeatability.
    int smooth_changes = 0;
    for (int y = 0; y < 2; ++y) for (int x = 0; x < 2; ++x) {
        int previous = 15;
        for (int alpha = 0; alpha < 256; ++alpha) {
            int code = mix_ink_smooth(alpha, 0, 15, x, y);
            assert(code >= 0 && code <= 15 && code != 14 && code <= previous);
            previous = code;
            assert(code == mix_ink_smooth(alpha, 0, 15, x+100, y+100));
            smooth_changes += code != mix_ink_edge(alpha, 0, 15);
            if (alpha == 0) assert(code == 15);
            if (alpha == 255) assert(code == 0);
        }
    }
    assert(smooth_changes > 0);
    for (int alpha = 0; alpha < 256; ++alpha) {
        int target = 255 - s_cover[alpha], sum = 0;
        int gap = 0;
        for (int k = 1; k < 16; ++k)
            if (s_land[k-1] <= target && target <= s_land[k] && s_land[k] > s_land[k-1]) {
                gap = s_land[k] - s_land[k-1]; break;
            }
        for (int y = 0; y < 2; ++y) for (int x = 0; x < 2; ++x)
            sum += s_land[mix_ink_smooth(alpha, 0, 15, x, y)];
        assert(abs(sum - 4*target)*2 <= gap);
        // 非黑字白底保持旧落点反演；不把空间网纹引入其它UI灰色组合。
        // Other color combinations retain legacy inversion, keeping spatial patterns out of other UI blends.
        for (int fg = 0; fg < 16; ++fg) for (int bg = 0; bg < 16; ++bg) {
            int code = mix_ink_smooth(alpha, fg, bg, 0, 0);
            if (alpha == 0) assert(code == bg);
            else if (fg != 0 || bg != 15) assert(code == mix_ink_edge(alpha, fg, bg));
        }
    }
    puts("Spatial comparison: 256 coverages, stable2x2, exact endpoints, no tied14, monotonicity and uniform-block mean error <= gap/8 PASS");
    assert(ttf_get_edge_optimize() == false);
    ttf_set_edge_optimize(true);
    // 落点表自身：端点精确、13/14 同落、其余单调且档距不小于一级。
    // Landing table itself: exact endpoints, 13/14 tied, monotone with level-scale spacing.
    assert(s_land[0] == 0 && s_land[15] == 255);
    assert(s_land[13] == s_land[14]);
    for (int k = 1; k < 16; ++k) {
        assert(s_land[k] > s_land[k - 1] || k == 14);
    }
    // 黑字白底（fg=0,bg=15）：覆盖升→取码单调不增，端点精确，反演偏差≤半级+舍入。
    // Black ink on white (fg=0,bg=15): coverage rising keeps codes non-increasing,
    // endpoints exact and inversion error within half a level plus rounding.
    int prev = 15;
    bool seen[16] = {false};
    // 反演允许的误差上界：相邻可分落点的最大间距的一半加舍入；浅端 13→15 的空洞按此计。
    // Allowed inversion error: half the largest gap between adjacent distinguishable
    // landings plus rounding; the light-end 13-to-15 hole is covered by this bound.
    int max_gap = 0;
    for (int k = 1; k < 16; ++k) {
        int gap = (int)s_land[k] - s_land[k - 1];
        if (gap > max_gap) max_gap = gap;
    }
    int bound = max_gap / 2 + 1;
    for (int alpha = 0; alpha < 256; ++alpha) {
        uint8_t code = mix_ink_edge((uint8_t)alpha, 0, 15);
        if (alpha == 0) assert(code == 15);
        assert((int)code <= prev);
        prev = code;
        seen[code] = true;
        // 反演性质：所选码的落点距理想光学位置不超过相邻落点最大间距的一半加舍入。
        // Inversion property: the chosen landing stays within half the largest
        // adjacent-landing gap plus rounding of the ideal optical position.
        int t = s_land[15] + (int)(s_cover[alpha] * ((int)s_land[0] - (int)s_land[15]) / 255);
        int d = (int)s_land[code] - t;
        if (d < 0) d = -d;
        assert(d <= bound);
    }
    assert(mix_ink_edge(255, 0, 15) == 0);
    // 白字黑底（fg=15,bg=0）方向同样单调不减、端点精确。
    // White ink on black (fg=15,bg=0) stays non-decreasing with exact endpoints.
    prev = 0;
    for (int alpha = 0; alpha < 256; ++alpha) {
        uint8_t code = mix_ink_edge((uint8_t)alpha, 15, 0);
        assert((int)code >= prev);
        prev = code;
    }
    assert(mix_ink_edge(255, 15, 0) == 15);
    // 并列码确定性：14 永远排在 13 之后取值，浅端外圈一致取 13；关闭开关回原混合。
    // Tie determinism: 14 never wins over 13 for the tied landing; the toggle restores the original mix.
    assert(!seen[14] && seen[13]);
    // 开关有效性：默认落点表下差异集中在浅端外圈（并列码重构），覆盖率重分配确有取码变化。
    // Toggle efficacy: under the default landing table the delta concentrates in the light
    // fringe (tied-code restructuring), yet code selection demonstrably changes.
    int changed = 0;
    for (int alpha = 1; alpha < 255; ++alpha)
        if (mix_ink_edge((uint8_t)alpha, 0, 15) != mix_ink((uint8_t)alpha, 0, 15)) ++changed;
    assert(changed > 0);
    ttf_set_edge_optimize(false);
    for (int alpha = 0; alpha < 256; ++alpha) {
        // 关闭开关时原混合路径逐点保持：bg + 覆盖×(fg−bg)/255。
        // With the toggle off the legacy blend holds pointwise: bg + cover*(fg-bg)/255.
        assert(mix_ink((uint8_t)alpha, 0, 15) ==
               (uint8_t)(15 + s_cover[alpha] * ((int)0 - (int)15) / 255));
    }
    puts("edge optimize mapping: monotone ramp, exact endpoints, bounded inversion error and deterministic ties pass");
    return 0;
}
'''
with tempfile.TemporaryDirectory(prefix='font-edge-') as temp:
    path = Path(temp)
    (path / 'test.c').write_text(unit, encoding='utf-8')
    subprocess.run(['cc', '-std=c11', '-Wall', '-Wextra', '-Werror', '-fsanitize=address,undefined',
                    '-Imain/font', str(path / 'test.c'), '-lm', '-o', str(path / 'test')],
                   cwd=ROOT, check=True)
    subprocess.run([str(path / 'test')], check=True)
