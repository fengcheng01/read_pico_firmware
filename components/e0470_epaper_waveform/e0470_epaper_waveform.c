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
 * 冻结：用户要求继续改善直刷字缘；独立可选灰边档保留厂家DU20端点及完整8灰GL30中间目标，旧灰真实、未变保持，仅补一中性尾。
 * Frozen: The user requests continued direct-text edge improvements; the optional gray-edge profile retains vendor DU20 endpoints and complete 8-gray GL30 intermediate targets, truthful old grays and held unchanged pixels, with one neutral tail only.
 * 修订：阈值细边实机无可见改善；新档保留真实灰阶目标而非二值取墨，最多31相慢于原21相，变化灰边会局部擦写，不能宣称无闪或光学改善已验证。
 * Revision: Threshold-based fine edges gave no visible device improvement; this profile retains actual gray targets instead of binary coverage, takes up to 31 phases versus the original 21, and locally erases changed gray edges without a flicker-free or device-proven optical claim.
 * 实机修订：灰缘抗锯齿已获用户确认，但翻页先显二值细字再闪成灰缘；测得端点动作相0-17、灰边首动作迟至相16且全部拖到相27。灰缘序列去前导空等左对齐到相0，脉冲序列逐字保留，总相31→28；仍不宣称光学改善已验证。
 * Device revision: The user confirms gray-edge anti-aliasing but reports binary-then-flash page turns; endpoints act in phases 0-17 while gray edges start as late as 16 and all end at 27. Gray sequences now drop the leading idle, left-aligning to phase 0 with byte-identical pulse sequences, 31 to 28 phases; optical improvement still awaits device proof.
 * 实机修订：左对齐后用户仍见先锯齿后抖动——厂家灰序列是过冲再修（压过头再拉回），中间态必然过黑。灰边改用跟手DU同款单调剂量公式从背景色按灰差打同向脉冲，无过冲无回修，与端点同向同动；灰阶绝对精度换观感，边纹质量须实机再验。
 * Device revision: After left-alignment the user still sees jagged-then-shake — vendor gray sequences overshoot then trim, so the intermediate state must pass blacker than target. Gray edges now use the follow-DU monotonic dose formula, pulsing toward the target from the background without overshoot or trim, moving with the endpoints; absolute gray accuracy is traded for look, and fringe quality needs device revalidation.
 * 修订：用户继续报告夜间黑底残影；白预驱档把15→0升级为厂家0→0对角线等剂量的W18先白后黑（GL逐相拷贝、直刷族同结构），并给保持黑底0→0加W2+B18微洗，模仿日间对角线重饱和；除残影与微洗观感须同板实测。
 * Revision: The user still reports night black-background ghosts; the white pre-drive profile upgrades 15→0 to the vendor 0→0 diagonal dose (W18 then B18, phase-exact on GL and same structure on the direct family) and adds a W2+B18 mini-wash to held 0→0, mimicking daytime diagonal re-saturation; ghost relief and wash visibility need same-board proof.
 * 实机决策：用户要求移除Crossmux夜间对照；其表、装配与裁剪器一并删除，不再保留孤立参考。
 * Device decision: The user orders the Crossmux night comparison removed; its tables, assembly and trimmer are deleted without keeping an isolated reference.
 * 实机修订：单调公式实机锯齿更重且笔画内发白——线性脉冲在面板近黑压缩区欠驱，厂家真表是先归一再成形的过冲结构。灰行整行回抄厂家压缩灰8表（30相），端点仍DU；EE保持改走14→14行并配主机保持掩码（见display.c），0→0行微洗从未被驱动过。
 * Device revision: The monotonic formula reads heavier jaggies with whitish stroke cores on hardware — linear pulses under-dose the panel's compressive near-black region, while vendor tables normalize first, then form (an overshoot structure). Gray rows copy the vendor compact gray8 table verbatim (30 phases) with DU endpoints; the EE hold now routes to the 14→14 row with a host hold mask (see display.c), the 0→0 wash having never been driven.
 * 实机修订：灰8终态获用户认可但仍有"先细锯齿后抗锯齿"两段——灰8行相27收尾而DU端点相0-17先完成，字形先显字芯后补边。端点行整体右移至相10-27与灰行同刻收尾，终态逐字节不变。
 * Device revision: The gray8 final state wins user approval yet the thin-then-antialiased stages remain — gray8 rows finish at phase 27 while DU endpoints finish first at 0-17, cores landing before fringes. Endpoint rows now shift as a block into phases 10-27 to finish together, with byte-identical final states.
 * 实机修订：同刻收尾后两段观感减轻，但厂家灰8"压黑再拉回"的过冲结构整页同步上演，读作明显闪动。灰行改按DU自身中灰源行的差值剂量单向直驱（降到g=DU(0,15)计数−DU(0,g)计数个黑脉冲，升向同理），相0同起点单调逼近；21相与黑白直刷同速，剂量来自厂家实测而非线性假设。
 * Device revision: With simultaneous finishing the two-stage look eases, yet the vendor gray8 overshoot-then-pull-back structure now plays synchronized across the page and reads as a distinct flash. Gray rows switch to one-way drives dosed by the DU's own mid-gray source rows (descending to g takes DU(0,15)−DU(0,g) black pulses, climbing symmetric), starting together at phase 0; 21 phases match binary-direct speed, with doses taken from vendor measurements rather than a linear assumption.
 * 实机修订：单向翻页观感获用户认可，但抗锯齿退化——厂家灰8对每个灰目标都是B10成形+W回拉，白→黑趋进约10脉冲即近饱和，0.5.36撤销差值剂量（13-16脉冲）把灰缘压到近黑、梯度塌缩。白底降灰改用厂家锚点陡降曲线（4脉冲未出浅区、10脉冲成形位、18满黑），to=1保持厂家成形深度；旧灰差值与升向白侧不变，仍21相单向、端点厂家DU，落点请实机再验。
 * Device revision: The one-way turn wins user approval while anti-aliasing regresses — vendor gray8 forms every gray target with B10 plus a W pull-back, so the white-to-black approach saturates near 10 pulses and 0.5.36's undo-difference doses (13-16 pulses) pushed fringes toward black, collapsing the gradient. White-background descent now follows the vendor-anchored plunge curve (4 pulses still light, 10 at the formation level, 18 full black) with to=1 keeping the vendor formation depth; old-gray differences and the white side stay, still 21 one-way phases with vendor DU endpoints — landings need device revalidation.
 * 实机修订：0.5.38观感再进一步、字芯不发白，仅余轻微锯齿。按落点模型做最近落点微调：to=12原5脉冲落在约10（偏浅约2级，唯一大偏差），to=13原4脉冲落在12；改为4与3个脉冲后误差降到0与半级，浅端外圈梯度恢复连续，深端（含to=1的厂家成形位）不动。
 * Device revision: 0.5.38 reads one step better with sound stroke cores and only mild jaggies. Nearest-landing micro-adjustment: to=12's five pulses landed near 10 (about two levels light, the only large error) and to=13's four landed at 12; four and three pulses cut the errors to zero and half a level, restoring a continuous outer gradient while the dark end (including to=1's vendor formation level) stays untouched.
 * 实机修订：0.5.39又进一步但横竖笔画欠直——相邻灰档共用整数剂量，落点成对塌陷，边缘梯度是两级一档的阶梯。奇数档3/5/7/9/11改为短一档主段+相14迟到脉冲（间隔后单脉冲剂量较弱，落点介于两档之间），有效剂量呈半档步进；厂家端点行本就是前导段+相17迟到脉冲结构。笔画偶发偏淡与档位成对相关，同轮拆开；若迟到脉冲等效满脉冲则落点退回上一版。
 * Device revision: 0.5.39 improves again yet H/V strokes read crooked — adjacent gray targets share integer doses, so landings collapse in pairs and the edge gradient steps two levels at a time. Odd targets 3/5/7/9/11 now run one dose shorter plus a late pulse at phase 14 (a pulse after a rest doses less, landing between the two integer doses), giving half-step effective spacing; vendor endpoint rows already use leading-run-plus-late-tick form. The occasional faint strokes track the same paired targets and split together; if the late pulse reads as a full pulse, landings fall back to the previous version.
 * 实机修订：0.5.40偶发偏淡解决、锯齿再进一步；残余锯齿来自旧灰行的并列塌陷——撤销计数表里2&3、4&5&6、7&8、9&10同值，文字重叠区的差值剂量落点仍成对。半档拆对推广到旧灰降灰（差值减并列深度+相14迟到脉冲）与升向（差值加并列深度减一+迟到脉冲），方向均为更浅档落更浅；翻页仍21相单向无闪。
 * Device revision: 0.5.40 resolves the faint strokes and improves jaggies further; the remainder tracks tied undo counts on old-gray rows — 2&3, 4&5&6, 7&8 and 9&10 share values, so difference doses still collapse in text-overlap regions. The half-step split now extends to old-gray descent (difference minus tie depth plus the phase-14 late pulse) and climbing (difference plus tie depth minus one plus the late pulse), always landing lighter targets lighter; turns stay 21 one-way flash-free phases.
 * 实机修订：0.5.41减法拆对实机部分笔画变淡——重叠区药量被削薄。降灰行回退纯差值并只在并列组最深档追加迟到脉冲（加法拆对，只加深），升向行回退纯差值；任何档位不比0.5.40更浅，白底阶梯与厂家端点不变。
 * Device revision: 0.5.41's subtractive splitting turned some strokes faint on device — overlap doses thinned. Descent rows return to pure differences with one additive late pulse on each tie group's darkest member (splitting darker only), and climbing reverts to pure differences; no target lands lighter than 0.5.40, with the white ladder and vendor endpoints unchanged.
 */

#include "e0470_epaper_waveform.h"

#include <assert.h>
#include <stdbool.h>
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

/* ---- 单相载波 / One-phase carrier ---- */
// 相位内容永远由 epd_set_line/col_phase_luts 的逐行/逐列覆盖提供；载波本身只有一相
// 且全部保持，脱离覆盖单独误用时不会驱动面板。
// Phase content always comes from the per-line/column overrides; the lone carrier
// phase holds everywhere, so misuse without overrides never drives the panel.
static uint8_t e0470_apply_data[1][16][4];
static const EpdWaveformPhases e0470_apply_phases = {
    .phases = 1,
    .phase_times = NULL,
    .luts = (const uint8_t*)&e0470_apply_data[0],
};
static const EpdWaveformPhases* e0470_apply_ranges[] = { &e0470_apply_phases };
static const EpdWaveformMode e0470_apply_mode = {
    .type = 1,
    .temp_ranges = 1,
    .range_data = &e0470_apply_ranges[0],
};
static const EpdWaveformMode* e0470_apply_modes[] = { &e0470_apply_mode };
const EpdWaveform E0470_APPLY_WAVEFORM = {
    .num_modes = 1,
    .num_temp_ranges = 1,
    .mode_data = e0470_apply_modes,
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

/* ---- 灰边直刷对照 / Gray-edge direct comparison ---- */
// 黑白端点保持DU原20相位置；灰目标单向直驱从相0同起点单调逼近。白底降灰用厂家锚点陡降
// 曲线，旧灰降灰与升向用DU行差值。
// Keep the original 20 DU phase positions for black/white endpoints; gray targets drive one way
// from phase 0. White-background descent uses the vendor-anchored plunge curve while old-gray
// descent and climbing use DU row differences.
static uint8_t e0470_gray_direct_data[E0470_GRAY_DIRECT_FRAMES][16][4];
static const EpdWaveformPhases e0470_gray_direct_phases = {
    .phases = E0470_GRAY_DIRECT_FRAMES, .phase_times = NULL,
    .luts = (const uint8_t*)e0470_gray_direct_data,
};
static const EpdWaveformPhases* e0470_gray_direct_ranges[] = { &e0470_gray_direct_phases };
static const EpdWaveformMode e0470_gray_direct_mode = {
    .type = 5, .temp_ranges = 1, .range_data = e0470_gray_direct_ranges,
};
static const EpdWaveformMode* e0470_gray_direct_modes[] = { &e0470_gray_direct_mode };
const EpdWaveform E0470_GRAY_DIRECT_WAVEFORM = {
    .num_modes = 1, .num_temp_ranges = 1,
    .mode_data = e0470_gray_direct_modes, .temp_intervals = e0470_intervals,
};

/* ---- 夜间白预驱 / Night white pre-drive ---- */
// 夜间旧白字回黑底（15→0）借用厂家0→0对角线的先白后黑满剂量往返：白像素加白光学不可见，
// 黑脉冲从重饱和白出发；保持黑底（0→0）再走一次W2+B18微洗，每翻一页重饱和一遍，
// 模仿日间对角线定稿。只在可选开关下装配。
// Night's old white glyphs returning to black (15→0) borrow the vendor 0→0 diagonal's
// full white-then-black round trip: white drive on white pixels is optically invisible and
// the black pulses start from re-saturated white; held black (0→0) also takes a W2+B18
// mini-wash each turn, mimicking daytime diagonal settling. Assembled only behind the switch.
static uint8_t e0470_night_prewipe_gl_data[E0470_PAGE_GL16_FRAMES][16][4];
static const EpdWaveformPhases e0470_night_prewipe_gl_phases = {
    .phases = E0470_PAGE_GL16_FRAMES, .phase_times = NULL,
    .luts = (const uint8_t*)e0470_night_prewipe_gl_data,
};
static const EpdWaveformPhases* e0470_night_prewipe_gl_ranges[] = { &e0470_night_prewipe_gl_phases };
static const EpdWaveformMode e0470_night_prewipe_gl_mode = {
    .type = 5, .temp_ranges = 1, .range_data = e0470_night_prewipe_gl_ranges,
};
static const EpdWaveformMode* e0470_night_prewipe_gl_modes[] = { &e0470_night_prewipe_gl_mode };
const EpdWaveform E0470_NIGHT_PREWIPE_GL_WAVEFORM = {
    .num_modes = 1, .num_temp_ranges = 1,
    .mode_data = e0470_night_prewipe_gl_modes, .temp_intervals = e0470_intervals,
};

static uint8_t e0470_direct_prewipe_data[E0470_DIRECT_PREWIPE_FRAMES][16][4];
static const EpdWaveformPhases e0470_direct_prewipe_phases = {
    .phases = E0470_DIRECT_PREWIPE_FRAMES, .phase_times = NULL,
    .luts = (const uint8_t*)e0470_direct_prewipe_data,
};
static const EpdWaveformPhases* e0470_direct_prewipe_ranges[] = { &e0470_direct_prewipe_phases };
static const EpdWaveformMode e0470_direct_prewipe_mode = {
    .type = 5, .temp_ranges = 1, .range_data = e0470_direct_prewipe_ranges,
};
static const EpdWaveformMode* e0470_direct_prewipe_modes[] = { &e0470_direct_prewipe_mode };
const EpdWaveform E0470_DIRECT_PREWIPE_WAVEFORM = {
    .num_modes = 1, .num_temp_ranges = 1,
    .mode_data = e0470_direct_prewipe_modes, .temp_intervals = e0470_intervals,
};

static uint8_t e0470_gray_direct_prewipe_data[E0470_GRAY_DIRECT_PREWIPE_FRAMES][16][4];
static const EpdWaveformPhases e0470_gray_direct_prewipe_phases = {
    .phases = E0470_GRAY_DIRECT_PREWIPE_FRAMES, .phase_times = NULL,
    .luts = (const uint8_t*)e0470_gray_direct_prewipe_data,
};
static const EpdWaveformPhases* e0470_gray_direct_prewipe_ranges[] = { &e0470_gray_direct_prewipe_phases };
static const EpdWaveformMode e0470_gray_direct_prewipe_mode = {
    .type = 5, .temp_ranges = 1, .range_data = e0470_gray_direct_prewipe_ranges,
};
static const EpdWaveformMode* e0470_gray_direct_prewipe_modes[] = { &e0470_gray_direct_prewipe_mode };
const EpdWaveform E0470_GRAY_DIRECT_PREWIPE_WAVEFORM = {
    .num_modes = 1, .num_temp_ranges = 1,
    .mode_data = e0470_gray_direct_prewipe_modes, .temp_intervals = e0470_intervals,
};

// 直刷族预驱：15→0为W18前导加完整18个DU黑动作；EE保持码走14→14行，W2微洗紧贴同一黑窗。
// Direct-family pre-drive: 15→0 is a W18 lead plus the complete 18 DU black actions;
// the EE hold code routes to the 14→14 row, whose W2 wash hugs the same black window.
static void e0470_night_prewipe_build(void) {
    // GL路径：整表复制夜间标准页表；15→0逐相拷厂家GL48的0→0对角线（白块落在原空闲相
    // 10-27，黑块28-45原位）；EE行微洗仅保留对角线末2相白加完整黑段。
    // GL path: copy the night standard page table; 15→0 takes the vendor GL48 0→0 diagonal
    // phase by phase (the white block lands on the former idle 10-27, black 28-45 in place);
    // the EE-row wash keeps only the diagonal's last two white phases plus its full black block.
    memcpy(e0470_night_prewipe_gl_data, e0470_page_gl16_data, sizeof(e0470_night_prewipe_gl_data));
    for (int phase = 0; phase < E0470_FULL_GL16_FRAMES; ++phase) {
        const int action = lut_get(e0470_full_gl16_data, phase, 0, 0);
        lut_set(e0470_night_prewipe_gl_data, phase, 0, 15, action);
        lut_set(e0470_night_prewipe_gl_data, phase, 14, 14,
                action == 2 && phase < E0470_FULL_GL16_FRAMES - 22 ? 0 : action);
    }
    // 直刷族：以各自成品表为底，15→0与EE行共用相18起的黑窗；0→0对角线保持零不被写入。
    // Direct family: start from each finished table; 15→0 and the EE row share the black window from phase 18;
    // the 0→0 diagonal stays zero because nothing writes it.
    memset(e0470_direct_prewipe_data, 0, sizeof(e0470_direct_prewipe_data));
    memcpy(e0470_direct_prewipe_data, e0470_direct_data, sizeof(e0470_direct_data));
    memset(e0470_gray_direct_prewipe_data, 0, sizeof(e0470_gray_direct_prewipe_data));
    memcpy(e0470_gray_direct_prewipe_data, e0470_gray_direct_data, sizeof(e0470_gray_direct_data));
    uint8_t (*dests[])[16][4] = { e0470_direct_prewipe_data, e0470_gray_direct_prewipe_data };
    for (unsigned t = 0; t < 2; ++t) {
        for (int phase = 0; phase < E0470_DU_TRAVEL_PULSES; ++phase) {
            const int action = lut_get(e0470_full_du_data, phase, 0, 15);
            lut_set(dests[t], phase + E0470_PREWIPE_DIRECT_LEAD, 0, 15, action);
            lut_set(dests[t], phase + E0470_PREWIPE_DIRECT_LEAD, 14, 14, action);
        }
        for (int lead = 0; lead < E0470_PREWIPE_DIRECT_LEAD; ++lead)
            lut_set(dests[t], lead, 0, 15, 2);
        lut_set(dests[t], E0470_PREWIPE_DIRECT_LEAD - 2, 14, 14, 2);
        lut_set(dests[t], E0470_PREWIPE_DIRECT_LEAD - 1, 14, 14, 2);
    }
}

static void e0470_gray_direct_build(void) {
    memset(e0470_gray_direct_data, 0, sizeof(e0470_gray_direct_data));
    // 白底降灰不按撤销行差值：厂家灰8对每个灰目标都用B10成形+W回拉，白→黑趋进约10脉冲
    // 即近饱和，0.5.36差值剂量（13-16脉冲）把灰缘压到近黑、抗锯齿梯度塌缩。白底剂量改用
    // 厂家锚点陡降曲线：4脉冲未出浅灰区（0.5.33实机欠驱）、10脉冲即成形位（to=1保持厂家
    // 成形深度）、18脉冲满黑（DU端点实测）。旧灰降灰仍用DU行差值（端点行真机在用），升向
    // 保持白侧差值（厂家W侧近似可加）；至少1脉冲，从相0同起点单向排布，端点厂家DU逐字复制。
    // White-background descent drops the undo-row differences: vendor gray8 forms every gray
    // target with B10 plus a W pull-back, so the white-to-black approach saturates near 10 pulses
    // and 0.5.36's difference doses (13-16 pulses) pushed fringes toward black, collapsing the
    // anti-aliasing gradient. White-background doses follow the vendor-anchored plunge curve:
    // 4 pulses still sit in the light region (0.5.33 under-dose on device), 10 pulses reach the
    // formation level (to=1 keeps the vendor formation depth) and 18 saturate black (device-proven
    // DU endpoint). Descent from old grays keeps the DU row differences (endpoint rows run on
    // device) and climbing keeps the white-side differences (near-additive per vendor data); at
    // least one pulse, laid one way from phase 0, with vendor DU endpoints verbatim.
    // 白底阶梯：偶数档纯主段，奇数档主段短一档并在相14补一个迟到脉冲——间隔后的单脉冲
    // 剂量弱于密集段，落点介于两档之间；厂家端点行本就采用"前导段+相17迟到脉冲"结构。
    // 若实机迟到脉冲等效满脉冲，落点退回上一版，不会更差。
    // White ladder: even targets run pure, odd targets run one shorter plus a late pulse at
    // phase 14 — a pulse after a rest doses less than one inside the dense run, landing between
    // the two integer doses; vendor endpoint rows already use leading-run-plus-late-tick form.
    // If the late pulse reads as a full pulse on hardware, landings fall back to the previous
    // version and never get worse.
    static const uint8_t white_descend[14] = {10, 9, 8, 8, 7, 7, 6, 6, 5, 5, 4, 4, 3, 3};
    static const bool white_late[14] = {false, false, true, false, true, false, true, false, true, false, true, false, false, false};
    // DU撤销计数表：并列档位（同一计数）在旧灰行里落点成对塌陷。0.5.41按"差值减并列
    // 深度"拆对实机反淡——减法把重叠区药量削薄；改回纯差值，只在降灰行并列组的最深档
    // 追加迟到脉冲（加法拆对，只加深不变浅），升向行回退纯差值。任何档都不会比0.5.40更浅。
    // DU undo counts: tied targets (equal counts) collapse in pairs on old-gray rows. 0.5.41
    // split them by subtracting tie depth and device strokes turned faint — subtraction
    // thinned overlap doses. Rows return to pure differences with one ADDITIVE late pulse on
    // the darkest member of each descent tie group (splitting upward only, never lighter),
    // and climbing reverts to pure differences. No target lands lighter than 0.5.40.
    int count_b[16] = {0}, count_w[16] = {0};
    for (int g = 0; g < 16; ++g)
        for (int phase = 0; phase < E0470_FULL_DU_FRAMES; ++phase) {
            count_b[g] += !!lut_get(e0470_full_du_data, phase, 0, g);
            count_w[g] += !!lut_get(e0470_full_du_data, phase, 15, g);
        }
    for (int to = 0; to < 16; ++to) {
        const bool endpoint = to == 0 || to == 15;
        for (int from = 0; from < 16; ++from) {
            // EE是选择性未变保持码；灰对角线也必须全部保持，不能把背景纳入灰阶擦写。
            // EE represents selective unchanged holds; every gray diagonal must hold as well, without admitting backgrounds to gray erasure.
            if (to == from) continue;
            if (endpoint) {
                // 端点逐字复制厂家DU（动作相0-17）。
                // Endpoints copy vendor DU verbatim (actions in phases 0-17).
                for (int phase = 0; phase < E0470_FULL_DU_FRAMES; ++phase)
                    lut_set(e0470_gray_direct_data, phase, to, from,
                            lut_get(e0470_full_du_data, phase, to, from));
                continue;
            }
            int pulses;
            bool late = false;
            if (from == 15) {
                // 白底陡降曲线，to=1..14 逐目标标定。
                // Plunge curve from white, per target to=1..14.
                pulses = white_descend[to - 1];
                late = white_late[to - 1];
            } else if (to < from) {
                pulses = count_b[from] - count_b[to];
                if (pulses < 1) pulses = 1;
                // 并列组最深档（更暗邻档计数不同、更浅邻档同计数）加迟到脉冲拆对。
                // The darkest member of a tie group (darker neighbor's count differs,
                // lighter neighbor shares the count) takes the late pulse to split pairs.
                late = count_b[to - 1] != count_b[to] && to + 1 <= 15 && count_b[to + 1] == count_b[to];
            } else {
                pulses = count_w[from] - count_w[to];
                if (pulses < 1) pulses = 1;
            }
            const int action = to < from ? 1 : 2;
            for (int phase = 0; phase < pulses; ++phase)
                lut_set(e0470_gray_direct_data, phase, to, from, action);
            if (late) lut_set(e0470_gray_direct_data, 14, to, from, action);
        }
    }
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
    e0470_gray_direct_build();
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
    // 预驱表以成品夜间页表/直刷表为底，必须在两者装配完后再拷贝。
    // Pre-drive tables copy the finished night page and direct tables, so they must build after both are assembled.
    e0470_night_prewipe_build();
}
