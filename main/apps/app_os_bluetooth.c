/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 * 中文：蓝牙翻页器设置、选择与按键学习；只使用产品服务，不访问无线句柄。
 * English: Bluetooth page-turner settings, peer selection and learning through the product service only.
 * 冻结：连接前展示所选设备，遗忘需确认；render 纯绘制，测试按键不翻动图书。
 * Frozen: Show the selected peer before connecting, confirm Forget, keep render pure, and never turn books from the key test.
 * 冻结修订：返回沿用进入来源，不把阅读来源标成设置；无线启停和返回开书前回收可重建字体资源，保持文字可用。
 * Frozen revision: Return to the actual entry origin without labeling reader entry as Settings; reclaim rebuildable font resources before radio startup and book return, retaining readable text.
 */
#include "app.h"
#include "app_registry.h"
#include "ttf_font.h"
#include "app_loop.h"
#include "os_page_turner.h"
#include "ui_gesture.h"
#include "ui_menu.h"
#include "ui_press.h"
#include "ui_product.h"
#include <stdio.h>
#include <string.h>

static ui_press_t s_press;
static os_page_turner_info_t s_info;
static os_ble_peer_t s_candidate;
static int s_view, s_page;
static bool s_forget;
static int64_t s_painted_at;
static EpdRect button(int id) {
    if (id == 0) return ui_product_back_rect();
    if (id == 1) return (EpdRect){448, 202, 196, 76};
    if (id == 2) return (EpdRect){40, 452, 604, 100};
    if (id == 3 || id == 4) return (EpdRect){40 + (id - 3) * 310, 574, 294, 84};
    if (id == 5 || id == 6) return (EpdRect){40 + (id - 5) * 310, 682, 294, 84};
    if (id == 7 || id == 12) return (EpdRect){id == 12 ? 40 : 350, 790, 294, 80};
    if (id == 8) return (EpdRect){40, 886, 294, 80};
    if (id == 9) return (EpdRect){350, 886, 294, 80};
    if (id == 10 || id == 11) return (EpdRect){40 + (id - 10) * 310, 990, 294, 80};
    if (id >= 20 && id < 24) return (EpdRect){40, 328 + (id - 20) * 134, 604, 118};
    if (id >= 24 && id <= 26) return (EpdRect){40 + (id - 24) * 206, 906, 192, 84};
    if (id == 30) return (EpdRect){40, 672, 604, 92};
    return (EpdRect){0};
}
static const char* phase_name(os_ble_phase_t phase) {
    switch (phase) {
        case OS_BLE_OFF: return "蓝牙已关闭";
        case OS_BLE_STARTING: return "正在开启蓝牙…";
        case OS_BLE_SCANNING: return "正在搜索附近设备…";
        case OS_BLE_CONNECTING: return "正在建立连接…";
        case OS_BLE_DISCOVERING: return "链路已连接，正在准备按键…";
        case OS_BLE_READY: return "已连接 · 按键就绪";
        case OS_BLE_ERROR: return "连接未完成";
        default: return "蓝牙已开启 · 未连接";
    }
}
static void peer_name(uint8_t* fb, EpdRect rect, const os_ble_peer_t* peer, int px) {
    char addr[32];
    snprintf(addr, sizeof(addr), "%02X:%02X:%02X:%02X:%02X:%02X", peer->addr[5], peer->addr[4], peer->addr[3], peer->addr[2], peer->addr[1], peer->addr[0]);
    ui_product_title(fb, rect, peer->name[0] ? peer->name : addr, px, 1);
}
static void draw_button(uint8_t* fb, int id, const char* label, bool selected) {
    if (ui_press_active(&s_press) && s_press.id == id) ui_draw_pressed_round_rect(fb, button(id), UI_BTN_RADIUS);
    ui_draw_button(fb, button(id), label, selected);
}
static void render(app_ctx_t* ctx, uint8_t* fb) {
    (void)ctx;
    ui_clear_page(fb);
    ui_product_header(fb, "蓝牙翻页器", "BLE 设备 · 一次连接一个");
    draw_button(fb, 0, s_view ? "返回翻页器" : "返回", false);
    if (s_view == 1) {
        char summary[64];
        snprintf(summary, sizeof(summary), "%u 个设备 · 第 %d/%d 页", s_info.radio.count, s_page + 1,
            s_info.radio.count ? (s_info.radio.count + 3) / 4 : 1);
        ui_text(fb, 40, 238, 30, phase_name(s_info.radio.phase), EPD_DRAW_ALIGN_LEFT, false);
        ui_text(fb, 40, 284, 26, summary, EPD_DRAW_ALIGN_LEFT, false);
        for (int row = 0; row < 4; ++row) {
            unsigned index = s_page * 4 + row;
            if (index >= s_info.radio.count) break;
            EpdRect r = button(20 + row); const os_ble_peer_t* p = &s_info.radio.peers[index];
            if (ui_press_active(&s_press) && s_press.id == 20 + row) ui_draw_pressed_round_rect(fb, r, UI_CHIP_RADIUS);
            ui_draw_round_rect(fb, r, UI_CHIP_RADIUS, UI_GRAY_BLACK);
            peer_name(fb, (EpdRect){r.x + 16, r.y + 14, r.width - 32, 44}, p, 32);
            char detail[72]; snprintf(detail, sizeof(detail), "%s · %d dBm", p->hid ? "广播 HID 服务" : "需连接核实是否支持", p->rssi);
            ui_text(fb, r.x + 16, r.y + 72, 24, detail, EPD_DRAW_ALIGN_LEFT, false);
        }
        if (!s_info.radio.count) ui_text(fb, 40, 440, 30, "请让翻页器进入配对模式", EPD_DRAW_ALIGN_LEFT, false);
        draw_button(fb, 24, "上一页", false); draw_button(fb, 25, "重新搜索", false); draw_button(fb, 26, "下一页", false);
        ui_product_title(fb, (EpdRect){40, 1020, 604, 80}, "只选择你认识的翻页器；没有找到时，先断开它与手机的连接。", 26, 2);
    } else if (s_view == 2) {
        ui_text(fb, 40, 264, 36, "连接此设备？", EPD_DRAW_ALIGN_LEFT, false);
        peer_name(fb, (EpdRect){40, 348, 604, 100}, &s_candidate, 42);
        ui_product_title(fb, (EpdRect){40, 478, 604, 120}, "设备需要支持 BLE HID；传统蓝牙或夹屏式接收器不能直接配对。", 30, 3);
        draw_button(fb, 30, "连接此设备", true);
    } else {
        ui_text(fb, 40, 222, 34, "蓝牙", EPD_DRAW_ALIGN_LEFT, false);
        draw_button(fb, 1, s_info.enabled ? "已开启" : "开启蓝牙", s_info.enabled);
        ui_draw_round_rect(fb, (EpdRect){40, 302, 604, 126}, UI_CHIP_RADIUS, UI_GRAY_BLACK);
        ui_product_title(fb, (EpdRect){58, 322, 568, 44}, phase_name(s_info.radio.phase), 30, 1);
        ui_product_title(fb, (EpdRect){58, 378, 568, 36}, s_info.test[0] ? s_info.test : "按一下翻页器，可在此测试方向", 25, 1);
        if (s_info.saved) {
            EpdRect r = button(2);
            if (ui_press_active(&s_press) && s_press.id == 2) ui_draw_pressed_round_rect(fb, r, UI_CHIP_RADIUS);
            ui_draw_round_rect(fb, r, UI_CHIP_RADIUS, UI_GRAY_BLACK);
            peer_name(fb, (EpdRect){58, 463, 568, 42}, &s_info.selected, 32);
            ui_text(fb, 58, 516, 24, "已保存设备 · 点按重新连接", EPD_DRAW_ALIGN_LEFT, false);
        } else ui_text(fb, 40, 486, 30, "还没有保存翻页器", EPD_DRAW_ALIGN_LEFT, false);
        draw_button(fb, 3, "搜索翻页器", false); draw_button(fb, 4, "断开连接", false);
        draw_button(fb, 5, s_info.learning == -1 ? "取消学习" : s_info.bound_prev ? "上一页 · 已学习" : "学习上一页", s_info.learning == -1);
        draw_button(fb, 6, s_info.learning == 1 ? "取消学习" : s_info.bound_next ? "下一页 · 已学习" : "学习下一页", s_info.learning == 1);
        draw_button(fb, 12, s_info.learning == OS_PAGE_TURNER_LOCK ? "取消学习" : s_info.bound_lock ? "锁屏 · 已学习" : "学习锁屏", s_info.learning == OS_PAGE_TURNER_LOCK);
        draw_button(fb, 7, s_info.reversed ? "方向反转：开" : "方向反转：关", s_info.reversed);
        draw_button(fb, 8, "恢复标准按键", false); draw_button(fb, 9, "遗忘此设备", false);
        if (s_forget) {
            ui_text(fb, 40, 954, 27, "确认遗忘设备和自定义按键？", EPD_DRAW_ALIGN_LEFT, false);
            draw_button(fb, 10, "取消", false); draw_button(fb, 11, "确认遗忘", true);
        } else ui_product_title(fb, (EpdRect){40, 964, 604, 100}, s_info.note[0] ? s_info.note : "支持方向键与音量键；独立按钮可学习锁屏，仅阅读正文生效。", 26, 3);
    }
    if (!s_forget) ui_product_title(fb, (EpdRect){40, 1090, 560, 82}, "导航保持已连接；锁屏或使用WiFi时断开，续读后回连。蓝牙不能唤醒本机。", 23, 3);
    ui_draw_menu_handle(fb, false);
}
static int hit(uint16_t x, uint16_t y) {
    if (ui_rect_hit(button(0), x, y)) return 0;
    if (!s_view && s_forget) {
        if (ui_rect_hit(button(10), x, y)) return 10;
        if (ui_rect_hit(button(11), x, y)) return 11;
        return -1;
    }
    if (s_view == 2) return ui_rect_hit(button(30), x, y) ? 30 : -1;
    if (s_view == 1) {
        for (int id = 20; id <= 26; ++id) if ((id >= 24 || (s_page * 4 + id - 20) < s_info.radio.count) && ui_rect_hit(button(id), x, y)) return id;
        return -1;
    }
    if (!s_view && !s_forget && ui_rect_hit(button(12), x, y)) return 12;
    for (int id = 1; id <= (s_forget ? 11 : 9); ++id) if (ui_rect_hit(button(id), x, y)) return id;
    return -1;
}
static app_redraw_t action(app_ctx_t* ctx, int id) {
    if (!id) { if (s_view) s_view = 0; else ctx->request_return = true; }
    else if (id == 1) os_page_turner_enable(!s_info.enabled);
    else if (id == 2 && s_info.saved) os_page_turner_connect(&s_info.selected);
    else if (id == 3 || id == 25) { if (os_page_turner_scan()) { s_view = 1; s_page = 0; } }
    else if (id == 4) os_page_turner_disconnect();
    else if (id == 5 || id == 6) os_page_turner_learn(s_info.learning ? 0 : id == 5 ? -1 : 1);
    else if (id == 12) os_page_turner_learn(s_info.learning ? 0 : OS_PAGE_TURNER_LOCK);
    else if (id == 7) os_page_turner_reverse();
    else if (id == 8) os_page_turner_restore_mapping();
    else if (id == 9 && s_info.saved) s_forget = true;
    else if (id == 10) s_forget = false;
    else if (id == 11) { os_page_turner_forget(); s_forget = false; }
    else if (id >= 20 && id < 24) { s_candidate = s_info.radio.peers[s_page * 4 + id - 20]; s_view = 2; }
    else if (id == 24 && s_page) --s_page;
    else if (id == 26 && (s_page + 1) * 4 < s_info.radio.count) ++s_page;
    else if (id == 30) { if (os_page_turner_connect(&s_candidate)) s_view = 0; }
    os_page_turner_poll(ctx->now_ms, false, true); s_info = *os_page_turner_info();
    return APP_REDRAW_PAGE;
}
static app_redraw_t gesture(app_ctx_t* ctx, const ui_gesture_event_t* event) {
    int id = hit(event->x0, event->y0);
    if (event->type == UI_GESTURE_PRESS) return id >= 0 ? ui_press_begin(ctx, &s_press, id, button(id), render) : APP_REDRAW_NONE;
    if (event->type != UI_GESTURE_TAP) return ui_press_restore(ctx, &s_press, render);
    int held = s_press.id; EpdRect rect = s_press.rect; ui_press_forget(&s_press);
    if (id >= 0 && id == hit(event->x, event->y)) return action(ctx, id);
    return held >= 0 ? ui_press_rewind(ctx, &s_press, rect, render) : APP_REDRAW_NONE;
}
static void enter(app_ctx_t* ctx) {
    ttf_font_reclaim();
    s_view = s_page = 0; s_forget = false; ui_press_reset(&s_press);
    os_page_turner_enter(); os_page_turner_poll(ctx->now_ms, false, true);
    s_info = *os_page_turner_info(); s_painted_at = ctx->now_ms;
}
static void leave(app_ctx_t* ctx) { (void)ctx; os_page_turner_leave(); ttf_font_reclaim(); }
static app_redraw_t tick(app_ctx_t* ctx) {
    ui_press_settle(ctx, &s_press, ctx->now_ms);
    if (s_info.learning || (s_info.radio.phase >= OS_BLE_SCANNING && s_info.radio.phase <= OS_BLE_DISCOVERING)) app_loop_stay_awake();
    os_page_turner_poll(ctx->now_ms, false, true);
    os_page_turner_info_t next = *os_page_turner_info();
    bool changed = next.radio.phase != s_info.radio.phase || next.radio.count != s_info.radio.count ||
        next.learning != s_info.learning || next.saved != s_info.saved || strcmp(next.note, s_info.note) || strcmp(next.test, s_info.test);
    if (changed && !ctx->consumed && ctx->now_ms - s_painted_at >= 500) {
        s_info = next; s_painted_at = ctx->now_ms; return APP_REDRAW_PAGE;
    }
    return APP_REDRAW_NONE;
}
static app_redraw_t key(app_ctx_t* ctx, int key) { return key == UI_KEY_1 ? action(ctx, 0) : APP_REDRAW_NONE; }
const app_desc_t app_os_bluetooth = {
    .title = "蓝牙翻页器", .detail = "连接 · 测试 · 按键学习", .enter_full = false,
    .render = render, .on_gesture = gesture, .on_key = key,
    .on_enter = enter, .on_exit = leave, .on_tick = tick,
};
