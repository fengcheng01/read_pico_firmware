/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 * 中文：USB 卡盘的一次性重启请求；只在解锁后的专用开机路径运行。
 * English: One-shot USB card-disk reboot request; runs only in the unlocked dedicated boot path.
 * 冻结：电脑独占 TF 卡，不导出内置存储，不自动格式化；普通启动保留 USB Serial/JTAG。
 * Frozen: The computer owns the TF card, never exports internal storage or auto-formats; normal boot retains USB Serial/JTAG.
 */
#include "os_usb_disk.h"
#include "nvs.h"
#include "esp_system.h"
esp_err_t os_usb_disk_request(void) {
    nvs_handle_t h;
    esp_err_t err = nvs_open("rp_usb", NVS_READWRITE, &h);
    if (err != ESP_OK) return err;
    err = nvs_set_u8(h, "next", 1);
    if (err == ESP_OK) err = nvs_commit(h);
    nvs_close(h);
    if (err == ESP_OK) esp_restart();
    return err;
}
bool os_usb_disk_take_request(void) {
    nvs_handle_t h;
    if (nvs_open("rp_usb", NVS_READWRITE, &h) != ESP_OK) return false;
    uint8_t next = 0;
    bool requested = nvs_get_u8(h, "next", &next) == ESP_OK && next == 1;
    esp_err_t err = ESP_OK;
    if (requested) { err = nvs_erase_key(h, "next"); if (err == ESP_OK) err = nvs_commit(h); }
    nvs_close(h);
    return requested && err == ESP_OK;
}
