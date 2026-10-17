/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 *
 * 只做开机装配：拉起板级硬件、读设置、定 VCOM、放开机图、开字体；
 * 未标定则先拦住进设定页。然后把控制权交给 app_loop。
 *
 * Boot wiring only: board init, settings, VCOM, splash, fonts. If VCOM
 * is unset, the factory page blocks first. Then control goes to app_loop.
 * 用户修订：独立 PageRest 插画以两帧短淡入替换原开机图，不加等待或伪进度。
 * User revision: Replace the original splash with PageRest artwork and a two-frame fade, without waits or fake progress.
 */

#include <stdint.h>

#include "app_loop.h"
#include "app_registry.h"
#include "boot_state.h"
#include "display.h"
#include "firmware_version.h"
#include "epd_highlevel.h"
#include "epdiy.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
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
#include "book_cover.h"
#include "os_page_turner.h"
#include "ui_image_cache.h"
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

// 蓝牙启动内存不足时释放可重建缓存：插图成品、封面复用与字形缓存，随后按需重建。
// Reclaim rebuildable caches when BLE start headroom runs short: decoded images, cover reuse slots and glyph caches, rebuilt on demand.
static void radio_reclaim(void) {
    ui_image_cache_clear();
    book_cover_join();
    ttf_font_reclaim();
}

void app_main(void) {
    app_settings_init();
    os_page_turner_set_reclaimer(radio_reclaim);
    const bool usb_requested = os_usb_disk_take_request();
    if (!usb_requested) usb_disk_restore_serial();
    read_pico_handle_t hw;
    if (read_pico_init_with_sd(&hw, !usb_requested) != ESP_OK) return;
    // 产品扫描余量统一来自 display 配置。/ Product scan margin follows the display configuration.
    read_pico_epd_set_pclk(DISPLAY_PCLK_DEFAULT_MHZ);
    // 先记上次复位原因；异常复位在锁屏挑战前写入内置存储日志。
    // Record the last reset reason first; abnormal ones reach the internal log before the lock challenge.
    os_crash_boot_check();
    // 启动事务在硬件初始化前落 NVS：开书中断与半途启动下次开机可查。
    // The startup transaction hits NVS before hardware: interrupted opens and half-finished boots are visible next power-up.
    pico_boot_init(os_crash_boot_abnormal());
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
    size_t splash_bytes = (size_t)epd_width() * epd_height() / 2;
    uint8_t* splash = heap_caps_malloc(splash_bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    bool splash_ok = splash && asset_pack_unpack(loading_4bpp_pack_start,
        (size_t)(loading_4bpp_pack_end - loading_4bpp_pack_start), splash, splash_bytes);
    ui_draw_boot_splash(framebuffer, splash_ok ? splash : NULL, firmware_version(), false);
    // 开机图必须绝对刷：异常状态后的首帧差分盖不住旧墨（跨面板通用结论）。
    // The splash must be absolute: a first differential frame cannot clear old
    // ink after an abnormal state (cross-panel rule).
    guard_draw_result(&hl, update_display_full(&hl));
    if (splash_ok) {
        ui_draw_boot_splash(framebuffer, splash, firmware_version(), true);
        guard_draw_result(&hl, update_display_mode(&hl, MODE_GL16));
    }
    heap_caps_free(splash);

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
    // 密码页按电源键＝不开了，转软睡（短按再开机）。
    // A power key on the boot PIN page means "not now": drop into soft sleep (short press re-powers).
    if (!app_lock_pin_challenge(&hl, hw.touch)) app_enter_host_sleep(APP_SLEEP_DEEP);

    // 用户请求 USB 独占卡盘：通过工厂/PIN 检查后，不启动普通页面或后台消费者。
    // User-requested exclusive USB disk: after factory/PIN gates, never start normal pages or background consumers.
    if (usb_requested && !st_resume) usb_disk_run(&hl, framebuffer, hw.touch);
    if (usb_requested) read_pico_sd_start_probe();

    // 自检跑到一半断电的话，重新上电直接回自检页接着跑；其余断电唤醒按锁屏来源
    // 直达锁屏那一刻的页面（阅读页配合续读标记自动开书回到进度）。
    // Resume the self-test page if a power-cut interrupted it; otherwise a power-cut
    // wake returns to the page showing at lock time (the reader pairs with the reading
    // marker to reopen at the saved position).
    const app_desc_t* boot_page = app_home_page();
    if (!st_resume) {
        const app_desc_t* origin = app_by_id((os_app_id_t)app_settings_take_lock_origin());
        if (origin) boot_page = origin;
    }
    app_loop_run(&(app_loop_config_t){
        .hl = &hl,
        .fb = framebuffer,
        .acc = hw.sensor,
        .tp = hw.touch,
        .sensor_ready = hw.sensor_ready,
        .first_app = st_resume ? app_selftest_page() : boot_page,
    });
}
