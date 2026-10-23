/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 * 中文：设置下的电量历史页，展示24小时/七天真实快照与采样空档。
 * English: Battery history under Settings, showing actual 24-hour/seven-day samples and gaps.
 * 冻结：不主动读取硬件、不编造休眠曲线、不推算分项耗电；render只绘图。
 * Frozen: No active hardware reads, fabricated sleep curves or per-feature energy estimates; render only paints.
 */
#include "app.h"
#include "app_registry.h"
#include "os_history_service.h"
#include "ui_product.h"
#include "ui_gesture.h"
#include "ui_press.h"
#include "ui_menu.h"
#include <stdio.h>

static bool s_week;
static unsigned s_revision;
static int64_t s_poll_ms;
static os_history_snapshot_t s_snapshot;
static ui_press_t s_press;
static EpdRect control(int id) {
    return id == 2 ? ui_product_back_rect() : ui_row_rect(id, 2, 350, 68);
}
static const char* charging(uint8_t state) {
    return state == 2 ? "充电中" : state == 3 ? "已充满" : state == 1 ? "未充电" : state == 4 ? "充电状态异常" : "充电状态未知";
}
static void render(app_ctx_t* ctx, uint8_t* fb) {
    (void)ctx; ui_clear_page(fb);
    ui_product_header(fb, "电量历史", "真实快照 · 不额外唤醒设备");
    ui_draw_button(fb, control(2), "返回设备", false);
    char line[96];
    if (s_snapshot.permille >= 0) snprintf(line, sizeof(line), "%d%%", s_snapshot.permille / 10);
    else snprintf(line, sizeof(line), "--%%");
    ui_text(fb, UI_MARGIN, 210, 64, line, EPD_DRAW_ALIGN_LEFT, false);
    if (s_snapshot.permille >= 0 && s_snapshot.millivolts)
        snprintf(line, sizeof(line), "%u.%03u V · %s", s_snapshot.millivolts / 1000,
                 s_snapshot.millivolts % 1000, charging(s_snapshot.charge_state));
    else snprintf(line, sizeof(line), "尚无有效电量快照");
    ui_product_title(fb, (EpdRect){UI_MARGIN, 292, ui_content_width(), 40}, line, 28, 1);
    ui_draw_button(fb, control(0), "近24小时", !s_week);
    ui_draw_button(fb, control(1), "近七天", s_week);
    EpdRect graph = {90, 476, 540, 320};
    for (int percent = 0; percent <= 100; percent += 25) {
        int y = graph.y + graph.height - graph.height * percent / 100;
        snprintf(line, sizeof(line), "%d", percent);
        ui_text_vc(fb, graph.x - 12, y, 22, line, EPD_DRAW_ALIGN_RIGHT, false);
        ui_hairline(fb, y, graph.x, graph.width, UI_GRAY_LIGHT);
    }
    const os_battery_history_t* history = os_history_battery();
    uint32_t window = s_week ? OS_BATTERY_WINDOW : 86400;
    uint32_t end = s_snapshot.utc, start = end > window ? end - window : 0;
    size_t visible = 0; const os_battery_sample_t* previous = NULL;
    int last_x = 0, last_y = 0;
    for (size_t i = 0; s_snapshot.valid && history && i < history->count; ++i) {
        const os_battery_sample_t* sample = &history->samples[i];
        if (sample->at < start || sample->at > end) continue;
        int x = graph.x + (int)((uint64_t)(sample->at - start) * graph.width / window);
        int y = graph.y + graph.height - graph.height * sample->permille / 1000;
        if (previous && sample->at - previous->at <= 1800)
            epd_draw_line(last_x, last_y, x, y, UI_GRAY_BLACK, fb);
        epd_fill_rect((EpdRect){x - 1, y - 1, 3, 3}, UI_GRAY_BLACK, fb);
        previous = sample; last_x = x; last_y = y; ++visible;
    }
    if (!visible) ui_text_vc(fb, graph.x + graph.width / 2, graph.y + graph.height / 2,
        28, s_snapshot.valid ? "该时段尚无采样" : "校时后开始记录", EPD_DRAW_ALIGN_CENTER, false);
    ui_text(fb, graph.x, 818, 24, s_week ? "七天前" : "24小时前", EPD_DRAW_ALIGN_LEFT, false);
    ui_text(fb, graph.x + graph.width, 818, 24, "现在", EPD_DRAW_ALIGN_RIGHT, false);
    ui_product_title(fb, (EpdRect){UI_MARGIN, 880, ui_content_width(), 80},
        "运行时每15分钟采样；超过30分钟空档不连线。", 26, 2);
    if (!history) snprintf(line,sizeof(line),"历史尚未就绪，将自动重试");
    else if (os_history_battery_read_error()) snprintf(line, sizeof(line), "TF历史读取失败，原文件保留");
    else if (!history->count) snprintf(line,sizeof(line),"等待有效时间与电量快照");
    else if (!os_history_media_ready()) snprintf(line, sizeof(line), "无TF卡，仅保留本次运行记录");
    else snprintf(line, sizeof(line), "%u个真实样本 · %s", (unsigned)visible,
        os_history_battery_pending() ? "待保存，将自动重试" : "已保存到TF卡");
    ui_product_title(fb, (EpdRect){UI_MARGIN, 974, ui_content_width(), 82}, line, 26, 2);
    ui_draw_menu_handle(fb, false);
}
static int hit(uint16_t x, uint16_t y) {
    for (int i = 0; i < 3; ++i) if (ui_rect_hit(control(i), x, y)) return i;
    return -1;
}
static app_redraw_t gesture(app_ctx_t* ctx, const ui_gesture_event_t* ev) {
    int id = hit(ev->x0, ev->y0);
    if (ev->type == UI_GESTURE_PRESS)
        return id >= 0 ? ui_press_begin(ctx, &s_press, id, control(id), render) : APP_REDRAW_NONE;
    if (ev->type != UI_GESTURE_TAP) return ui_press_restore(ctx, &s_press, render);
    ui_press_forget(&s_press);
    if (id < 0 || id != hit(ev->x, ev->y)) return APP_REDRAW_NONE;
    if (id == 2) { ctx->request_app = app_by_id(OS_APP_STORAGE); return APP_REDRAW_NONE; }
    s_week = id == 1; return APP_REDRAW_PAGE;
}
static void enter(app_ctx_t* ctx) {
    ui_press_reset(&s_press); os_history_snapshot(ctx->now_ms, &s_snapshot);
    s_revision = os_history_revision(); s_poll_ms = ctx->now_ms;
}
static app_redraw_t tick(app_ctx_t* ctx) {
    ui_press_settle(ctx, &s_press, ctx->now_ms);
    if (ctx->consumed || ctx->now_ms - s_poll_ms < 1000) return APP_REDRAW_NONE;
    s_poll_ms = ctx->now_ms;
    os_history_snapshot_t fresh; os_history_snapshot(ctx->now_ms, &fresh);
    bool changed = s_revision != os_history_revision() || (fresh.permille<0)!=(s_snapshot.permille<0) || fresh.permille / 10 != s_snapshot.permille / 10 ||
                   fresh.charge_state != s_snapshot.charge_state || fresh.valid != s_snapshot.valid;
    s_snapshot = fresh; s_revision = os_history_revision();
    return changed ? APP_REDRAW_PAGE : APP_REDRAW_NONE;
}
const app_desc_t app_os_battery = {
    .title = "电量历史", .detail = "24小时 · 七天 · 真实快照", .clean_page = true,
    .render = render, .on_enter = enter, .on_tick = tick, .on_gesture = gesture,
};
