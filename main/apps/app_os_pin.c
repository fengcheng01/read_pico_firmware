/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 *
 * 中文：设置下的锁屏密码管理页。已设密码先验旧密码，再设置新密码（两次
 * 确认）或清除；开机校验在主循环之前由 app_lock_pin_challenge 阻塞完成。
 * English: Lock-PIN management under Settings. An armed PIN verifies the old
 * one first, then sets a new PIN (confirmed twice) or clears it; the boot gate
 * blocks before the loop via app_lock_pin_challenge.
 *
 * 冻结：密码只存 NVS 的 4 位数字；本页不执行开机拦截；render 只绘图。
 * Frozen: The PIN is only a 4-digit NVS value; this page never gates boot
 * itself; render only paints.
 */
#include "app.h"
#include "app_registry.h"
#include "os_catalog.h"
#include "settings.h"
#include "ui_gesture.h"
#include "ui_menu.h"
#include "ui_product.h"
#include <string.h>

typedef enum {
    PIN_VERIFY, ///< 验证旧密码 / Verify the old PIN
    PIN_CHOICE, ///< 设置或清除 / Set or clear
    PIN_NEW, ///< 输入新密码 / Enter the new PIN
    PIN_CONFIRM, ///< 再次确认 / Confirm the new PIN
} pin_stage_t;

static pin_stage_t s_stage;
static char s_input[8];
static char s_draft[8];
static unsigned s_count;
static char s_message[64];

static EpdRect choice_rect(int i) { return ui_row_rect(i, 2, 560, UI_BTN_H); }

static void reset_stage(void) {
    char pin[8];
    s_stage = app_settings_lock_pin(pin, sizeof(pin)) && pin[0] ? PIN_VERIFY : PIN_NEW;
    s_count = 0;
    s_message[0] = 0;
}

static void on_enter(app_ctx_t* ctx) { (void)ctx; reset_stage(); }

static void render(app_ctx_t* ctx, uint8_t* fb) {
    (void)ctx;
    if (s_stage == PIN_CHOICE) {
        ui_clear_page(fb);
        ui_product_header(fb, "锁屏密码", "已验证，选择操作");
        ui_draw_button(fb, (EpdRect){470, 68, 174, 68}, "返回", false);
        ui_draw_button(fb, choice_rect(0), "设置新密码", true);
        ui_draw_button(fb, choice_rect(1), "清除密码", true);
        if (s_message[0]) ui_text(fb, UI_MARGIN, 760, 28, s_message, EPD_DRAW_ALIGN_LEFT, false);
        ui_product_title(fb, (EpdRect){UI_MARGIN, 700, ui_content_width(), 64},
                         "清除后开机电量键即可进入；新密码需输入两次。", 26, 2);
        return;
    }
    const char* title = s_stage == PIN_VERIFY ? "输入当前密码" :
                        s_stage == PIN_NEW ? "输入新密码" : "再次输入新密码";
    ui_product_lock_keypad(fb, title, s_message, s_count, true);
}

static void back_to_sleep(app_ctx_t* ctx) {
    ctx->request_app = app_by_id(OS_APP_SLEEP);
}

static app_redraw_t gesture(app_ctx_t* ctx, const ui_gesture_event_t* ev) {
    if (ev->type != UI_GESTURE_TAP) return APP_REDRAW_NONE;
    if (ui_rect_hit((EpdRect){470, 68, 174, 68}, ev->x0, ev->y0) &&
        ui_rect_hit((EpdRect){470, 68, 174, 68}, ev->x, ev->y)) {
        back_to_sleep(ctx);
        return APP_REDRAW_NONE;
    }
    if (s_stage == PIN_CHOICE) {
        for (int i = 0; i < 2; ++i) {
            EpdRect r = choice_rect(i);
            if (!ui_rect_hit(r, ev->x0, ev->y0) || !ui_rect_hit(r, ev->x, ev->y)) continue;
            if (i == 0) {
                s_stage = PIN_NEW;
                s_count = 0;
                s_message[0] = 0;
            } else if (app_settings_set_lock_pin("")) {
                snprintf(s_message, sizeof(s_message), "已清除");
                back_to_sleep(ctx);
                return APP_REDRAW_NONE;
            } else {
                snprintf(s_message, sizeof(s_message), "清除失败，请重试");
            }
            return APP_REDRAW_PAGE;
        }
        return APP_REDRAW_NONE;
    }
    int key = ui_product_lock_keypad_hit(ev->x0, ev->y0);
    if (key < 0 || key != ui_product_lock_keypad_hit(ev->x, ev->y)) return APP_REDRAW_NONE;
    s_message[0] = 0;
    if (key == 10) s_count = 0;
    else if (key == 11) {
        if (s_count) --s_count;
    } else if (s_count < 4) {
        s_input[s_count++] = (char)('0' + key);
    }
    if (s_count < 4) return APP_REDRAW_PAGE;
    s_input[4] = 0;
    if (s_stage == PIN_VERIFY) {
        char pin[8];
        app_settings_lock_pin(pin, sizeof(pin));
        if (!strcmp(s_input, pin)) {
            s_stage = PIN_CHOICE;
            s_count = 0;
        } else {
            snprintf(s_message, sizeof(s_message), "密码错误，请重试");
            s_count = 0;
        }
    } else if (s_stage == PIN_NEW) {
        strcpy(s_draft, s_input);
        s_stage = PIN_CONFIRM;
        s_count = 0;
    } else if (!strcmp(s_input, s_draft)) {
        if (app_settings_set_lock_pin(s_input)) {
            snprintf(s_message, sizeof(s_message), "已保存");
            back_to_sleep(ctx);
            return APP_REDRAW_NONE;
        }
        snprintf(s_message, sizeof(s_message), "保存失败，请重试");
        s_stage = PIN_NEW;
        s_count = 0;
    } else {
        snprintf(s_message, sizeof(s_message), "两次输入不一致，请重新设置");
        s_stage = PIN_NEW;
        s_count = 0;
    }
    return APP_REDRAW_PAGE;
}

static app_redraw_t key(app_ctx_t* ctx, int key_) {
    if (key_ == UI_KEY_1) back_to_sleep(ctx);
    return APP_REDRAW_NONE;
}

const app_desc_t app_os_pin = {
    .title = "锁屏密码", .detail = "设置 · 修改 · 清除", .enter_full = true,
    .render = render, .on_gesture = gesture, .on_key = key, .on_enter = on_enter,
};
