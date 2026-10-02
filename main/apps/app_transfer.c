/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 *
 * 传书页负责两种接入模式生命周期、书源注入与状态展示；HTTP和文件接收属于独立组件。
 * Transfer page owns both network modes, storage injection and status; the component receives files.
 *
 * 冻结：按用户双模式要求提供热点与已有WiFi，离页停止网络；沿用全局锁屏/睡眠策略，不新增协同；深睡/断电会中断传输。
 * render只画快照；内置存储单文件上限由book_store提供，不格式化TF卡。
 * 上传并发扫描曾欠载，页内增加预填余量；离页停服后恢复，不改波形或像素时钟。
 * 用户要求设备端配网：停服后扫描选网，ASCII密码只在连接保存时写入，离开输入页即清空。
 * 用户验收要求停止后恢复进入传书前的页面或菜单位置；遗忘网络必须确认。
 * 用户要求联网后提供网址二维码；热点用单码切换连接/网页，网络变更清除旧码。
 * 卡失效时停止并汇合接收任务，清除旧容量与二维码；当前请求不得切换存储源。
 * 用户批准补全产品功能：已有 WiFi 会话取得上行后驱动 SNTP 校时，校准写入 PMU 后即停；不影响传书生命周期。
 * 用户批准优化：单请求后台同步，离页取消并收齐；UI 串行准备/应用进度，下载明确确认，文件变更与同步互斥。
 * User-approved optimization: one network worker, cancel/join on exit; UI serializes progress snapshots/applications, confirms pulls and excludes file mutations during sync.
 * 用户批准补全产品功能：新增进度同步视图（kosync 协议，兼容 KOReader），仅在传书 STA 会话期间联网，可手动上传/下载或自动上传；密码仅存 MD5。
 * Frozen: user-requested AP/STA modes stop networking on exit; retain global lock/sleep policy without coordination; deep sleep or power loss interrupts transfer.
 * Render only paints snapshots; book_store defines the flash file limit. Never format the TF card.
 * Concurrent uploads underrun scan queues; increase prefill until service exit without changing waveforms or pixel clocks.
 * User-requested device provisioning scans while stopped; save ASCII passwords only on connect and clear input on leaving the editor.
 * Acceptance requires returning to the page or menu position used to enter transfer; forgetting WiFi requires confirmation.
 * User-requested URL QR follows network readiness; AP switches one code between joining and browsing, discarding stale codes on changes.
 * Lost media stops and joins reception and clears capacity/QR; never switch storage beneath an active request.
 * User-approved completion: STA sessions with an uplink drive SNTP calibration that writes the PMU once and stops; the transfer lifecycle itself is unchanged.
 * User-approved completion: a progress-sync view (kosync protocol, KOReader-compatible) networks only inside transfer STA sessions with manual push/pull or auto-push; passwords persist only as MD5.
 */
// 整个传书页暂停 SD 字体并使用内置字库；HTTP 停止后才恢复读取。
// Use the built-in font and suspend SD font opens for this page; resume only after HTTP stops.
#include <stdio.h>
#include <string.h>

#include "app.h"
#include "book_store.h"
#include "book_progress.h"
#include "display.h"
#include "e0470_epaper_waveform.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "os_sync.h"
#include "app_sleep_hooks.h"
#include "os_time.h"
#include "settings.h"
#include "read_pico_transfer.h"
#include "ttf_font.h"
#include "ui_gesture.h"
#include "ui_kit.h"
#include "ui/product/ui_product.h"
#include "ui_menu.h"
#include "ui_wifi_qr.h"

static const char* TAG = "transfer_page";
static book_store_root_t s_root;
static read_pico_transfer_status_t s_status;
static bool s_start_pending, s_settle, s_any_changed;
static bool s_media_lost;
static esp_err_t s_font_error;
static int s_pressed = -1;
static read_pico_transfer_mode_t s_mode = READ_PICO_TRANSFER_MODE_AP;
static bool s_session_started;
static int64_t s_poll_ms;
static uint64_t s_free;
static size_t s_heap_before, s_internal_before;
static EpdRect s_area;
static bool s_qr_url, s_qr_ready;

static void clear_qr(void) {
    ui_wifi_qr_clear();
    s_qr_ready = false;
}

// 仅在网络快照变化或用户切换时编码，render与上传进度不触发编码。
// Encode only on network snapshot changes or user switching, never from render or upload progress.
static void prepare_qr(void) {
    clear_qr();
    if (!s_status.network_ready) return;
    s_qr_ready = s_qr_url || s_mode == READ_PICO_TRANSFER_MODE_STA
        ? ui_wifi_qr_prepare_url(s_status.url)
        : ui_wifi_qr_prepare(s_status.ssid, READ_PICO_TRANSFER_PASSWORD);
}

typedef enum { TRANSFER_HOME, TRANSFER_NETWORKS, TRANSFER_PASSWORD, TRANSFER_SYNC } transfer_view_t;
// 密码键盘的目标字段：WiFi 密码或进度同步的三项配置。/ Keyboard target: the WiFi password or one of three sync fields.
typedef enum { EDIT_WIFI = 0, EDIT_SYNC_URL, EDIT_SYNC_USER, EDIT_SYNC_PASS } transfer_edit_t;
#define NETWORK_ROWS 6
#define PASSWORD_MAX 64
static transfer_view_t s_view;
static read_pico_transfer_network_t s_networks[READ_PICO_TRANSFER_SCAN_MAX], s_selected;
static size_t s_network_count, s_network_page;
static bool s_scan_pending, s_saved_configured, s_password_visible, s_forget_confirm;
static char s_saved_ssid[33], s_password[PASSWORD_MAX + 1], s_network_message[96];
static int s_keyboard_mode;
static transfer_edit_t s_edit_field;
static char s_sync_message[96];
static bool s_sync_pushed, s_sync_claimed;

static void render(app_ctx_t* ctx, uint8_t* fb);
static void stop_session(void);

/* ---- 设备端配网 / Device provisioning ---- */
static void fit_label(char* text, int px, int width) {
    while (*text && ttf_text_width_px(px, text) > width) {
        size_t n = strlen(text) - 1;
        while (n && ((unsigned char)text[n] & 0xc0) == 0x80) --n;
        text[n] = 0;
    }
}

static void clear_password(void) {
    volatile char* p = s_password;
    for (size_t i = 0; i < sizeof(s_password); ++i) p[i] = 0;
    s_password_visible = false;
}

static EpdRect network_control_rect(int id) {
    if (id < NETWORK_ROWS) return (EpdRect){UI_MARGIN, 350 + id * 100, ui_content_width(), 88};
    if (id == 6) return ui_row_rect(0, 2, 176, 84);
    if (id == 11) return s_forget_confirm ? ui_row_rect(0, 2, 600, UI_BTN_H) : ui_row_rect(1, 2, 176, 84);
    if (id == 12) return ui_row_rect(1, 2, 600, UI_BTN_H);
    if (id == 10) return ui_bar_rect(0, 1);
    return ui_row_rect(id - 7, 3, 968, 84);
}

static EpdRect password_control_rect(int id) {
    if (id < 40) {
        const int gap = 6;
        int width = (ui_content_width() - 9 * gap) / 10;
        return (EpdRect){UI_MARGIN + (id % 10) * (width + gap), 420 + (id / 10) * 100, width, 88};
    }
    if (id < 44) return ui_row_rect(id - 40, 4, 830, 78);
    return ui_bar_rect(id - 44, 3);
}

static const char* keyboard_chars(void) {
    static const char* const keys[] = {
        "1234567890" "qwertyuiop" "asdfghjkl-" "zxcvbnm,./",
        "1234567890" "QWERTYUIOP" "ASDFGHJKL-" "ZXCVBNM,./",
        "!\"#$%&'()*" "+,-./:;<=>" "?@[\\]^_`{|" "}~01234567",
    };
    return keys[s_keyboard_mode];
}

static void provisioning_button(uint8_t* fb, EpdRect rect, const char* text, int id) {
    if (s_pressed == id) ui_draw_pressed_round_rect(fb, rect, UI_BTN_RADIUS);
    ui_draw_button(fb, rect, text, false);
}

static void draw_networks(uint8_t* fb) {
    ui_clear_page(fb);
    if (s_forget_confirm) {
        ui_draw_header(fb, "遗忘已保存网络？", "仅清除 WiFi 连接信息，不影响图书");
        char name[33];
        snprintf(name, sizeof(name), "%s", s_saved_ssid);
        fit_label(name, UI_PX_BODY, ui_content_width());
        ui_text(fb, UI_MARGIN, 350, UI_PX_BODY, name, EPD_DRAW_ALIGN_LEFT, false);
        ui_text(fb, UI_MARGIN, 450, UI_PX_CAPTION, s_network_message, EPD_DRAW_ALIGN_LEFT, false);
        provisioning_button(fb, network_control_rect(11), "确认遗忘", 11);
        provisioning_button(fb, network_control_rect(12), "取消", 12);
        ui_draw_menu_handle(fb, false);
        return;
    }
    ui_draw_header(fb, "选择 WiFi", "选择 2.4 GHz 网络，连接后同网传书");
    if (s_saved_configured) {
        provisioning_button(fb, network_control_rect(6), "连接已保存", 6);
        provisioning_button(fb, network_control_rect(11), "遗忘网络", 11);
        char saved[64];
        snprintf(saved, sizeof(saved), "已保存网络：%s", s_saved_ssid);
        int name_px = UI_PX_CAPTION;
        while (name_px > 12 && ttf_text_width_px(name_px, saved) > ui_content_width()) --name_px;
        ui_text(fb, UI_MARGIN, 270, name_px, saved, EPD_DRAW_ALIGN_LEFT, false);
    } else ui_text(fb, UI_MARGIN, 196, UI_PX_CAPTION, "尚未保存 WiFi，点下方网络配网", EPD_DRAW_ALIGN_LEFT, false);
    char count[64];
    snprintf(count, sizeof(count), "%u 个网络 · 第 %u/%u 页", (unsigned)s_network_count,
             (unsigned)s_network_page + 1, (unsigned)(s_network_count ? (s_network_count + NETWORK_ROWS - 1) / NETWORK_ROWS : 1));
    ui_text(fb, UI_MARGIN, s_saved_configured ? 310 : 280, UI_PX_CAPTION,
            s_scan_pending ? "正在扫描，请稍候…" : s_network_message[0] ? s_network_message : count,
            EPD_DRAW_ALIGN_LEFT, false);
    for (int row = 0; row < NETWORK_ROWS; ++row) {
        size_t index = s_network_page * NETWORK_ROWS + row;
        if (index >= s_network_count) break;
        const read_pico_transfer_network_t* network = &s_networks[index];
        EpdRect rect = network_control_rect(row);
        if (s_pressed == row) ui_draw_pressed_round_rect(fb, rect, UI_BTN_RADIUS);
        ui_draw_round_rect(fb, rect, UI_BTN_RADIUS, UI_GRAY_BLACK);
        char name[33], detail[64];
        snprintf(name, sizeof(name), "%s", network->ssid);
        fit_label(name, UI_PX_BTN, rect.width - 2 * UI_PAD);
        ui_text(fb, rect.x + UI_PAD, rect.y + 8, UI_PX_BTN, name, EPD_DRAW_ALIGN_LEFT, false);
        snprintf(detail, sizeof(detail), "%d dBm · %s", network->rssi,
                 !network->supported ? "暂不支持" : network->requires_password ? "需要密码" : "开放网络");
        ui_text(fb, rect.x + UI_PAD, rect.y + 50, UI_PX_CAPTION, detail, EPD_DRAW_ALIGN_LEFT, false);
    }
    provisioning_button(fb, network_control_rect(7), "上一页", 7);
    provisioning_button(fb, network_control_rect(8), "重新扫描", 8);
    provisioning_button(fb, network_control_rect(9), "下一页", 9);
    provisioning_button(fb, network_control_rect(10), "返回传书", 10);
    ui_draw_menu_handle(fb, false);
}

static void draw_password(uint8_t* fb) {
    ui_clear_page(fb);
    ui_draw_header(fb, "WiFi 密码", "输入完成后点连接，密码将保存在设备");
    char name[33];
    snprintf(name, sizeof(name), "%s", s_selected.ssid);
    fit_label(name, UI_PX_BODY, ui_content_width());
    ui_text(fb, UI_MARGIN, 176, UI_PX_BODY, name, EPD_DRAW_ALIGN_LEFT, false);
    char caption[80];
    size_t len = strlen(s_password);
    const char* edit_title = s_edit_field == EDIT_SYNC_URL ? "服务器地址" :
                             s_edit_field == EDIT_SYNC_USER ? "同步用户名" :
                             s_edit_field == EDIT_SYNC_PASS ? "同步密码（输入新值）" : NULL;
    snprintf(caption, sizeof(caption), "%s · %u/%u",
             edit_title ? edit_title : s_selected.requires_password ? "密码" : "开放网络，无需密码",
             (unsigned)len, PASSWORD_MAX);
    ui_text(fb, UI_MARGIN, 240, UI_PX_CAPTION, caption, EPD_DRAW_ALIGN_LEFT, false);
    EpdRect field = {UI_MARGIN, 288, ui_content_width(), 86};
    ui_draw_round_rect(fb, field, UI_BTN_RADIUS, UI_GRAY_BLACK);
    char shown[PASSWORD_MAX + 1];
    if (s_password_visible) memcpy(shown, s_password, len + 1);
    else { memset(shown, '*', len); shown[len] = 0; }
    const char* tail = shown;
    while (*tail && ttf_text_width_px(UI_PX_BODY, tail) > field.width - 2 * UI_PAD) ++tail;
    ui_text_vc(fb, field.x + UI_PAD, field.y + field.height / 2, UI_PX_BODY, tail, EPD_DRAW_ALIGN_LEFT, false);
    const char* keys = keyboard_chars();
    for (int i = 0; i < 40; ++i) {
        char label[2] = {keys[i], 0};
        provisioning_button(fb, password_control_rect(i), label, i);
    }
    provisioning_button(fb, password_control_rect(40), s_keyboard_mode == 1 ? "abc" : "ABC", 40);
    provisioning_button(fb, password_control_rect(41), s_keyboard_mode == 2 ? "字母" : "#+=", 41);
    provisioning_button(fb, password_control_rect(42), "空格", 42);
    provisioning_button(fb, password_control_rect(43), "退格", 43);
    ui_text(fb, UI_MARGIN, 940, UI_PX_CAPTION,
            s_network_message[0] ? s_network_message : "支持字母、数字、符号和空格", EPD_DRAW_ALIGN_LEFT, false);
    provisioning_button(fb, password_control_rect(44), "取消", 44);
    provisioning_button(fb, password_control_rect(45), s_password_visible ? "隐藏" : "显示", 45);
    provisioning_button(fb, password_control_rect(46), s_edit_field ? "保存" : "连接", 46);
    ui_draw_menu_handle(fb, false);
}

static void queue_network_start(read_pico_transfer_mode_t mode) {
    s_mode = mode;
    s_sync_pushed = false;
    s_qr_url = mode == READ_PICO_TRANSFER_MODE_STA;
    clear_qr();
    s_view = TRANSFER_HOME;
    s_forget_confirm = false;
    s_scan_pending = false;
    s_pressed = -1;
    clear_password();
    memset(&s_status, 0, sizeof(s_status));
    s_status.mode = s_mode;
    read_pico_transfer_get_saved_wifi(s_status.wifi_ssid, &s_status.wifi_configured);
    s_start_pending = true;
}

static bool enter_networks(void) {
    if (!read_pico_transfer_try_stop_if_idle()) return false;
    stop_session();
    s_start_pending = false;
    s_view = TRANSFER_NETWORKS;
    s_forget_confirm = false;
    s_network_count = s_network_page = 0;
    s_scan_pending = true;
    s_network_message[0] = 0;
    s_saved_ssid[0] = 0;
    s_saved_configured = false;
    read_pico_transfer_get_saved_wifi(s_saved_ssid, &s_saved_configured);
    clear_password();
    return true;
}

// 提示页先呈现，下一轮才同步扫描；输入页不轮询已停服的传书状态。
// Present the scan notice first, scan synchronously next tick, and do not poll stopped transfer state in the editor.
static app_redraw_t network_ui_tick(app_ctx_t* ctx) {
    if (s_view != TRANSFER_NETWORKS || s_forget_confirm || !s_scan_pending || ctx->consumed) return APP_REDRAW_NONE;
    s_scan_pending = false;
    s_network_count = s_network_page = 0;
    esp_err_t err = read_pico_transfer_scan_wifi(s_networks, &s_network_count);
    if (err != ESP_OK) {
        s_network_count = 0;
        snprintf(s_network_message, sizeof(s_network_message), "扫描失败，请点重新扫描");
    }
    else if (!s_network_count) snprintf(s_network_message, sizeof(s_network_message), "未发现网络，请靠近路由器后重扫");
    else s_network_message[0] = 0;
    s_pressed = -1;
    return APP_REDRAW_PAGE;
}

static int provisioning_hit(uint16_t x, uint16_t y) {
    if (s_forget_confirm) {
        for (int i = 11; i <= 12; ++i) if (ui_rect_hit(network_control_rect(i), x, y)) return i;
        return -1;
    }
    if (s_view == TRANSFER_PASSWORD) {
        for (int i = 0; i < 47; ++i) if (ui_rect_hit(password_control_rect(i), x, y)) return i;
    } else {
        for (int i = 0; i < 12; ++i) {
            if (i < NETWORK_ROWS && (s_scan_pending || s_network_page * NETWORK_ROWS + i >= s_network_count)) continue;
            if ((i == 6 || i == 11) && !s_saved_configured) continue;
            if (ui_rect_hit(network_control_rect(i), x, y)) return i;
        }
    }
    return -1;
}

static app_redraw_t provisioning_action(int id) {
    if (s_forget_confirm) {
        if (id == 12) { s_forget_confirm = false; s_network_message[0] = 0; }
        else if (id == 11) {
            esp_err_t err = read_pico_transfer_forget_wifi();
            if (err == ESP_OK) {
                s_forget_confirm = false;
                s_saved_configured = false;
                s_saved_ssid[0] = 0;
                snprintf(s_network_message, sizeof(s_network_message), "已遗忘网络");
            } else snprintf(s_network_message, sizeof(s_network_message), "遗忘失败，请重试或取消");
        }
        return APP_REDRAW_PAGE;
    }
    if (s_view == TRANSFER_NETWORKS) {
        if (id < NETWORK_ROWS) {
            s_selected = s_networks[s_network_page * NETWORK_ROWS + id];
            if (!s_selected.supported) {
                snprintf(s_network_message, sizeof(s_network_message), "此网络认证暂不支持，请选择其他网络");
                return APP_REDRAW_PAGE;
            }
            clear_password();
            s_network_message[0] = 0;
            s_keyboard_mode = 0;
            s_edit_field = EDIT_WIFI;
            s_view = TRANSFER_PASSWORD;
        } else if (id == 6) queue_network_start(READ_PICO_TRANSFER_MODE_STA);
        else if (id == 7 && s_network_page) --s_network_page;
        else if (id == 8) { s_scan_pending = true; s_network_message[0] = 0; }
        else if (id == 9 && (s_network_page + 1) * NETWORK_ROWS < s_network_count) ++s_network_page;
        else if (id == 10) queue_network_start(s_mode);
        else if (id == 11) { s_forget_confirm = true; s_network_message[0] = 0; }
        return APP_REDRAW_PAGE;
    }
    size_t len = strlen(s_password);
    if (id < 40 || id == 42) {
        if (len < PASSWORD_MAX) {
            s_password[len] = id == 42 ? ' ' : keyboard_chars()[id];
            s_password[len + 1] = 0;
            s_network_message[0] = 0;
        } else snprintf(s_network_message, sizeof(s_network_message), "密码最多 64 个字符");
    } else if (id == 40) s_keyboard_mode = s_keyboard_mode == 1 ? 0 : 1;
    else if (id == 41) s_keyboard_mode = s_keyboard_mode == 2 ? 0 : 2;
    else if (id == 43) { if (len) s_password[len - 1] = 0; s_network_message[0] = 0; }
    else if (id == 44) {
        clear_password();
        s_network_message[0] = 0;
        if (s_edit_field) { s_edit_field = EDIT_WIFI; s_view = TRANSFER_SYNC; }
        else s_view = TRANSFER_NETWORKS;
        return APP_REDRAW_PAGE;
    }
    else if (id == 45) s_password_visible = !s_password_visible;
    else if (id == 46 && s_edit_field) {
        // 保存同步配置：密码留空视为不变。/ Save sync settings; an empty password keeps the old one.
        if (s_edit_field == EDIT_SYNC_URL) app_settings_set_sync_url(s_password);
        else if (s_edit_field == EDIT_SYNC_USER) app_settings_set_sync_user(s_password);
        else if (len) os_sync_set_password(s_password);
        snprintf(s_sync_message, sizeof(s_sync_message), "已保存%s", len || s_edit_field != EDIT_SYNC_PASS ? "" : "（密码未变）");
        s_edit_field = EDIT_WIFI;
        clear_password();
        s_view = TRANSFER_SYNC;
        return APP_REDRAW_PAGE;
    }
    else if (id == 46) {
        if (s_selected.requires_password && len < 8) {
            snprintf(s_network_message, sizeof(s_network_message), "加密网络密码至少 8 个字符");
        } else {
            esp_err_t err = read_pico_transfer_save_wifi(s_selected.ssid,
                s_selected.requires_password ? s_password : "");
            if (err == ESP_OK) { queue_network_start(READ_PICO_TRANSFER_MODE_STA); return APP_REDRAW_PAGE; }
            snprintf(s_network_message, sizeof(s_network_message), "%s", err == ESP_ERR_INVALID_ARG ?
                     "密码须 8–63 字符或 64 位十六进制" : "保存失败，输入已保留，请重试");
        }
    }
    return APP_REDRAW_AREA;
}

static app_redraw_t provisioning_gesture(app_ctx_t* ctx, const ui_gesture_event_t* ev) {
    int old = s_pressed;
    int origin = provisioning_hit(ev->x0, ev->y0);
    s_pressed = ev->type == UI_GESTURE_PRESS ? origin : -1;
    bool action = ev->type == UI_GESTURE_TAP && old >= 0 && old == origin && provisioning_hit(ev->x, ev->y) == origin;
    EpdRect old_rect = s_view == TRANSFER_PASSWORD ? password_control_rect(old >= 0 ? old : origin >= 0 ? origin : 0)
                                                  : network_control_rect(old >= 0 ? old : origin >= 0 ? origin : 0);
    if (action) {
        app_redraw_t redraw = provisioning_action(origin);
        if (redraw == APP_REDRAW_PAGE) return redraw;
        render(ctx, ctx->fb);
        s_area = ui_rect_union(old_rect, (EpdRect){UI_MARGIN, 240, ui_content_width(), 744});
        s_settle = false;
        return APP_REDRAW_AREA;
    }
    if (old == s_pressed) return APP_REDRAW_NONE;
    render(ctx, ctx->fb);
    s_area = old_rect;
    s_settle = false;
    return APP_REDRAW_AREA;
}

static EpdRect status_rect(void) { return (EpdRect){UI_MARGIN, 704, ui_content_width(), 340}; }

/* ---- 进度同步视图 / Progress-sync view ---- */
static EpdRect sync_row_rect(int i) { return (EpdRect){UI_MARGIN, 200 + i * 76, ui_content_width(), 68}; }
static EpdRect sync_action_rect(int i) { return ui_row_rect(i % 2, 2, 516 + (i / 2) * 96, 80); }
static EpdRect sync_back_rect(void) { return ui_product_back_rect(); }
static const char* sync_host(void) {
    static char host[72];
    const char* url = app_settings_sync_url();
    const char* at = strstr(url, "://");
    snprintf(host, sizeof(host), "%s", at ? at + 3 : url);
    char* slash = strchr(host, '/');
    if (slash) *slash = 0;
    return host;
}
static bool sync_online(void) {
    return s_mode == READ_PICO_TRANSFER_MODE_STA && s_status.network_ready;
}
static void open_sync_edit(transfer_edit_t field) {
    s_edit_field = field;
    s_keyboard_mode = 1;
    s_password_visible = field != EDIT_SYNC_PASS;
    const char* prefill = field == EDIT_SYNC_URL ? app_settings_sync_url() :
                          field == EDIT_SYNC_USER ? app_settings_sync_user() : "";
    // 编辑缓冲 64 字节；放不下的现值不预填，避免保存时把长地址悄悄截断。
    // The edit buffer holds 64 bytes; values that do not fit are not prefilled so saving cannot silently truncate a long URL.
    snprintf(s_password, sizeof(s_password), "%s", strlen(prefill) < sizeof(s_password) ? prefill : "");
    s_network_message[0] = 0;
    s_view = TRANSFER_PASSWORD;
}
static void sync_button(uint8_t* fb, int i, const char* label, bool enabled) {
    EpdRect r = sync_action_rect(i);
    ui_draw_round_rect(fb, r, UI_BTN_RADIUS, enabled ? UI_GRAY_BLACK : UI_GRAY_LIGHT);
    ui_text_vc(fb, r.x + r.width / 2, r.y + r.height / 2, 32, label, EPD_DRAW_ALIGN_CENTER, false);
}
static void draw_sync(uint8_t* fb) {
    ui_clear_page(fb);
    ui_product_header(fb, "进度同步", "KOReader (kosync) 协议");
    ui_draw_button(fb, sync_back_rect(), "返回", false);
    const char* values[] = {sync_host(), app_settings_sync_user()[0] ? app_settings_sync_user() : "未设置",
                            app_settings_sync_key()[0] ? "已设置 ›" : "未设置 ›",
                            app_settings_sync_auto() ? "开" : "关"};
    const char* labels[] = {"服务器", "用户名", "密码", "自动上传"};
    for (int i = 0; i < 4; ++i) {
        EpdRect r = sync_row_rect(i);
        ui_text_vc(fb, r.x, r.y + r.height / 2, 30, labels[i], EPD_DRAW_ALIGN_LEFT, false);
        ui_product_title(fb, (EpdRect){r.x + 180, r.y + 20, r.width - 180, 34}, values[i], 26, 1);
        ui_hairline(fb, r.y + r.height, r.x, r.width, UI_GRAY_LIGHT);
    }
    bool enabled = sync_online() && !os_sync_job_busy() && !os_sync_pull_pending();
    sync_button(fb, 0, "测试连接", enabled);
    sync_button(fb, 1, "注册账号", enabled);
    sync_button(fb, 2, os_sync_pull_pending() ? "保留本地" : "上传进度", enabled || os_sync_pull_pending());
    sync_button(fb, 3, os_sync_pull_pending() ? "确认应用" : "下载进度", enabled || os_sync_pull_pending());
    if (s_sync_message[0])
        ui_product_title(fb, (EpdRect){UI_MARGIN, 726, ui_content_width(), 80}, s_sync_message, 28, 2);
    else
        ui_text(fb, UI_MARGIN, 728, 26, sync_online() ? "已连接，可上传或下载" : "上传/下载需先连接已有 WiFi",
                EPD_DRAW_ALIGN_LEFT, false);
    ui_product_title(fb, (EpdRect){UI_MARGIN, 850, ui_content_width(), 160},
                     "上传/下载针对最后一本读过的书；下载完成后确认应用，取消保留本地位置。同步时暂停文件上传与删除；自动上传每次联网一次。兼容 KOReader 与 kosync 服务器；自建服务器 http/https 均可，https 需有效证书。",
                     24, 5);
    ui_draw_menu_handle(fb, false);
}
static void cancel_sync(void) {
    os_sync_job_cancel();
    if (s_sync_claimed) read_pico_transfer_release_sync();
    s_sync_claimed = false;
}
static bool start_sync(os_sync_job_t job) {
    if (os_sync_job_busy() || os_sync_pull_pending()) return false;
    if (!read_pico_transfer_claim_sync()) {
        snprintf(s_sync_message, sizeof(s_sync_message), "正在上传或删除，请完成后再同步");
        return false;
    }
    s_sync_claimed = true;
    if (os_sync_job_start(job, s_sync_message, sizeof(s_sync_message))) return true;
    read_pico_transfer_release_sync(); s_sync_claimed = false;
    return false;
}
static app_redraw_t sync_action(app_ctx_t* ctx, int id) {
    (void)ctx;
    if (id == 914) { cancel_sync(); s_view = TRANSFER_HOME; return APP_REDRAW_PAGE; }
    if (os_sync_pull_pending()) {
        if (id == 912 || id == 913) {
            os_sync_pull_confirm(id == 913, s_sync_message, sizeof(s_sync_message));
            read_pico_transfer_release_sync(); s_sync_claimed = false;
        }
        return APP_REDRAW_PAGE;
    }
    if (os_sync_job_busy()) return APP_REDRAW_NONE;
    if (id >= 900 && id < 903) { open_sync_edit((transfer_edit_t)(EDIT_SYNC_URL + id - 900)); return APP_REDRAW_PAGE; }
    if (id == 903) { app_settings_set_sync_auto(!app_settings_sync_auto()); return APP_REDRAW_PAGE; }
    if (!sync_online()) {
        snprintf(s_sync_message, sizeof(s_sync_message), "请先用已有 WiFi 连接");
        return APP_REDRAW_PAGE;
    }
    if (id >= 910 && id <= 913) start_sync((os_sync_job_t)(id - 910));
    else return APP_REDRAW_NONE;
    return APP_REDRAW_PAGE;
}
static int sync_hit(uint16_t x, uint16_t y) {
    if (ui_rect_hit(sync_back_rect(), x, y)) return 914;
    for (int i = 0; i < 4; ++i) if (ui_rect_hit(sync_row_rect(i), x, y)) return 900 + i;
    for (int i = 0; i < 4; ++i) if (ui_rect_hit(sync_action_rect(i), x, y)) return 910 + i;
    return -1;
}
static app_redraw_t sync_gesture(app_ctx_t* ctx, const ui_gesture_event_t* ev) {
    if (ev->type != UI_GESTURE_TAP) return APP_REDRAW_NONE;
    int id = sync_hit(ev->x0, ev->y0);
    if (id >= 0 && id == sync_hit(ev->x, ev->y)) return sync_action(ctx, id);
    return APP_REDRAW_NONE;
}
static uint64_t free_bytes(void* arg) { return book_store_free_bytes(arg); }

static void draw_capacity(uint8_t* fb) {
    char capacity[96];
    ui_clear_rect_fast(fb, (EpdRect){UI_MARGIN, 518, ui_content_width(), 42});
    if (s_media_lost) snprintf(capacity, sizeof(capacity), "TF 卡已移除，原存储已停用");
    else snprintf(capacity, sizeof(capacity), "%s · 剩余 %.1f MB%s", s_root.is_flash ? "内置" : "TF 卡", s_free / 1048576.0, s_root.is_flash ? " · 单文件 ≤ 1 MB" : "");
    ui_text(fb, UI_MARGIN, 528, UI_PX_CAPTION, capacity, EPD_DRAW_ALIGN_LEFT, false);
}

static void draw_status(uint8_t* fb) {
    EpdRect area = status_rect();
    ui_clear_rect_fast(fb, area);
    const char* state = "网络已停止";
    if (s_start_pending || s_status.state == READ_PICO_TRANSFER_STARTING) state = "正在准备传书…";
    else if (s_status.state == READ_PICO_TRANSFER_READY) state = "等待上传";
    else if (s_status.state == READ_PICO_TRANSFER_UPLOADING) state = "正在接收";
    else if (s_status.state == READ_PICO_TRANSFER_ERROR) state = s_status.network_ready ? "上传失败，请重试" : "连接失败，可重试或切回热点";
    if (s_media_lost) state = "存储已移除，传书已停止";
    ui_text(fb, area.x, area.y, UI_PX_BODY, state, EPD_DRAW_ALIGN_LEFT, false);
    char line[160];
    if (s_media_lost) snprintf(line, sizeof(line), "重新进入传书可使用内置存储");
    else if (s_mode == READ_PICO_TRANSFER_MODE_AP)
        snprintf(line, sizeof(line), "已连接 %u 台 · 已完成 %u 个文件", s_status.sta_count, s_status.done_count);
    else snprintf(line, sizeof(line), "同网浏览器上传 · 已完成 %u 个文件", s_status.done_count);
    ui_text(fb, area.x, area.y + 56, UI_PX_CAPTION, line, EPD_DRAW_ALIGN_LEFT, false);
    snprintf(line, sizeof(line), "%s", s_status.cur_name);
    while (*line && ttf_text_width_px(UI_PX_CAPTION, line) > area.width) {
        size_t n = strlen(line) - 1;
        while (n && ((unsigned char)line[n] & 0xc0) == 0x80) --n;
        line[n] = 0;
    }
    ui_text(fb, area.x, area.y + 98, UI_PX_CAPTION, line, EPD_DRAW_ALIGN_LEFT, false);
    snprintf(line, sizeof(line), "%.1f / %.1f MB", s_status.cur_bytes / 1048576.0, s_status.cur_total / 1048576.0);
    ui_text(fb, area.x, area.y + 140, UI_PX_BODY, line, EPD_DRAW_ALIGN_LEFT, false);
    EpdRect track = {area.x, area.y + 188, area.width, 12};
    epd_fill_rect(track, UI_GRAY_LIGHT, fb);
    if (s_status.cur_total) {
        uint64_t width = (uint64_t)s_status.cur_bytes * track.width / s_status.cur_total;
        track.width = width > (uint64_t)track.width ? track.width : (int)width;
        epd_fill_rect(track, UI_GRAY_BLACK, fb);
    }
    if (s_status.last_error != ESP_OK)
        ui_text(fb, area.x, area.y + 222, UI_PX_CAPTION, s_status.network_ready ? "上传未完成，请在浏览器查看原因" : "检查WiFi配置后重新选择接入方式", EPD_DRAW_ALIGN_LEFT, false);
    ui_text(fb, area.x, area.y + 262, UI_PX_CAPTION, s_status.state == READ_PICO_TRANSFER_UPLOADING ? "上传完成后可切换接入方式" : "离开本页会断开网络", EPD_DRAW_ALIGN_LEFT, false);
    ui_text(fb, area.x, area.y + 300, UI_PX_CAPTION, "同名文件上传后将覆盖", EPD_DRAW_ALIGN_LEFT, false);
}

static EpdRect control_rect(int id) {
    if (id == 3) return (EpdRect){UI_MARGIN, 454, 350, 60};
    if (id == 4) return (EpdRect){UI_MARGIN, 596, ui_content_width(), 84};
    return id == 2 ? ui_bar_rect(0, 1) : ui_row_rect(id, 2, UI_CONTENT_TOP, UI_BTN_H);
}

static void render(app_ctx_t* ctx, uint8_t* fb) {
    if (s_view == TRANSFER_NETWORKS) { draw_networks(fb); return; }
    if (s_view == TRANSFER_PASSWORD) { draw_password(fb); return; }
    if (s_view == TRANSFER_SYNC) { draw_sync(fb); return; }
    (void)ctx;
    ui_clear_page(fb);
    ui_product_header(fb, "传书", "手机或电脑浏览器上传 · TXT / EPUB / TTF");
    for (int i = 0; i < 2; ++i) {
        EpdRect r = control_rect(i);
        if (s_pressed == i) ui_draw_pressed_round_rect(fb, r, UI_BTN_RADIUS);
        ui_draw_button(fb, r, i == 0 ? "设备热点" : "已有 WiFi", (int)s_mode == i);
    }
    // 连接卡片：网络名、口令/状态与二维码同框。/ Connection card: network name, secret/state and the QR in one frame.
    EpdRect card = {UI_MARGIN - 8, 262, ui_content_width() + 16, 246};
    ui_draw_round_rect(fb, card, UI_BTN_RADIUS, UI_GRAY_BLACK);
    char name[64];
    snprintf(name, sizeof(name), "%s", s_mode == READ_PICO_TRANSFER_MODE_AP ? s_status.ssid : s_status.wifi_ssid);
    int name_px = UI_PX_CAPTION;
    int name_width = 330;
    while (name_px > 12 && ttf_text_width_px(name_px, name) > name_width) --name_px;
    ui_text(fb, UI_MARGIN, 282, name_px, name, EPD_DRAW_ALIGN_LEFT, false);
    ui_text(fb, UI_MARGIN, 334, UI_PX_CAPTION,
            s_mode == READ_PICO_TRANSFER_MODE_AP ? "口令：" READ_PICO_TRANSFER_PASSWORD :
            s_status.network_ready ? "设备已连接；手机需同网" :
            s_start_pending || s_status.state == READ_PICO_TRANSFER_STARTING ? "正在连接已保存网络…" :
            s_status.wifi_configured ? "已保存，尚未连接" : "点已有 WiFi 扫描选网",
            EPD_DRAW_ALIGN_LEFT, false);
    int url_px = UI_PX_CAPTION;
    while (url_px > 12 && ttf_text_width_px(url_px, s_status.url) > 330) --url_px;
    ui_text(fb, UI_MARGIN, 378, url_px,
            s_status.network_ready ? s_status.url : "等待网络地址…", EPD_DRAW_ALIGN_LEFT, false);
    ui_text(fb, UI_MARGIN, 420, 24,
            !s_status.network_ready ? "等待网络连接…" : !s_qr_ready ? "二维码失败，请手输网址" :
            s_qr_url || s_mode == READ_PICO_TRANSFER_MODE_STA ? "扫码打开网页" : "扫码连接 WiFi",
            EPD_DRAW_ALIGN_LEFT, false);
    if (s_mode == READ_PICO_TRANSFER_MODE_AP && s_status.network_ready) {
        EpdRect toggle = control_rect(3);
        if (s_pressed == 3) ui_draw_pressed_round_rect(fb, toggle, UI_BTN_RADIUS);
        ui_draw_button(fb, toggle, s_qr_url ? "切换为 WiFi 码" : "切换为网页码", false);
    }
    if (s_status.network_ready && s_qr_ready)
        ui_wifi_qr_draw(fb, (EpdRect){404, 282, 224, 224});
    draw_capacity(fb);
    ui_hairline(fb, 578, UI_MARGIN, ui_content_width(), UI_GRAY_BLACK);
    if (s_pressed == 4) ui_draw_pressed_round_rect(fb, control_rect(4), UI_BTN_RADIUS);
    ui_draw_button(fb, control_rect(4), "进度同步 ›", false);
    // 状态卡片：接收进度与提示同框。/ Status card: reception progress and hints in one frame.
    EpdRect status_card = {UI_MARGIN - 8, status_rect().y - 14, ui_content_width() + 16, status_rect().height + 28};
    ui_draw_round_rect(fb, status_card, UI_BTN_RADIUS, UI_GRAY_BLACK);
    draw_status(fb);
    EpdRect back = control_rect(2);
    if (s_pressed == 2) ui_draw_pressed_round_rect(fb, back, UI_BTN_RADIUS);
    ui_draw_button(fb, back, "停止并返回", false);
    ui_draw_menu_handle(fb, false);
}

static bool transfer_prepare_sleep(void) { os_sync_job_request_cancel(); return true; }
static void on_enter(app_ctx_t* ctx) {
    app_sleep_prepare_register(transfer_prepare_sleep);
    (void)ctx;
    // 先关闭卡上字库，再开放替换；主循环的字体重试也会被暂停。
    // Close the card font before allowing replacement; event-loop font retries are suspended too.
    s_font_error = ttf_font_suspend_sd(true);
    display_set_bulk_io(true);
    s_media_lost = false;
    memset(&s_status, 0, sizeof(s_status));
    s_qr_url = s_mode == READ_PICO_TRANSFER_MODE_STA;
    clear_qr();
    memset(&s_root, 0, sizeof(s_root));
    s_heap_before = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    s_internal_before = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    s_start_pending = true;
    s_view = TRANSFER_HOME;
    s_forget_confirm = false;
    s_scan_pending = false;
    clear_password();
    s_session_started = false;
    s_pressed = -1;
    s_edit_field = EDIT_WIFI;
    s_sync_pushed = false;
    s_sync_message[0] = 0;
    s_sync_claimed = false;
    s_settle = s_any_changed = false;
    s_status.mode = s_mode;
    read_pico_transfer_get_saved_wifi(s_status.wifi_ssid, &s_status.wifi_configured);
    s_poll_ms = 0;
    s_free = 0;
}

static void stop_session(void) {
    cancel_sync();
    clear_qr();
    // 停服后读最终计数，保留本页跨模式文件变更的记录。
    // Join reception before reading counts and retain file changes across mode switches.
    read_pico_transfer_stop();
    read_pico_transfer_get_status(&s_status);
    if (s_session_started && s_status.changed_count) s_any_changed = true;
    s_session_started = false;
}

// 必须先等待 HTTP 退出，再清除旧根；不会让在途请求写到另一存储。
// Join HTTP before clearing the old root; in-flight requests never move to another storage.
static void transfer_on_media_lost(app_ctx_t* ctx) {
    (void)ctx;
    if (s_root.is_flash) return;
    stop_session();
    s_start_pending = s_scan_pending = false;
    s_view = TRANSFER_HOME;
    clear_password();
    s_pressed = -1;
    s_free = 0;
    memset(&s_root, 0, sizeof(s_root));
    s_media_lost = true;
    s_settle = false;
}

static void transfer_on_exit(app_ctx_t* ctx) {
    app_sleep_prepare_unregister(transfer_prepare_sleep);
    clear_password();
    s_scan_pending = false;
    stop_session();
    os_time_network(false);
    ttf_font_suspend_sd(false);
    display_set_bulk_io(false);
    guard_draw_result(ctx->hl, update_display_white(ctx->hl));
    if (s_any_changed) book_store_notify_changed();
    ESP_LOGI(TAG, "heap before=%u after=%u internal_before=%u internal_after=%u",
             (unsigned)s_heap_before, (unsigned)heap_caps_get_free_size(MALLOC_CAP_8BIT),
             (unsigned)s_internal_before, (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
}

static app_redraw_t on_tick(app_ctx_t* ctx) {
    if (os_sync_job_poll(s_sync_message, sizeof(s_sync_message))) {
        if (!os_sync_pull_pending() && s_sync_claimed) {
            read_pico_transfer_release_sync(); s_sync_claimed = false;
        }
        return APP_REDRAW_PAGE;
    }
    if (s_view != TRANSFER_HOME && s_view != TRANSFER_SYNC) return network_ui_tick(ctx);
    if (s_view == TRANSFER_HOME && s_start_pending) {
        s_start_pending = false;
        if (s_font_error != ESP_OK) s_font_error = ttf_font_suspend_sd(true);
        esp_err_t err = s_font_error == ESP_OK ? book_store_upload_root(&s_root) : s_font_error;
        if (err == ESP_OK) {
            s_media_lost = false;
            s_free = book_store_free_bytes(&s_root);
            read_pico_transfer_cfg_t cfg = {.mode = s_mode, .root_dir = s_root.path, .is_flash = s_root.is_flash,
                .font_dir = s_root.is_flash ? NULL : "/sdcard/fonts",
                .file_limit = book_store_file_limit(&s_root), .free_bytes_cb = free_bytes, .free_bytes_ctx = &s_root, .file_changed_cb = book_progress_forget};
            err = read_pico_transfer_start(&cfg);
            s_session_started = err == ESP_OK;
            read_pico_transfer_get_status(&s_status);
            if (err == ESP_OK) s_free = book_store_free_bytes(&s_root);
        }
        s_status.mode = s_mode;
        if (err != ESP_OK) { s_status.state = READ_PICO_TRANSFER_ERROR; s_status.last_error = err; }
        if (err == ESP_OK) prepare_qr();
        else clear_qr();
        ESP_LOGI(TAG, "start root=%s result=%s", s_root.path, esp_err_to_name(err));
        return APP_REDRAW_PAGE;
    }
    if (ctx->consumed || ctx->now_ms - s_poll_ms < 2000) return APP_REDRAW_NONE;
    s_poll_ms = ctx->now_ms;
    read_pico_transfer_service_poll();
    read_pico_transfer_status_t next = {0};
    read_pico_transfer_get_status(&next);
    // STA 拿到上行后顺带校时；AP 或停止态传 false，SNTP 随会话结束释放。
    // Calibrate opportunistically once STA has an uplink; false for AP or stopped states releases SNTP with the session.
    bool sta_uplink = next.mode == READ_PICO_TRANSFER_MODE_STA && next.network_ready;
    os_time_network(sta_uplink);
    if (!sta_uplink) { if (os_sync_job_busy() || os_sync_pull_pending()) cancel_sync(); s_sync_pushed = false; }
    // 自动上传：每次 STA 上行会话只推一次，结果留在同步页可见。
    // Auto-push: once per STA uplink session, with the result visible on the sync view.
    if (sta_uplink && app_settings_sync_auto() && !s_sync_pushed &&
        !os_sync_job_busy() && !os_sync_pull_pending() && next.state != READ_PICO_TRANSFER_UPLOADING) {
        s_sync_pushed = true;
        start_sync(OS_SYNC_JOB_PUSH);
        return APP_REDRAW_PAGE;
    }
    if (s_status.state == READ_PICO_TRANSFER_ERROR && next.state == READ_PICO_TRANSFER_STOPPED) return APP_REDRAW_NONE;
    bool network_changed = next.network_ready != s_status.network_ready ||
        next.mode != s_status.mode || strcmp(next.ssid, s_status.ssid) ||
        next.wifi_configured != s_status.wifi_configured || strcmp(next.url, s_status.url) ||
        strcmp(next.wifi_ssid, s_status.wifi_ssid);
    if (!network_changed && next.state == s_status.state && next.sta_count == s_status.sta_count &&
        next.cur_bytes == s_status.cur_bytes && next.cur_total == s_status.cur_total &&
        next.done_count == s_status.done_count && next.changed_count == s_status.changed_count && next.last_error == s_status.last_error &&
        !strcmp(next.cur_name, s_status.cur_name)) return APP_REDRAW_NONE;
    s_settle = next.changed_count != s_status.changed_count || next.done_count != s_status.done_count ||
        (s_status.state == READ_PICO_TRANSFER_UPLOADING && next.state != READ_PICO_TRANSFER_UPLOADING);
    s_status = next;
    if (network_changed) prepare_qr();
    if (s_settle) { s_free = book_store_free_bytes(&s_root); return APP_REDRAW_FULL; }
    if (network_changed) { s_free = book_store_free_bytes(&s_root); return APP_REDRAW_PAGE; }
    draw_status(ctx->fb);
    s_area = status_rect();
    return APP_REDRAW_AREA;
}

static int hit_control(uint16_t x, uint16_t y) {
    if (s_mode == READ_PICO_TRANSFER_MODE_AP && s_status.network_ready && ui_rect_hit(control_rect(3), x, y)) return 3;
    if (ui_rect_hit(control_rect(4), x, y)) return 4;
    for (int i = 0; i < 3; ++i) if (ui_rect_hit(control_rect(i), x, y)) return i;
    return -1;
}

static app_redraw_t on_gesture(app_ctx_t* ctx, const ui_gesture_event_t* ev) {
    if (s_view == TRANSFER_SYNC) return sync_gesture(ctx, ev);
    if (s_view != TRANSFER_HOME) return provisioning_gesture(ctx, ev);
    int old = s_pressed;
    int origin = hit_control(ev->x0, ev->y0);
    s_pressed = ev->type == UI_GESTURE_PRESS ? origin : -1;
    if (ev->type == UI_GESTURE_TAP && old >= 0 && old == origin &&
        hit_control(ev->x, ev->y) == origin) {
        if (origin == 3) {
            s_qr_url = !s_qr_url;
            prepare_qr();
            return APP_REDRAW_PAGE;
        } else if (origin == 2) {
            // 即使没有切页历史也先停服，取消尚未执行的启动请求。
            // Stop even without navigation history and cancel a queued start.
            s_start_pending = false;
            stop_session();
            ctx->request_return = true;
        } else if (origin == 1) {
            if (enter_networks()) return APP_REDRAW_PAGE;
        } else if (origin == 4) {
            s_pressed = -1;
            s_view = TRANSFER_SYNC;
            return APP_REDRAW_PAGE;
        } else {
            read_pico_transfer_status_t latest;
            read_pico_transfer_get_status(&latest);
            if (((int)s_mode != origin || !latest.network_ready) &&
                read_pico_transfer_try_stop_if_idle()) {
                stop_session();
                s_mode = origin == 0 ? READ_PICO_TRANSFER_MODE_AP : READ_PICO_TRANSFER_MODE_STA;
                s_qr_url = s_mode == READ_PICO_TRANSFER_MODE_STA;
                clear_qr();
                memset(&s_status, 0, sizeof(s_status));
                s_status.mode = s_mode;
                read_pico_transfer_get_saved_wifi(s_status.wifi_ssid, &s_status.wifi_configured);
                s_start_pending = true;
                return APP_REDRAW_PAGE;
            }
        }
    }
    if (s_pressed == old) return APP_REDRAW_NONE;
    render(ctx, ctx->fb);
    s_settle = false;
    s_area = control_rect(old >= 0 ? old : s_pressed);
    return APP_REDRAW_AREA;
}

static bool present(app_ctx_t* ctx, app_redraw_t redraw) {
    if (redraw == APP_REDRAW_FULL && s_settle) {
        render(ctx, ctx->fb);
        guard_draw_result(ctx->hl, update_display_full(ctx->hl));
        s_settle = false;
        return true;
    }
    if (redraw != APP_REDRAW_AREA) return false;
    guard_draw_result(ctx->hl, update_display_area_with(ctx->hl, s_settle ? &E0470_WAVEFORM : &E0470_FOLLOW_WAVEFORM,
                      s_settle ? MODE_GL16 : MODE_DU, s_area));
    s_settle = false;
    return true;
}

const app_desc_t app_transfer = {
    .on_media_lost = transfer_on_media_lost,
    .title = "传书 Transfer", .detail = "热点或已有 WiFi，浏览器上传", .enter_full = false,
    .on_enter = on_enter, .on_exit = transfer_on_exit, .render = render, .present = present,
    .on_tick = on_tick, .on_gesture = on_gesture,
};
