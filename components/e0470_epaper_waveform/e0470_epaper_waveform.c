/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 *
 * E0470A01 波形装配：厂家完整默认表、8 灰阶、跟随 DU。
 * 自行调整屏幕波形会使设备失去保修。
 *
 * E0470A01 waveform assembly: complete vendor default, 8-gray, follow DU.
 * Changing panel waveforms voids the warranty.
 *
 * 冻结：0.5.19单向灰阶估算、18相统一擦白和三相入口推白已被实机退化否定，保留厂家灰阶路径；单相入口表仅留诊断对照。
 * Frozen: Device regression rejects 0.5.19 estimated monotonic gray, uniform 18-tick whitening and three entry ticks; retain vendor gray paths, with the single-tick entry table for diagnostics only.
 * 修订：端点预算差不能建立校准灰阶；未经面板实测，不再把任意单向灰阶或额外白推动交付为修复。
 * Revision: Endpoint-budget differences cannot establish calibrated gray transitions; do not ship arbitrary monotonic gray or added white drive as a fix without panel measurements.
 * 冻结：正文及普通重绘白白保持，不按历史字形补擦，不触发正文周期GC16。
 * Frozen: Body turns and ordinary redraws hold white without historical glyph cleanup or scheduled body GC16.
 * 修订：0.5.20标准正文实机黑芯发白；日间恢复厂家黑/灰对角线并用真实整页差分，夜间保留原选择性保持，避免0→0定稿使黑底整屏亮闪。
 * Revision: Standard body black cores fade on 0.5.20 hardware; day retains vendor black/gray diagonals with actual full-page differences, while night retains prior selective holds to avoid a whole-screen light flash from black-background 0→0 settling.
 * 冻结：用户本次明确优先无闪速度，直刷改用厂家黑白DU；目标仅0/15，旧灰码与20相动作顺序不得量化或重排。
 * Frozen: The user now prioritizes flicker-free speed, choosing vendor black/white DU for direct turns; targets are only 0/15, without quantizing old gray codes or reordering the 20 source phases.
 * 实机修订：0.5.25定向补黑和后置局部擦写未改善夜间残影，后者增加旧字闪动；撤回两实验，保留普通翻页与已有周期/手动完整清理。
 * Device revision: 0.5.25 black reinforcement and post-DU local cleaning failed to improve night ghosts, with local cleaning flashing old glyphs; withdraw both experiments and retain ordinary turns with existing interval/manual full cleaning.
 * 冻结：用户要求继续比较Crossmux Pico方案；独立对照表逐字节复现freeink-sdk 96de1be默认DU20/GC36/GL37，仅由夜间可选路径调用，不改本地默认、日间或厂家源表。
 * Frozen: The user requests further comparison with Crossmux Pico; an isolated reference reproduces freeink-sdk 96de1be default DU20/GC36/GL37 byte for byte, selected only by optional night paths without changing local defaults, daytime or vendor source tables.
 * 修订：此前只比较源码，没有交付完整Crossmux波形对照；本次复现其裁剪与白白单相动作，不增加新剂量，也不承诺光学改善。
 * Revision: Prior work compared source without delivering a complete Crossmux waveform reference; this reproduces its trimming and single white-to-white action without new doses or an optical-improvement claim.
 */

#include "e0470_epaper_waveform.h"

#include <assert.h>
#include <string.h>

#include "e0470_waveform_trim.h"
#include "du.h"
#include "gc16.h"
#include "gl16.h"
#include "gray8_gc16.h"
#include "gray8_gl16.h"

// 温度档 0-50°C。在此固件中没有温度分档的演示。
// / One 0–50°C temp range. This firmware has no multi-range demo.
static const EpdWaveformTempInterval e0470_intervals[] = {
    { .min = 0, .max = 50 },
};

// 把 (from, to) 的一个 2bit 动作写进 epdiy 的表：data[frame][to][from/4]，高位是 from0。
// / Write one 2-bit (from, to) action into the epdiy table: data[frame][to][from/4], MSB is from0.
static inline void lut_or(uint8_t (*data)[16][4], int f, int to, int from, int action) {
    data[f][to][from / 4] |= (uint8_t)(action << (6 - 2 * (from % 4)));
}

static inline void lut_set(uint8_t (*data)[16][4], int f, int to, int from, int action) {
    unsigned shift = 6 - 2 * (from % 4);
    data[f][to][from / 4] = (uint8_t)((data[f][to][from / 4] & ~(3u << shift)) | action << shift);
}

static inline int lut_get(const uint8_t (*data)[16][4], int f, int to, int from) {
    return (data[f][to][from / 4] >> (6 - 2 * (from % 4))) & 3;
}

void e0470_follow_lut_build(int frames, uint8_t (*dst)[16][4]) {
    memset(dst, 0, (size_t)frames * 16 * 4);
    // 两个方向各有自己的满推次数；短表（连续 DU 单帧、连调 dufr n）按表长封顶。
    // / Each direction has its own full-push count; short tables (1-frame
    // continuous DU, live-tune dufr n) cap at the table length.
    const int black_max = frames < E0470_FOLLOW_BLACK_FRAMES ? frames : E0470_FOLLOW_BLACK_FRAMES;
    const int white_max = frames < E0470_FOLLOW_WHITE_FRAMES ? frames : E0470_FOLLOW_WHITE_FRAMES;
    for (int to = 0; to < 16; to++) {
        for (int from = 0; from < 16; from++) {
            if (to == from) continue;
            const int diff = to > from ? to - from : from - to;
            const int action = to > from ? 2 : 1;  // 往白推 0b10，往黑推 0b01 / 0b10 erase, 0b01 darken
            const int budget = action == 2 ? white_max : black_max;
            // 推动次数 = ceil(diff · 满推 / 15)，至少 1：差得远多推，差得近少推。
            // / Push count = ceil(diff · full / 15), at least 1: far travels more, near travels less.
            const int pushes = (diff * budget + 14) / 15;
            for (int f = 0; f < pushes; f++) lut_or(dst, f, to, from, action);
        }
    }
}

/* ---- 跟随 DU：开机按公式生成 / Follow DU: built at boot ---- */
static uint8_t e0470_follow_data[E0470_FOLLOW_FRAMES][16][4];
static const EpdWaveformPhases e0470_follow_phases = {
    .phases = E0470_FOLLOW_FRAMES,
    .phase_times = NULL,
    .luts = (const uint8_t*)&e0470_follow_data[0],
};
static const EpdWaveformPhases* e0470_follow_ranges[] = { &e0470_follow_phases };
static const EpdWaveformMode e0470_follow_mode = {
    .type = 1,  // MODE_DU / MODE_DU
    .temp_ranges = 1,
    .range_data = &e0470_follow_ranges[0],
};
static const EpdWaveformMode* e0470_follow_modes[] = { &e0470_follow_mode };

const EpdWaveform E0470_FOLLOW_WAVEFORM = {
    .num_modes = 1,
    .num_temp_ranges = 1,
    .mode_data = e0470_follow_modes,
    .temp_intervals = e0470_intervals,
};

/* ---- 阈值 DU / Threshold DU ---- */
// 源表只认目标 0/15。中间灰按 50/50 切开，暗的走整段到黑、亮的走整段到白。
// from 不切片，沿用源表对真实起点的时间序列，上一帧残留的浅墨也会被推到黑或白。
// / Source tables only drive dest 0/15. Mid grays split 50/50: dark runs
// the full path to black, light the full path to white. from is not sliced;
// the source time series for the real start is reused, so leftover ink
// from the last frame is also pushed to black or white.
static uint8_t e0470_complete_du_data[E0470_FULL_DU_FRAMES][16][4];
static const EpdWaveformPhases e0470_complete_du_phases = {
    .phases = E0470_FULL_DU_FRAMES,
    .phase_times = NULL,
    .luts = (const uint8_t*)&e0470_complete_du_data[0],
};
static const EpdWaveformPhases* e0470_complete_du_ranges[] = {
    &e0470_complete_du_phases,
};
static const EpdWaveformMode e0470_complete_du_mode = {
    .type = 1,
    .temp_ranges = 1,
    .range_data = &e0470_complete_du_ranges[0],
};

static void e0470_complete_du_build(void) {
    memset(e0470_complete_du_data, 0, sizeof(e0470_complete_du_data));
    for (int to = 0; to < 16; to++) {
        const int to_bin = to < 8 ? 0 : 15;
        for (int from = 0; from < 16; from++) {
            for (int f = 0; f < E0470_FULL_DU_FRAMES; f++) {
                const int action = lut_get(e0470_full_du_data, f, to_bin, from);
                if (action != 0) lut_or(e0470_complete_du_data, f, to, from, action);
            }
        }
    }
}

/* ---- 完整厂家表 / Complete vendor tables ---- */
// 实机持续灰底：保留全部擦除/饱和相与对角线，不额外推白或重排时序。
// Persistent device gray backgrounds require every erase/saturation phase and diagonal, without extra white drive or reordered timing.
static const EpdWaveformMode* e0470_full_modes[] = {
    &e0470_full_du_mode,
    &e0470_full_gc16_mode,
    &e0470_full_gl16_mode,
};
const EpdWaveform E0470_FULL_WAVEFORM = {
    .num_modes = 3, .num_temp_ranges = 1,
    .mode_data = e0470_full_modes, .temp_intervals = e0470_intervals,
};

// GL16源表尾部两相保持；补一相中性扫描增加下电前收尾余量，不增加黑白推动。
// GL16 ends in two neutral holds; one more neutral scan adds settling margin before power-off without extra black/white drive.
static uint8_t e0470_settled_gl16_data[E0470_GL16_FRAMES][16][4];
static const EpdWaveformPhases e0470_settled_gl16_phases = {
    .phases = E0470_GL16_FRAMES, .phase_times = NULL,
    .luts = (const uint8_t*)e0470_settled_gl16_data,
};
static const EpdWaveformPhases* e0470_settled_gl16_ranges[] = { &e0470_settled_gl16_phases };
static const EpdWaveformMode e0470_settled_gl16_mode = {
    .type = 5, .temp_ranges = 1, .range_data = e0470_settled_gl16_ranges,
};

// 夜间标准变化保留厂家迁移，未变像素保持，避免整片黑底参与0→0擦除/重写。
// Standard night changes retain vendor transitions and hold unchanged pixels so the whole black background never enters 0→0 erase/rewrite.
static uint8_t e0470_page_gl16_data[E0470_PAGE_GL16_FRAMES][16][4];
static const EpdWaveformPhases e0470_page_gl16_phases = {
    .phases = E0470_PAGE_GL16_FRAMES, .phase_times = NULL,
    .luts = (const uint8_t*)e0470_page_gl16_data,
};
static const EpdWaveformPhases* e0470_page_gl16_ranges[] = { &e0470_page_gl16_phases };
static const EpdWaveformMode e0470_page_gl16_mode = {
    .type = 5, .temp_ranges = 1, .range_data = e0470_page_gl16_ranges,
};
static const EpdWaveformMode* e0470_page_modes[] = { &e0470_page_gl16_mode };
// 日间标准复用完整厂家表；与夜间同相数，且不增加LUT内存。
// Standard day reuses the complete vendor table with the same phase count as night and no extra LUT memory.
static const EpdWaveformMode* e0470_text_modes[] = { &e0470_settled_gl16_mode };
const EpdWaveform E0470_TEXTTURN_WAVEFORM = {
    .num_modes = 1, .num_temp_ranges = 1,
    .mode_data = e0470_text_modes, .temp_intervals = e0470_intervals,
};
const EpdWaveform E0470_TEXTTURN_NIGHT_WAVEFORM = {
    .num_modes = 1, .num_temp_ranges = 1,
    .mode_data = e0470_page_modes, .temp_intervals = e0470_intervals,
};
// 普通导航重绘保留完整厂家GL16；白白保持，无新增推动。
// Ordinary navigation redraws retain full vendor GL16 with held white and no added drive.
static const EpdWaveformMode* e0470_navigation_modes[] = { &e0470_settled_gl16_mode };
const EpdWaveform E0470_NAVIGATION_WAVEFORM = {
    .num_modes = 1, .num_temp_ranges = 1,
    .mode_data = e0470_navigation_modes, .temp_intervals = e0470_intervals,
};

// 诊断边界表仅为真实白白追加一次白动作，保留厂家迁移及三相中性尾；产品布局入口不调用。
// The diagnostic boundary table adds one white action to actual white-to-white pixels, retaining vendor transitions and three neutral tails; product layout entries do not use it.
static uint8_t e0470_navigation_entry_data[E0470_GL16_FRAMES][16][4];
static const EpdWaveformPhases e0470_navigation_entry_phases = {
    .phases = E0470_GL16_FRAMES, .phase_times = NULL,
    .luts = (const uint8_t*)e0470_navigation_entry_data,
};
static const EpdWaveformPhases* e0470_navigation_entry_ranges[] = { &e0470_navigation_entry_phases };
static const EpdWaveformMode e0470_navigation_entry_mode = {
    .type = 5, .temp_ranges = 1, .range_data = e0470_navigation_entry_ranges,
};
static const EpdWaveformMode* e0470_navigation_entry_modes[] = { &e0470_navigation_entry_mode };
const EpdWaveform E0470_NAVIGATION_ENTRY_WAVEFORM = {
    .num_modes = 1, .num_temp_ranges = 1,
    .mode_data = e0470_navigation_entry_modes, .temp_intervals = e0470_intervals,
};

static void e0470_navigation_entry_build(void) {
    memcpy(e0470_navigation_entry_data, e0470_settled_gl16_data, sizeof(e0470_navigation_entry_data));
    // 合并到厂家最后白饱和相，不向中性尾追加推动；找不到动作时保持原表。
    // Merge into the last vendor white saturation phase without driving the neutral tail; retain the source if none exists.
    for (int f = E0470_FULL_GL16_FRAMES - 1; f >= 0; --f) {
        for (int from = 0; from < 15; ++from) {
            if (lut_get(e0470_full_gl16_data, f, 15, from) != 2) continue;
            lut_set(e0470_navigation_entry_data, f, 15, 15, 2);
            return;
        }
    }
}

/* ---- 黑白直刷 / Black-white direct ---- */
// 用户选择黑白目标换取无反向迁移；厂家DU20相完整复制，只补一相中性，不再混合灰阶路径。
// The user chooses black/white targets for transitions without reverse drive; copy all 20 vendor DU phases and append one neutral phase without mixed gray paths.
static uint8_t e0470_direct_data[E0470_DIRECT_FRAMES][16][4];
static const EpdWaveformPhases e0470_direct_phases = {
    .phases = E0470_DIRECT_FRAMES, .phase_times = NULL,
    .luts = (const uint8_t*)e0470_direct_data,
};
static const EpdWaveformPhases* e0470_direct_ranges[] = { &e0470_direct_phases };
static const EpdWaveformMode e0470_direct_mode = {
    .type = 5, .temp_ranges = 1, .range_data = e0470_direct_ranges,
};
static const EpdWaveformMode* e0470_direct_modes[] = { &e0470_direct_mode };
const EpdWaveform E0470_DIRECT_WAVEFORM = {
    .num_modes = 1, .num_temp_ranges = 1,
    .mode_data = e0470_direct_modes, .temp_intervals = e0470_intervals,
};

/* ---- Crossmux Pico 对照 / Crossmux Pico reference ---- */
// 来源：0x1abin/freeink-sdk@96de1be6ce08eb732909e6e8149af8f892b9a2c5，EpdiyLcd/src/e0470/e0470_epaper_waveform.c 默认装配。
// Source: 0x1abin/freeink-sdk@96de1be6ce08eb732909e6e8149af8f892b9a2c5, EpdiyLcd/src/e0470/e0470_epaper_waveform.c default assembly.
// 裁剪器要求源相数容量；只读厂家48相源，不把白白动作先写入源表，也不套用TextTurn左对齐或对角线清零。
// The trimmer requires source-phase capacity; read the vendor 48-phase sources without adding white-to-white drive to them or applying TextTurn left alignment/diagonal clearing.
static uint8_t e0470_crossmux_gc16_data[E0470_FULL_GC16_FRAMES][16][4];
static uint8_t e0470_crossmux_gl16_data[E0470_FULL_GL16_FRAMES][16][4];
static const EpdWaveformPhases e0470_crossmux_gc16_phases = {
    .phases = E0470_CROSSMUX_GC16_FRAMES, .phase_times = NULL,
    .luts = (const uint8_t*)e0470_crossmux_gc16_data,
};
static const EpdWaveformPhases e0470_crossmux_gl16_phases = {
    .phases = E0470_CROSSMUX_GL16_FRAMES, .phase_times = NULL,
    .luts = (const uint8_t*)e0470_crossmux_gl16_data,
};
static const EpdWaveformPhases* e0470_crossmux_gc16_ranges[] = { &e0470_crossmux_gc16_phases };
static const EpdWaveformPhases* e0470_crossmux_gl16_ranges[] = { &e0470_crossmux_gl16_phases };
static const EpdWaveformMode e0470_crossmux_gc16_mode = {
    .type = 2, .temp_ranges = 1, .range_data = e0470_crossmux_gc16_ranges,
};
static const EpdWaveformMode e0470_crossmux_gl16_mode = {
    .type = 5, .temp_ranges = 1, .range_data = e0470_crossmux_gl16_ranges,
};
static const EpdWaveformMode* e0470_crossmux_modes[] = {
    &e0470_complete_du_mode, &e0470_crossmux_gc16_mode, &e0470_crossmux_gl16_mode,
};
const EpdWaveform E0470_CROSSMUX_WAVEFORM = {
    .num_modes = 3, .num_temp_ranges = 1,
    .mode_data = e0470_crossmux_modes, .temp_intervals = e0470_intervals,
};

static void e0470_crossmux_build(void) {
    const e0470_trim_t trim = {
        .erase_max = 11, .sat_cut = 5, .white_sat_cut = 0, .hold = 3,
    };
    const int gc = e0470_waveform_trim(&e0470_full_gc16_phases, &trim, e0470_crossmux_gc16_data);
    const int gl = e0470_waveform_trim(&e0470_full_gl16_phases, &trim, e0470_crossmux_gl16_data);
    assert(gc == E0470_CROSSMUX_GC16_FRAMES && gl == E0470_CROSSMUX_GL16_FRAMES);
    // 与SDK一致：在裁剪GL最后的白动作相给15→15补一次白，不新增相；没有白动作时沿用其尾前第三相回退。
    // Match the SDK: add one 15→15 white action at the trimmed GL's last white-action phase without adding phases, using its third-from-last fallback when none exists.
    int tick = -1;
    for (int f = E0470_CROSSMUX_GL16_FRAMES - 1; f >= 0 && tick < 0; --f)
        for (int from = 0; from < 15; ++from)
            if (lut_get(e0470_crossmux_gl16_data, f, 15, from) == 2) { tick = f; break; }
    if (tick < 0) tick = E0470_CROSSMUX_GL16_FRAMES - 3;
    lut_or(e0470_crossmux_gl16_data, tick, 15, 15, 2);
}

// FF选择三相原厂擦白尾段，00和其他动作码保持；不会压黑任何像素。
// FF selects three vendor erase-tail phases; 00 and other action codes hold, never darkening pixels.
static uint8_t e0470_white_cleanup_data[E0470_WHITE_CLEANUP_FRAMES][16][4];
static const EpdWaveformPhases e0470_white_cleanup_phases = {
    .phases = E0470_WHITE_CLEANUP_FRAMES, .phase_times = NULL,
    .luts = (const uint8_t*)e0470_white_cleanup_data,
};
static const EpdWaveformPhases* e0470_white_cleanup_ranges[] = { &e0470_white_cleanup_phases };
static const EpdWaveformMode e0470_white_cleanup_mode = {
    .type = 1, .temp_ranges = 1, .range_data = e0470_white_cleanup_ranges,
};
static const EpdWaveformMode* e0470_white_cleanup_modes[] = { &e0470_white_cleanup_mode };
const EpdWaveform E0470_WHITE_CLEANUP_WAVEFORM = {
    .num_modes = 1, .num_temp_ranges = 1,
    .mode_data = e0470_white_cleanup_modes, .temp_intervals = e0470_intervals,
};

/* ---- 8 灰阶表 / 8-gray tables ---- */
// GC16 / GL16 各 30 相，拿灰阶档数换速度。
// / GC16 / GL16 30 phases each; trade gray steps for speed.
static const EpdWaveformMode* e0470_gray8_modes[] = {
    &e0470_complete_du_mode,
    &e0470_gray8_gc16_mode,
    &e0470_gray8_gl16_mode,
};

const EpdWaveform E0470_GRAY8_WAVEFORM = {
    .num_modes = 3,
    .num_temp_ranges = 1,
    .mode_data = e0470_gray8_modes,
    .temp_intervals = e0470_intervals,
};

/* ---- 默认表 / Default tables ---- */
// 默认GL16保留48相厂家动作并补中性收尾；阈值 DU 仅用于动态控件。
// Default GL16 retains 48 vendor phases plus neutral settling; threshold DU is for dynamic controls only.
static const EpdWaveformMode* e0470_modes[] = {
    &e0470_complete_du_mode,
    &e0470_full_gc16_mode,
    &e0470_settled_gl16_mode,
};
const EpdWaveform E0470_WAVEFORM = {
    .num_modes = 3, .num_temp_ranges = 1,
    .mode_data = e0470_modes, .temp_intervals = e0470_intervals,
};

const EpdWaveformPhases* e0470_waveform_phases(const EpdWaveform* waveform, int mode) {
    if (waveform == NULL) return NULL;
    const int type = mode & 0x3F;
    for (int i = 0; i < waveform->num_modes; i++) {
        if (waveform->mode_data[i]->type == type) return waveform->mode_data[i]->range_data[0];
    }
    return NULL;
}

int e0470_phase_action(const EpdWaveformPhases* phases, int phase, int to, int from) {
    if (phases == NULL || phases->luts == NULL) return 0;
    if (phase < 0 || phase >= phases->phases) return 0;
    if ((unsigned)to > 15 || (unsigned)from > 15) return 0;
    const uint8_t* cell = phases->luts + ((size_t)phase * 16 + to) * 4 + from / 4;
    return (*cell >> (6 - 2 * (from % 4))) & 3;
}

void e0470_waveform_init(void) {
    e0470_follow_lut_build(E0470_FOLLOW_FRAMES, e0470_follow_data);
    e0470_complete_du_build();
    e0470_crossmux_build();
    memset(e0470_settled_gl16_data, 0, sizeof(e0470_settled_gl16_data));
    memcpy(e0470_settled_gl16_data, e0470_full_gl16_data, sizeof(e0470_full_gl16_data));
    e0470_navigation_entry_build();
    memset(e0470_page_gl16_data, 0, sizeof(e0470_page_gl16_data));
    memcpy(e0470_page_gl16_data, e0470_full_gl16_data, sizeof(e0470_full_gl16_data));
    memset(e0470_direct_data, 0, sizeof(e0470_direct_data));
    // 中间目标全中性，使EE选择码保持；真实旧灰仍沿用厂家到0/15的每一相动作。
    // Intermediate targets stay neutral for held EE selectors; every actual old gray retains each vendor phase toward 0/15.
    for (int to = 0; to <= 15; to += 15) for (int from = 0; from < 16; ++from)
        for (int f = 0; f < E0470_FULL_DU_FRAMES; ++f)
            lut_set(e0470_direct_data, f, to, from, lut_get(e0470_full_du_data, f, to, from));
    // 夜间与黑白直刷使用EE选择性保持；日间直接复用厂家全部对角线，白白原表仍保持。
    // Night and black/white direct use selective EE holds; day directly reuses every vendor diagonal, with held vendor white-to-white.
    for (int f = 0; f < E0470_PAGE_GL16_FRAMES; ++f)
        for (int gray = 0; gray < 16; ++gray) lut_set(e0470_page_gl16_data, f, gray, gray, 0);
    for (int f = 0; f < E0470_DIRECT_FRAMES; ++f)
        for (int gray = 0; gray < 16; ++gray) lut_set(e0470_direct_data, f, gray, gray, 0);
    memset(e0470_white_cleanup_data, 0, sizeof(e0470_white_cleanup_data));
    for (int f = 0; f < 3; ++f)
        lut_set(e0470_white_cleanup_data, f, 15, 15,
                (e0470_full_du_data[15 + f][15][0] >> 6) & 3);
}
