/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 *
 * 中文：设置下的存储子页。展示 TF 卡与内置书库的只读状态和容量、系统崩溃
 * 记录摘要，可手动重新检测；格式化、蜂鸣器等操作保留在诊断页，不在本页出现。
 * English: Storage subpage under Settings. Read-only TF and internal library
 * status with capacities plus the crash-record summary and manual re-probing;
 * formatting and the buzzer stay in diagnostics and never appear here.
 *
 * 用户批准 USB 卡盘入口：确认后重启进入电脑独占模式，避免两端同时挂载。
 * User-approved USB disk entry: confirm then reboot to computer-exclusive mode to avoid dual mounts.
 * 冻结：不格式化、不建目录、不写卡；USB 导出整张 TF 卡由电脑写入；探测失败如实显示；render 只绘图。
 * Frozen: Never format, create directories or write cards locally; USB exports TF for host writes; probe failures
 * display as-is; render only paints.
 */
#include "app.h"
#include "app_registry.h"
#include "book_store.h"
#include "os_catalog.h"
#include "os_crash.h"
#include "os_device.h"
#include "os_usb_disk.h"
#include "ttf_font.h"
#include "read_pico_sd.h"
#include "ui_gesture.h"
#include "ui_menu.h"
#include "ui_product.h"
#include <stdio.h>

static EpdRect back_rect(void) { return ui_product_back_rect(); }
static EpdRect refresh_rect(void) { return (EpdRect){UI_MARGIN, 880, 260, 84}; }

static EpdRect usb_rect(void) { return (EpdRect){324, 880, 320, 84}; }
static bool s_usb_confirm, s_usb_start;
static char s_usb_message[96];
static bool s_probing = true;
static read_pico_sd_info_t s_sd;
static book_store_root_t s_roots[2];
static int s_root_count;
static uint64_t s_flash_free;
static bool s_flash_usable;
static char s_crash[64];
static os_crash_summary_state_t s_crash_state;

static void format_bytes(uint64_t bytes, char* out, size_t cap) {
    if (bytes >= 1024ULL * 1024ULL * 1024ULL)
        snprintf(out, cap, "%.1f GB", bytes / 1073741824.0);
    else if (bytes >= 1024ULL * 1024ULL)
        snprintf(out, cap, "%.1f MB", bytes / 1048576.0);
    else
        snprintf(out, cap, "%llu KB", (unsigned long long)(bytes / 1024ULL));
}

static void start_probe(app_ctx_t* ctx) {
    (void)ctx;
    s_probing = true;
    os_storage_probe();
}

static void on_enter(app_ctx_t* ctx) { s_usb_confirm = s_usb_start = false; s_usb_message[0] = 0; start_probe(ctx); }

static app_redraw_t on_tick(app_ctx_t* ctx) {
    if (ctx->consumed) return APP_REDRAW_NONE;
    if (s_usb_start) {
        esp_err_t err = ttf_font_suspend_sd(true);
        if (err == ESP_OK) err = read_pico_sd_sync();
        if (err == ESP_ERR_NOT_FINISHED) return APP_REDRAW_NONE;
        if (err == ESP_OK) err = os_usb_disk_request();
        ttf_font_suspend_sd(false);
        s_usb_start = s_usb_confirm = false;
        snprintf(s_usb_message, sizeof(s_usb_message), "USB 模式未启动：%s", esp_err_to_name(err));
        read_pico_sd_remount();
        start_probe(ctx);
        return APP_REDRAW_PAGE;
    }
    if (!s_probing) return APP_REDRAW_NONE;
    if (!os_storage_probe_complete()) return APP_REDRAW_NONE;
    s_probing = false;
    read_pico_sd_get_info(&s_sd);
    s_flash_usable = false;
    if (book_store_read_roots(s_roots, &s_root_count) != ESP_OK) s_root_count = 0;
    s_flash_free = 0;
    for (int i = 0; i < s_root_count; ++i)
        if (s_roots[i].is_flash) { s_flash_usable = true; s_flash_free = book_store_free_bytes(&s_roots[i]); }
    // 只读摘要；崩溃日志本身由开机 os_crash 流程写入。/ Read-only summary; the log itself is written at boot by os_crash.
    s_crash[0] = 0;
    s_crash_state = s_flash_usable ? os_crash_summary_read(s_crash, sizeof(s_crash)) : OS_CRASH_SUMMARY_ERROR;
    return APP_REDRAW_PAGE;
}

static void render(app_ctx_t* ctx, uint8_t* fb) {
    (void)ctx;
    ui_clear_page(fb);
    ui_product_header(fb, "存储", "卡与容量");
    ui_draw_button(fb, back_rect(), "返回设置", false);
    ui_text(fb, UI_MARGIN, 214, 30, "TF 卡（图书与字库）", EPD_DRAW_ALIGN_LEFT, false);
    char line[96];
    if (s_probing) snprintf(line, sizeof(line), "检测中…");
    else if (!s_sd.present) snprintf(line, sizeof(line), "未插入");
    else if (s_sd.mounted) {
        char total[24], free_text[24];
        format_bytes(s_sd.capacity_bytes, total, sizeof(total));
        format_bytes(s_sd.free_bytes, free_text, sizeof(free_text));
        snprintf(line, sizeof(line), "已挂载 · 总量 %s · 剩余 %s", total, free_text);
    } else snprintf(line, sizeof(line), "已插入但未挂载 · 到诊断页检查");
    ui_text(fb, UI_MARGIN, 268, 36, line, EPD_DRAW_ALIGN_LEFT, false);
    ui_hairline(fb, 348, UI_MARGIN, ui_content_width(), UI_GRAY_BLACK);
    ui_text(fb, UI_MARGIN, 378, 30, "内置书库", EPD_DRAW_ALIGN_LEFT, false);
    if (!s_probing && s_flash_usable) {
        char free_text[24];
        format_bytes(s_flash_free, free_text, sizeof(free_text));
        snprintf(line, sizeof(line), "剩余 %s · 单本图书 ≤ 1 MiB", free_text);
    } else if (!s_probing) snprintf(line, sizeof(line), "暂不可用");
    else snprintf(line, sizeof(line), "检测中…");
    ui_text(fb, UI_MARGIN, 432, 36, line, EPD_DRAW_ALIGN_LEFT, false);
    ui_hairline(fb, 512, UI_MARGIN, ui_content_width(), UI_GRAY_BLACK);
    ui_text(fb, UI_MARGIN, 542, 30, "系统记录", EPD_DRAW_ALIGN_LEFT, false);
    ui_text(fb, UI_MARGIN, 596, 36, s_probing ? "检测中…" : s_crash_state == OS_CRASH_SUMMARY_READY ? s_crash :
            s_crash_state == OS_CRASH_SUMMARY_EMPTY ? "无异常重启记录" : "记录暂不可读，请重新检测", EPD_DRAW_ALIGN_LEFT, false);
    ui_text(fb, UI_MARGIN, 656, 22, "崩溃时无反栈；记录保存在内置存储 crash.log", EPD_DRAW_ALIGN_LEFT, false);
    ui_hairline(fb, 700, UI_MARGIN, ui_content_width(), UI_GRAY_BLACK);
    ui_product_title(fb, (EpdRect){UI_MARGIN, 726, ui_content_width(), 130},
                     "图书优先保存到 TF 卡；未插卡时使用内置存储。拔插卡或上传后可重新检测，结果只用于展示，不改动数据。",
                     26, 3);
    ui_draw_button(fb, refresh_rect(), "重新检测", !s_probing);
    ui_draw_button(fb, usb_rect(), "USB 连接电脑", false);
    if (s_usb_confirm || s_usb_start || s_usb_message[0]) {
        EpdRect panel = {UI_MARGIN - 16, 726, ui_content_width() + 32, 320};
        ui_clear_rect_fast(fb, panel);
        ui_draw_round_rect(fb, panel, UI_BTN_RADIUS, UI_GRAY_BLACK);
        ui_product_title(fb, (EpdRect){UI_MARGIN, 750, ui_content_width(), 100},
            s_usb_start ? "正在重启进入 USB 卡盘…" : s_usb_message[0] ? s_usb_message : "重启后电脑独占 TF 卡；阅读和传书暂停。退出前先在电脑安全弹出。", 28, 3);
        if (s_usb_confirm && !s_usb_start) {
            ui_draw_button(fb, refresh_rect(), "取消", false);
            ui_draw_button(fb, usb_rect(), "确认连接", false);
        }
    }
    ui_draw_menu_handle(fb, false);
}

static app_redraw_t gesture(app_ctx_t* ctx, const ui_gesture_event_t* ev) {
    if (ev->type != UI_GESTURE_TAP) return APP_REDRAW_NONE;
    if (s_usb_start) return APP_REDRAW_NONE;
    if (s_usb_confirm) {
        if (ui_rect_hit(refresh_rect(), ev->x0, ev->y0) && ui_rect_hit(refresh_rect(), ev->x, ev->y)) s_usb_confirm = false;
        else if (ui_rect_hit(usb_rect(), ev->x0, ev->y0) && ui_rect_hit(usb_rect(), ev->x, ev->y)) s_usb_start = true;
        return APP_REDRAW_PAGE;
    }
    if (ui_rect_hit(usb_rect(), ev->x0, ev->y0) && ui_rect_hit(usb_rect(), ev->x, ev->y)) {
        if (s_probing) snprintf(s_usb_message, sizeof(s_usb_message), "请等待 TF 卡检测完成");
        else if (!s_sd.present) snprintf(s_usb_message, sizeof(s_usb_message), "请先插入 TF 卡，再重新检测");
        else { s_usb_message[0] = 0; s_usb_confirm = true; }
        return APP_REDRAW_PAGE;
    }
    if (ui_rect_hit(back_rect(), ev->x0, ev->y0) && ui_rect_hit(back_rect(), ev->x, ev->y)) {
        ctx->request_app = app_by_id(OS_APP_SETTINGS);
        return APP_REDRAW_NONE;
    }
    if (!s_probing && ui_rect_hit(refresh_rect(), ev->x0, ev->y0) &&
        ui_rect_hit(refresh_rect(), ev->x, ev->y)) {
        start_probe(ctx);
        return APP_REDRAW_PAGE;
    }
    return APP_REDRAW_NONE;
}

static app_redraw_t key(app_ctx_t* ctx, int key) {
    if (s_usb_start) return APP_REDRAW_NONE;
    if (key == UI_KEY_1 && (s_usb_confirm || s_usb_message[0])) { s_usb_confirm = false; s_usb_message[0] = 0; return APP_REDRAW_PAGE; }
    if (key == UI_KEY_1) ctx->request_app = app_by_id(OS_APP_SETTINGS);
    return APP_REDRAW_NONE;
}

const app_desc_t app_os_storage = {
    .title = "存储与设备", .detail = "TF · 内置容量", .enter_full = false,
    .render = render, .on_gesture = gesture, .on_key = key,
    .on_enter = on_enter, .on_tick = on_tick,
};
