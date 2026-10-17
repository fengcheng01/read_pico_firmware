/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 * 冻结：蓝牙翻页器在画锁屏与睡眠前完整停栈；停止失败留在原页，成功唤醒后由原页恢复连接，不支持蓝牙唤醒。
 * Frozen: Stop the page-turner stack before painting/entering sleep; retain the page after stop failure, reconnect from the resumed page, and never claim Bluetooth wake.
 *
 * 冻结修订：用户要求摘录时钟走时；浅睡按分钟局刷底栏，深睡选择保留浅睡，摘录不因30分钟无操作冻结。
 * Frozen revision: User feedback requires a live excerpt clock; repaint its lower band each minute in light sleep, retain light sleep under Deep, and exempt excerpts from thirty-minute idle descent.
 * 显式关机仍断电并标注关机前时刻；无摘录回退内置图，不改变所选摘录。
 * Explicit Off still powers down with a pre-off time label; missing excerpts use the built-in face without changing the selected quote.
 * 第三批修订：时钟与黄历的关机提示改为长按开机，不改变电源策略。
 * Batch-three revision: Clock/almanac power-off hints say hold to boot, without changing power policy.
 * 锁屏、浅睡等待、软睡/关机下电、锁屏密码。
 *
 * Lock, light-sleep wait, soft-sleep / power-off, and the lock PIN.
 *
 * 冻结：动态锁屏在浅睡中按本地分钟定时更新；深睡设置对动态锁屏使用浅睡，关机仍断电。
 * 原因：用户反馈 PMU 闹钟断电开机无法可靠走时；保留帧缓冲与单调钟避免重复冷启动。
 * Frozen: Dynamic faces use minute-timed light sleep, including when deep sleep is selected;
 * explicit power-off still powers down. Hardware feedback requires retaining the framebuffer
 * and monotonic clock rather than repeatedly cold-booting through the PMU alarm.
 * 修订：用户要求修复长期慢钟；启用自动校时后，分钟唤醒可短暂连接已保存WiFi，电源键取消。
 * Revision: User-requested drift repair permits brief saved-WiFi sessions on minute wakes when enabled; the power key cancels them.
 * 修订：静态浅睡锁屏按自动校时期限定时维护但不刷新画面，补齐整夜无第二锚点的缺口；静态深睡仍由PMU断电。
 * Revision: Static light-sleep locks service automatic-sync deadlines without repainting, closing overnight missing-second-anchor coverage; static deep sleep still powers off through the PMU.
 * 修订：用户要求自定义锁屏图；样式4=壁纸读取 TF 图片整屏绘制，读取失败回退默认静态图；
 * 壁纸不随分钟变化，沿用静态的时间维护与深睡策略。
 * Revision: User-requested custom lock images; style 4=wallpaper draws a TF image full
 * screen, falling back to the default static image when unreadable; wallpapers never change
 * per minute and reuse the static time-maintenance and deep-sleep policy.
 * 修订：用户要求去掉静态图选项、统一图片：样式0=图片（有壁纸画壁纸，缺图回退内置图），
 * 时钟/日历/黄历不变；旧值4读取时并入0。壁纸仍不随分钟变化，深睡策略同内置图。
 * Revision: The user removes the static option, unifying on image: style 0=image (wallpaper
 * when present, built-in image otherwise) with clock/calendar/almanac unchanged; legacy
 * value 4 merges into 0 at load. Wallpapers still never change per minute and share the
 * built-in image's deep-sleep policy.
 * 实机修订：壁纸锁屏后照片透出上一页残影——混合灰阶照片单次差分GC16擦不净；锁屏入口
 * 改为先物理清白再厂家GC16重画（update_display_clean），其余锁屏面保持单次GC16。
 * Device revision: wallpaper locks ghosted the previous page — mixed-gray photos need more
 * than one differential GC16; the lock entry now physically clears white and repaints with
 * vendor GC16 (update_display_clean), while other faces keep the single GC16 pass.
 * 实机修订：已设壁纸偶发回退内置图一次——整幅解码在阅读后的内存压力下偶发分配失败；
 * 改为路径+大小+mtime 键控的解码缓存（首次成功后不再依赖临时内存），stat 与解码失败
 * 均留日志。
 * Device revision: a configured wallpaper once fell back to the built-in image — full-image
 * decoding intermittently fails allocation under post-reading memory pressure; the decoded
 * gray is now cached keyed by path+size+mtime (after the first success it never depends on
 * transient memory), with stat and decode failures logged.
 * 实机修订：误触电源键锁屏后无人操作会分钟唤醒整夜刷新；浅睡锁屏累计30分钟只有定时唤醒
 * 即降级软睡（摘录时钟除外）；任何按键/拿起唤醒照常立即返回原页。
 * Device revision: an accidental power-key lock used to minute-refresh all night untouched;
 * after 30 minutes of nothing but timer wakes non-excerpt locks descend to soft sleep
 * (the panel keeps the face; a short press re-powers), while any key or pickup still
 * returns to the page immediately as before.
 */

#include "sleep.h"

#include <sys/stat.h>

#include "app.h"
#include "esp_heap_caps.h"
#include "app_sleep_hooks.h"
#include "os_page_turner.h"
#include "display.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_sleep.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "os_lunar.h"
#include "e0470_epaper_waveform.h"
#include "os_time.h"
#include "pmu_selftest.h"
#include "read_pico_board.h"
#include "read_pico_pmu.h"
#include "sc7a20h_lab.h"
#include "settings.h"
#include "ui_kit.h"
#include "ui_image.h"
#include "ttf_font.h"
#include "ui/product/ui_product.h"
#include "book_quotes.h"
#include "book_stats.h"

static const char* TAG = "read_pico";

extern const uint8_t lock_4bpp_pack_start[] asm("_binary_lock_4bpp_pack_start");
extern const uint8_t lock_4bpp_pack_end[] asm("_binary_lock_4bpp_pack_end");

static void draw_static_lock(uint8_t* fb) {
    ui_draw_packed_full_image(fb, lock_4bpp_pack_start,
                             (size_t)(lock_4bpp_pack_end - lock_4bpp_pack_start));
}

static void lock_arm_ioe_wakeup(void) {
    gpio_config_t io = {
        .pin_bit_mask = 1ULL << READ_PICO_IOE_INT_GPIO,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io);
    read_pico_clear_ioe_int();
    gpio_wakeup_enable((gpio_num_t)READ_PICO_IOE_INT_GPIO, GPIO_INTR_LOW_LEVEL);
    esp_sleep_enable_gpio_wakeup();
}

void app_lock_wait_key_idle(int timeout_ms) {
    int64_t start = esp_timer_get_time() / 1000;
    int64_t high_from = 0;
    while (esp_timer_get_time() / 1000 - start < timeout_ms) {
        read_pico_pmu_drain_events();
        read_pico_clear_ioe_int();
        bool held = false;
        if (read_pico_pmu_poll() == ESP_OK) {
            held = (read_pico_pmu_get()->key_state & 0x01) != 0;
        }
        bool int_high = gpio_get_level((gpio_num_t)READ_PICO_IOE_INT_GPIO) != 0;
        if (!held && int_high) {
            if (high_from == 0) high_from = esp_timer_get_time() / 1000;
            if (esp_timer_get_time() / 1000 - high_from >= 150) return;
        } else {
            high_from = 0;
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

app_wake_source_t app_last_wake_source(void) {
    return (app_wake_source_t)app_settings_last_wake();
}

static void pickup_ack(sc7a20h_handle_t acc) {
    sc7a20h_events_t ev;
    sc7a20h_read_events(acc, &ev);
    sc7a20h_ack_int(acc);
}

static bool pickup_ia(sc7a20h_handle_t acc) {
    sc7a20h_events_t ev;
    sc7a20h_aoi_src_t aoi;
    if (sc7a20h_read_events(acc, &ev) != ESP_OK) return false;
    sc7a20h_aoi_decode(ev.aoi1_src, &aoi);
    return aoi.ia;
}

// 第一次低电平不够：高通余波还会再拉高。要连续安静一段时间才进浅睡。
// 浅睡锁屏只剩分钟唤醒的降级时限：误锁后被搁置的面板停止逐分钟刷新，转软睡。
// How long a light lock may run on timer wakes alone before descending to soft sleep.
#define LOCK_IDLE_SOFT_SLEEP_MIN 30u

// A first low is not enough: high-pass ringing can rise again. Wait for a quiet stretch before light sleep.
static bool pickup_wait_quiet(sc7a20h_handle_t acc, int quiet_ms, int timeout_ms) {
    int64_t start = esp_timer_get_time() / 1000;
    int64_t low_from = 0;
    while (esp_timer_get_time() / 1000 - start < timeout_ms) {
        if (sc7a20h_int1_level(acc) == 0) {
            if (low_from == 0) low_from = esp_timer_get_time() / 1000;
            if (esp_timer_get_time() / 1000 - low_from >= quiet_ms) return true;
        } else {
            pickup_ack(acc);
            low_from = 0;
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
    return false;
}

// 分钟期限也用于浅睡失败/中断线常低时退回有延时的等待，不能陷入忙循环。
// The minute deadline also bounds delayed polling when sleep fails or an interrupt stays low.
static int64_t s_minute_deadline_ms;

static void arm_minute_wake(void) {
    os_time_poll(esp_timer_get_time() / 1000);
    uint32_t seconds = os_time_info()->state == OS_TIME_VALID
        ? 60 - os_time_info()->unix_utc % 60 : 60;
    s_minute_deadline_ms = esp_timer_get_time() / 1000 + (int64_t)seconds * 1000;
    esp_sleep_enable_timer_wakeup((uint64_t)seconds * 1000000);
}

static void arm_lock_wake(void) {
    // 时钟/日历/黄历随分钟走时；静态与壁纸只做时间维护。
    // Clock/calendar/almanac faces tick per minute; static and wallpaper faces only maintain time.
    uint8_t style = app_settings_lock_style();
    if ((style >= 1 && style <= 3) || style == APP_LOCK_STYLE_QUOTE) { arm_minute_wake(); return; }
    int64_t now_ms = esp_timer_get_time() / 1000;
    int64_t delay_ms = os_time_lock_sync_delay_ms(now_ms);
    if (!delay_ms) return;
    s_minute_deadline_ms = now_ms + delay_ms;
    esp_sleep_enable_timer_wakeup((uint64_t)delay_ms * 1000);
}

void app_sleep_disarm_minute_wake(void) {
    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_TIMER);
    s_minute_deadline_ms = 0;
}

static bool lock_idle_tick(unsigned* idle_minutes) {
    if (app_settings_lock_style() == APP_LOCK_STYLE_QUOTE) return false;
    return ++*idle_minutes >= LOCK_IDLE_SOFT_SLEEP_MIN;
}

static EpdRect lock_time_band(uint8_t style) {
    if (style == APP_LOCK_STYLE_QUOTE) return (EpdRect){0, 1036, UI_LOCK_WIDTH, UI_LOCK_HEIGHT - 1036};
    if (style == 1) return (EpdRect){0, 260, UI_LOCK_WIDTH, 380};
    if (style == 2) return (EpdRect){0, 980, UI_LOCK_WIDTH, 90};
    return (EpdRect){0, 780, UI_LOCK_WIDTH, 120};
}

void app_sleep_alarm_clock(bool on) {
    if (!read_pico_pmu_ready()) return;
    uint8_t payload[5] = {on ? (uint8_t)2 : (uint8_t)0, 60, 0, 0, 0};
    if (read_pico_pmu_cmd(PMU_CMD_ALARM_SET, payload, sizeof(payload)) != ESP_OK)
        ESP_LOGW(TAG, "alarm %s failed", on ? "on" : "off");
}

app_wake_source_t app_light_sleep_wait(sc7a20h_handle_t acc) {
    bool pickup = acc != NULL && app_settings_pickup_wake();
    bool acc_armed = false;
    lock_arm_ioe_wakeup();
    if (pickup) {
        sc7a20h_motion_cfg_t motion = SC7A20H_MOTION_DEFAULT();
        motion.ths_mg = 350;
        motion.duration = 3;
        sc7a20h_arm_pickup_wake(acc, &motion);
        acc_armed = true;
        pickup_ack(acc);
        if (!pickup_wait_quiet(acc, 400, 2000)) {
            pickup = false;
            ESP_LOGW(TAG, "pickup INT1 stuck high, key only");
        } else {
            sc7a20h_config_light_sleep_wakeup(acc);
            if (!pickup_wait_quiet(acc, 400, 1500)) {
                gpio_wakeup_disable(sc7a20h_int1_gpio(acc));
                pickup = false;
                ESP_LOGW(TAG, "pickup INT1 re-asserted, key only");
            } else {
                // 安静窗口里若 INT1 闪过，ESP 会留下高电平唤醒挂起，关掉再打开清掉。
                // If INT1 glitched in the quiet window, ESP keeps a high-level wake pending; disable then re-enable to clear it.
                gpio_wakeup_disable(sc7a20h_int1_gpio(acc));
                sc7a20h_config_light_sleep_wakeup(acc);
                if (sc7a20h_int1_level(acc) > 0) {
                    gpio_wakeup_disable(sc7a20h_int1_gpio(acc));
                    pickup = false;
                    ESP_LOGW(TAG, "pickup INT1 high at sleep, key only");
                } else {
                    ESP_LOGI(
                        TAG, "wait key GPIO%d or pickup GPIO%d",
                        READ_PICO_IOE_INT_GPIO, (int)sc7a20h_int1_gpio(acc)
                    );
                }
            }
        }
    } else {
        ESP_LOGI(TAG, "wait key on IOE_INT GPIO%d", READ_PICO_IOE_INT_GPIO);
    }

    app_wake_source_t wake = APP_WAKE_NONE;
    for (;;) {
        // 先收事件再清 IOE；包括睡前到达的按键，不能被 drain 吞掉。
        // Collect events before clearing IOE, including keys arriving before the first sleep.
        uint8_t ev = read_pico_pmu_take_wake_events();
        if (ev & READ_PICO_PMU_WAKE_KEY) { wake = APP_WAKE_KEY; break; }
        if (s_minute_deadline_ms && esp_timer_get_time() / 1000 >= s_minute_deadline_ms) {
            wake = APP_WAKE_TIMER;
            break;
        }
        read_pico_clear_ioe_int();
        if (gpio_get_level((gpio_num_t)READ_PICO_IOE_INT_GPIO) == 0) {
            vTaskDelay(pdMS_TO_TICKS(20));
            continue;
        }
        if (pickup && sc7a20h_int1_level(acc) > 0 && pickup_ia(acc)) {
            wake = APP_WAKE_PICKUP;
            ESP_LOGI(TAG, "pickup wake");
            break;
        }
        if (pickup && sc7a20h_int1_level(acc) > 0) {
            pickup_ack(acc);
            pickup_wait_quiet(acc, 400, 1500);
            continue;
        }
        int64_t t0 = esp_timer_get_time();
        if (s_minute_deadline_ms) {
            int64_t remaining_us = s_minute_deadline_ms * 1000 - t0;
            if (remaining_us <= 0) { wake = APP_WAKE_TIMER; break; }
            esp_sleep_enable_timer_wakeup((uint64_t)remaining_us);
        }
        esp_err_t sleep_err = esp_light_sleep_start();
        if (sleep_err != ESP_OK) {
            vTaskDelay(pdMS_TO_TICKS(20));
            continue;
        }
        int64_t duration_us = esp_timer_get_time() - t0;
        os_time_record_sleep(duration_us);
        int64_t dt_ms = duration_us / 1000;
        ev = read_pico_pmu_take_wake_events();
        if (ev & READ_PICO_PMU_WAKE_KEY) {
            wake = APP_WAKE_KEY;
            break;
        }
        if (esp_sleep_get_wakeup_cause() == ESP_SLEEP_WAKEUP_TIMER) {
            app_sleep_disarm_minute_wake();
            wake = APP_WAKE_TIMER;
            ESP_LOGI(TAG, "minute wake");
            break;
        }
        if (pickup && sc7a20h_int1_level(acc) > 0 && pickup_ia(acc)) {
            if (dt_ms < 80) {
                ESP_LOGW(TAG, "pickup ignored, sleep %lld ms", (long long)dt_ms);
                pickup_ack(acc);
                pickup_wait_quiet(acc, 400, 1500);
                continue;
            }
            wake = APP_WAKE_PICKUP;
            ESP_LOGI(TAG, "pickup wake");
            break;
        }
        if (pickup && sc7a20h_int1_level(acc) > 0) {
            pickup_ack(acc);
            pickup_wait_quiet(acc, 400, 1500);
        }
    }

    gpio_wakeup_disable((gpio_num_t)READ_PICO_IOE_INT_GPIO);
    app_sleep_disarm_minute_wake();
    if (acc_armed) {
        gpio_wakeup_disable(sc7a20h_int1_gpio(acc));
        sc7a20h_events_t ev;
        sc7a20h_read_events(acc, &ev);
        sc7a20h_power_down(acc);
        sc7a20h_int1_begin(acc);
    }
    app_settings_set_last_wake((uint8_t)wake);
    return wake;
}

void app_enter_host_sleep(app_sleep_mode_t mode) {
    // 先放掉可能保活的高压轨再交给 PMU 下电，避免停留在 HALT 循环时轨道常开。
    // Release any held HV rails before handing off to the PMU so they never stay on through the halt loop.
    epd_poweroff();
    pmu_selftest_prepare_powerdown();
    if (mode == APP_SLEEP_OFF) {
        ESP_LOGI(TAG, "power off %s", esp_err_to_name(read_pico_pmu_power_off()));
    } else {
        ESP_LOGI(TAG, "SOFT_SLEEP %s", esp_err_to_name(read_pico_pmu_report_sleep()));
    }
    while (true) vTaskDelay(pdMS_TO_TICKS(1000));
}

// 时钟锁屏：整屏铺白打底，时钟超大居中。/ Clock face: white ground, extra-large centered digits.
static void draw_lock_clock(uint8_t* fb) {
    ui_clear_page(fb);
    char clock[16], date[48];
    os_time_format_clock(clock, sizeof(clock));
    os_time_format_date(date, sizeof(date));
    // 按实际字宽收缩，兼容不同 TF 字体。/ Fit actual glyph widths across TF fonts.
    int px = 300;
    while (px > 100 && ttf_text_width_px(px, clock) > UI_LOCK_WIDTH - 64) px -= 4;
    ui_text(fb, UI_LOCK_WIDTH / 2, 300, px, clock, EPD_DRAW_ALIGN_CENTER, false);
    ui_text(fb, UI_LOCK_WIDTH / 2, 680, 48, date, EPD_DRAW_ALIGN_CENTER, false);
    ui_text(fb, UI_LOCK_WIDTH / 2, 780, 28, app_settings_sleep_mode() == APP_SLEEP_OFF ? "已锁定 · 长按电源键开机" : "已锁定 · 短按电源键唤醒", EPD_DRAW_ALIGN_CENTER, false);
}
// 当月日历锁屏：周标题 + 六行网格，今日加框；时间未校准时由调用方回退。
// Monthly calendar face: weekday header and six rows with today boxed; uncalibrated time falls back.
static void draw_lock_calendar(uint8_t* fb) {
    ui_clear_page(fb);
    const os_time_info_t* info = os_time_info();
    static const char* names[] = {"一", "二", "三", "四", "五", "六", "日"};
    int lead = (info->weekday + 6) % 7 - (int)(info->day - 1) % 7;
    int first = ((lead % 7) + 7) % 7;
    int days = info->month == 2 ? (info->year % 4 == 0 && (info->year % 100 != 0 || info->year % 400 == 0) ? 29 : 28)
              : (info->month == 4 || info->month == 6 || info->month == 9 || info->month == 11) ? 30 : 31;
    char title[48];
    snprintf(title, sizeof(title), "%u年%u月", info->year, info->month);
    ui_text(fb, UI_LOCK_WIDTH / 2, 150, 60, title, EPD_DRAW_ALIGN_CENTER, false);
    int grid_x = 60, grid_w = UI_LOCK_WIDTH - 2 * 60, cell = grid_w / 7;
    for (int i = 0; i < 7; ++i)
        ui_text(fb, grid_x + i * cell + cell / 2, 280, 34, names[i], EPD_DRAW_ALIGN_CENTER, false);
    for (int row = 0; row < 6; ++row)
        ui_hairline(fb, 360 + row * 108, grid_x, grid_w, UI_GRAY_LIGHT);
    for (int day = 1; day <= days; ++day) {
        int cell_index = first + day - 1;
        int col = cell_index % 7, row = cell_index / 7;
        if (row >= 6) break;
        char label[8];
        snprintf(label, sizeof(label), "%d", day);
        int cx = grid_x + col * cell + cell / 2, cy = 360 + row * 108 + 44;
        if ((unsigned)day == info->day) ui_draw_selected_round_rect(fb, (EpdRect){cx - 34, cy - 38, 68, 76}, 10);
        ui_text_vc(fb, cx, cy, 40, label, EPD_DRAW_ALIGN_CENTER, false);
    }
    char now[24];
    os_time_format_clock(now, sizeof(now));
    ui_text(fb, UI_LOCK_WIDTH / 2, 1030, 36, now, EPD_DRAW_ALIGN_CENTER, false);
}
// 黄历锁屏：宜读书 + 农历日期 + 时钟。/ Almanac face: reading advice, lunar date, and clock.
static void draw_lock_almanac(uint8_t* fb) {
    ui_clear_page(fb);
    const os_time_info_t* info = os_time_info();
    os_lunar_date_t lunar;
    char clock[16];
    os_time_format_clock(clock, sizeof(clock));
    ui_text(fb, UI_LOCK_WIDTH / 2, 150, 56, "小纸 Pico", EPD_DRAW_ALIGN_CENTER, false);
    ui_hairline(fb, 230, 60, UI_LOCK_WIDTH - 120, UI_GRAY_LIGHT);
    if (os_lunar_from_solar(info->year, info->month, info->day, &lunar)) {
        char lunar_text[96];
        snprintf(lunar_text, sizeof(lunar_text), "%s · %s", os_lunar_month_name(lunar.month, lunar.leap), os_lunar_day_name(lunar.day));
        ui_text(fb, UI_LOCK_WIDTH / 2, 290, 44, "农历", EPD_DRAW_ALIGN_CENTER, false);
        ui_text(fb, UI_LOCK_WIDTH / 2, 370, 52, lunar_text, EPD_DRAW_ALIGN_CENTER, false);
    }
    ui_text(fb, UI_LOCK_WIDTH / 2, 726, 36, "宜读书", EPD_DRAW_ALIGN_CENTER, false);
    ui_text(fb, UI_LOCK_WIDTH / 2, 838, 44, clock, EPD_DRAW_ALIGN_CENTER, false);
    ui_text(fb, UI_LOCK_WIDTH / 2, 960, 26, app_settings_sleep_mode() == APP_SLEEP_OFF ? "已锁定 · 长按电源键开机" : "已锁定 · 短按电源键唤醒", EPD_DRAW_ALIGN_CENTER, false);
}
// 壁纸解码缓存：每次锁屏都整幅解码既慢又在阅读后的内存压力下偶发失败回退内置图；
// 以路径+大小+mtime 为键缓存解码灰度，stat 命中直接绘制，失败留日志定位。
// Wallpaper decode cache: decoding per lock is slow and intermittently fails under
// post-reading memory pressure, falling back to the built-in image; cache the decoded
// gray keyed by path+size+mtime, blit on stat hits and log failures for diagnosis.
typedef struct {
    char path[192];
    long size;
    long mtime;
    uint8_t* gray;
    uint16_t width, height;
} wallpaper_cache_t;
static wallpaper_cache_t s_wallpaper;

static bool draw_lock_wallpaper(uint8_t* framebuffer) {
    const char* path = app_settings_lock_image();
    if (!path[0]) {
        if (s_wallpaper.gray) { free(s_wallpaper.gray); s_wallpaper.gray = NULL; s_wallpaper.path[0] = 0; }
        return false;
    }
    struct stat st;
    if (stat(path, &st) || st.st_size <= 0) {
        ESP_LOGW(TAG, "wallpaper stat failed: %s", path);
        return false;
    }
    if (!s_wallpaper.gray || strcmp(s_wallpaper.path, path) ||
        s_wallpaper.size != st.st_size || s_wallpaper.mtime != st.st_mtime) {
        free(s_wallpaper.gray);
        s_wallpaper.gray = NULL;
        uint16_t width = 0, height = 0;
        // 先读设壁纸时存的灰度快照（免解码、内存峰值 ~600KB）；未命中再实时解码，
        // 解码成功回写快照自愈（旧固件设置的壁纸或替换过的图片）。
        // Read the snapshot stored when the wallpaper was set (no decode, ~600KB peak);
        // fall back to a live decode and write the snapshot back to self-heal (wallpapers
        // set on older firmware or replaced images).
        if (!ui_image_wallpaper_cache_load(UI_IMAGE_WALLPAPER_CACHE, path, st.st_size, st.st_mtime,
                                           &s_wallpaper.gray, &width, &height)) {
            if (!ui_image_load_path(path, &s_wallpaper.gray, &width, &height)) {
                ESP_LOGW(TAG, "wallpaper decode failed size=%ld psram=%u", (long)st.st_size,
                         (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
                return false;
            }
            if (!ui_image_wallpaper_cache_store(UI_IMAGE_WALLPAPER_CACHE, path, st.st_size, st.st_mtime,
                                                s_wallpaper.gray, width, height))
                ESP_LOGW(TAG, "wallpaper snapshot store failed");
        }
        snprintf(s_wallpaper.path, sizeof(s_wallpaper.path), "%s", path);
        s_wallpaper.size = st.st_size;
        s_wallpaper.mtime = st.st_mtime;
        s_wallpaper.width = width;
        s_wallpaper.height = height;
    }
    ui_image_blit(framebuffer, s_wallpaper.gray, s_wallpaper.width, s_wallpaper.height,
                  (EpdRect){0, 0, UI_LOCK_WIDTH, UI_LOCK_HEIGHT});
    return true;
}

static bool draw_lock_excerpt(uint8_t* framebuffer) {
    book_quote_t quote;
    if (!book_quotes_lock_get(&quote)) return false;
    os_time_poll(esp_timer_get_time() / 1000);
    const os_time_info_t* time = os_time_info();
    uint32_t date = (*time).state==OS_TIME_VALID ? (*time).year*10000u+(*time).month*100u+(*time).day : 0;
    unsigned minutes = date ? book_stats_minutes(date) : 0, days = 0;
    if (date) for (int i=0;i<30;++i) if (book_stats_minutes(os_time_date_shift(date,-i))) ++days;
    ui_product_excerpt_lock(framebuffer, quote.text, quote.path, quote.chapter_title, app_settings_lock_wake_hint(),time,minutes,days,
                           app_settings_effective_lock_mode() != APP_SLEEP_OFF,false);
    return true;
}
// 按设置画锁屏面；图片样式先画壁纸、缺图回退内置静态图；时钟/日历/黄历需有效时间。
// 返回 true 表示画的是 TF 壁纸（大面积灰阶照片需要物理清白基线）。
// Paint the configured face; the image style draws the wallpaper first with the built-in
// image as fallback, while clock/calendar/almanac need valid time. True means a TF wallpaper
// was painted (full-screen photo grays need the physically cleaned baseline).
static bool draw_lock_face(uint8_t* framebuffer) {
    uint8_t style = app_settings_lock_style();
    if (style == APP_LOCK_STYLE_QUOTE) {
        if (!draw_lock_excerpt(framebuffer)) draw_static_lock(framebuffer);
        return false;
    }
    if (!style) {
        if (draw_lock_wallpaper(framebuffer)) return true;
        draw_static_lock(framebuffer);
        return false;
    }
    os_time_force_poll();
    os_time_poll(esp_timer_get_time() / 1000);
    bool valid = os_time_info()->state == OS_TIME_VALID;
    if (style == 1 && valid) { draw_lock_clock(framebuffer); return false; }
    if (style == 2 && valid) { draw_lock_calendar(framebuffer); return false; }
    if (style == 3 && valid) { draw_lock_almanac(framebuffer); return false; }
    draw_static_lock(framebuffer);
    return false;
}

/* ---- 锁屏密码 / Lock PIN ---- */
// 键盘绘制与命中共用 ui_product；挑战循环只负责输入与校验。
// The keypad draw/hit lives in ui_product; the challenge loop only feeds and verifies.
// 锁屏和输入共用无压黑清理出口。/ Lock and input share the white-clean display path.
bool app_lock_pin_challenge(EpdiyHighlevelState* hl, cst836u_handle_t tp) {
    char pin[8];
    if (!app_settings_lock_pin(pin, sizeof(pin)) || !pin[0]) return true;
    char input[8] = {0};
    char message[64] = {0};
    unsigned count = 0;
    uint8_t* fb = epd_hl_get_framebuffer(hl);
    // 唤醒用的这次按键在松开瞬间还会补一条 SHORT 事件：先等松开并抽干，
    // 否则入口循环会立刻把残余事件误读成“回锁屏”请求，密码页一闪而过。
    // The wake press completes with a SHORT event at release: wait for the
    // release and drain it, or the entry loop instantly mistakes the leftover
    // for a relock request and the keypad flashes away.
    app_lock_wait_key_idle(800);
    ui_product_lock_keypad(fb, "输入锁屏密码", message, count, false);
    // 唤醒首帧用真实旧帧进行 GC16 清理，避免丢失锁屏参考。
    // Clean the first wake frame with GC16 against the real previous lock image.
    guard_draw_result(hl, update_display_full(hl));

    // 键位高亮与圆点走跟手 DU（反馈即时且轨道保活），累计后用 GL16 定稿。
    // Key highlights and dots use follow DU (instant, rails stay hot); GL16 settles the buildup.
    unsigned quick = 0;
    for (;;) {
        int key = -1;
        for (;;) {
            vTaskDelay(pdMS_TO_TICKS(20));
            rails_idle_check(esp_timer_get_time() / 1000);
            // 输入途中按电源键＝回锁屏重睡；事件不吞掉。
            // A power-key press during entry returns to the lock face; the event is not drained away.
            if (read_pico_pmu_take_wake_events() & READ_PICO_PMU_WAKE_KEY) return false;
            cst836u_touch_t touch;
            if (cst836u_read(tp, &touch) != ESP_OK || !touch.touched || touch.count != 1) continue;
            int held = ui_product_lock_keypad_hit(touch.x, touch.y);
            if (held < 0) continue;
            // 按下立即高亮，不再等抬起才给反馈。/ Highlight on press; feedback no longer waits for release.
            ui_product_lock_key(fb, held, true);
            guard_draw_result(hl, update_display_area_with(
                hl, &E0470_FOLLOW_WAVEFORM, MODE_DU, ui_product_lock_key_rect(held)));
            ++quick;
            // 同键抬起才算一次输入；滑离键位的取消。/ Count on release over the same key; sliding off cancels.
            key = held;
            for (;;) {
                esp_err_t err = cst836u_read(tp, &touch);
                if (err != ESP_OK || touch.count > 1) { key = -1; break; }
                if (!touch.touched) break;
                if (ui_product_lock_keypad_hit(touch.x, touch.y) != key) key = -1;
                rails_idle_check(esp_timer_get_time() / 1000);
                vTaskDelay(pdMS_TO_TICKS(20));
            }
            if (key >= 0) break;
            // 滑离取消：恢复常态键位。/ Slide-off cancel: restore the key.
            ui_product_lock_key(fb, held, false);
            guard_draw_result(hl, update_display_area_with(
                hl, &E0470_FOLLOW_WAVEFORM, MODE_DU, ui_product_lock_key_rect(held)));
            ++quick;
        }
        bool clear_message = message[0] != 0;
        message[0] = 0;
        if (key == 10) count = 0;
        else if (key == 11) {
            if (count) --count;
        } else if (count < 4) {
            input[count++] = (char)('0' + key);
        }
        if (count == 4) {
            input[4] = 0;
            if (!strcmp(input, pin)) {
                return true;
            }
            snprintf(message, sizeof(message), "密码错误，请重试");
            count = 0;
            ui_product_lock_keypad(fb, "输入锁屏密码", message, count, false);
            guard_draw_result(hl, update_display_mode(hl, MODE_GL16));
            quick = 0;
            continue;
        }
        // 高层接口接收逻辑区域；整键盘重绘后只推圆点带与键位的并集。
        // High-level updates take logical coordinates; repaint the keypad and push the dots/key union only.
        ui_product_lock_keypad(fb, "输入锁屏密码", message, count, false);
        if (clear_message || quick >= UI_SETTLE_DU_MAX) {
            guard_draw_result(hl, update_display_mode(hl, MODE_GL16));
            quick = 0;
        } else {
            EpdRect dots = {(UI_LOCK_WIDTH - 336) / 2 - 10, 216, 360, 56};
            EpdRect pressed = ui_product_lock_key_rect(key);
            EpdRect both = {
                .x = dots.x < pressed.x ? dots.x : pressed.x,
                .y = dots.y < pressed.y ? dots.y : pressed.y,
                .width = 0,
                .height = 0,
            };
            int right = dots.x + dots.width, bottom = dots.y + dots.height;
            if (pressed.x + pressed.width > right) right = pressed.x + pressed.width;
            if (pressed.y + pressed.height > bottom) bottom = pressed.y + pressed.height;
            both.width = right - both.x;
            both.height = bottom - both.y;
            guard_draw_result(hl, update_display_area_with(
                hl, &E0470_FOLLOW_WAVEFORM, MODE_DU, both));
            ++quick;
        }
    }
}

void enter_lock_and_sleep(
    EpdiyHighlevelState* hl, int64_t* ignore_until_ms, sc7a20h_handle_t acc,
    cst836u_handle_t tp, uint8_t origin_app
) {
lock_again:
    // 无线仍在运行时不能进入浅睡；保留界面供关闭后重试。
    // Never enter light sleep while the radio still runs; retain the page for an explicit retry.
    if (!os_page_turner_pause()) return;
    // 先统一保存（正文进度、统计检查点），再画锁屏下电；失败不阻塞睡眠。
    // Save uniformly first (reader progress, stats checkpoints), then paint the lock face; failures never block sleep.
    app_sleep_prepare_run();
    uint8_t* framebuffer = epd_hl_get_framebuffer(hl);
    // 壁纸照片是大面积混合灰阶，单次差分 GC16 会透出上一页残影；锁屏入口先物理清白
    // 再按厂家 GC16 重画，其余锁屏面保持单次 GC16。
    // Wallpaper photos are full-screen mixed grays that ghost the previous page through a
    // single differential GC16; the lock entry physically clears white first and repaints
    // with vendor GC16, while other faces keep the single GC16 pass.
    bool wallpaper = draw_lock_face(framebuffer);
    guard_draw_result(hl, wallpaper ? update_display_clean(hl) : update_display_full(hl));
    app_lock_wait_key_idle(800);
    app_sleep_mode_t mode = app_settings_sleep_mode();
    // 动态锁屏保留本地钟与帧缓冲；壁纸与静态不随分钟变化可深睡；显式关机仍服从用户选择。
    // Dynamic faces retain the local clock and framebuffer; wallpaper and static faces never
    // change per minute so deep sleep stands; explicit power-off keeps its meaning.
    {
        uint8_t style = app_settings_lock_style();
        if (mode == APP_SLEEP_DEEP && ((style >= 1 && style <= 3) || style == APP_LOCK_STYLE_QUOTE)) mode = APP_SLEEP_LIGHT;
    }
    app_sleep_alarm_clock(false);
    ESP_LOGI(TAG, "lock %s", app_sleep_mode_name(mode));

    switch (mode) {
        case APP_SLEEP_OFF:
        case APP_SLEEP_DEEP:
            // 断电级睡眠前记录锁屏来源：下次开机直达该页；阅读标记由页内睡眠回调按需置位。
            // Record the lock origin before a power-cut sleep: the next boot returns to
            // it; the reading marker is set by the page's sleep callback when applicable.
            app_settings_set_lock_origin(origin_app);
            app_enter_host_sleep(mode);
            break;
        case APP_SLEEP_LIGHT:
        default: {
            // 分钟更新不离开锁屏；只按键/拿起返回页面。/ Minute updates stay locked; only keys/pickup return to the page.
            unsigned prev_day = os_time_info()->day;
            bool prev_valid = os_time_info()->state == OS_TIME_VALID;
            unsigned minutes = 0, idle_minutes = 0;
            for (;;) {
                epd_poweroff();
                arm_lock_wake();
                app_wake_source_t wake = app_light_sleep_wait(acc);
                if (wake != APP_WAKE_TIMER) break;
                // 定时维护在浅睡之外进行，电源键可中止；不会把联网计入睡眠补偿。
                // Maintain time outside sleep, allowing power-key cancellation without charging network time as sleep.
                while (os_time_lock_sync_tick(esp_timer_get_time() / 1000)) {
                    if (read_pico_pmu_take_wake_events() & READ_PICO_PMU_WAKE_KEY) { wake = APP_WAKE_KEY; break; }
                    vTaskDelay(pdMS_TO_TICKS(50));
                }
                os_time_lock_sync_cancel();
                if (wake != APP_WAKE_TIMER) break;
                // 锁屏久置降级：只剩分钟唤醒累计满30分钟说明是误锁后被搁置，转软睡省电。
                // Long-idle descent: thirty minutes of nothing but timer wakes means an
                // accidental lock left sitting; descend to soft sleep to save battery.
                if (lock_idle_tick(&idle_minutes)) break;
                os_time_poll(esp_timer_get_time() / 1000);
                // 静态与壁纸锁屏只维护时间；画面未改变，不重复推屏。
                // Static and wallpaper locks maintain time only; their unchanged frames need no extra presentation.
                if (app_settings_lock_style() == 0 || app_settings_lock_style() == 4) continue;
                draw_lock_face(framebuffer);
                const os_time_info_t* info = os_time_info();
                uint8_t style = app_settings_lock_style();
                // 时钟在 y=300 绘制；区域必须覆盖整字高及旧笔画，不能截在 y=360。
                // The clock starts at y=300; cover the entire glyph and previous ink, never cut at y=360.
                EpdRect band = lock_time_band(style);
                if (info->state != OS_TIME_VALID || !prev_valid || info->day != prev_day || ++minutes >= 15) {
                    guard_draw_result(hl, update_display_area_quiet(hl,
                        (EpdRect){0, 0, UI_LOCK_WIDTH, UI_LOCK_HEIGHT}));
                    minutes = 0;
                    prev_day = info->day;
                    prev_valid = info->state == OS_TIME_VALID;
                } else {
                    guard_draw_result(hl, update_display_area_quiet(hl, band));
                }
            }
            if (idle_minutes >= LOCK_IDLE_SOFT_SLEEP_MIN) {
                app_sleep_alarm_clock(false);
                app_sleep_disarm_minute_wake();
                // 降级同样断电：记录锁屏来源，开机回到锁屏那一刻的页面。
                // The descent also cuts power: record the lock origin so boot
                // returns to the page showing when locking happened.
                app_settings_set_lock_origin(origin_app);
                ESP_LOGI(TAG, "lock idle %u min, soft sleep", idle_minutes);
                app_enter_host_sleep(APP_SLEEP_DEEP);
            }
            app_sleep_alarm_clock(false);
            app_sleep_disarm_minute_wake();
            break;
        }
    }

    // 浅睡唤醒首刷保持原路径（实测浅睡不花屏）；花屏出在深睡后的冷启动，见 app_main 的双清。
    // Light-sleep wake keeps the original first-push path (no garble in testing);
    // the garble lives in the post-deep-sleep cold boot, handled by app_main's double clear.
    read_pico_pmu_drain_events();
    if (tp && app_settings_lock_pin_wake() && !app_lock_pin_challenge(hl, tp)) {
        // 密码页按了电源键：重画锁屏面再睡一轮（入口的等键释放避免同键立刻唤醒）。
        // The power key was pressed on the PIN page: repaint the lock face and sleep
        // another round (the entry key-idle wait stops the same key waking it instantly).
        ESP_LOGI(TAG, "relock from pin challenge");
        goto lock_again;
    }
    read_pico_pmu_drain_events();
    if (ignore_until_ms) {
        *ignore_until_ms = esp_timer_get_time() / 1000 + APP_LOCK_IGNORE_BOOT_MS;
    }
    ESP_LOGI(TAG, "unlocked");
}
