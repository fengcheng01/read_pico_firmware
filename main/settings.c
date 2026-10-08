/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 *
 * NVS 读写。打开失败就用深睡默认值，不擦除整个分区。
 *
 * NVS load/store. A failed open keeps the deep-sleep default; the
 * partition is not erased.
 *
 * 冻结：用户授权可切换的夜间残影测试版，新增实验项默认定向补黑；既有夜间、直刷及清理周期选项不变。
 * Frozen: The user authorizes a switchable night-ghosting test build with black reinforcement as the new experiment default; existing night, direct and cleanup interval choices remain intact.
 */

#include "settings.h"

#include <stdio.h>
#include <string.h>

#include "os_sync.h"

#include "esp_log.h"
#include "nvs.h"
#include "nvs_flash.h"

#define TAG "settings"
#define NVS_NS "read_pico"
#define NVS_KEY_SLEEP "sleep"
#define NVS_KEY_FONT "font"
#define NVS_KEY_WAKE "lwake"
#define NVS_KEY_BOOT "lboot"
#define NVS_KEY_PICKUP "pickup"
#define NVS_KEY_BOOK_PX "bk_px"
#define NVS_KEY_BOOK_SHAKE "bk_shake"
#define NVS_KEY_TZ "tz_qh"
#define NVS_KEY_LEAD "bk_lead"
#define NVS_KEY_MARGIN "bk_marg"
#define NVS_KEY_GUIDE "bk_guide"
#define NVS_KEY_INDENT "bk_ind"
#define NVS_KEY_PARA "bk_para"
#define NVS_KEY_AUTO "bk_auto"
#define NVS_KEY_TAP "bk_tap"
#define NVS_KEY_NIGHT "bk_night"
#define NVS_KEY_LOCK_STYLE "lk_style"
#define NVS_KEY_LOCK_PIN "lk_pin"
#define NVS_KEY_LOCK_PIN_WAKE "lk_pinwk"
#define NVS_KEY_SYNC_URL "sy_url"
#define NVS_KEY_SYNC_USER "sy_user"
#define NVS_KEY_SYNC_KEY "sy_key"
#define NVS_KEY_SYNC_AUTO "sy_auto"
#define NVS_KEY_ALIGN "bk_align"
#define NVS_KEY_F_CLOCK "ft_clock"
#define NVS_KEY_F_BATT "ft_batt"
#define NVS_KEY_F_BAR "ft_bar"
#define NVS_KEY_IDLE "idle_min"
#define NVS_KEY_GC_EVERY "gc_every"
#define NVS_KEY_BOOK_DIRECT "bk_direct"
#define NVS_KEY_BOOK_NIGHT_CLEAN "bk_ngclean"
#define FONT_PATH_MAX 160

static app_sleep_mode_t s_sleep = APP_SLEEP_DEEP;
static char s_font[FONT_PATH_MAX];
static uint8_t s_last_wake;
static uint8_t s_last_boot;
static bool s_pickup_wake;
static uint8_t s_book_px = 48;
static bool s_book_shake;
static int8_t s_tz_qh = 32;
static int32_t s_sleep_clock_ppm;
static bool s_sleep_clock_valid, s_clock_auto = true;
static uint8_t s_book_leading;
static uint8_t s_book_margin;
static uint8_t s_book_guide;
static bool s_book_indent = true;
static uint8_t s_book_para;
static uint8_t s_book_auto;
static bool s_book_tap = true;
static uint8_t s_book_tap_layout;
static uint8_t s_book_tap_zones[9] = {1, 3, 2, 1, 3, 2, 1, 3, 2}; // 默认九宫格：左=上页(1), 中=菜单(3), 右=下页(2)
static bool s_book_night;
static uint8_t s_lock_style;
static char s_lock_pin[8];
static bool s_lock_pin_wake = true;
static char s_sync_url[OS_SYNC_URL_MAX] = "https://sync.koreader.rocks";
static char s_sync_user[OS_SYNC_USER_MAX];
static char s_sync_key[33];
static bool s_sync_auto;
static uint8_t s_book_align;
static bool s_footer_clock, s_footer_battery, s_footer_bar = true;
static uint8_t s_idle_lock;
// 与 app_config.h 的原编译期档一致，保持升级无行为变化。/ Matches the old compile-time tier in app_config.h; upgrades keep behavior.
static uint8_t s_gc_every = 5;
static bool s_book_direct;
static uint8_t s_book_night_cleanup = BOOK_NIGHT_CLEAN_BLACK;

static uint8_t valid_book_px(uint8_t px) {
    return px >= 36 && px <= 72 && (px - 36) % 4 == 0 ? px : 48;
}

// 档位与 app_config.h 原值兼容：0 关，或 3..30 之间的常用档。/ Tiers stay compatible with the old app_config.h value: 0 off, or common steps within 3..30.
static bool gc_every_valid(uint8_t every) {
    return every == 0 || every == 3 || every == 5 || every == 10 ||
           every == 14 || every == 20 || every == 30;
}

void app_settings_init(void) {
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        err = nvs_flash_init();
    }
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "nvs init %s, use deep sleep", esp_err_to_name(err));
        return;
    }

    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READONLY, &h) != ESP_OK) return;
    uint8_t raw = APP_SLEEP_DEEP;
    if (nvs_get_u8(h, NVS_KEY_SLEEP, &raw) == ESP_OK && raw <= APP_SLEEP_OFF) {
        s_sleep = (app_sleep_mode_t)raw;
    }
    size_t font_len = sizeof(s_font);
    if (nvs_get_str(h, NVS_KEY_FONT, s_font, &font_len) != ESP_OK) {
        s_font[0] = '\0';
    }
    uint8_t wake = 0;
    if (nvs_get_u8(h, NVS_KEY_WAKE, &wake) == ESP_OK) s_last_wake = wake;
    uint8_t boot = 0;
    if (nvs_get_u8(h, NVS_KEY_BOOT, &boot) == ESP_OK) s_last_boot = boot;
    uint8_t pickup = 0;
    if (nvs_get_u8(h, NVS_KEY_PICKUP, &pickup) == ESP_OK) s_pickup_wake = pickup != 0;
    uint8_t book_px = 48, book_shake = 0;
    if (nvs_get_u8(h, NVS_KEY_BOOK_PX, &book_px) == ESP_OK) s_book_px = valid_book_px(book_px);
    if (nvs_get_u8(h, NVS_KEY_BOOK_SHAKE, &book_shake) == ESP_OK) s_book_shake = book_shake != 0;
    int8_t tz = 32;
    if (nvs_get_i8(h, NVS_KEY_TZ, &tz) == ESP_OK && tz >= -47 && tz <= 47) s_tz_qh = tz;
    uint8_t lead = 0, margin = 0, guide = 0, tap = 1, night = 0, style = 0;
    if (nvs_get_u8(h, NVS_KEY_LEAD, &lead) == ESP_OK && lead <= 30) s_book_leading = lead;
    if (nvs_get_u8(h, NVS_KEY_MARGIN, &margin) == ESP_OK && margin <= 2) s_book_margin = margin;
    if (nvs_get_u8(h, NVS_KEY_GUIDE, &guide) == ESP_OK && guide <= 2) s_book_guide = guide;
    uint8_t indent = 1, para = 0, auto_turn = 0;
    if (nvs_get_u8(h, NVS_KEY_INDENT, &indent) == ESP_OK) s_book_indent = indent != 0;
    if (nvs_get_u8(h, NVS_KEY_PARA, &para) == ESP_OK && para <= 1) s_book_para = para;
    if (nvs_get_u8(h, NVS_KEY_AUTO, &auto_turn) == ESP_OK && auto_turn <= 3) s_book_auto = auto_turn;
    if (nvs_get_u8(h, NVS_KEY_TAP, &tap) == ESP_OK) s_book_tap = tap != 0;
    uint8_t zones = 0;
    if (nvs_get_u8(h, "bk_zones", &zones) == ESP_OK && zones < 4) s_book_tap_layout = zones;
    uint32_t packed_zones = 0;
    if (nvs_get_u32(h, "bk_zones9", &packed_zones) == ESP_OK) {
        for (int i = 0; i < 9; ++i) s_book_tap_zones[i] = (uint8_t)((packed_zones >> (i * 2)) & 0x03);
    }
    if (nvs_get_u8(h, NVS_KEY_NIGHT, &night) == ESP_OK) s_book_night = night != 0;
    if (nvs_get_u8(h, NVS_KEY_LOCK_STYLE, &style) == ESP_OK && style <= 3) s_lock_style = style;
    size_t sync_len = sizeof(s_sync_url);
    if (nvs_get_str(h, NVS_KEY_SYNC_URL, s_sync_url, &sync_len) != ESP_OK) s_sync_url[0] = 0;
    sync_len = sizeof(s_sync_user);
    if (nvs_get_str(h, NVS_KEY_SYNC_USER, s_sync_user, &sync_len) != ESP_OK) s_sync_user[0] = 0;
    sync_len = sizeof(s_sync_key);
    if (nvs_get_str(h, NVS_KEY_SYNC_KEY, s_sync_key, &sync_len) != ESP_OK) s_sync_key[0] = 0;
    uint8_t sync_auto = 0;
    if (nvs_get_u8(h, NVS_KEY_SYNC_AUTO, &sync_auto) == ESP_OK) s_sync_auto = sync_auto != 0;
    uint8_t align = 0, idle = 0, f_clock = 0, f_batt = 0, f_bar = 1;
    if (nvs_get_u8(h, NVS_KEY_ALIGN, &align) == ESP_OK && align <= 2) s_book_align = align;
    if (nvs_get_u8(h, NVS_KEY_F_CLOCK, &f_clock) == ESP_OK) s_footer_clock = f_clock != 0;
    if (nvs_get_u8(h, NVS_KEY_F_BATT, &f_batt) == ESP_OK) s_footer_battery = f_batt != 0;
    if (nvs_get_u8(h, NVS_KEY_F_BAR, &f_bar) == ESP_OK) s_footer_bar = f_bar != 0;
    if (nvs_get_u8(h, NVS_KEY_IDLE, &idle) == ESP_OK &&
        (idle == 0 || idle == 5 || idle == 10 || idle == 30)) s_idle_lock = idle;
    uint8_t gc_every = 5;
    if (nvs_get_u8(h, NVS_KEY_GC_EVERY, &gc_every) == ESP_OK && gc_every_valid(gc_every)) s_gc_every = gc_every;
    uint8_t direct = 0;
    if (nvs_get_u8(h, NVS_KEY_BOOK_DIRECT, &direct) == ESP_OK) s_book_direct = direct == 1;
    uint8_t night_cleanup = BOOK_NIGHT_CLEAN_BLACK;
    if (nvs_get_u8(h, NVS_KEY_BOOK_NIGHT_CLEAN, &night_cleanup) != ESP_OK ||
        night_cleanup > BOOK_NIGHT_CLEAN_LOCAL) night_cleanup = BOOK_NIGHT_CLEAN_BLACK;
    s_book_night_cleanup = night_cleanup;
    uint32_t clock_ppm = 0;
    if (nvs_get_u32(h, "sl_clk_ppm", &clock_ppm) == ESP_OK && clock_ppm <= 20000) {
        s_sleep_clock_ppm = (int32_t)clock_ppm - 10000;
        s_sleep_clock_valid = true;
    }
    uint8_t clock_auto = 1;
    if (nvs_get_u8(h, "clk_auto", &clock_auto) == ESP_OK) s_clock_auto = clock_auto != 0;
    size_t pin_len = sizeof(s_lock_pin);
    if (nvs_get_str(h, NVS_KEY_LOCK_PIN, s_lock_pin, &pin_len) != ESP_OK) s_lock_pin[0] = '\0';
    // 旧数据不是 4 位数字就视为未设密码，不阻塞启动。/ Legacy junk other than 4 digits counts as unarmed.
    size_t pin_size = strnlen(s_lock_pin, sizeof(s_lock_pin));
    bool pin_ok = pin_size == 0 || pin_size == 4;
    for (size_t i = 0; pin_ok && i < pin_size; ++i) pin_ok = s_lock_pin[i] >= '0' && s_lock_pin[i] <= '9';
    if (!pin_ok) s_lock_pin[0] = '\0';
    uint8_t pin_wake = 1;
    if (nvs_get_u8(h, NVS_KEY_LOCK_PIN_WAKE, &pin_wake) == ESP_OK) s_lock_pin_wake = pin_wake != 0;
    nvs_close(h);
    ESP_LOGI(
        TAG, "sleep mode %s, font %s",
        app_sleep_mode_name(s_sleep),
        s_font[0] != '\0' ? s_font : "(builtin)"
    );
}

app_sleep_mode_t app_settings_sleep_mode(void) {
    return s_sleep;
}

const char* app_sleep_mode_name(app_sleep_mode_t mode) {
    switch (mode) {
        case APP_SLEEP_LIGHT: return "light";
        case APP_SLEEP_DEEP: return "deep";
        case APP_SLEEP_OFF: return "off";
        default: return "?";
    }
}

void app_settings_set_sleep_mode(app_sleep_mode_t mode) {
    if (mode > APP_SLEEP_OFF) mode = APP_SLEEP_DEEP;
    s_sleep = mode;
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READWRITE, &h) != ESP_OK) return;
    nvs_set_u8(h, NVS_KEY_SLEEP, (uint8_t)mode);
    nvs_commit(h);
    nvs_close(h);
}

const char* app_settings_font_path(void) {
    return s_font;
}

static void nvs_put_u8(const char* key, uint8_t value) {
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READWRITE, &h) != ESP_OK) return;
    nvs_set_u8(h, key, value);
    nvs_commit(h);
    nvs_close(h);
}

uint8_t app_settings_last_wake(void) {
    return s_last_wake;
}

void app_settings_set_last_wake(uint8_t src) {
    if (src == s_last_wake) return;
    s_last_wake = src;
    nvs_put_u8(NVS_KEY_WAKE, src);
}

uint8_t app_settings_last_boot(void) {
    return s_last_boot;
}

void app_settings_set_last_boot(uint8_t reason) {
    if (reason == 0 || reason == s_last_boot) return;
    s_last_boot = reason;
    nvs_put_u8(NVS_KEY_BOOT, reason);
}

bool app_settings_pickup_wake(void) {
    return s_pickup_wake;
}

void app_settings_set_pickup_wake(bool on) {
    if (s_pickup_wake == on) return;
    s_pickup_wake = on;
    nvs_put_u8(NVS_KEY_PICKUP, on ? 1 : 0);
}

void app_settings_set_font_path(const char* path) {
    if (path == NULL) path = "";
    strlcpy(s_font, path, sizeof(s_font));
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READWRITE, &h) != ESP_OK) return;
    nvs_set_str(h, NVS_KEY_FONT, s_font);
    nvs_commit(h);
    nvs_close(h);
}

uint8_t app_settings_book_px(void) {
    return s_book_px;
}

void app_settings_set_book_px(uint8_t px) {
    px = valid_book_px(px);
    if (s_book_px == px) return;
    s_book_px = px;
    nvs_put_u8(NVS_KEY_BOOK_PX, px);
}

bool app_settings_book_shake(void) {
    return s_book_shake;
}

void app_settings_set_book_shake(bool on) {
    if (s_book_shake == on) return;
    s_book_shake = on;
    nvs_put_u8(NVS_KEY_BOOK_SHAKE, on ? 1 : 0);
}

int8_t app_settings_tz_qh(void) {
    return s_tz_qh;
}

void app_settings_set_tz_qh(int8_t qh) {
    if (qh < -47 || qh > 47) return;
    if (s_tz_qh == qh) return;
    s_tz_qh = qh;
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READWRITE, &h) != ESP_OK) return;
    nvs_set_i8(h, NVS_KEY_TZ, qh);
    nvs_commit(h);
    nvs_close(h);
}

static void nvs_put_u8_checked(const char* key, uint8_t value) {
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READWRITE, &h) != ESP_OK) return;
    nvs_set_u8(h, key, value);
    nvs_commit(h);
    nvs_close(h);
}

uint8_t app_settings_book_leading(void) { return s_book_leading; }

void app_settings_set_book_leading(uint8_t percent) {
    if (percent > 30) return;
    if (s_book_leading == percent) return;
    s_book_leading = percent;
    nvs_put_u8_checked(NVS_KEY_LEAD, percent);
}

uint8_t app_settings_book_margin(void) { return s_book_margin; }

void app_settings_set_book_margin(uint8_t tier) {
    if (tier > 2) return;
    if (s_book_margin == tier) return;
    s_book_margin = tier;
    nvs_put_u8_checked(NVS_KEY_MARGIN, tier);
}

uint8_t app_settings_book_guide(void) { return s_book_guide; }

void app_settings_set_book_guide(uint8_t style) {
    if (style > 2) return;
    if (s_book_guide == style) return;
    s_book_guide = style;
    nvs_put_u8_checked(NVS_KEY_GUIDE, style);
}

bool app_settings_book_indent(void) { return s_book_indent; }

void app_settings_set_book_indent(bool on) {
    if (s_book_indent == on) return;
    s_book_indent = on;
    nvs_put_u8_checked(NVS_KEY_INDENT, on ? 1 : 0);
}

uint8_t app_settings_book_para(void) { return s_book_para; }

void app_settings_set_book_para(uint8_t tier) {
    if (tier > 1) return;
    if (s_book_para == tier) return;
    s_book_para = tier;
    nvs_put_u8_checked(NVS_KEY_PARA, tier);
}

uint8_t app_settings_book_auto(void) { return s_book_auto; }

void app_settings_set_book_auto(uint8_t tier) {
    if (tier > 3) return;
    if (s_book_auto == tier) return;
    s_book_auto = tier;
    nvs_put_u8_checked(NVS_KEY_AUTO, tier);
}

bool app_settings_book_tap(void) { return s_book_tap; }

void app_settings_set_book_tap(bool on) {
    if (s_book_tap == on) return;
    s_book_tap = on;
    nvs_put_u8_checked(NVS_KEY_TAP, on ? 1 : 0);
}

bool app_settings_book_night(void) { return s_book_night; }

void app_settings_set_book_night(bool on) {
    if (s_book_night == on) return;
    s_book_night = on;
    nvs_put_u8_checked(NVS_KEY_NIGHT, on ? 1 : 0);
}

uint8_t app_settings_lock_style(void) { return s_lock_style; }

void app_settings_set_lock_style(uint8_t style) {
    if (style > 3) return;
    if (s_lock_style == style) return;
    s_lock_style = style;
    nvs_put_u8_checked(NVS_KEY_LOCK_STYLE, style);
}

bool app_settings_lock_pin_wake(void) { return s_lock_pin_wake; }

void app_settings_set_lock_pin_wake(bool on) {
    if (s_lock_pin_wake == on) return;
    s_lock_pin_wake = on;
    nvs_put_u8_checked(NVS_KEY_LOCK_PIN_WAKE, on ? 1 : 0);
}

bool app_settings_lock_pin(char* out, size_t cap) {
    if (out && cap) {
        strncpy(out, s_lock_pin, cap - 1);
        out[cap - 1] = '\0';
    }
    return s_lock_pin[0] != '\0';
}

static void nvs_put_str(const char* key, const char* value) {
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READWRITE, &h) != ESP_OK) return;
    nvs_set_str(h, key, value);
    nvs_commit(h);
    nvs_close(h);
}

const char* app_settings_sync_url(void) { return s_sync_url; }

void app_settings_set_sync_url(const char* url) {
    snprintf(s_sync_url, sizeof(s_sync_url), "%s", url ? url : "");
    nvs_put_str(NVS_KEY_SYNC_URL, s_sync_url);
}

const char* app_settings_sync_user(void) { return s_sync_user; }

void app_settings_set_sync_user(const char* user) {
    snprintf(s_sync_user, sizeof(s_sync_user), "%s", user ? user : "");
    nvs_put_str(NVS_KEY_SYNC_USER, s_sync_user);
}

const char* app_settings_sync_key(void) { return s_sync_key; }

void app_settings_set_sync_key(const char* key) {
    snprintf(s_sync_key, sizeof(s_sync_key), "%s", key ? key : "");
    nvs_put_str(NVS_KEY_SYNC_KEY, s_sync_key);
}

uint8_t app_settings_book_align(void) { return s_book_align; }

void app_settings_set_book_align(uint8_t align) {
    if (align > 2) return;
    if (s_book_align == align) return;
    s_book_align = align;
    nvs_put_u8_checked(NVS_KEY_ALIGN, align);
}

bool app_settings_footer_clock(void) { return s_footer_clock; }

void app_settings_set_footer_clock(bool on) {
    if (s_footer_clock == on) return;
    s_footer_clock = on;
    nvs_put_u8_checked(NVS_KEY_F_CLOCK, on ? 1 : 0);
}

bool app_settings_footer_battery(void) { return s_footer_battery; }

void app_settings_set_footer_battery(bool on) {
    if (s_footer_battery == on) return;
    s_footer_battery = on;
    nvs_put_u8_checked(NVS_KEY_F_BATT, on ? 1 : 0);
}

bool app_settings_footer_bar(void) { return s_footer_bar; }

void app_settings_set_footer_bar(bool on) {
    if (s_footer_bar == on) return;
    s_footer_bar = on;
    nvs_put_u8_checked(NVS_KEY_F_BAR, on ? 1 : 0);
}

uint8_t app_settings_idle_lock_min(void) { return s_idle_lock; }

void app_settings_set_idle_lock_min(uint8_t minutes) {
    if (minutes != 0 && minutes != 5 && minutes != 10 && minutes != 30) return;
    if (s_idle_lock == minutes) return;
    s_idle_lock = minutes;
    nvs_put_u8_checked(NVS_KEY_IDLE, minutes);
}

uint8_t app_settings_gc_every(void) { return s_gc_every; }

bool app_settings_book_direct(void) { return s_book_direct; }
void app_settings_set_book_direct(bool on) {
    if (s_book_direct == on) return;
    s_book_direct = on;
    nvs_put_u8_checked(NVS_KEY_BOOK_DIRECT, on);
}

uint8_t app_settings_book_night_cleanup(void) { return s_book_night_cleanup; }
void app_settings_set_book_night_cleanup(uint8_t mode) {
    if (mode > BOOK_NIGHT_CLEAN_LOCAL || s_book_night_cleanup == mode) return;
    s_book_night_cleanup = mode;
    nvs_put_u8_checked(NVS_KEY_BOOK_NIGHT_CLEAN, mode);
}

void app_settings_set_gc_every(uint8_t every) {
    if (!gc_every_valid(every) || s_gc_every == every) return;
    s_gc_every = every;
    nvs_put_u8_checked(NVS_KEY_GC_EVERY, every);
}

bool app_settings_sync_auto(void) { return s_sync_auto; }

void app_settings_set_sync_auto(bool on) {
    if (s_sync_auto == on) return;
    s_sync_auto = on;
    nvs_put_u8_checked(NVS_KEY_SYNC_AUTO, on ? 1 : 0);
}

bool app_settings_set_lock_pin(const char* pin) {
    size_t len = pin ? strnlen(pin, 8) : 0;
    if (len != 0 && len != 4) return false;
    for (size_t i = 0; i < len; ++i)
        if (pin[i] < '0' || pin[i] > '9') return false;
    snprintf(s_lock_pin, sizeof(s_lock_pin), "%s", pin ? pin : "");
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READWRITE, &h) != ESP_OK) return false;
    nvs_set_str(h, NVS_KEY_LOCK_PIN, s_lock_pin);
    esp_err_t err = nvs_commit(h);
    nvs_close(h);
    return err == ESP_OK;
}

uint8_t app_settings_book_tap_layout(void) { return s_book_tap_layout; }
void app_settings_set_book_tap_layout(uint8_t layout) {
    if (layout > 3 || layout == s_book_tap_layout) return;
    s_book_tap_layout = layout;
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READWRITE, &h) == ESP_OK) {
        nvs_set_u8(h, "bk_zones", layout); nvs_commit(h); nvs_close(h);
    }
}

uint8_t app_settings_book_tap_zone(uint8_t zone_idx) {
    if (zone_idx >= 9) return 0;
    return s_book_tap_zones[zone_idx];
}

void app_settings_set_book_tap_zone(uint8_t zone_idx, uint8_t action) {
    if (zone_idx >= 9 || action > 3) return;
    s_book_tap_zones[zone_idx] = action;
    uint32_t packed = 0;
    for (int i = 0; i < 9; ++i) packed |= ((uint32_t)(s_book_tap_zones[i] & 0x03) << (i * 2));
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READWRITE, &h) == ESP_OK) {
        nvs_set_u32(h, "bk_zones9", packed);
        nvs_commit(h);
        nvs_close(h);
    }
}

void app_settings_reset_book_tap_zones_default(void) {
    static const uint8_t def[9] = {1, 3, 2, 1, 3, 2, 1, 3, 2};
    for (int i = 0; i < 9; ++i) s_book_tap_zones[i] = def[i];
    uint32_t packed = 0;
    for (int i = 0; i < 9; ++i) packed |= ((uint32_t)(s_book_tap_zones[i] & 0x03) << (i * 2));
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READWRITE, &h) == ESP_OK) {
        nvs_set_u32(h, "bk_zones9", packed);
        nvs_commit(h);
        nvs_close(h);
    }
}

int32_t app_settings_sleep_clock_ppm(void) { return s_sleep_clock_ppm; }
void app_settings_set_sleep_clock_ppm(int32_t ppm) {
    if (ppm < -10000 || ppm > 10000 || (ppm == s_sleep_clock_ppm && s_sleep_clock_valid)) return;
    s_sleep_clock_ppm = ppm;
    s_sleep_clock_valid = false;
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READWRITE, &h) != ESP_OK) return;
    if (nvs_set_u32(h, "sl_clk_ppm", (uint32_t)(ppm + 10000)) == ESP_OK && nvs_commit(h) == ESP_OK)
        s_sleep_clock_valid = true;
    nvs_close(h);
}
bool app_settings_sleep_clock_valid(void) { return s_sleep_clock_valid; }
bool app_settings_clock_auto(void) { return s_clock_auto; }
void app_settings_set_clock_auto(bool on) {
    s_clock_auto = on;
    nvs_handle_t h;
    if (nvs_open(NVS_NS, NVS_READWRITE, &h) != ESP_OK) return;
    nvs_set_u8(h, "clk_auto", on); nvs_commit(h); nvs_close(h);
}
