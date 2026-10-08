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
 * 冻结：默认正文及普通重绘白白保持；用户授权的二值夜间A/B实验可在成功DU后只处理选中黑像素，实验表不得改动其他档位。
 * Frozen: Default body turns and ordinary redraws hold white; user-authorized binary-night A/B experiments may act only on selected black pixels after successful DU, without changing other profiles.
 * 修订：持续旧白字轮廓促使用户授权定向补黑与局部擦写对照；推动剂量未经光学校准，仅作实验，不承诺无残影。
 * Revision: Persistent old white glyph outlines lead the user to authorize targeted black boosting and local erase/rewrite comparisons; drive doses are not optically calibrated and remain experiments without a ghost-free claim.
 * 修订：0.5.20标准正文实机黑芯发白；日间恢复厂家黑/灰对角线并用真实整页差分，夜间保留原选择性保持，避免0→0定稿使黑底整屏亮闪。
 * Revision: Standard body black cores fade on 0.5.20 hardware; day retains vendor black/gray diagonals with actual full-page differences, while night retains prior selective holds to avoid a whole-screen light flash from black-background 0→0 settling.
 * 冻结：用户本次明确优先无闪速度，直刷改用厂家黑白DU；目标仅0/15，旧灰码与20相动作顺序不得量化或重排。
 * Frozen: The user now prioritizes flicker-free speed, choosing vendor black/white DU for direct turns; targets are only 0/15, without quantizing old gray codes or reordering the 20 source phases.
 */

#include "e0470_epaper_waveform.h"

#include <string.h>

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

/* ---- 二值夜间清理实验 / Binary-night cleanup experiments ---- */
// 00选择已成功DU后的实际黑像素，EE与其他码保持；不作虚构的旧灰迁移，也不修改前后缓冲。
// Selector 00 addresses actual black pixels after successful DU; EE and every other code hold, without fictional old-gray transitions or framebuffer changes.
// 两相黑推动取厂家DU15→0的最后两黑相，随后三相中性；剂量未标定，仅用于用户授权实验。
// Two black actions come from the last two black phases of vendor DU15→0, followed by three neutral phases; the uncalibrated dose is only for the user-authorized experiment.
static uint8_t e0470_night_black_boost_data[E0470_NIGHT_BLACK_BOOST_FRAMES][16][4];
static const EpdWaveformPhases e0470_night_black_boost_phases = {
    .phases = E0470_NIGHT_BLACK_BOOST_FRAMES, .phase_times = NULL,
    .luts = (const uint8_t*)e0470_night_black_boost_data,
};
static const EpdWaveformPhases* e0470_night_black_boost_ranges[] = { &e0470_night_black_boost_phases };
static const EpdWaveformMode e0470_night_black_boost_mode = {
    .type = 5, .temp_ranges = 1, .range_data = e0470_night_black_boost_ranges,
};
static const EpdWaveformMode* e0470_night_black_boost_modes[] = { &e0470_night_black_boost_mode };
const EpdWaveform E0470_NIGHT_BLACK_BOOST_WAVEFORM = {
    .num_modes = 1, .num_temp_ranges = 1,
    .mode_data = e0470_night_black_boost_modes, .temp_intervals = e0470_intervals,
};

// 仅00逐相复制厂家GC16的0→0擦写；其余码保持，局部亮闪和光学清理效果须实机验证。
// Only 00 copies vendor GC16's 0→0 erase/rewrite phase by phase; other codes hold, with local light flashing and optical cleanup requiring device verification.
static uint8_t e0470_night_local_clean_data[E0470_NIGHT_LOCAL_CLEAN_FRAMES][16][4];
static const EpdWaveformPhases e0470_night_local_clean_phases = {
    .phases = E0470_NIGHT_LOCAL_CLEAN_FRAMES, .phase_times = NULL,
    .luts = (const uint8_t*)e0470_night_local_clean_data,
};
static const EpdWaveformPhases* e0470_night_local_clean_ranges[] = { &e0470_night_local_clean_phases };
static const EpdWaveformMode e0470_night_local_clean_mode = {
    .type = 5, .temp_ranges = 1, .range_data = e0470_night_local_clean_ranges,
};
static const EpdWaveformMode* e0470_night_local_clean_modes[] = { &e0470_night_local_clean_mode };
const EpdWaveform E0470_NIGHT_LOCAL_CLEAN_WAVEFORM = {
    .num_modes = 1, .num_temp_ranges = 1,
    .mode_data = e0470_night_local_clean_modes, .temp_intervals = e0470_intervals,
};

static void e0470_night_cleanup_build(void) {
    memset(e0470_night_black_boost_data, 0, sizeof(e0470_night_black_boost_data));
    int phase = E0470_NIGHT_BLACK_BOOST_ACTIVE_FRAMES - 1;
    for (int f = E0470_FULL_DU_FRAMES - 1; f >= 0 && phase >= 0; --f) {
        const int action = lut_get(e0470_full_du_data, f, 0, 15);
        if (action == 1) lut_set(e0470_night_black_boost_data, phase--, 0, 0, action);
    }
    memset(e0470_night_local_clean_data, 0, sizeof(e0470_night_local_clean_data));
    for (int f = 0; f < E0470_NIGHT_LOCAL_CLEAN_FRAMES; ++f)
        lut_set(e0470_night_local_clean_data, f, 0, 0, lut_get(e0470_full_gc16_data, f, 0, 0));
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
    e0470_night_cleanup_build();
    memset(e0470_white_cleanup_data, 0, sizeof(e0470_white_cleanup_data));
    for (int f = 0; f < 3; ++f)
        lut_set(e0470_white_cleanup_data, f, 15, 15,
                (e0470_full_du_data[15 + f][15][0] >> 6) & 3);
}
