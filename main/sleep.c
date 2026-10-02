/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 *
 * 锁屏进睡、浅睡等待按键或拿起、软睡/关机拉掉 EN。
 *
 * Enter lock and sleep, light-sleep wait for key or pickup, and drop EN
 * for soft sleep / power-off.
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
#include "os_time.h"
#include "pmu_selftest.h"
#include "read_pico_board.h"
#include "read_pico_pmu.h"
#include "sc7a20h_lab.h"
#include "settings.h"
#include "ui_kit.h"
#include "asset_pack.h"
#include "ui/product/ui_product.h"

static const char* TAG = "read_pico";

extern const uint8_t lock_4bpp_pack_start[] asm("_binary_lock_4bpp_pack_start");
extern const uint8_t lock_4bpp_pack_end[] asm("_binary_lock_4bpp_pack_end");

static void draw_static_lock(uint8_t* fb) {
    if (!asset_pack_unpack(lock_4bpp_pack_start, (size_t)(lock_4bpp_pack_end - lock_4bpp_pack_start),
                           fb, (size_t)UI_LOCK_WIDTH * UI_LOCK_HEIGHT / 2)) ui_clear_page(fb);
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
    bool slept = false;
    for (;;) {
        read_pico_clear_ioe_int();
        if (!slept && gpio_get_level((gpio_num_t)READ_PICO_IOE_INT_GPIO) == 0) {
            read_pico_pmu_drain_events();
            read_pico_clear_ioe_int();
            vTaskDelay(pdMS_TO_TICKS(20));
            continue;
        }
        if (!slept && pickup && sc7a20h_int1_level(acc) > 0) {
            pickup_ack(acc);
            vTaskDelay(pdMS_TO_TICKS(20));
            continue;
        }
        if (slept && gpio_get_level((gpio_num_t)READ_PICO_IOE_INT_GPIO) == 0) {
            if (read_pico_pmu_take_key_wakeup()) {
                wake = APP_WAKE_KEY;
                break;
            }
            read_pico_clear_ioe_int();
            continue;
        }
        if (slept && pickup && sc7a20h_int1_level(acc) > 0 && pickup_ia(acc)) {
            wake = APP_WAKE_PICKUP;
            ESP_LOGI(TAG, "pickup wake");
            break;
        }
        if (slept && pickup && sc7a20h_int1_level(acc) > 0) {
            pickup_ack(acc);
            pickup_wait_quiet(acc, 400, 1500);
            continue;
        }
        int64_t t0 = esp_timer_get_time();
        esp_light_sleep_start();
        slept = true;
        int64_t dt_ms = (esp_timer_get_time() - t0) / 1000;
        read_pico_clear_ioe_int();
        if (read_pico_pmu_take_key_wakeup()) {
            wake = APP_WAKE_KEY;
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
    pmu_selftest_prepare_powerdown();
    if (mode == APP_SLEEP_OFF) {
        ESP_LOGI(TAG, "power off %s", esp_err_to_name(read_pico_pmu_power_off()));
    } else {
        ESP_LOGI(TAG, "SOFT_SLEEP %s", esp_err_to_name(read_pico_pmu_report_sleep()));
    }
    while (true) vTaskDelay(pdMS_TO_TICKS(1000));
}

// 时钟锁屏：大字时间 + 日期；时间未校准时由调用方回退静态图。
// Clock lock face: large time plus date; an uncalibrated clock falls back to the static image.
static void draw_lock_clock(uint8_t* fb) {
    char clock[16], date[48];
    os_time_format_clock(clock, sizeof(clock));
    os_time_format_date(date, sizeof(date));
    ui_text(fb, UI_LOCK_WIDTH / 2, 380, 176, clock, EPD_DRAW_ALIGN_CENTER, false);
    ui_text(fb, UI_LOCK_WIDTH / 2, 620, 44, date, EPD_DRAW_ALIGN_CENTER, false);
    ui_text(fb, UI_LOCK_WIDTH / 2, 700, 26, "已锁定 · 短按电源键唤醒", EPD_DRAW_ALIGN_CENTER, false);
}
// 当月日历锁屏：周标题 + 六行网格，今日加框；时间未校准时由调用方回退。
// Monthly calendar face: weekday header and six rows with today boxed; uncalibrated time falls back.
static void draw_lock_calendar(uint8_t* fb) {
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
// 黄历锁屏：农历日大字 + 干支年/农历月 + 公历日期与时间；农历来自公开年表，
// 「宜读书」为固定文案，不是逐日宜忌数据。
// Almanac lock face: the lunar day large, ganzhi year/lunar month, plus solar
// date and clock; lunar data comes from the public table, and 宜读书 is fixed
// copy, never a per-day do/don't list.
static void draw_lock_almanac(uint8_t* fb) {
    const os_time_info_t* info = os_time_info();
    os_lunar_date_t lunar;
    if (!os_lunar_from_solar(info->year, info->month, info->day, &lunar)) {
        draw_static_lock(fb);
        return;
    }
    static const char* weekdays[] = {"日", "一", "二", "三", "四", "五", "六"};
    char solar[64], ganzhi[8], line[96], clock[16];
    snprintf(solar, sizeof(solar), "%u年%u月%u日 · 周%s",
             info->year, info->month, info->day, weekdays[info->weekday % 7]);
    os_lunar_year_ganzhi(lunar.year, ganzhi);
    ui_text(fb, UI_LOCK_WIDTH / 2, 216, 40, solar, EPD_DRAW_ALIGN_CENTER, false);
    ui_text(fb, UI_LOCK_WIDTH / 2, 320, 150, os_lunar_day_name(lunar.day), EPD_DRAW_ALIGN_CENTER, false);
    snprintf(line, sizeof(line), "%s%s年 · %s", ganzhi, os_lunar_zodiac(lunar.year),
             os_lunar_month_name(lunar.month, lunar.leap));
    ui_text(fb, UI_LOCK_WIDTH / 2, 548, 56, line, EPD_DRAW_ALIGN_CENTER, false);
    ui_hairline(fb, 672, 140, UI_LOCK_WIDTH - 280, UI_GRAY_LIGHT);
    ui_text(fb, UI_LOCK_WIDTH / 2, 726, 36, "宜读书", EPD_DRAW_ALIGN_CENTER, false);
    os_time_format_clock(clock, sizeof(clock));
    ui_text(fb, UI_LOCK_WIDTH / 2, 838, 44, clock, EPD_DRAW_ALIGN_CENTER, false);
    ui_text(fb, UI_LOCK_WIDTH / 2, 960, 26, "已锁定 · 短按电源键唤醒", EPD_DRAW_ALIGN_CENTER, false);
}
// 按设置画锁屏面；时钟/日历/黄历需要有效时间，否则回退静态图。/ Paint the configured face; clock/calendar/almanac need valid time or fall back to the static image.
static void draw_lock_face(uint8_t* framebuffer) {
    uint8_t style = app_settings_lock_style();
    if (style) {
        os_time_invalidate();
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
// The keypad draw/hit lives in ui_product; the challenge loop only feeds and verifies input.
bool app_lock_pin_challenge(EpdiyHighlevelState* hl, cst836u_handle_t tp) {
    char pin[8];
    if (!app_settings_lock_pin(pin, sizeof(pin)) || !pin[0]) return true;
    char input[8] = {0};
    char message[64] = {0};
    unsigned count = 0;
    uint8_t* fb = epd_hl_get_framebuffer(hl);
    epd_poweron();
    epd_clear();
    epd_hl_set_all_white(hl);
    for (;;) {
        ui_product_lock_keypad(fb, "输入锁屏密码", message, count, false);
        epd_hl_update_screen(hl, MODE_GC16, 25);
        int key = -1;
        for (;;) {
            vTaskDelay(pdMS_TO_TICKS(20));
            read_pico_pmu_drain_events();
            cst836u_touch_t touch;
            if (cst836u_read(tp, &touch) != ESP_OK || !touch.touched || touch.count != 1) continue;
            key = ui_product_lock_keypad_hit(touch.x, touch.y);
            if (key < 0) continue;
            // 同键抬起才算一次输入；滑离键位的取消。/ Count on release over the same key; sliding off cancels.
            while (cst836u_read(tp, &touch) == ESP_OK && touch.touched && touch.count == 1) {
                if (ui_product_lock_keypad_hit(touch.x, touch.y) != key) key = -1;
                vTaskDelay(pdMS_TO_TICKS(20));
            }
            if (key >= 0) break;
        }
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
                epd_hl_set_all_white(hl);
                epd_clear();
                return true;
            }
            snprintf(message, sizeof(message), "密码错误，请重试");
            count = 0;
        }
    }
}

void enter_lock_and_sleep(
    EpdiyHighlevelState* hl, int64_t* ignore_until_ms, sc7a20h_handle_t acc
) {
    // 先统一保存（正文进度、统计检查点），再画锁屏下电；失败不阻塞睡眠。
    // Save uniformly first (reader progress, stats checkpoints), then paint the lock face; failures never block sleep.
    app_sleep_prepare_run();
    uint8_t* framebuffer = epd_hl_get_framebuffer(hl);
    epd_poweron();
    epd_clear();
    epd_hl_set_all_white(hl);
    draw_lock_face(framebuffer);
    epd_hl_update_screen_from_white(hl, MODE_GC16, 25);

    app_lock_wait_key_idle(800);
    app_sleep_mode_t mode = app_settings_sleep_mode();
    ESP_LOGI(TAG, "lock %s", app_sleep_mode_name(mode));

    switch (mode) {
        case APP_SLEEP_OFF:
        case APP_SLEEP_DEEP:
            app_enter_host_sleep(mode);
            break;
        case APP_SLEEP_LIGHT:
        default:
            epd_poweroff();
            app_light_sleep_wait(acc);
            break;
    }

    // 参考帧和屏幕都归零，回到主循环后由当前页自己画一遍，不必知道是哪一页。
    // Zero the reference frame and the panel; the current page redraws after the loop resumes, without knowing which page it is.
    read_pico_pmu_drain_events();
    epd_poweron();
    epd_clear();
    epd_hl_set_all_white(hl);
    read_pico_pmu_drain_events();
    if (ignore_until_ms) {
        *ignore_until_ms = esp_timer_get_time() / 1000 + APP_LOCK_IGNORE_BOOT_MS;
    }
    ESP_LOGI(TAG, "unlocked");
}
