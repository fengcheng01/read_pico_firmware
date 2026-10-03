/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 *
 * 刷屏出口：按模式推屏、HV 轨空闲下电、pclk 欠载回退。
 *
 * Present path: push by mode, drop HV rails on idle, fall back pclk on
 * underrun.
 */

#include "display.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include "app_config.h"
#include "e0470_epaper_waveform.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "settings.h"

static const char* TAG = "read_pico";
static bool s_bulk_io;

void display_set_bulk_io(bool active) {
    s_bulk_io = active;
    ESP_LOGI(TAG, "bulk I/O scan margin %s", active ? "on" : "off");
}

// HV 轨道空闲多久才断电。断电要等 500ms 放电，再上电又要几十毫秒，
// 所以连续操作期间一直保持常开，只有真的没人动才关掉省电并卸掉 VCOM。
// How long HV rails stay up when idle. Power-off waits 500 ms to discharge,
// and power-on takes tens of ms, so keep them on during a burst of work and
// drop them — and VCOM — only when nothing is happening.
#define RAILS_IDLE_TIMEOUT_MS 8000

// 0 表示轨道已断电；否则是到期时间（ms），到点后主循环断电。
// 0 means the rails are off; otherwise a deadline (ms) after which the loop powers them down.
static int64_t rails_deadline_ms;

void rails_keepalive(void) {
    rails_deadline_ms = esp_timer_get_time() / 1000 + RAILS_IDLE_TIMEOUT_MS;
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

// 单遍白基准刷新：back_fb 归白后一次 GL16 扫描，(15,15) 的白推 tick 顺带清底。
// 真机验证过的 from_white 机制换 GL16 档；不压黑、不伪造参考帧、不双遍扫描。
// Single-pass white-baseline update: reset back_fb to white, then one GL16 scan
// whose (15,15) white-push tick also cleans the floor. The device-proven
// from_white mechanism on the GL16 mode; no black flash, no fabricated
// reference, no double scan.
static enum EpdDrawError white_baseline_draw(EpdiyHighlevelState* hl, enum EpdDrawMode mode) {
    return epd_hl_update_screen_from_white(hl, mode, 25);
}

// 灰阶页必须全像素；累计局部更新达到周期时清理整屏，防止其他区域残影保留。
// Grayscale pages drive every pixel; accumulated partial updates clean the whole panel at the configured interval.
// 跟随 DU 专用于跟手，不参与页级清理计数。/ FOLLOW DU is for live tracking and excluded from page cleanup counting.
static enum EpdDrawError hl_update(
    EpdiyHighlevelState* hl, const EpdWaveform* waveform, enum EpdDrawMode mode, bool full,
    const EpdRect* area
) {
    full = full || (mode & 0xF) == MODE_GL16 || (mode & 0xF) == MODE_GC16;
    bool promoted = false;
    if (waveform != &E0470_FOLLOW_WAVEFORM) {
        unsigned every = app_settings_gc_every();
        if ((mode & 0xF) == MODE_GC16) {
            s_soft_refreshes = 0;
        } else if (every > 0 && (unsigned)++s_soft_refreshes >= every) {
            s_soft_refreshes = 0;
            mode = (enum EpdDrawMode)((mode & ~0xF) | MODE_GL16);
            epd_hl_waveform(hl, &E0470_FULL_WAVEFORM);
            use_scan_for(&E0470_FULL_WAVEFORM, mode);
            promoted = true;
            area = NULL;
            full = true;
            ESP_LOGI(TAG, "white cleanup after %u soft refreshes", every);
        }
    }
    enum EpdDrawError result;
    if (area != NULL && area->x == 0 && area->y == 0 &&
        area->width == epd_width() && area->height == epd_height() &&
        (mode & 0xF) == MODE_GL16) {
        // 整屏面积的 GL16 与整页同路：白基准一遍出，直接差分留残影。
        // Full-screen-area GL16 takes the page path: one white-baseline pass instead of a direct diff.
        result = white_baseline_draw(hl, mode);
    }
    else if (area != NULL) result = full ? epd_hl_update_area_full(hl, mode, 25, *area)
                                   : epd_hl_update_area(hl, mode, 25, *area);
    else if ((mode & 0xF) == MODE_GL16) result = white_baseline_draw(hl, mode);
    else result = full ? epd_hl_update_screen_full(hl, mode, 25) : epd_hl_update_screen(hl, mode, 25);
    if (promoted) epd_hl_waveform(hl, waveform);
    return result;
}

enum EpdDrawError update_display_mode(
    EpdiyHighlevelState* hl, enum EpdDrawMode mode
) {
    if ((mode & 0xF) == MODE_GL16 || (mode & 0xF) == MODE_GC16)
        return update_display_with(hl, &E0470_FULL_WAVEFORM, mode);
    use_scan_for(&E0470_WAVEFORM, mode);
    epd_poweron();
    enum EpdDrawError result = hl_update(hl, &E0470_WAVEFORM, mode, false, NULL);
    rails_keepalive();
    return result;
}

enum EpdDrawError update_display_from_white_with(
    EpdiyHighlevelState* hl, const EpdWaveform* waveform, enum EpdDrawMode mode
) {
    use_scan_for(waveform, mode);
    epd_poweron();
    epd_hl_waveform(hl, waveform);
    enum EpdDrawError result = epd_hl_update_screen_from_white(hl, mode, 25);
    epd_hl_waveform(hl, &E0470_WAVEFORM);
    s_soft_refreshes = 0;
    rails_keepalive();
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
    memset(hl->front_fb, 255, bytes);
    memset(hl->back_fb, 255, bytes);
    use_scan_for(&E0470_FULL_WAVEFORM, MODE_GC16);
    epd_poweron();
    epd_clear();
    epd_hl_waveform(hl, &E0470_WAVEFORM);
    s_soft_refreshes = 0;
    rails_keepalive();
    return EPD_DRAW_SUCCESS;
}

enum EpdDrawError update_display_white(EpdiyHighlevelState* hl) {
    epd_hl_set_all_white(hl);
    return update_display_mode(hl, MODE_GL16);
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
    // 日常直刷保留完整厂家灰阶序列；GC16 和实验波形仍按明确请求选择。
    // Retain the complete vendor grayscale sequence for daily direct updates; honor explicit GC16/experimental choices.
    if (waveform == &E0470_WAVEFORM && (mode & 0xF) == MODE_GL16) waveform = &E0470_FULL_WAVEFORM;
    use_scan_for(waveform, mode);
    epd_poweron();
    epd_hl_waveform(hl, waveform);
    enum EpdDrawError result = hl_update(hl, waveform, mode, false, NULL);
    epd_hl_waveform(hl, &E0470_WAVEFORM);
    rails_keepalive();
    return result;
}

enum EpdDrawError update_display_area_with(
    EpdiyHighlevelState* hl, const EpdWaveform* waveform, enum EpdDrawMode mode,
    EpdRect area
) {
    if (waveform == &E0470_WAVEFORM && (mode & 0xF) == MODE_GL16) waveform = &E0470_FULL_WAVEFORM;
    use_scan_for(waveform, mode);
    epd_poweron();
    epd_hl_waveform(hl, waveform);
    enum EpdDrawError result = hl_update(hl, waveform, mode, false, &area);
    epd_hl_waveform(hl, &E0470_WAVEFORM);
    rails_keepalive();
    return result;
}

// 供数不足时的兜底：把频率退回安全值，整屏白一次，让后面的差分刷有干净参考帧。
// Underrun fallback: drop to the safe clock and wipe the panel white so later differentials have a clean reference.
static int s_pclk_mhz = DISPLAY_PCLK_DEFAULT_MHZ;

int display_pclk_mhz(void) { return s_pclk_mhz; }

void guard_draw_result(EpdiyHighlevelState* hl, enum EpdDrawError result) {
    if (!(result & EPD_DRAW_EMPTY_LINE_QUEUE)) return;
    s_pclk_mhz = DISPLAY_PCLK_SAFE_MHZ;
    read_pico_epd_set_pclk(DISPLAY_PCLK_SAFE_MHZ);
    use_scan_for(&E0470_WAVEFORM, MODE_GC16);
    epd_poweron();
    // 欠载恢复回到验证过的白基准出口，保留目标页。/ Underrun recovery takes the
    // proven white-baseline path while retaining the target page.
    epd_hl_waveform(hl, &E0470_FULL_WAVEFORM);
    epd_hl_update_screen_from_white(hl, MODE_GC16, 25);
    epd_hl_waveform(hl, &E0470_WAVEFORM);
    s_soft_refreshes = 0;
    rails_keepalive();
    ESP_LOGW(TAG, "line queue underrun, pclk back to %d MHz", DISPLAY_PCLK_SAFE_MHZ);
}
