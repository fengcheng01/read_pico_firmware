/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 *
 * 只做开机装配：拉起板级硬件、读设置、定 VCOM、放开机图、开字体；
 * 未标定则先拦住进设定页。然后把控制权交给 app_loop。
 *
 * Boot wiring only: board init, settings, VCOM, splash, fonts. If VCOM
 * is unset, the factory page blocks first. Then control goes to app_loop.
 */

#include <stdint.h>

#include "app_loop.h"
#include "app_registry.h"
#include "display.h"
#include "firmware_version.h"
#include "epd_highlevel.h"
#include "epdiy.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "os_crash.h"
#include "os_time.h"
#include "os_usb_disk.h"
#include "usb_disk.h"
#include "pmu_selftest.h"
#include "read_pico_board.h"
#include "read_pico_init.h"
#include "read_pico_sd.h"
#include "read_pico_pmu.h"
#include "settings.h"
#include "ttf_font.h"
#include "ui_kit.h"
#include "asset_pack.h"
#include "sleep.h"
#include "vcom_setup.h"

static const char* TAG = "read_pico";

extern const uint8_t lock_4bpp_pack_start[] asm("_binary_lock_4bpp_pack_start");
extern const uint8_t lock_4bpp_pack_end[] asm("_binary_lock_4bpp_pack_end");
extern const uint8_t loading_4bpp_pack_start[] asm("_binary_loading_4bpp_pack_start");
extern const uint8_t loading_4bpp_pack_end[] asm("_binary_loading_4bpp_pack_end");

// 已标定则开机读一次喂给 epdiy。未标定先用板级默认出开机图，随后拦住进标定页。
// Load VCOM once when set. Otherwise keep the board default for the splash,
// then gate into the setup page.
static bool resolve_vcom_at_boot(void) {
    int mv = 0;
    for (int i = 0; i < 3; i++) {
        if (read_pico_pmu_vcom_get(&mv) == ESP_OK) {
            epd_set_vcom((uint16_t)mv);
            ESP_LOGI(TAG, "panel VCOM loaded from PMU");
            return true;
        }
    }
    ESP_LOGW(TAG, "panel VCOM unset, keeping board default until setup");
    return false;
}

// 开机图和锁屏图是预先转好的 4bpp 全屏位图，尺寸对不上说明资源和面板不匹配。
// Splash and lock images are pre-baked 4bpp full-frames. A size mismatch
// means the assets do not match the panel.
static bool images_match_panel(void) {
    size_t lock_size = asset_pack_size(lock_4bpp_pack_start, (size_t)(lock_4bpp_pack_end - lock_4bpp_pack_start));
    size_t loading_size = asset_pack_size(loading_4bpp_pack_start, (size_t)(loading_4bpp_pack_end - loading_4bpp_pack_start));
    size_t expected = (size_t)epd_width() * epd_height() / 2;
    if (lock_size == expected && loading_size == expected) return true;
    ESP_LOGE(
        TAG, "Image size mismatch: lock %u loading %u expected %u",
        (unsigned)lock_size, (unsigned)loading_size, (unsigned)expected
    );
    return false;
}

void app_main(void) {
    app_settings_init();
    const bool usb_requested = os_usb_disk_take_request();
    if (!usb_requested) usb_disk_restore_serial();
    read_pico_handle_t hw;
    if (read_pico_init_with_sd(&hw, !usb_requested) != ESP_OK) return;
    // 产品扫描余量统一来自 display 配置。/ Product scan margin follows the display configuration.
    read_pico_epd_set_pclk(DISPLAY_PCLK_DEFAULT_MHZ);
    // 先记上次复位原因；异常复位在锁屏挑战前写入内置存储日志。
    // Record the last reset reason first; abnormal ones reach the internal log before the lock challenge.
    os_crash_boot_check();
    const bool vcom_ok = resolve_vcom_at_boot();
    pmu_selftest_bind(hw.sensor);
    const bool st_resume = pmu_selftest_boot_resume();

    // 升级时清掉旧固件循环闹钟；动态锁屏现由 ESP 浅睡定时更新。
    // Clear legacy repeat alarms on upgrade; dynamic lock faces now use ESP light-sleep timers.
    app_sleep_alarm_clock(false);

    if (!images_match_panel()) return;

    EpdiyHighlevelState hl = hw.hl;
    uint8_t* framebuffer = hw.framebuffer;

    read_pico_i2c_census_take();
    if (usb_requested) ttf_font_open_builtin();
    else ttf_font_init();
    guard_draw_result(&hl, display_boot_white(&hl));
    // 冷启动（含深睡断电后）面板可能带着半驱动残荷：再物理清一遍才铺开机图；
    // 异常复位同理。参考固件对未知面板状态一律加强清屏。
    // A cold boot (including after deep-sleep power loss) may face half-driven
    // charge on the panel: clear once more before the splash; same for abnormal
    // resets. The reference firmware strengthens clears on unknown panel state.
    if (os_crash_boot_abnormal()) guard_draw_result(&hl, display_boot_white(&hl));
    guard_draw_result(&hl, display_boot_white(&hl));
    // 开机图无条件展示（含设密码用户）：锁屏挑战前就能核对固件版本。
    // Always show the splash (PIN users too): the version is checkable before the lock challenge.
    ui_draw_packed_full_image(framebuffer, loading_4bpp_pack_start,
                             (size_t)(loading_4bpp_pack_end - loading_4bpp_pack_start));
    ui_clear_rect_fast(framebuffer, (EpdRect){0, 1080, UI_LOCK_WIDTH, 80});
    ui_text(framebuffer, UI_LOCK_WIDTH / 2, 1100, 28, firmware_version(), EPD_DRAW_ALIGN_CENTER, false);
    // 开机图必须绝对刷：异常状态后的首帧差分盖不住旧墨（跨面板通用结论）。
    // The splash must be absolute: a first differential frame cannot clear old
    // ink after an abnormal state (cross-panel rule).
    guard_draw_result(&hl, update_display_full(&hl));

    if (!hw.touch_ready) {
        ESP_LOGE(TAG, "No touch controller, UI cannot run");
        return;
    }

    if (hw.pmu_ready && !vcom_ok) {
        vcom_setup_run(&hl, framebuffer, hw.touch);
    }

    // 密码挑战可能无限阻塞，崩溃日志先落盘。/ The PIN challenge may block forever; flush the crash log first.
    os_crash_flush();

    // 设有锁屏密码时先阻塞校验；主循环、菜单与页面在通过前都不可达。
    // With a lock PIN armed, block here first; the loop, menus and pages stay unreachable until it passes.
    app_lock_pin_challenge(&hl, hw.touch);

    // 用户请求 USB 独占卡盘：通过工厂/PIN 检查后，不启动普通页面或后台消费者。
    // User-requested exclusive USB disk: after factory/PIN gates, never start normal pages or background consumers.
    if (usb_requested && !st_resume) usb_disk_run(&hl, framebuffer, hw.touch);
    if (usb_requested) read_pico_sd_start_probe();

    app_loop_run(&(app_loop_config_t){
        .hl = &hl,
        .fb = framebuffer,
        .acc = hw.sensor,
        .tp = hw.touch,
        .sensor_ready = hw.sensor_ready,
        // 自检跑到一半断电的话，重新上电直接回自检页接着跑。
        // Resume the self-test page if a power-cut interrupted it.
        .first_app = st_resume ? app_selftest_page() : app_home_page(),
    });
}
