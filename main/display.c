/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 *
 * 刷屏出口：按模式推屏、普通刷新后下电、跟手空闲下电、pclk 欠载回退。
 *
 * Present path: power down after normal updates, idle timeout for tracking, and pclk underrun recovery.
 *
 * 冻结：用户明确要求正常阅读翻页无黑闪，正文不消耗或触发周期 GC16；清理由手动发起。
 * Frozen: User requires no black flashes during normal reading turns; body turns neither consume nor trigger scheduled GC16, leaving cleanup manual.
 * 冻结：用户实测否定0.5.19并明确优先无闪速度；直刷使用厂家黑白DU及真实二值目标，标准模式保留灰阶。
 * Frozen: Device feedback rejects 0.5.19 and the user explicitly prioritizes flash-free speed; direct uses vendor black/white DU with actual binary targets, while standard retains gray.
 * 冻结：旧参考保留实际灰码，不能伪造白基准；正文和普通导航白白保持，不恢复历史字形补擦。
 * Frozen: Prior references retain actual gray codes without fictional white baselines; body and ordinary navigation hold white without historical glyph cleanup.
 * 冻结：标准正文恢复厂家黑/灰对角线定稿，白白保持；用户因设置框和Tab残留允许仅布局入口GC16一次，不按按钮或周期触发。
 * Frozen: Standard body restores vendor black/gray diagonal settling and held white; persistent settings/Tab ghosts lead the user to allow one GC16 at layout entries, never per control or schedule.
 * 冻结：夜间正文沿用未变保持，不能让黑背景的厂家对角线产生整屏亮闪。
 * Frozen: Night body retains unchanged-pixel hold so vendor black-background diagonals cannot produce a full-screen bright flash.
 */

#include "display.h"
#include "display_pixels.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "app_config.h"
#include "e0470_epaper_waveform.h"
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

// 仅跟手供电保活，普通页面不保留静态 VCOM。/ Keep rails for tracking bursts only; ordinary pages never retain static VCOM.
#define RAILS_IDLE_TIMEOUT_MS 8000
// 0 表示轨道已断电；否则是到期时间（ms），到点后主循环断电。
// 0 means the rails are off; otherwise a deadline (ms) after which the loop powers them down.
static int64_t rails_deadline_ms;

void rails_keepalive(void) {
    rails_deadline_ms = esp_timer_get_time() / 1000 + RAILS_IDLE_TIMEOUT_MS;
}

// 普通推屏立即下电，避免静置时继续受静态电压驱动。/ Power off ordinary updates immediately to prevent static-voltage drift.
static void finish_update(const EpdWaveform* waveform) {
    if (waveform == &E0470_FOLLOW_WAVEFORM) rails_keepalive();
    else {
        rails_deadline_ms = 0;
        epd_poweroff();
    }
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
static bool s_baseline_unknown;

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
    const EpdRect* area
) {
    const bool is_full_screen = area == NULL ||
        (area->x == 0 && area->y == 0 && area->width >= epd_width() && area->height >= epd_height());
    const bool body_profile = waveform == &E0470_TEXTTURN_WAVEFORM || waveform == &E0470_TEXTTURN_NIGHT_WAVEFORM || waveform == &E0470_DIRECT_WAVEFORM;
    // 日间标准定稿黑/灰像素；夜间标准和黑白直刷用选择码保持全部未变像素。
    // Standard day settles black/gray pixels; standard night and binary direct use selectors to hold every unchanged pixel.
    full = full || (mode & 0xF) == MODE_GC16 || waveform == &E0470_NAVIGATION_WAVEFORM || waveform == &E0470_NAVIGATION_ENTRY_WAVEFORM ||
           body_profile;
    bool promoted = false;
    // 失败后的面板状态未知，下一次先物理清白并完整重画。/ After a failed draw the panel is unknown; physically clear before the next complete redraw.
    if (s_baseline_unknown) {
        epd_clear();
        memset(hl->back_fb, 255, (size_t)epd_width() * epd_height() / 2);
        mode = (enum EpdDrawMode)((mode & ~0xF) | MODE_GC16);
        epd_hl_waveform(hl, &E0470_FULL_WAVEFORM);
        use_scan_for(&E0470_FULL_WAVEFORM, mode);
        area = NULL;
        full = promoted = true;
        s_soft_refreshes = 0;
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
    else if ((waveform == &E0470_DIRECT_WAVEFORM || waveform == &E0470_TEXTTURN_NIGHT_WAVEFORM) && (mode & 0xF) != MODE_GC16)
        result = epd_hl_update_screen_selective(hl, mode, 25, NULL);
    else result = full ? epd_hl_update_screen_full(hl, mode, 25) : epd_hl_update_screen(hl, mode, 25);
    s_baseline_unknown = result != EPD_DRAW_SUCCESS;
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
    enum EpdDrawError result = hl_update(hl, &E0470_WAVEFORM, mode, false, NULL);
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
    if (result == EPD_DRAW_SUCCESS) s_navigation_entry = false;
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

// 指定波形整屏刷一次，刷完把默认波形装回去。用来 A/B 两条灰阶表。
// Present the whole screen with a given waveform, then restore the default. Used to A/B two gray tables.
enum EpdDrawError update_display_with(
    EpdiyHighlevelState* hl, const EpdWaveform* waveform, enum EpdDrawMode mode
) {
    // 布局入口一次厂家清理；保留真实旧帧，不再追加未校准白推动。
    // Layout entries use one vendor cleaning update from the actual prior frame, without uncalibrated added white drive.
    const bool clean_entry = waveform == &E0470_NAVIGATION_WAVEFORM &&
        (mode & 0xF) == MODE_GL16 && s_navigation_entry;
    const EpdWaveform* applied = clean_entry ? &E0470_FULL_WAVEFORM : waveform;
    if (clean_entry) mode = (enum EpdDrawMode)((mode & ~0xF) | MODE_GC16);
    use_scan_for(applied, mode);
    if (!power_ready()) return EPD_DRAW_POWER_NOT_READY;
    epd_hl_waveform(hl, applied);
    enum EpdDrawError result = hl_update(hl, applied, mode, false, NULL);
    epd_hl_waveform(hl, &E0470_WAVEFORM);
    finish_update(applied);
    return result;
}

// 正文整页保留厂家GL16黑/灰定稿，白白保持，不触发周期GC16。
// Whole-page body turns retain vendor GL16 black/gray settling and held white without scheduled GC16.
enum EpdDrawError update_display_text_turn(EpdiyHighlevelState* hl, bool white_on_black) {
    return update_display_with(hl, white_on_black ? &E0470_TEXTTURN_NIGHT_WAVEFORM : &E0470_TEXTTURN_WAVEFORM, MODE_GL16);
}

enum EpdDrawError update_display_text_direct(EpdiyHighlevelState* hl, bool white_on_black) {
    // 按用户速度取舍提交真实黑白目标；厂家DU仍读取实际旧灰帧。
    // Commit actual black/white targets per the user's speed preference; vendor DU still reads the real prior gray frame.
    display_prepare_direct_frame(hl->front_fb, epd_width(), epd_height(), white_on_black);
    return update_display_with(hl, &E0470_DIRECT_WAVEFORM, MODE_GL16);
}

// 分钟字带用产品GL16局推，保持灰阶且不计入整屏清理周期。
// Minute bands use product local GL16, retaining grays outside whole-screen cleanup counting.
enum EpdDrawError update_display_area_quiet(EpdiyHighlevelState* hl, EpdRect area) {
    use_scan_for(&E0470_WAVEFORM, MODE_GL16);
    if (!power_ready()) return EPD_DRAW_POWER_NOT_READY;
    epd_hl_waveform(hl, &E0470_WAVEFORM);
    enum EpdDrawError result;
    if (s_baseline_unknown) result = hl_update(hl, &E0470_WAVEFORM, MODE_GL16, true, &area);
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
    enum EpdDrawError result = hl_update(hl, waveform, mode, false, &area);
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
    if (!s_baseline_unknown) s_navigation_entry = false;
    epd_hl_waveform(hl, &E0470_WAVEFORM);
    s_soft_refreshes = 0;
    finish_update(&E0470_FULL_WAVEFORM);
    ESP_LOGW(TAG, "line queue underrun, pclk back to %d MHz", DISPLAY_PCLK_SAFE_MHZ);
}
