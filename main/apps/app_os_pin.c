/*
 * SPDX-FileCopyrightText: 2026 mindreset
 *
 * 中文：设置下的锁屏密码管理页。已设密码先验旧密码，再设置新密码（两次
 * 确认）、清除或切换解锁验证频率；开机校验在主循环之前由 app_lock_pin_challenge 阻塞完成。
 * English: Lock-PIN management under Settings. An armed PIN verifies the old
 * one first, then sets a new PIN (confirmed twice), clears it, or switches the
 * unlock-verification cadence; the boot gate blocks before the loop via
 * app_lock_pin_challenge.
 *
 * 冻结：密码只存 NVS 的 4 位数字；本页不执行开机拦截；render 只绘图。
 * Frozen: The PIN is only a 4-digit NVS value; this page never gates boot
 * itself; render only paints.
 */
#include "app.h"
#include "app_registry.h"
#include "display.h"
#include "e0470_epaper_waveform.h"
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
// 按下目标：-1 无、0..11 键位、100+ 选项行、200 返回。抬起命中同一目标才生效。
// Press target: -1 none, 0..11 keypad keys, 100+ choice rows, 200 back. Only a
// release over the same target commits — the same rule as the challenge loop.
static int s_press_target = -1;
static unsigned s_quick;

static EpdRect choice_rect(int i) { return ui_row_rect(i, 2, 560, UI_BTN_H); }
// 解锁验证开关整行宽：长文案不再挤出按钮框。/ The unlock-verification row is full
// width so its long label never overflows the button frame.
static EpdRect wake_rect(void) { return (EpdRect){UI_MARGIN, 560 + 2 * (UI_BTN_H + UI_GAP), ui_content_width(), UI_BTN_H}; }
static EpdRect choice_target_rect(int target) { return target == 102 ? wake_rect() : choice_rect(target - 100); }
static const char* wake_label(void) {
    return app_settings_lock_pin_wake() ? "解锁验证：每次解锁 ›" : "解锁验证：仅开机 ›";
}
static EpdRect dots_rect(void) { return (EpdRect){(UI_LOCK_WIDTH - 336) / 2 - 10, 216, 360, 56}; }

// 圆点带与键位的并集一次推完。/ Push the dots strip and key union in one pass.
static void push_union(app_ctx_t* ctx, int key) {
    EpdRect dots = dots_rect();
    EpdRect pressed = ui_product_lock_key_rect(key);
    EpdRect both = {
        .x = dots.x < pressed.x ? dots.x : pressed.x,
        .y = dots.y < pressed.y ? dots.y : pressed.y,
        .width = 0,
        .height = 0,
    };
    int right = dots.x + dots.width, bottom = dots.y + dots.height;
    if (pressed.x + pressed.width > right) right = pressed.x + pressed.width;
    if (pressed.y + pressed.height > bottom) bottom = pressed.y + pressed.height;
    both.width = right - both.x;
    both.height = bottom - both.y;
    guard_draw_result(ctx->hl, update_display_area_with(
        ctx->hl, &E0470_FOLLOW_WAVEFORM, MODE_DU, both));
    ++s_quick;
}

static void settle(app_ctx_t* ctx) {
    guard_draw_result(ctx->hl, update_display_mode(ctx->hl, MODE_GL16));
    s_quick = 0;
}

static void reset_stage(void) {
    char pin[8];
    s_stage = app_settings_lock_pin(pin, sizeof(pin)) && pin[0] ? PIN_VERIFY : PIN_NEW;
    s_count = 0;
    s_message[0] = 0;
    s_press_target = -1;
    s_quick = 0;
}

static void on_enter(app_ctx_t* ctx) { (void)ctx; reset_stage(); }

static const char* stage_detail(void) {
    switch (s_stage) {
        case PIN_VERIFY: return "输入当前密码";
        case PIN_NEW: return "输入新密码";
        case PIN_CONFIRM: return "再次输入新密码";
        default: return "已验证，选择操作";
    }
}

static void render(app_ctx_t* ctx, uint8_t* fb) {
    (void)ctx;
    ui_clear_page(fb);
    ui_product_header(fb, "锁屏密码", stage_detail());
    if (s_stage == PIN_CHOICE) {
        ui_draw_button(fb, choice_rect(0), "设置新密码", true);
        ui_draw_button(fb, choice_rect(1), "清除密码", true);
        ui_draw_button(fb, wake_rect(), wake_label(), false);
        if (s_message[0]) ui_text(fb, UI_MARGIN, 900, 28, s_message, EPD_DRAW_ALIGN_LEFT, false);
        ui_product_title(fb, (EpdRect){UI_MARGIN, 936, ui_content_width(), 64},
                         "仅开机时，浅睡唤醒不再输入密码；开机仍需验证。", 26, 2);
    } else {
        ui_product_lock_keypad_body(fb, s_message, s_count);
    }
    ui_draw_button(fb, ui_product_back_rect(), "返回", false);
}

static void back_to_sleep(app_ctx_t* ctx) {
    ctx->request_app = app_by_id(OS_APP_SLEEP);
}

static void choice_action(app_ctx_t* ctx, int i) {
    if (i == 0) {
        s_stage = PIN_NEW;
        s_count = 0;
        s_message[0] = 0;
    } else if (i == 1) {
        if (app_settings_set_lock_pin("")) {
            back_to_sleep(ctx);
            return;
        }
        snprintf(s_message, sizeof(s_message), "清除失败，请重试");
    } else {
        app_settings_set_lock_pin_wake(!app_settings_lock_pin_wake());
    }
}

// 一位输入完成后的阶段推进；返回 true 表示已切换页面。
// Stage advance after one input completes; true means the page already switched.
static bool digit_done(app_ctx_t* ctx) {
    if (s_count < 4) return false;
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
            back_to_sleep(ctx);
            return true;
        }
        snprintf(s_message, sizeof(s_message), "保存失败，请重试");
        s_stage = PIN_NEW;
        s_count = 0;
    } else {
        snprintf(s_message, sizeof(s_message), "两次输入不一致，请重新设置");
        s_stage = PIN_NEW;
        s_count = 0;
    }
    return false;
}

// 抬起命中同一目标才生效；与挑战循环一致。/ Only a release over the same target counts; matching the challenge loop.
static app_redraw_t on_touch(app_ctx_t* ctx, const cst836u_touch_t* touch) {
    if (!touch || !touch->touched) return APP_REDRAW_NONE;
    EpdRect rect = {0};
    int target = -1;
    if (ui_rect_hit(ui_product_back_rect(), touch->x, touch->y)) {
        target = 200;
        rect = ui_product_back_rect();
    } else if (s_stage == PIN_CHOICE) {
        for (int i = 0; i < 3; ++i)
            if (ui_rect_hit(choice_target_rect(100 + i), touch->x, touch->y)) {
                target = 100 + i;
                rect = choice_target_rect(target);
                break;
            }
    } else {
        int hit = ui_product_lock_keypad_hit(touch->x, touch->y);
        if (hit >= 0) {
            target = hit;
            rect = ui_product_lock_key_rect(hit);
        }
    }
    if (target < 0) return APP_REDRAW_NONE;
    // 按下立即高亮，不整页重绘。/ Highlight on press; no full-page repaint.
    if (target < 100) ui_product_lock_key(ctx->fb, target, true);
    else {
        ui_clear_rect_fast(ctx->fb, rect);
        ui_draw_pressed_round_rect(ctx->fb, rect, UI_BTN_RADIUS);
        if (target == 200) ui_text_vc(ctx->fb, rect.x + rect.width / 2, rect.y + rect.height / 2,
                                      UI_PX_BTN, "返回", EPD_DRAW_ALIGN_CENTER, false);
        else ui_text_vc(ctx->fb, rect.x + rect.width / 2, rect.y + rect.height / 2,
                        UI_PX_BTN, target == 100 ? "设置新密码" : target == 101 ? "清除密码" : wake_label(),
                        EPD_DRAW_ALIGN_CENTER, false);
        guard_draw_result(ctx->hl, update_display_area_with(
            ctx->hl, &E0470_FOLLOW_WAVEFORM, MODE_DU, rect));
        ++s_quick;
    }
    if (target < 100) {
        guard_draw_result(ctx->hl, update_display_area_with(
            ctx->hl, &E0470_FOLLOW_WAVEFORM, MODE_DU, ui_product_lock_key_rect(target)));
        ++s_quick;
    }
    s_press_target = target;
    return APP_REDRAW_NONE;
}

static int target_hit(int target, uint16_t x, uint16_t y) {
    if (target == 200) return ui_rect_hit(ui_product_back_rect(), x, y);
    if (target >= 100) return target < 103 && ui_rect_hit(choice_target_rect(target), x, y);
    return ui_product_lock_keypad_hit(x, y) == target;
}

static app_redraw_t on_tick(app_ctx_t* ctx) {
    if (ctx->consumed) return APP_REDRAW_NONE;
    const cst836u_touch_t* touch = ctx->touch;
    if (s_press_target < 0) return APP_REDRAW_NONE;
    uint16_t x = touch ? touch->x : 0, y = touch ? touch->y : 0;
    if (touch && touch->touched) {
        // 滑离目标：恢复常态，取消本次输入。/ Sliding off restores the key and cancels.
        if (!target_hit(s_press_target, x, y)) {
            int held = s_press_target;
            s_press_target = -1;
            if (held < 100) {
                ui_product_lock_key(ctx->fb, held, false);
                guard_draw_result(ctx->hl, update_display_area_with(
                    ctx->hl, &E0470_FOLLOW_WAVEFORM, MODE_DU, ui_product_lock_key_rect(held)));
            } else {
                EpdRect rect = held == 200 ? ui_product_back_rect() : choice_target_rect(held);
                ui_clear_rect_fast(ctx->fb, rect);
                if (held == 200) ui_draw_button(ctx->fb, rect, "返回", false);
                else if (held == 100) ui_draw_button(ctx->fb, rect, "设置新密码", true);
                else if (held == 101) ui_draw_button(ctx->fb, rect, "清除密码", true);
                else ui_draw_button(ctx->fb, rect, wake_label(), false);
                guard_draw_result(ctx->hl, update_display_area_with(
                    ctx->hl, &E0470_FOLLOW_WAVEFORM, MODE_DU, rect));
            }
            ++s_quick;
        }
        return APP_REDRAW_NONE;
    }
    // 抬起：同一目标才生效。/ Released: commit only over the same target.
    int target = s_press_target;
    s_press_target = -1;
    if (!target_hit(target, x, y)) return APP_REDRAW_NONE;
    if (target == 200) {
        back_to_sleep(ctx);
        return APP_REDRAW_NONE;
    }
    if (target >= 100) {
        choice_action(ctx, target - 100);
        return ctx->request_app ? APP_REDRAW_NONE : APP_REDRAW_PAGE;
    }
    int key = target;
    bool clear_message = s_message[0] != 0;
    s_message[0] = 0;
    if (key == 10) s_count = 0;
    else if (key == 11) {
        if (s_count) --s_count;
    } else if (s_count < 4) {
        s_input[s_count++] = (char)('0' + key);
    }
    if (digit_done(ctx)) return APP_REDRAW_NONE;
    ui_product_lock_keypad_body(ctx->fb, s_message, s_count);
    if (clear_message || s_quick >= UI_SETTLE_DU_MAX) settle(ctx);
    else push_union(ctx, key);
    return APP_REDRAW_NONE;
}

static app_redraw_t key(app_ctx_t* ctx, int key_) {
    if (key_ == UI_KEY_1) back_to_sleep(ctx);
    return APP_REDRAW_NONE;
}

const app_desc_t app_os_pin = {
    .title = "锁屏密码", .detail = "设置 · 修改 · 清除", .enter_full = false,
    .render = render, .on_touch = on_touch, .on_tick = on_tick, .on_key = key, .on_enter = on_enter,
};
