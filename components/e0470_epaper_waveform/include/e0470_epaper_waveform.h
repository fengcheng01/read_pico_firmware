/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 *
 * E0470A01（684×1216，40pin）的 epdiy 波形。
 * 自行调整屏幕波形会使设备失去保修。
 *   E0470_WAVEFORM         默认：GC16 48相，GL16 48相厂家动作+1相中性保持 + 50/50 阈值 DU
 *   E0470_FULL_WAVEFORM    GC16 / GL16 各 48 相，一个温度档 0-50°C
 *   E0470_GRAY8_WAVEFORM   8 灰阶：GC16 / GL16 各 30 相，灰阶少一半换来整屏约 360ms
 *   E0470_FOLLOW_WAVEFORM  跟随 DU：8 帧（黑 7 / 白 8），用于触摸笔迹
 * 默认与 8 灰阶波形挂阈值 DU（目标 0-7→黑、8-15→白）；波形保留只驱动 0/15 的源表。
 * 开机先调一次 e0470_waveform_init()。
 *
 * epdiy waveforms for the E0470A01 (684×1216, 40-pin).
 * Changing panel waveforms voids the warranty.
 *   E0470_WAVEFORM         Default: GC16 48; GL16 48 vendor phases + 1 neutral hold + 50/50 threshold DU
 *   E0470_FULL_WAVEFORM    GC16 / GL16 48 phases each, one 0–50°C temp range
 *   E0470_GRAY8_WAVEFORM   8-gray: GC16 / GL16 30 phases; half the steps, ~360 ms full
 *   E0470_FOLLOW_WAVEFORM  Follow DU: 8 frames (black 7 / white 8) for touch ink
 * Default and 8-gray hang a threshold DU (dest 0–7→black, 8–15→white); the
 * source tables still drive only 0/15. Gray tables retain all vendor phases.
 * Call e0470_waveform_init() once at boot.
 */

#pragma once

#include <stdint.h>

#include "epd_waveform.h"

#ifdef __cplusplus
extern "C" {
#endif

/// 完整灰阶表：GC16 / GL16 各 48 相，白推动梯子 17 档、去重后 11 级可分辨灰阶。
/// DU 是 20 相，只驱动目标 0/15；E0470_FULL_WAVEFORM 原样保留对照。
/// / Full gray tables: GC16 / GL16 48 phases, 17-step white-push ladder,
/// 11 distinguishable after collapse. DU is 20 phases, dest 0/15 only;
/// E0470_FULL_WAVEFORM is kept as a reference.
#define E0470_FULL_DU_FRAMES 20
#define E0470_FULL_GC16_FRAMES 48
#define E0470_FULL_GL16_FRAMES 48
extern const EpdWaveform E0470_FULL_WAVEFORM;

/// 另一份表：只有 8 级可分辨灰阶（净推动梯子 8 档），30 相，整屏约 360ms（默认表约 550ms）。
/// 灰阶档数 = 白推动梯子的档数，在这块屏上 16 级与 30 相不可兼得，所以它是备选不是默认。
/// / Alternate table: 8 distinguishable grays (8-step net push), 30 phases,
/// ~360 ms full (default ~550 ms). Gray count is the white-push ladder; 16
/// levels and 30 phases cannot both fit this panel, so this is optional.
#define E0470_GRAY8_GC16_FRAMES 30
#define E0470_GRAY8_GL16_FRAMES 30
extern const EpdWaveform E0470_GRAY8_WAVEFORM;

/// 日间标准转页：原厂GL16完整迁移与黑/灰对角线定稿，白白保持，尾部三相中性；不加推动。
/// Standard day turns retain every vendor GL16 transition and black/gray diagonal settling, hold white and end in three neutral phases without added drive.
/// 必须使用真实差分整页API；EE是厂家14→14迁移，不能作为选择性未变保持码。
/// Use the actual-difference full-page API; EE is vendor 14→14 settling and cannot represent a selective unchanged hold.
#define E0470_PAGE_GL16_FRAMES (E0470_FULL_GL16_FRAMES + 1)
extern const EpdWaveform E0470_TEXTTURN_WAVEFORM;
/// 夜间标准转页：变化逐相保留厂家GL16，全部对角线保持，配EE选择性整页API避免黑底整屏亮闪。
/// Standard night turns retain each changed vendor GL16 phase and hold every diagonal, paired with the selective EE page API to avoid a whole-screen light flash on black backgrounds.
extern const EpdWaveform E0470_TEXTTURN_NIGHT_WAVEFORM;

/// 普通导航重绘：完整厂家GL16，白白保持，保留黑/灰定稿。
/// Ordinary navigation redraws use full vendor GL16, holding white and retaining black/gray settling.
extern const EpdWaveform E0470_NAVIGATION_WAVEFORM;
/// 导航边界诊断对照：完整GL16仅对白白追加一相白动作，三相中性尾不变；产品布局入口与正文不调用。
/// Navigation-boundary diagnostic: full GL16 with one white-to-white action and unchanged neutral tails; product layout entries and body turns do not use it.
extern const EpdWaveform E0470_NAVIGATION_ENTRY_WAVEFORM;
/// 黑白直刷：厂家DU20相加一相中性；真实目标仅0/15，旧灰码完整保留，中间目标全部保持。
/// Black/white direct: all 20 vendor DU phases plus one neutral phase; actual targets are only 0/15, every old gray code is retained, and intermediate targets hold.
#define E0470_DIRECT_FRAMES (E0470_FULL_DU_FRAMES + 1)
extern const EpdWaveform E0470_DIRECT_WAVEFORM;

/// 最近墨迹动作掩码专用：FF擦白三相，其他码保持，再三相中性。/ Recent-ink action masks only: FF erases for three phases, others hold, then three neutral phases.
#define E0470_WHITE_CLEANUP_FRAMES 6
extern const EpdWaveform E0470_WHITE_CLEANUP_WAVEFORM;

/// 默认灰阶相数；裁剪器仅保留给诊断实验，不用于产品默认刷新。
/// Default gray phase counts; the trimmer remains for diagnostic experiments, outside product updates.
#define E0470_GC16_FRAMES E0470_FULL_GC16_FRAMES
#define E0470_GL16_FRAMES (E0470_FULL_GL16_FRAMES + 1)

/// 帧周期取算法文档第 8 节的设计点 ν≈90Hz。
/// / Frame period is the §8 design point ν≈90 Hz.
#define E0470_WAVEFORM_FRAME_US 11090
extern const EpdWaveform E0470_WAVEFORM;

/// 触摸笔迹跟手用的 8 帧短 DU，走 FAST 扫描（帧周期约 7ms）。
/// 推动次数按 |to-from| 比例分配，满幅迁移往黑推 7 相、往白推 8 相。
/// / 8-frame short DU for touch ink, FAST scan (~7 ms/frame). Push count
/// scales with |to-from|; full travel is 7 black / 8 white.
#define E0470_FOLLOW_BLACK_FRAMES 7
#define E0470_FOLLOW_WHITE_FRAMES 8
#define E0470_FOLLOW_FRAMES 8
extern const EpdWaveform E0470_FOLLOW_WAVEFORM;

/// 按上面的公式生成 frames 帧的跟随表写进 dst（容量 frames×16×4 字节）。
/// / Build a follow table of `frames` into dst (frames×16×4 bytes).
void e0470_follow_lut_build(int frames, uint8_t (*dst)[16][4]);

/// 在一条 EpdWaveform 里按模式（MODE_GC16 / MODE_GL16 / MODE_DU）找唯一温度档的相位表。
/// / Look up the single temp-range phase table for MODE_GC16 / MODE_GL16 / MODE_DU.
const EpdWaveformPhases* e0470_waveform_phases(const EpdWaveform* waveform, int mode);

/// 查 (from→to) 在第 phase 相的 2bit 动作：0 保持、1 压黑、2 擦白。
/// / 2-bit action for (from→to) at `phase`: 0 hold, 1 darken, 2 erase.
int e0470_phase_action(const EpdWaveformPhases* phases, int phase, int to, int from);

/// 生成 E0470_WAVEFORM 与 E0470_FOLLOW_WAVEFORM 的表。必须在第一次刷新之前调用一次。
/// / Build E0470_WAVEFORM and E0470_FOLLOW_WAVEFORM. Call once before the first refresh.
void e0470_waveform_init(void);

#ifdef __cplusplus
}
#endif
