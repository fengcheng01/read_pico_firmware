/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 * 中文：产品设置根页，将现有功能分组，诊断不进入日常导航。
 * English: Group existing features under Settings, separating diagnostics from everyday navigation.
 * 冻结：不写电源或 VCOM；设置项委托已有页面，render 只绘图。
 * Frozen: No power or VCOM writes; delegate settings to existing pages; render only paints.
 */
#include "app.h"
#include "app_registry.h"
#include "os_device.h"
#include "ui_product.h"
#include "ui_gesture.h"

static const struct { const char* title; const char* detail; os_app_id_t id; } items[] = {
    {"阅读与字体", "默认字号 · 正文字体 · 晃动实验", OS_APP_READING},
    {"连接与传书", "设备热点 / WiFi · TXT / EPUB", OS_APP_TRANSFER},
    {"时间与时区", "自动校时 · 15 分钟步进时区", OS_APP_TIME},
    {"睡眠与锁屏", "睡眠模式 · 拿起唤醒", OS_APP_SLEEP},
    {"存储与设备", "TF 卡与内置书库容量", OS_APP_STORAGE},
    {"进阶与诊断", "硬件自检 · 刷新 · 触摸与传感器", OS_APP_TOOLS},
};
static EpdRect row_rect(int i) { return (EpdRect){UI_MARGIN, 206 + i * 140, ui_content_width(), 128}; }
static void render(app_ctx_t* ctx, uint8_t* fb) {
    (void)ctx;
    ui_clear_page(fb);
    ui_product_header(fb, "设置", "按需连接，其余时间安心阅读");
    for (unsigned i = 0; i < sizeof(items) / sizeof(items[0]); ++i) {
        EpdRect r = row_rect(i);
        ui_text(fb, r.x, r.y + 10, 44, items[i].title, EPD_DRAW_ALIGN_LEFT, false);
        bool disabled = items[i].id == OS_APP_TRANSFER && os_device()->transfer != OS_CAP_PRESENT;
        ui_text(fb, r.x, r.y + 64, 30, disabled ? "此设备暂不支持连接与传书" : items[i].detail, EPD_DRAW_ALIGN_LEFT, false);
        ui_text_vc(fb, r.x + r.width - 8, r.y + 36, 44, "›", EPD_DRAW_ALIGN_RIGHT, false);
        ui_hairline(fb, r.y + r.height, r.x, r.width, UI_GRAY_LIGHT);
    }
    ui_product_root_bar(fb, OS_APP_SETTINGS);
}
static int hit(uint16_t x, uint16_t y) {
    for (unsigned i = 0; i < sizeof(items) / sizeof(items[0]); ++i) if (ui_rect_hit(row_rect(i), x, y)) return (int)i;
    return -1;
}
static app_redraw_t gesture(app_ctx_t* ctx, const ui_gesture_event_t* ev) {
    if (ev->type != UI_GESTURE_TAP) return APP_REDRAW_NONE;
    os_app_id_t id = ui_product_root_hit(ev->x0, ev->y0);
    if (id != OS_APP_NONE && id != OS_APP_SETTINGS && id == ui_product_root_hit(ev->x, ev->y)) {
        ui_product_navigate(ctx, id);
        return APP_REDRAW_NONE;
    }
    int i = hit(ev->x0, ev->y0);
    if (i >= 0 && i == hit(ev->x, ev->y)) {
        if (items[i].id == OS_APP_TRANSFER && os_device()->transfer != OS_CAP_PRESENT) return APP_REDRAW_NONE;
        ctx->request_app = app_by_id(items[i].id);
    }
    return APP_REDRAW_NONE;
}
const app_desc_t app_os_settings = {
    .title = "设置", .detail = "阅读 · 连接 · 存储 · 诊断", .enter_full = false,
    .render = render, .on_gesture = gesture,
};
