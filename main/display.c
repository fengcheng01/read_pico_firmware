/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 * 冻结修订：0.7.0用户仅保留当前快速/细腻灰阶效果，撤回所有灰阶刷新实验；旧内部档均回原均衡。
 * 两生产表逐相不改，不新增分区/补刷，不改FULL时序、周期清理或失败恢复。
 * Frozen revision: User-approved0.7.0 retains current Fast/Fine Gray effects and withdraws gray-refresh experiments,
 * mapping old internal modes to original balance. Retain every phase of both production tables without new
 * bands/post-scans or FULL timing/cleanup/failure-recovery changes.
 * 冻结修订：用户授权日间纯文字28相均衡对照，独立正文档只推一次；周期清理与失败恢复保留。
 * Frozen revision: The user authorizes daytime text28-phase balance as a separate single-scan body mode, retaining cleanup and failure recovery.
 *
 * 冻结修订：用户授权可选日间纯文字动画柔化，保留原档位及49相/16带/节拍，仅交换白到灰脉冲对。
 * Frozen revision: User authorizes optional daytime text-animation softening, retaining original profiles and49 phases/16 bands/pacing while interleaving only white-to-gray pulse pairs.
 * 刷屏出口：按模式推屏、普通刷新后下电、跟手空闲下电、pclk 欠载回退。
 *
 * Present path: power down after normal updates, idle timeout for tracking, and pclk underrun recovery.
 *
 * 冻结：用户确认日间0.5.21效果并要求夜间优先无闪、按设置周期清理；实机GC残留设置框，当前夜间方案先物理清白再厂家GC16，日间产品正文按设置周期清理，0关闭，旧底层日间出口不单独计数；历史对照例外见下。
 * Frozen: The user accepts 0.5.21 daytime behavior and flash-free night turns with scheduled cleaning; hardware GC leaves setting outlines, so the current night profile physically clears before vendor GC16, with product day turns honoring the interval (0 disables it) and legacy day primitives uncounted; historical exceptions follow below.
 * 冻结：用户实测否定0.5.19并明确优先无闪速度；直刷使用厂家黑白DU及真实二值目标，当前标准模式保留灰阶。
 * Frozen: Device feedback rejects 0.5.19 and the user explicitly prioritizes flash-free speed; direct uses vendor black/white DU with actual binary targets, while current standard retains gray.
 * 冻结：旧参考保留实际灰码，不能伪造白基准；正文和普通导航白白保持，不恢复历史字形补擦。
 * Frozen: Prior references retain actual gray codes without fictional white baselines; body and ordinary navigation hold white without historical glyph cleanup.
 * 冻结：标准日间正文恢复厂家黑/灰对角线定稿，白白保持；日间布局入口GC16一次，当前夜间入口先物理清白，按钮和页脚不清理；成功清理重置夜间计数。
 * Frozen: Standard day body restores vendor black/gray diagonal settling and held white; day layout entries use one GC16 while current night entries physically clear first, controls/footer never clean, and successful cleaning resets night counts.
 * 冻结：普通夜间保持未变背景，只在用户设置周期到期清理；失败不累计，不伪造黑基准或交换厂家动作。
 * Frozen: Ordinary night turns hold unchanged backgrounds and clean only at the user-selected interval; failures never count, without fabricated black baselines or swapped vendor actions.
 * 冻结：用户要求继续探索Crossmux，本测试版可选其Pico DU20/入口GL37/周期和手动GC36；已知参考单扫不先清白，未知仍完整恢复，原档位不改。
 * Frozen: The user requests further Crossmux exploration; this test build optionally compares Pico DU20/entry GL37/due or manual GC36, with one known-baseline scan, full unknown recovery and unchanged original profiles.
 * 实机决策：用户要求移除Crossmux对照出口；对照函数与波形表删除，原两档（标准/白重画）不变。
 * Device decision: The user orders the Crossmux comparison exit removed; its function and waveform tables are deleted, leaving the original two profiles (standard/white repaint) unchanged.
 * 实机修订：0.5.25定向补黑和后置局部擦写未改善夜间残影，后者增加旧字闪动；撤回两实验，保留普通翻页与已有周期/手动完整清理。
 * Device revision: 0.5.25 black reinforcement and post-DU local cleaning failed to improve night ghosts, with local cleaning flashing old glyphs; withdraw both experiments and retain ordinary turns with existing interval/manual full cleaning.
 * 实机修订：0.5.29黑基准双GC清理后第一张静止页仍有残影且亮闪；撤回该实验，保留普通翻页和已有效的物理清白后厂家GC恢复。
 * Device revision: The first settled page after 0.5.29 dual-GC black-baseline cleaning still ghosts and flashes; withdraw the experiment, retaining ordinary turns and the effective physical-white-clear/vendor-GC recovery.
 * 冻结：不得恢复全黑双GC或将数字参考提交视为光学洁净；进一步黑底改善需要同板实测或本板校准波形证据。
 * Frozen: Do not restore dual-GC all-black cleaning or treat digital reference commits as optical cleanliness; further dark-background improvements require same-board measurements or panel-calibrated waveform evidence.
 * 修订：用户要求继续改进清理；独立可选白重画保留已有效的三轮物理清白，随后厂家GL灰阶或DU二值写回，避免清白后再GC擦写白字；普通翻页和未知恢复保持原策略。
 * Revision: The user requests further cleanup work; optional white repaint retains the effective three physical-clear rounds, then vendor GL grayscale or DU binary repainting to avoid re-erasing white glyphs with GC; ordinary turns and unknown recovery retain their existing policies.
 * 修订：用户要求继续改善直刷锯齿；独立可选灰缘保留真实目标灰码，使用厂家灰阶及DU端点，不修改原黑白档或伪造旧参考；实际边缘质量仍需同板验证。
 * Revision: The user requests further direct-edge improvement; optional gray edges retain actual target gray codes with vendor gray and DU endpoint transitions, without changing original binary direct or fabricating prior references; actual edge quality still needs same-board testing.
 * 实机修订：灰缘抗锯齿获用户确认但翻页先二值细字后闪成灰缘；灰边迁移现去前导空等左对齐相0与端点同步起扫，脉冲序列逐字保留，总相31→28。
 * Device revision: The user confirms gray-edge anti-aliasing but reports binary-first-then-gray turns; gray transitions now drop their leading idle, left-align to phase 0 beside the endpoints with byte-identical pulse sequences, 31 to 28 phases.
 * 实机修订：左对齐后仍先锯齿后抖动，厂家灰序列过冲再修是根因；灰边改按跟手DU公式单调同向逼近（见波形组件），灰缘表21相与黑白直刷同速，出口契约不变。
 * Device revision: After left-alignment the jagged-then-shake look remains, rooted in vendor overshoot-then-trim gray sequences; gray edges now approach monotonically per the follow-DU formula (see the waveform component), the gray table at 21 phases as fast as binary direct, with the exit contract unchanged.
 * 修订：用户继续要求改善夜间黑底残影；可选白预驱把15→0改为先白后黑（GL逐相拷厂家0→0对角线、直刷族W6前导），白像素加白不可见；除残影效果须同板实测，清理与日间路径不变。
 * Revision: The user keeps asking for night black-background relief; the optional white pre-drive turns 15→0 into white-then-black (GL copies the vendor 0→0 diagonal phase by phase, the direct family uses a W6 lead), invisible on white pixels; ghost relief needs same-board proof while cleaning and day paths stay unchanged.
 * 实机修订：残影仍在；白预驱升级为厂家对角线满剂量（直刷族同样W18+B18）并叠加保持黑底W2微洗（见波形组件），开关语义与生效范围不变。
 * Device revision: Ghosts remain; the white pre-drive upgrades to the full vendor diagonal dose (W18+B18 on the direct family too) plus a W2 wash on held black (see the waveform component), with the switch semantics and scope unchanged.
 * 实机修订：开关两态残影无差；根因是选择性编码把一切未变像素发EE，渲染器按码字节查14→14行，写在0→0行的微洗从未被驱动。
 * 现微洗移到14→14行，并配夜间保持掩码：目标非纯黑的未变像素发FF走15→15保持行，黑底独享重饱和，正文灰缘不受洗。
 * Device revision: Ghosts show no switch difference; the selective encoder emits EE for every unchanged pixel and the renderer indexes the 14→14 row by that byte, so the wash written on 0→0 was never driven.
 * The wash now sits on 14→14 with a night hold mask: unchanged non-pure-black targets take FF into the 15→15 hold row, so only the background gets re-saturated and text fringes stay out of the wash.
 * 实机决策（暂时）：多轮夜间残影调优未果，用户要求暂时下线夜间模式；入口移除、存量设置不生效恒为日间（见app_book.c与settings.c），本文件夜间机制保留待恢复。
 * Device decision (temporary): after repeated failed night tuning the user orders night mode withdrawn; entries are removed and stored settings no longer apply, always rendering day (see app_book.c and settings.c), while this file's night machinery awaits restoration.
 * 冻结修订：用户实机确认两种日间模式残影，产品正文统一出口覆盖旧日间不计周期决定；
 * 标准、快刷、灰缘与动画共享成功翻页周期，到期只输出一次厂家GC16目标页，不追加独立擦白。
 * Frozen revision: Hardware feedback authorizes a day body entry overriding the former day-cycle exclusion;
 * standard, fast, gray-edge and animated turns share successful-turn intervals, using one vendor GC16 target without a white prepass.
 * 冻结修订：用户明确保留当前速度与翻页观感；撤回0.6.6覆盖率补偿，但仍保留21相灰缘，不切完整灰阶、不增补刷或驱动剂量。
 * Frozen revision: The user explicitly preserves speed and turn appearance; withdraw0.6.6 coverage compensation but retain21-phase gray edges without full-gray fallback, added scans or doses.
 */

#include "display.h"
#include "display_pixels.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "app_config.h"
#include "e0470_epaper_waveform.h"
#include "e0470_page_turn.h"
#include "display_animation.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "settings.h"
#include "read_pico_board.h"

static const char* TAG = "read_pico";
static bool s_bulk_io;
// 入口标记仅由布局切换设置，失败时保留到成功的整页呈现。
// Layout transitions arm the entry marker, retained until successful whole-page presentation.
static bool s_navigation_entry;
void display_request_navigation_settle(void) { s_navigation_entry = true; }

void display_set_bulk_io(bool active) {
    s_bulk_io = active;
    ESP_LOGI(TAG, "bulk I/O scan margin %s", active ? "on" : "off");
}

// 跟手与正文翻页共用闲置保活窗口；其余页面不保留静态 VCOM。/ Tracking and body turns share the idle keepalive window; other pages never retain static VCOM.
#define RAILS_IDLE_TIMEOUT_MS 8000
// 0 表示轨道已断电；否则是到期时间（ms），到点后主循环断电。
// 0 means the rails are off; otherwise a deadline (ms) after which the loop powers them down.
static int64_t rails_deadline_ms;

void rails_keepalive(void) {
    rails_deadline_ms = esp_timer_get_time() / 1000 + RAILS_IDLE_TIMEOUT_MS;
}

// 正文翻页波形集合：TEXTTURN/DIRECT/GRAY_DIRECT 及夜间/预驱变体。
// Body-turn waveform family: TEXTTURN/DIRECT/GRAY_DIRECT plus night/pre-drive variants.
static bool body_turn_waveform(const EpdWaveform* waveform) {
    return waveform == &E0470_TEXTTURN_WAVEFORM || waveform == &E0470_TEXTTURN_NIGHT_WAVEFORM ||
        waveform == &E0470_DIRECT_WAVEFORM || waveform == &E0470_GRAY_DIRECT_WAVEFORM || waveform == &E0470_GRAY_BALANCED_WAVEFORM ||
        waveform == &E0470_NIGHT_PREWIPE_GL_WAVEFORM || waveform == &E0470_DIRECT_PREWIPE_WAVEFORM ||
        waveform == &E0470_GRAY_DIRECT_PREWIPE_WAVEFORM;
}

// 决策变更（2026-10-17，二轮）：实机日志实测每次翻页/翻屏 display≈950-1250ms，其中断电保持
// 500ms 加上电≈590ms 是纯电源时序，扫描仅≈240ms。所有推屏统一走 8 秒闲置保活（原仅触摸
// 跟手享有）：连续交互不再逐屏上下电，静置到期仍按原序列断电防静态电压漂移；睡眠、锁屏、
// 关机与上电失败路径保持立即断电。
// Decision change (2026-10-17, second round): device logs measured ~950-1250ms per refresh,
// ~590ms of it pure power sequencing (the 500ms power-off hold plus power-up) against a ~240ms
// scan. Every update now shares the 8-second idle keepalive (formerly touch-only): continuous
// interaction stops cycling rails per refresh, while the idle deadline still runs the full off
// sequence against static-voltage drift; sleep, lock, shutdown and failed power-on paths keep
// powering off immediately.
static void finish_update(const EpdWaveform* waveform) {
    (void)waveform;
    rails_keepalive();
}

void rails_idle_check(int64_t now_ms) {
    if (rails_deadline_ms != 0 && now_ms >= rails_deadline_ms) {
        rails_deadline_ms = 0;
        epd_poweroff();
    }
}

// 20 相完整/原厂 DU 按厂家时序走 FULL；只有触摸笔迹用的 8 帧跟随 DU 走 FAST。
// 20-phase full / vendor DU uses FULL timing; the 8-frame FOLLOW DU used for ink trails uses FAST.
static void use_scan_for(const EpdWaveform* waveform, enum EpdDrawMode mode) {
    const bool fast = (mode & 0xF) == MODE_DU && waveform == &E0470_FOLLOW_WAVEFORM;
    read_pico_epd_use_scan(fast ? READ_PICO_EPD_SCAN_FAST : READ_PICO_EPD_SCAN_FULL);
    // 高层刷新保持整屏扫描；两条63行可用队列在第127行入队前启动。
    // High-level updates scan the full panel; two 63-slot queues start before line 127 is enqueued.
    epd_lcd_set_prefill_lines(fast ? 64 : 127);
}

// 自上次 GC16 以来的差分刷（DU/GL16）次数。
// Soft (DU/GL16) updates since the last GC16.
static int s_soft_refreshes;
// 夜间两效果共用成功翻页计数，实际整屏清理成功后归零。
// Both night effects share successful turns, reset after successful actual whole-screen cleaning.
static unsigned s_night_body_turns;
static unsigned s_day_body_turns;
static uint32_t s_clean_generation;
static bool s_baseline_unknown;

// 夜间保持掩码（1位/像素，目标非纯黑置位）：选择编码把掩码白位发FF走15→15保持行，
// 其余未变像素发EE走14→14微洗行，正文与灰缘因此不参与黑底重饱和。
// Night hold mask (1 bit/pixel, set for non-pure-black targets): the selective encoder
// turns masked whites into FF on the 15→15 hold row, while other unchanged pixels take
// EE into the 14→14 wash row, keeping text and gray fringes out of background re-saturation.
static uint8_t* s_night_hold_mask;

static const uint8_t* display_night_hold_mask(const uint8_t* front_fb) {
    const size_t mask_bytes = (size_t)epd_width() * epd_height() / 8;
    if (!s_night_hold_mask)
        s_night_hold_mask = heap_caps_malloc(mask_bytes, MALLOC_CAP_SPIRAM);
    if (!s_night_hold_mask) return NULL;
    for (size_t byte = 0; byte < mask_bytes; ++byte) {
        const uint8_t* quad = front_fb + byte * 4;
        uint8_t bits = 0;
        for (int nibble = 0; nibble < 8; ++nibble)
            if ((quad[nibble / 2] >> ((nibble % 2) * 4) & 15) != 0) bits |= (uint8_t)(1u << nibble);
        s_night_hold_mask[byte] = bits;
    }
    return s_night_hold_mask;
}

static bool night_prewipe_waveform(const EpdWaveform* waveform) {
    return waveform == &E0470_NIGHT_PREWIPE_GL_WAVEFORM || waveform == &E0470_DIRECT_PREWIPE_WAVEFORM ||
        waveform == &E0470_GRAY_DIRECT_PREWIPE_WAVEFORM;
}

// 上电失败不扫描、不提交虚假的成功参考帧；重试沿用未知基准恢复。
// Failed power-on must not scan or commit a false successful baseline; retries use unknown-baseline recovery.
static bool power_ready(void) {
    epd_poweron();
    if (read_pico_rails_on()) return true;
    s_baseline_unknown = true;
    rails_deadline_ms = 0;
    epd_poweroff();
    return false;
}

// 整页 render 也可能只改一个按钮；按真实差分范围识别，不能因此消耗页级清理次数。
// A whole-page render may change one button only; use actual difference bounds so it cannot consume page cleanup counts.
static bool changed_page(const EpdiyHighlevelState* hl) {
    const int width = epd_width() / 2, height = epd_height();
    int x0 = width, x1 = -1, y0 = height, y1 = -1;
    for (int y = 0; y < height; ++y) {
        const uint8_t* to = hl->front_fb + (size_t)y * width;
        const uint8_t* from = hl->back_fb + (size_t)y * width;
        if (!memcmp(to, from, (size_t)width)) continue;
        if (y0 == height) y0 = y;
        y1 = y;
        for (int x = 0; x < width; ++x) if (to[x] != from[x]) {
            if (x < x0) x0 = x;
            if (x > x1) x1 = x;
        }
    }
    return (y1 - y0 + 1) * 2 >= height && (x1 - x0 + 1) * 2 >= width;
}

// 仅整页刷新参与周期清理，局部控件不得扩大成整屏黑闪。
// Only page updates count toward scheduled cleanup; local controls must never expand into a full-screen flash.
// 跟随 DU 专用于跟手，不参与页级清理计数。/ FOLLOW DU is for live tracking and excluded from page cleanup counting.
static enum EpdDrawError hl_update(
    EpdiyHighlevelState* hl, const EpdWaveform* waveform, enum EpdDrawMode mode, bool full,
    const EpdRect* area, bool night_body_turn, bool physical_clean
) {
    const bool is_full_screen = area == NULL ||
        (area->x == 0 && area->y == 0 && area->width >= epd_width() && area->height >= epd_height());
    const bool body_profile = body_turn_waveform(waveform);
    night_body_turn = night_body_turn && is_full_screen && body_profile;
    const unsigned night_every = night_body_turn ? app_settings_gc_every() : 0;
    // 日间标准定稿黑/灰像素；夜间标准和黑白直刷用选择码保持全部未变像素。
    // Standard day settles black/gray pixels; standard night and binary direct use selectors to hold every unchanged pixel.
    full = full || (mode & 0xF) == MODE_GC16 || waveform == &E0470_NAVIGATION_WAVEFORM || waveform == &E0470_NAVIGATION_ENTRY_WAVEFORM ||
           body_profile;
    bool promoted = false;
    physical_clean = physical_clean || s_baseline_unknown;
    if (physical_clean) {
        mode = (enum EpdDrawMode)((mode & ~0xF) | MODE_GC16);
        epd_hl_waveform(hl, &E0470_FULL_WAVEFORM);
        use_scan_for(&E0470_FULL_WAVEFORM, mode);
        area = NULL;
        full = promoted = true;
        s_soft_refreshes = 0;
    }
    if (night_body_turn && (mode & 0xF) != MODE_GC16 && night_every > 0 && s_night_body_turns + 1 >= night_every) {
        mode = (enum EpdDrawMode)((mode & ~0xF) | MODE_GC16);
        epd_hl_waveform(hl, &E0470_FULL_WAVEFORM);
        use_scan_for(&E0470_FULL_WAVEFORM, mode);
        promoted = full = true;
        area = NULL;
        physical_clean = true;
    }
    // 光学残影超出旧灰码参考；只有实际清白后才可建立白基准，正文目标始终保留。
    // Optical ghosts exceed the prior gray-code reference; establish white only after an actual clear, retaining the body target throughout.
    if (physical_clean) {
        epd_clear();
        memset(hl->back_fb, 255, (size_t)epd_width() * epd_height() / 2);
    }
    const bool page_update = is_full_screen &&
        (area != NULL || (mode & 0xF) == MODE_GC16 || body_profile || changed_page(hl));
    if (page_update && waveform != &E0470_FOLLOW_WAVEFORM) {
        unsigned every = app_settings_gc_every();
        if ((mode & 0xF) == MODE_GC16) {
            s_soft_refreshes = 0;
        } else if (waveform != &E0470_NAVIGATION_WAVEFORM && waveform != &E0470_NAVIGATION_ENTRY_WAVEFORM && !body_profile && every > 0 &&
                   (unsigned)++s_soft_refreshes >= every) {
            s_soft_refreshes = 0;
            mode = (enum EpdDrawMode)((mode & ~0xF) | MODE_GC16);
            epd_hl_waveform(hl, &E0470_FULL_WAVEFORM);
            use_scan_for(&E0470_FULL_WAVEFORM, mode);
            promoted = true;
            area = NULL;
            full = true;
            ESP_LOGI(TAG, "promote to GC16 after %u soft refreshes", every);
        }
    }
    enum EpdDrawError result;
    if (area != NULL && !is_full_screen) result = full ? epd_hl_update_area_full(hl, mode, 25, *area)
                                       : epd_hl_update_area(hl, mode, 25, *area);
    else if ((waveform == &E0470_DIRECT_WAVEFORM || waveform == &E0470_TEXTTURN_NIGHT_WAVEFORM || waveform == &E0470_GRAY_DIRECT_WAVEFORM || waveform == &E0470_GRAY_BALANCED_WAVEFORM ||
              waveform == &E0470_NIGHT_PREWIPE_GL_WAVEFORM || waveform == &E0470_DIRECT_PREWIPE_WAVEFORM ||
              waveform == &E0470_GRAY_DIRECT_PREWIPE_WAVEFORM) && (mode & 0xF) != MODE_GC16)
        result = epd_hl_update_screen_selective(hl, mode, 25,
            night_prewipe_waveform(waveform) ? display_night_hold_mask(hl->front_fb) : NULL);
    else result = full ? epd_hl_update_screen_full(hl, mode, 25) : epd_hl_update_screen(hl, mode, 25);
    s_baseline_unknown = result != EPD_DRAW_SUCCESS;
    // 按实际扫描结果提交计数：恢复GC也归零，失败或普通控件不得推进。
    // Commit counts from the actual scan: recovery GC resets them, while failures and ordinary controls never advance them.
    if (result == EPD_DRAW_SUCCESS) {
        if ((mode & 0xF) == MODE_GC16 && full && (area == NULL || is_full_screen)) {
            s_night_body_turns = s_day_body_turns = 0;
            ++s_clean_generation;
        }
        else if (night_body_turn) s_night_body_turns = night_every ? s_night_body_turns + 1 : 0;
    }
    // 局部出口的未知基准恢复也已整屏清理，不能遗留入口补偿给下一次后台重绘。
    // Unknown-baseline recovery through a local API also settles the whole screen; never leak its entry marker into a later background redraw.
    if (result == EPD_DRAW_SUCCESS && full && (area == NULL || is_full_screen)) s_navigation_entry = false;
    if (promoted) epd_hl_waveform(hl, waveform);
    return result;
}

enum EpdDrawError update_display_mode(
    EpdiyHighlevelState* hl, enum EpdDrawMode mode
) {
    if ((mode & 0xF) == MODE_GC16)
        return update_display_with(hl, &E0470_FULL_WAVEFORM, mode);
    // 产品GL16恢复厂家迁移；局部控件仍只更新指定区域。
    // Product GL16 restores vendor transitions; controls still update bounded areas.
    if ((mode & 0xF) == MODE_GL16)
        return update_display_with(hl, s_navigation_entry ? &E0470_NAVIGATION_WAVEFORM : &E0470_WAVEFORM, mode);
    use_scan_for(&E0470_WAVEFORM, mode);
    if (!power_ready()) return EPD_DRAW_POWER_NOT_READY;
    enum EpdDrawError result = hl_update(hl, &E0470_WAVEFORM, mode, false, NULL, false, false);
    finish_update(&E0470_WAVEFORM);
    return result;
}

enum EpdDrawError update_display_from_white_with(
    EpdiyHighlevelState* hl, const EpdWaveform* waveform, enum EpdDrawMode mode
) {
    use_scan_for(waveform, mode);
    if (!power_ready()) return EPD_DRAW_POWER_NOT_READY;
    epd_hl_waveform(hl, waveform);
    enum EpdDrawError result = epd_hl_update_screen_from_white(hl, mode, 25);
    s_baseline_unknown = result != EPD_DRAW_SUCCESS;
    if (result == EPD_DRAW_SUCCESS) {
        s_navigation_entry = false;
        if ((mode & 0xF) == MODE_GC16) {
            s_night_body_turns = s_day_body_turns = 0; ++s_clean_generation;
        }
    }
    epd_hl_waveform(hl, &E0470_WAVEFORM);
    s_soft_refreshes = 0;
    finish_update(waveform);
    return result;
}

enum EpdDrawError update_display_from_white(EpdiyHighlevelState* hl) {
    return update_display_from_white_with(hl, &E0470_FULL_WAVEFORM, MODE_GC16);
}

enum EpdDrawError display_boot_white(EpdiyHighlevelState* hl) {
    // 冷启动面板内容未知：物理清屏是唯一不依赖波形表差分的铺白方式（真机验证过），
    // 软件前后缓冲同时归白，首帧内容与白基准做差分。
    // At cold boot the panel state is unknown: the physical clear is the only
    // whitener that needs no waveform diff (device-proven). Both software
    // buffers go white so the first content frame diffs against white.
    size_t bytes = (size_t)epd_width() * epd_height() / 2;
    use_scan_for(&E0470_FULL_WAVEFORM, MODE_GC16);
    if (!power_ready()) return EPD_DRAW_POWER_NOT_READY;
    memset(hl->front_fb, 255, bytes);
    memset(hl->back_fb, 255, bytes);
    epd_clear();
    epd_hl_waveform(hl, &E0470_WAVEFORM);
    s_soft_refreshes = 0;
    s_night_body_turns = 0;
    s_baseline_unknown = false;
    s_navigation_entry = false;
    finish_update(&E0470_FULL_WAVEFORM);
    return EPD_DRAW_SUCCESS;
}

enum EpdDrawError update_display_white(EpdiyHighlevelState* hl) {
    epd_hl_set_all_white(hl);
    return update_display_full(hl);
}

static bool s_white_exit;

void display_hold_white_exit(bool hold) {
    s_white_exit = hold;
}

bool display_take_white_exit(void) {
    const bool hold = s_white_exit;
    s_white_exit = false;
    return hold;
}

enum EpdDrawError update_display_full(EpdiyHighlevelState* hl) {
    return update_display_with(hl, &E0470_FULL_WAVEFORM, MODE_GC16);
}

// 错相揭页动画：整页共用一次差分，16 条带一拍一带错开启动，每像素仍走完整 GL16 相位序列。
// Staggered page-turn animation: one shared whole-page difference, 16 bands launched one per tick, every pixel keeping its full GL16 phase sequence.
enum EpdDrawError update_display_page_turn(EpdiyHighlevelState* hl, EpdRect area, bool forward) {
    const EpdWaveform* waveform = &E0470_TEXTTURN_WAVEFORM;
    // 未知基准或入口补偿交给原文字翻页：动画路径只做差分，不做物理清白。
    // Unknown baselines and entry compensation belong to the original text turn: the animation only diffs, never physically clears.
    if (s_baseline_unknown || s_navigation_entry) return update_display_text_turn(hl, false);
    read_pico_epd_use_scan(READ_PICO_EPD_SCAN_FAST);
    epd_lcd_set_prefill_lines(64);
    if (!power_ready()) {
        use_scan_for(waveform, MODE_GL16);
        return EPD_DRAW_POWER_NOT_READY;
    }
    if (app_settings_book_turn_style() == 2) waveform = display_animation_waveform();
    e0470_page_turn_set_tick_us(E0470_TURN_FAST_TICK_US);
    enum EpdDrawError result = e0470_page_turn_with_waveform_compact(
        hl, area, forward ? E0470_TURN_RTL : E0470_TURN_LTR, waveform, 16);
    display_animation_release();
    waveform = &E0470_TEXTTURN_WAVEFORM;
    use_scan_for(waveform, MODE_GL16);
    if (result != EPD_DRAW_SUCCESS) {
        // 引擎失败不回写基准；原出口从真实旧基准完整重放这一翻。
        // The engine never commits the baseline on failure; the original path replays the whole turn from the real old baseline.
        s_baseline_unknown = true;
        return update_display_text_turn(hl, false);
    }
    s_baseline_unknown = false;
    s_navigation_entry = false;
    // 日间正文翻页不消耗通用页面周期，与 update_display_text_turn 的日间契约一致。
    // Day body turns never consume the generic page cycle, matching update_display_text_turn's day contract.
    finish_update(waveform);
    return result;
}

// 指定波形整屏刷一次，刷完把默认波形装回去。用来 A/B 两条灰阶表。
// Present the whole screen with a given waveform, then restore the default. Used to A/B two gray tables.
static enum EpdDrawError update_display_with_profile(
    EpdiyHighlevelState* hl, const EpdWaveform* waveform, enum EpdDrawMode mode, bool night_body_turn, bool physical_clean
) {
    // 日间入口保留真实旧帧；夜间入口清白后统一重画，避免设置框旧灰码留下轮廓。
    // Day entries retain actual prior frames; night entries clear before a uniform redraw so old setting gray codes cannot retain outlines.
    const bool clean_entry = (waveform == &E0470_NAVIGATION_WAVEFORM ||
        waveform == &E0470_TEXTTURN_WAVEFORM || waveform == &E0470_TEXTTURN_NIGHT_WAVEFORM ||
        waveform == &E0470_DIRECT_WAVEFORM || waveform == &E0470_GRAY_DIRECT_WAVEFORM || waveform == &E0470_GRAY_BALANCED_WAVEFORM ||
        waveform == &E0470_NIGHT_PREWIPE_GL_WAVEFORM || waveform == &E0470_DIRECT_PREWIPE_WAVEFORM ||
        waveform == &E0470_GRAY_DIRECT_PREWIPE_WAVEFORM) &&
        (mode & 0xF) == MODE_GL16 && s_navigation_entry;
    const EpdWaveform* applied = clean_entry ? &E0470_FULL_WAVEFORM : waveform;
    if (clean_entry) mode = (enum EpdDrawMode)((mode & ~0xF) | MODE_GC16);
    physical_clean = physical_clean || (clean_entry &&
        (waveform == &E0470_TEXTTURN_NIGHT_WAVEFORM || night_body_turn));
    use_scan_for(applied, mode);
    if (!power_ready()) return EPD_DRAW_POWER_NOT_READY;
    epd_hl_waveform(hl, applied);
    enum EpdDrawError result = hl_update(hl, applied, mode, false, NULL, night_body_turn, physical_clean);
    epd_hl_waveform(hl, &E0470_WAVEFORM);
    finish_update(applied);
    return result;
}

enum EpdDrawError update_display_with(
    EpdiyHighlevelState* hl, const EpdWaveform* waveform, enum EpdDrawMode mode
) {
    return update_display_with_profile(hl, waveform, mode, false, false);
}

enum EpdDrawError update_display_clean(EpdiyHighlevelState* hl) {
    return update_display_with_profile(hl, &E0470_FULL_WAVEFORM, MODE_GC16, false, true);
}

// 日间厂家GL16定稿，夜间普通保持并与直刷共用用户清理周期；夜间白预驱开时换预驱表。
// Day uses vendor GL16 settling; night ordinarily holds and shares the user cleanup interval with direct,
// switching to the pre-drive table when the night white pre-drive experiment is on.
enum EpdDrawError update_display_text_turn(EpdiyHighlevelState* hl, bool white_on_black) {
    const EpdWaveform* waveform = white_on_black
        ? (app_settings_book_night_prewipe() ? &E0470_NIGHT_PREWIPE_GL_WAVEFORM : &E0470_TEXTTURN_NIGHT_WAVEFORM)
        : &E0470_TEXTTURN_WAVEFORM;
    return update_display_with_profile(hl, waveform, MODE_GL16, white_on_black, false);
}

enum EpdDrawError update_display_text_direct(EpdiyHighlevelState* hl, bool white_on_black) {
    // 按用户速度取舍提交真实黑白目标；厂家DU仍读取实际旧灰帧，夜间预驱开时换预驱表。
    // Commit actual black/white targets per the user's speed preference; vendor DU still
    // reads the real prior gray frame, switching to the pre-drive table at night when enabled.
    display_prepare_direct_frame(hl->front_fb, epd_width(), epd_height(), white_on_black);
    const EpdWaveform* waveform = white_on_black && app_settings_book_night_prewipe()
        ? &E0470_DIRECT_PREWIPE_WAVEFORM : &E0470_DIRECT_WAVEFORM;
    return update_display_with_profile(hl, waveform, MODE_GL16, white_on_black, false);
}

enum EpdDrawError update_display_text_gray_direct(EpdiyHighlevelState* hl, bool white_on_black) {
    // 灰缘按DU中灰源行差值剂量单向直驱并复用端点DU，目标保留原灰码，按正文契约计数。
    // Gray edges drive one way with marginal doses from the DU mid-gray source rows beside endpoint DU,
    // retaining target gray codes and body counting.
    const EpdWaveform* waveform = white_on_black && app_settings_book_night_prewipe()
        ? &E0470_GRAY_DIRECT_PREWIPE_WAVEFORM : &E0470_GRAY_DIRECT_WAVEFORM;
    return update_display_with_profile(hl, waveform, MODE_GL16, white_on_black, false);
}

enum EpdDrawError update_display_reader_turn(EpdiyHighlevelState* hl, display_reader_mode_t mode,
                                            EpdRect area, bool forward) {
    if (mode < DISPLAY_READER_STANDARD || mode > DISPLAY_READER_GRAY_COMPACT) return EPD_DRAW_INVALID_CROP;
    unsigned every = app_settings_gc_every();
    bool due = every && s_day_body_turns >= every - 1;
    uint32_t generation = s_clean_generation;
    // 二值目标在周期清理时也保持二值，避免清理页与随后快刷粗细跳变。
    // Keep binary targets binary even at cleanup, preventing weight changes between cleanup and fast pages.
    if (mode == DISPLAY_READER_BINARY)
        display_prepare_direct_frame(hl->front_fb, epd_width(), epd_height(), false);
    enum EpdDrawError result;
    if (due) result = update_display_full(hl);
    else switch (mode) {
        case DISPLAY_READER_STANDARD: result = update_display_text_turn(hl, false); break;
        case DISPLAY_READER_BINARY: result = update_display_text_direct(hl, false); break;
        case DISPLAY_READER_GRAY: result = update_display_text_gray_direct(hl, false); break;
        case DISPLAY_READER_GRAY_BALANCED: result = update_display_with_profile(hl, &E0470_GRAY_BALANCED_WAVEFORM, MODE_GL16, false, false); break;
        case DISPLAY_READER_GRAY_STAGGERED: result = update_display_with_profile(hl, &E0470_GRAY_BALANCED_WAVEFORM, MODE_GL16, false, false); break;
        case DISPLAY_READER_GRAY_SYNC: result = update_display_with_profile(hl, &E0470_GRAY_BALANCED_WAVEFORM, MODE_GL16, false, false); break;
        case DISPLAY_READER_GRAY_MILD: result = update_display_with_profile(hl, &E0470_GRAY_BALANCED_WAVEFORM, MODE_GL16, false, false); break;
        case DISPLAY_READER_GRAY_COMPACT: result = update_display_with_profile(hl, &E0470_GRAY_BALANCED_WAVEFORM, MODE_GL16, false, false); break;
        default: result = update_display_page_turn(hl, area, forward); break;
    }
    // 成功的手动/入口/故障恢复全刷已统一清零；失败不推进，到期失败留给下一次重试。
    // Manual/entry/recovery GC already reset the count; failures never advance it, retaining due retries.
    if (result == EPD_DRAW_SUCCESS && generation == s_clean_generation)
        s_day_body_turns = every ? s_day_body_turns + 1 : 0;
    return result;
}

enum EpdDrawError update_display_night_white_repaint(
    EpdiyHighlevelState* hl, display_night_action_t action, bool direct, bool gray_direct
) {
    gray_direct = direct && gray_direct;
    // 二值档仍提交真实端点；灰缘档不得量化正文覆盖率。/ Binary direct retains actual endpoints; gray edges must never quantize body coverage.
    if (direct && !gray_direct)
        display_prepare_direct_frame(hl->front_fb, epd_width(), epd_height(), app_settings_book_night());
    if (!app_settings_book_night()) {
        // 防止误从日间调用新清理；日间仍沿用原导航与定稿出口。
        // Guard accidental day calls to the new cleaning route; preserve original day navigation and settling.
        if (action == DISPLAY_NIGHT_CLEAN) return update_display_full(hl);
        if (action == DISPLAY_NIGHT_ENTRY) display_request_navigation_settle();
        if (gray_direct) return update_display_text_gray_direct(hl, false);
        return direct ? update_display_text_direct(hl, false) : update_display_text_turn(hl, false);
    }
    const bool turn = action == DISPLAY_NIGHT_TURN;
    const bool entry = action == DISPLAY_NIGHT_ENTRY || s_navigation_entry;
    const unsigned every = app_settings_gc_every();
    const bool clean = action == DISPLAY_NIGHT_CLEAN || entry ||
        (turn && every && s_night_body_turns + 1 >= every);
    if (s_baseline_unknown) {
        // 失败后的屏状态未知，保留原物理清白加完整GC恢复，不把失败后的灰码当真实参考。
        // Unknown panel state after failure retains physical-clear/full-GC recovery without trusting failed gray codes.
        return update_display_with_profile(hl, &E0470_FULL_WAVEFORM, MODE_GC16, false, true);
    }
    const EpdWaveform* ordinary = direct ?
        (gray_direct ? (app_settings_book_night_prewipe() ? &E0470_GRAY_DIRECT_PREWIPE_WAVEFORM : &E0470_GRAY_DIRECT_WAVEFORM)
                     : (app_settings_book_night_prewipe() ? &E0470_DIRECT_PREWIPE_WAVEFORM : &E0470_DIRECT_WAVEFORM))
        : (app_settings_book_night_prewipe() ? &E0470_NIGHT_PREWIPE_GL_WAVEFORM : &E0470_TEXTTURN_NIGHT_WAVEFORM);
    const EpdWaveform* waveform = clean ? &E0470_FULL_WAVEFORM : ordinary;
    const enum EpdDrawMode mode = clean && direct && !gray_direct ? MODE_DU : MODE_GL16;
    use_scan_for(waveform, mode);
    if (!power_ready()) return EPD_DRAW_POWER_NOT_READY;
    epd_hl_waveform(hl, waveform);
    if (clean) {
        // 目标始终留在前缓冲；仅实际三轮清白结束后提交白参考，随后单次写回。
        // Retain the target front buffer; commit white only after the actual three-round clear, then repaint once.
        epd_clear();
        memset(hl->back_fb, 255, (size_t)epd_width() * epd_height() / 2);
    }
    enum EpdDrawError result = clean ? epd_hl_update_screen_full(hl, mode, 25) :
        epd_hl_update_screen_selective(hl, mode, 25,
            night_prewipe_waveform(waveform) ? display_night_hold_mask(hl->front_fb) : NULL);
    s_baseline_unknown = result != EPD_DRAW_SUCCESS;
    if (result == EPD_DRAW_SUCCESS) {
        if (clean) {
            s_night_body_turns = 0;
            s_soft_refreshes = 0;
            s_navigation_entry = false;
        } else if (turn) s_night_body_turns = every ? s_night_body_turns + 1 : 0;
    }
    epd_hl_waveform(hl, &E0470_WAVEFORM);
    finish_update(waveform);
    return result;
}

// 分钟字带用产品GL16局推，保持灰阶且不计入整屏清理周期。
// Minute bands use product local GL16, retaining grays outside whole-screen cleanup counting.
enum EpdDrawError update_display_area_quiet(EpdiyHighlevelState* hl, EpdRect area) {
    use_scan_for(&E0470_WAVEFORM, MODE_GL16);
    if (!power_ready()) return EPD_DRAW_POWER_NOT_READY;
    epd_hl_waveform(hl, &E0470_WAVEFORM);
    enum EpdDrawError result;
    if (s_baseline_unknown) result = hl_update(hl, &E0470_WAVEFORM, MODE_GL16, true, &area, false, false);
    else result = epd_hl_update_area_full(hl, MODE_GL16, 25, area);
    s_baseline_unknown = result != EPD_DRAW_SUCCESS;
    epd_hl_waveform(hl, &E0470_WAVEFORM);
    finish_update(&E0470_WAVEFORM);
    return result;
}

enum EpdDrawError update_display_area_with(
    EpdiyHighlevelState* hl, const EpdWaveform* waveform, enum EpdDrawMode mode,
    EpdRect area
) {
    use_scan_for(waveform, mode);
    if (!power_ready()) return EPD_DRAW_POWER_NOT_READY;
    epd_hl_waveform(hl, waveform);
    enum EpdDrawError result = hl_update(hl, waveform, mode, false, &area, false, false);
    epd_hl_waveform(hl, &E0470_WAVEFORM);
    finish_update(waveform);
    return result;
}

// 供数不足时的兜底：把频率退回安全值，整屏白一次，让后面的差分刷有干净参考帧。
// Underrun fallback: drop to the safe clock and wipe the panel white so later differentials have a clean reference.
static int s_pclk_mhz = DISPLAY_PCLK_DEFAULT_MHZ;

int display_pclk_mhz(void) { return s_pclk_mhz; }

void guard_draw_result(EpdiyHighlevelState* hl, enum EpdDrawError result) {
    if (result == EPD_DRAW_SUCCESS) return;
    s_baseline_unknown = true;
    if (!(result & EPD_DRAW_EMPTY_LINE_QUEUE)) return;
    s_pclk_mhz = DISPLAY_PCLK_SAFE_MHZ;
    read_pico_epd_set_pclk(DISPLAY_PCLK_SAFE_MHZ);
    use_scan_for(&E0470_WAVEFORM, MODE_GC16);
    if (!power_ready()) return;
    // 欠载后物理状态未知，先实际铺白，再以白基准重画保留的目标页。
    // After underrun the physical state is unknown: physically clear before redrawing the retained target from white.
    epd_hl_waveform(hl, &E0470_FULL_WAVEFORM);
    epd_clear();
    s_baseline_unknown = epd_hl_update_screen_from_white(hl, MODE_GC16, 25) != EPD_DRAW_SUCCESS;
    if (!s_baseline_unknown) {
        s_navigation_entry = false;
        s_night_body_turns = 0;
    }
    epd_hl_waveform(hl, &E0470_WAVEFORM);
    s_soft_refreshes = 0;
    finish_update(&E0470_FULL_WAVEFORM);
    ESP_LOGW(TAG, "line queue underrun, pclk back to %d MHz", DISPLAY_PCLK_SAFE_MHZ);
}
