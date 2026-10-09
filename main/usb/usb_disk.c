/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 * 中文：专用 USB 卡盘启动与触屏退出，电脑独占 TF 卡，普通事件循环不运行。
 * English: Dedicated USB card-disk boot and touch exit; the computer owns TF media and the normal event loop never runs.
 * 冻结：通过工厂/PIN 门禁后启动；只导出 TF，不自动格式化，不休眠、不启 WiFi；安全弹出/断开后退出重启。
 * Frozen: Start after factory/PIN gates; expose only TF, never auto-format, sleep or start WiFi; exit reboots after eject/disconnect.
 */
#include "usb_disk.h"
#include "display.h"
#include "read_pico_sd.h"
#include "read_pico_board.h"
#include "read_pico_sd_raw.h"
#include "tinyusb.h"
#include "tinyusb_default_config.h"
#include "tinyusb_msc.h"
#include "device/usbd_pvt.h"
#include "ui_product.h"
#include "esp_system.h"
#include "esp_mac.h"
#include "esp_private/usb_phy.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdatomic.h>
#include <stdio.h>

static atomic_bool s_attached, s_mount_failed;
static tinyusb_msc_storage_handle_t s_storage;
static bool s_usb, s_msc;
static atomic_int s_delete_result;
static sdmmc_card_t* s_card;
static EpdRect exit_rect(void) { return (EpdRect){UI_MARGIN, 884, ui_content_width(), 96}; }
static EpdRect confirm_rect(int i) { return ui_row_rect(i, 2, 884, 96); }

static void usb_event(tinyusb_event_t* event, void* arg) {
    (void)arg;
    if (event->id == TINYUSB_EVENT_ATTACHED) atomic_store(&s_attached, true);
    if (event->id == TINYUSB_EVENT_DETACHED) atomic_store(&s_attached, false);
}
static void storage_event(tinyusb_msc_storage_handle_t h, tinyusb_msc_event_t* event, void* arg) {
    (void)h; (void)arg;
    if (event->id == TINYUSB_MSC_EVENT_MOUNT_FAILED || event->id == TINYUSB_MSC_EVENT_FORMAT_REQUIRED)
        atomic_store(&s_mount_failed, true);
}
static esp_err_t start_disk(void) {
    esp_err_t err = read_pico_sd_open_raw(&s_card);
    if (err != ESP_OK) return err;
    const tinyusb_msc_driver_config_t msc = {.callback = storage_event};
    err = tinyusb_msc_install_driver(&msc);
    if (err != ESP_OK) return err;
    s_msc = true;
    const tinyusb_msc_storage_config_t storage = {
        .medium.card = s_card,
        .mount_point = TINYUSB_MSC_STORAGE_MOUNT_USB,
        .fat_fs = {
            .base_path = (char*)"/usbcard",
            .config = {.format_if_mount_failed = false, .max_files = 1},
            .do_not_format = true,
        },
    };
    err = tinyusb_msc_new_storage_sdmmc(&storage, &s_storage);
    if (err != ESP_OK) return err;
    static char serial[13];
    static const char* strings[] = {"\x09\x04", "mindreset", "Read Pico SD", serial, "TF Card"};
    uint8_t mac[6];
    err = esp_efuse_mac_get_default(mac);
    if (err != ESP_OK) return err;
    snprintf(serial, sizeof(serial), "%02X%02X%02X%02X%02X%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    tinyusb_config_t config = TINYUSB_DEFAULT_CONFIG(usb_event);
    config.descriptor.string = strings;
    config.descriptor.string_count = sizeof(strings) / sizeof(strings[0]);
    err = tinyusb_driver_install(&config);
    s_usb = err == ESP_OK;
    return err;
}
void usb_disk_restore_serial(void) {
    static usb_phy_handle_t serial_phy;
    const usb_phy_config_t config = {.controller = USB_PHY_CTRL_SERIAL_JTAG, .target = USB_PHY_TARGET_INT};
    if (!serial_phy) (void)usb_new_phy(&config, &serial_phy);
}
static bool disk_ejected(void) {
    tinyusb_msc_mount_point_t point;
    return s_storage && tinyusb_msc_get_storage_mount_point(s_storage, &point) == ESP_OK &&
        point == TINYUSB_MSC_STORAGE_MOUNT_APP;
}
// 删除在 USB 任务排队，读/写回调不能与释放介质并行。/ Queue deletion on the USB task so read/write callbacks cannot race medium release.
static void delete_storage(void* arg) {
    (void)arg;
    esp_err_t err = tinyusb_msc_delete_storage(s_storage);
    if (err == ESP_OK) s_storage = NULL;
    atomic_store(&s_delete_result, err == ESP_OK ? 2 : 3);
}
static bool stop_disk(void) {
    if (s_usb) {
        int result = atomic_load(&s_delete_result);
        if (!result || result == 3) {
            tud_disconnect();
            atomic_store(&s_delete_result, 1);
            usbd_defer_func(delete_storage, NULL, false);
            return false;
        }
        if (result == 1) return false;
        if (tinyusb_driver_uninstall() != ESP_OK) return false;
        s_usb = false;
    }
    // 安装失败时没有 USB 任务，可以直接收尾。/ Failed installation has no USB task, so cleanup is direct.
    if (s_storage) { if (tinyusb_msc_delete_storage(s_storage) != ESP_OK) return false; s_storage = NULL; }
    if (s_msc) { if (tinyusb_msc_uninstall_driver() != ESP_OK) return false; s_msc = false; }
    read_pico_sd_close_raw(s_card); s_card = NULL;
    return true;
}
static void draw(EpdiyHighlevelState* hl, uint8_t* fb, const char* status, bool confirm) {
    ui_clear_page(fb);
    ui_product_header(fb, "USB 连接电脑", "TF 卡磁盘模式");
    ui_product_title(fb, (EpdRect){UI_MARGIN, 240, ui_content_width(), 150}, status, 36, 3);
    ui_product_title(fb, (EpdRect){UI_MARGIN, 462, ui_content_width(), 320},
        "电脑可直接复制图书与字体。图书放在 books 文件夹，TTF 字体放在 assets/fonts。此时暂停阅读、传书和自动睡眠。请先在电脑安全弹出磁盘，再退出；不要在写入时拔卡。",
        30, 7);
    if (confirm) {
        ui_text(fb, UI_MARGIN, 822, 28, "确认电脑已安全弹出或断开？", EPD_DRAW_ALIGN_LEFT, false);
        ui_draw_button(fb, confirm_rect(0), "继续连接", false);
        ui_draw_button(fb, confirm_rect(1), "退出并重启", false);
    } else ui_draw_button(fb, exit_rect(), "结束连接 · 返回阅读", false);
    ui_product_title(fb, (EpdRect){UI_MARGIN, 1030, ui_content_width(), 120},
        "USB 模式暂不提供刷机串口。退出重启后恢复串口，可再次刷固件。", 24, 3);
    guard_draw_result(hl, update_display_mode(hl, MODE_GL16));
}
void usb_disk_run(EpdiyHighlevelState* hl, uint8_t* fb, cst836u_handle_t tp) {
    draw(hl, fb, "正在准备 TF 卡…", false);
    esp_err_t err = start_disk();
    bool removed = false, confirm = false, closing = false, down = false, valid_press = false;
    int x0 = 0, y0 = 0, previous = -1;
    for (;;) {
        if (!removed && s_card && !read_pico_sd_present()) {
            removed = true;
            if (s_usb) tud_disconnect();
        }
        bool attached = atomic_load(&s_attached), ejected = disk_ejected();
        int state = closing ? 5 : err != ESP_OK ? 4 : removed ? 3 : ejected ? 2 : attached ? 1 : 0;
        if (state != previous) {
            char failure[96];
            snprintf(failure, sizeof(failure), "卡盘启动失败：%s\n退出后检查 TF 卡", esp_err_to_name(err));
            const char* message = state == 0 ? "等待电脑连接，请接 USB 数据线" :
                state == 1 ? atomic_load(&s_mount_failed) ? "已连接；弹出异常时请先断开 USB" : "电脑正在访问 TF 卡" :
                state == 2 ? "电脑已弹出，可结束连接" : state == 3 ? "TF 卡已拔出，本次连接已停止" :
                state == 5 ? "正在结束连接，等待写入完成…" : failure;
            draw(hl, fb, message, confirm); previous = state;
        }
        if (closing) {
            if (stop_disk()) { usb_disk_restore_serial(); esp_restart(); }
            vTaskDelay(pdMS_TO_TICKS(50));
            continue;
        }
        cst836u_touch_t touch;
        if (cst836u_read(tp, &touch) != ESP_OK) { down = valid_press = false; vTaskDelay(pdMS_TO_TICKS(30)); continue; }
        if (touch.touched && touch.count == 1 && !down) { x0 = touch.x; y0 = touch.y; valid_press = true; }
        if (touch.touched && touch.count != 1) valid_press = false;
        if (touch.touched && down && valid_press) {
            EpdRect target = !confirm ? exit_rect() : ui_rect_hit(confirm_rect(0), x0, y0) ? confirm_rect(0) : confirm_rect(1);
            if (!ui_rect_hit(target, touch.x, touch.y)) valid_press = false;
        }
        if (!touch.touched && down && valid_press) {
            if (!confirm && ui_rect_hit(exit_rect(), x0, y0)) { confirm = true; previous = -1; }
            else if (confirm && ui_rect_hit(confirm_rect(0), x0, y0)) { confirm = false; previous = -1; }
            else if (confirm && ui_rect_hit(confirm_rect(1), x0, y0)) {
                // USB 配置状态不能证明物理拔线；由用户在确认框确认电脑已安全弹出。
                // USB configuration state cannot prove cable removal; the user confirms host safe-ejection in the dialog.
                closing = true; previous = -1;
            }
            valid_press = false;
        }
        down = touch.touched;
        vTaskDelay(pdMS_TO_TICKS(30));
    }
}
