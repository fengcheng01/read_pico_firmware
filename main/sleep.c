/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 *
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
 */

#include "sleep.h"

#include "app.h"
#include "app_sleep_hooks.h"
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
#include "ttf_font.h"
#include "ui/product/ui_product.h"

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

void app_sleep_disarm_minute_wake(void) {
    esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_TIMER);
    s_minute_deadline_ms = 0;
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
    ui_text(fb, UI_LOCK_WIDTH / 2, 780, 28, "已锁定 · 短按电源键唤醒", EPD_DRAW_ALIGN_CENTER, false);
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
    ui_text(fb, UI_LOCK_WIDTH / 2, 960, 26, "已锁定 · 短按电源键唤醒", EPD_DRAW_ALIGN_CENTER, false);
}
// 按设置画锁屏面；时钟/日历/黄历需要有效时间，否则回退静态图。/ Paint the configured face; clock/calendar/almanac need valid time or fall back.
static void draw_lock_face(uint8_t* framebuffer) {
    uint8_t style = app_settings_lock_style();
    if (style) {
        os_time_force_poll();
        os_time_poll(esp_timer_get_time() / 1000);
        bool valid = os_time_info()->state == OS_TIME_VALID;
        if (style == 1 && valid) { draw_lock_clock(framebuffer); return; }
        if (style == 2 && valid) { draw_lock_calendar(framebuffer); return; }
        if (style == 3 && valid) { draw_lock_almanac(framebuffer); return; }
    }
    draw_static_lock(framebuffer);
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
            read_pico_pmu_drain_events();
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
    cst836u_handle_t tp
) {
    // 先统一保存（正文进度、统计检查点），再画锁屏下电；失败不阻塞睡眠。
    // Save uniformly first (reader progress, stats checkpoints), then paint the lock face; failures never block sleep.
    app_sleep_prepare_run();
    uint8_t* framebuffer = epd_hl_get_framebuffer(hl);
    draw_lock_face(framebuffer);
    guard_draw_result(hl, update_display_full(hl));
    app_lock_wait_key_idle(800);
    app_sleep_mode_t mode = app_settings_sleep_mode();
    // 动态锁屏保留本地钟与帧缓冲；显式关机仍服从用户选择。
    // Dynamic faces retain the local clock and framebuffer; explicit power-off keeps its meaning.
    if (mode == APP_SLEEP_DEEP && app_settings_lock_style() != 0) mode = APP_SLEEP_LIGHT;
    app_sleep_alarm_clock(false);
    ESP_LOGI(TAG, "lock %s", app_sleep_mode_name(mode));

    switch (mode) {
        case APP_SLEEP_OFF:
        case APP_SLEEP_DEEP:
            app_enter_host_sleep(mode);
            break;
        case APP_SLEEP_LIGHT:
        default: {
            // 分钟更新不离开锁屏；只按键/拿起返回页面。/ Minute updates stay locked; only keys/pickup return to the page.
            unsigned prev_day = os_time_info()->day;
            unsigned minutes = 0;
            for (;;) {
                epd_poweroff();
                if (app_settings_lock_style() != 0) arm_minute_wake();
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
                os_time_poll(esp_timer_get_time() / 1000);
                draw_lock_face(framebuffer);
                const os_time_info_t* info = os_time_info();
                uint8_t style = app_settings_lock_style();
                // 时钟在 y=300 绘制；区域必须覆盖整字高及旧笔画，不能截在 y=360。
                // The clock starts at y=300; cover the entire glyph and previous ink, never cut at y=360.
                EpdRect band = style == 1 ? (EpdRect){0, 260, UI_LOCK_WIDTH, 380}
                    : style == 2 ? (EpdRect){0, 980, UI_LOCK_WIDTH, 90}
                    : (EpdRect){0, 780, UI_LOCK_WIDTH, 120};
                if (info->state != OS_TIME_VALID || info->day != prev_day || ++minutes >= 15) {
                    guard_draw_result(hl, update_display_area_quiet(hl,
                        (EpdRect){0, 0, UI_LOCK_WIDTH, UI_LOCK_HEIGHT}));
                    minutes = 0;
                    prev_day = info->day;
                } else {
                    guard_draw_result(hl, update_display_area_quiet(hl, band));
                }
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
    if (tp && app_settings_lock_pin_wake()) app_lock_pin_challenge(hl, tp);
    read_pico_pmu_drain_events();
    if (ignore_until_ms) {
        *ignore_until_ms = esp_timer_get_time() / 1000 + APP_LOCK_IGNORE_BOOT_MS;
    }
    ESP_LOGI(TAG, "unlocked");
}
