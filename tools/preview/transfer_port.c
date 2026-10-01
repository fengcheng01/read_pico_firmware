/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 * 中文：传书页的宿主夹具：真实页面代码 + 固定网络状态，不启动 Wi-Fi/HTTP/二维码。
 * English: Host fixtures for the transfer page: real page code over fixed network state, never starting Wi-Fi/HTTP/QR.
 * 冻结：夹具不联网；同步动作只回提示；进度/时间/电量为静态展示值。
 * Frozen: Fixtures stay offline; sync actions only message; progress/time/battery are static values.
 */
#include "os_sync.h"
#include "os_time.h"
#include "read_pico_transfer.h"
#include "settings.h"
#include "ttf_font.h"
#include "ui_kit.h"
#include "ui_wifi_qr.h"
#include <stddef.h>
#include <stdio.h>
#include <string.h>

static read_pico_transfer_status_t s_status;
static bool s_wifi_configured;
static char s_wifi_ssid[33] = "Home-WiFi";
static bool s_running;

esp_err_t read_pico_transfer_start(const read_pico_transfer_cfg_t* cfg) {
    (void)cfg;
    memset(&s_status, 0, sizeof(s_status));
    s_status.mode = cfg ? cfg->mode : READ_PICO_TRANSFER_MODE_AP;
    s_status.state = READ_PICO_TRANSFER_READY;
    s_status.network_ready = true;
    s_status.sta_count = 1;
    s_running = true;
    if (s_status.mode == READ_PICO_TRANSFER_MODE_AP) {
        strcpy(s_status.ssid, "Read-Pico-8888");
        strcpy(s_status.url, "http://192.168.4.1");
    } else {
        strcpy(s_status.url, "http://192.168.1.42:8000");
    }
    return ESP_OK;
}
void read_pico_transfer_stop(void) {
    s_running = false;
    memset(&s_status, 0, sizeof(s_status));
}
bool read_pico_transfer_try_stop_if_idle(void) {
    if (s_status.state == READ_PICO_TRANSFER_UPLOADING) return false;
    read_pico_transfer_stop();
    return true;
}
void read_pico_transfer_get_status(read_pico_transfer_status_t* out) {
    if (out) *out = s_status;
}
void read_pico_transfer_service_poll(void) {}
esp_err_t read_pico_transfer_get_saved_wifi(char ssid[33], bool* configured) {
    if (ssid) snprintf(ssid, 33, "%s", s_wifi_configured ? s_wifi_ssid : "");
    if (configured) *configured = s_wifi_configured;
    return ESP_OK;
}
esp_err_t read_pico_transfer_forget_wifi(void) {
    s_wifi_configured = false;
    return ESP_OK;
}
esp_err_t read_pico_transfer_scan_wifi(read_pico_transfer_network_t out[READ_PICO_TRANSFER_SCAN_MAX],
                                       size_t* count) {
    static const char* names[] = {"Home-WiFi", "Office-2.4G", "CoffeeBar"};
    for (size_t i = 0; i < 3 && i < READ_PICO_TRANSFER_SCAN_MAX; ++i) {
        memset(&out[i], 0, sizeof(out[i]));
        snprintf(out[i].ssid, sizeof(out[i].ssid), "%s", names[i]);
        out[i].rssi = -40 - (int8_t)i * 8;
        out[i].supported = true;
        out[i].requires_password = i != 2;
    }
    *count = 3;
    return ESP_OK;
}
esp_err_t read_pico_transfer_save_wifi(const char* ssid, const char* password) {
    (void)password;
    if (!ssid || !*ssid) return ESP_ERR_INVALID_ARG;
    snprintf(s_wifi_ssid, sizeof(s_wifi_ssid), "%s", ssid);
    s_wifi_configured = true;
    return ESP_OK;
}

size_t heap_caps_get_free_size(uint32_t caps) { (void)caps; return 5 * 1024 * 1024; }


// 二维码占位：真实矩阵在真机生成，预览画框示意。/ QR placeholder: real matrices come from hardware; preview frames the spot.
bool ui_wifi_qr_prepare(const char* ssid, const char* password) { (void)ssid; (void)password; return true; }
bool ui_wifi_qr_prepare_url(const char* url) { (void)url; return true; }
void ui_wifi_qr_clear(void) {}
void ui_wifi_qr_draw(uint8_t* fb, EpdRect area) {
    ui_draw_round_rect(fb, area, 12, UI_GRAY_BLACK);
    ui_text_vc(fb, area.x + area.width / 2, area.y + area.height / 2, 26, "二维码", EPD_DRAW_ALIGN_CENTER, false);
}

void os_sync_set_password(const char* plain) {
    (void)plain;
    // 预览不计算 MD5，写入合法长度占位以便页面显示“已设置”。
    // Preview skips MD5 and stores a valid-length placeholder so the page shows "set".
    app_settings_set_sync_key("preview00000000000000000000000000");
}
os_sync_result_t os_sync_device_auth(char* note, size_t cap) {
    if (note && cap) snprintf(note, cap, "预览不联网，请在真机验证");
    return OS_SYNC_OFFLINE;
}
os_sync_result_t os_sync_device_register(char* note, size_t cap) {
    if (note && cap) snprintf(note, cap, "预览不联网，请在真机验证");
    return OS_SYNC_OFFLINE;
}
os_sync_result_t os_sync_push_last(char* note, size_t cap) {
    if (note && cap) snprintf(note, cap, "预览不联网，请在真机验证");
    return OS_SYNC_OFFLINE;
}
os_sync_result_t os_sync_pull_last(char* note, size_t cap) {
    if (note && cap) snprintf(note, cap, "预览不联网，请在真机验证");
    return OS_SYNC_OFFLINE;
}

int os_time_battery_permille(void) { return 780; }
bool os_time_recently_synced(void) { return false; }

bool os_crash_summary(char* out, size_t cap) {
    // 预览读不到复位寄存器；返回空让存储页显示“暂无记录”。
    // Preview cannot read reset registers; empty output shows "no records" on the storage page.
    if (out && cap) out[0] = 0;
    return false;
}
