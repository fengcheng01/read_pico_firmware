/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 * 中文：直接验证触屏配网状态机及输入边界。
 * English: Exercise the device provisioning state machine and input bounds.
 * 冻结：仅供宿主测试，不访问网络。/ Frozen: Host tests only; no network access.
 */
#include "transfer_ui_test_env.h"
#include "../main/apps/app_transfer.c"
#include <assert.h>
// 产品服务替身仅验证页面生命周期；真实协议和任务另有宿主测试。
// Product service substitutes verify page lifecycle; separate host tests cover real protocol/tasks.
static bool test_auto, test_sync_busy, test_sync_pending, test_start_ok=true;
static int test_sync_starts, test_sync_cancels;
static char test_sync_url[192], test_sync_user[64];
const char* app_settings_sync_url(void) { return test_sync_url; }
const char* app_settings_sync_user(void) { return test_sync_user; }
const char* app_settings_sync_key(void) { return ""; }
void app_settings_set_sync_url(const char* s) { snprintf(test_sync_url,sizeof(test_sync_url),"%s",s); }
void app_settings_set_sync_user(const char* s) { snprintf(test_sync_user,sizeof(test_sync_user),"%s",s); }
bool app_settings_sync_auto(void) { return test_auto; }
void app_settings_set_sync_auto(bool on) { test_auto=on; }
void os_sync_set_password(const char* s) { (void)s; }
bool os_sync_job_busy(void) { return test_sync_busy; }
bool os_sync_pull_pending(void) { return test_sync_pending; }
bool os_sync_job_start(os_sync_job_t job,char* note,size_t cap) { (void)job; ++test_sync_starts; snprintf(note,cap,"test"); test_sync_busy=test_start_ok && read_pico_transfer_claim_sync(); return test_sync_busy; }
bool os_sync_job_poll(char* note,size_t cap) { (void)note;(void)cap;return false; }
bool os_sync_pull_confirm(bool apply,char* note,size_t cap) { (void)apply;snprintf(note,cap,"confirmed");test_sync_pending=false;read_pico_transfer_release_sync();return true; }
void os_sync_job_cancel(void) { ++test_sync_cancels;test_sync_busy=test_sync_pending=false;read_pico_transfer_release_sync(); }
void os_sync_job_request_cancel(void) { test_sync_busy=false; }
void os_time_network(bool up) { (void)up; }
bool app_sleep_prepare_register(app_sleep_prepare_fn fn) { (void)fn;return true; }
void app_sleep_prepare_unregister(app_sleep_prepare_fn fn) { (void)fn; }
EpdRect ui_product_back_rect(void) { return (EpdRect){470,60,174,80}; }
void ui_product_header(uint8_t* fb,const char* title,const char* detail) { (void)fb;(void)title;(void)detail; }
void ui_product_title(uint8_t* fb,EpdRect rect,const char* title,int px,int lines) { (void)fb;(void)rect;(void)title;(void)px;(void)lines; }
static void tap(app_ctx_t* ctx, EpdRect rect) {
    ui_gesture_event_t ev = {.type = UI_GESTURE_PRESS, .x0 = rect.x + 2, .y0 = rect.y + 2, .x = rect.x + 2, .y = rect.y + 2};
    on_gesture(ctx, &ev);
    ev.type = UI_GESTURE_TAP;
    on_gesture(ctx, &ev);
}
static void qr_regression(app_ctx_t* ctx) {
    test_status = (read_pico_transfer_status_t){.mode=READ_PICO_TRANSFER_MODE_AP,.state=READ_PICO_TRANSFER_READY,.network_ready=true};
    strcpy(test_status.ssid,"ReadPico-test");
    strcpy(test_status.url,"http://192.168.4.1");
    s_mode = READ_PICO_TRANSFER_MODE_AP;
    test_configured = false;
    on_enter(ctx);
    on_tick(ctx);
    assert(s_qr_ready && !s_qr_url && !strcmp(test_qr_payload,"ReadPico-test"));
    int encodes = test_qr_encodes;
    render(ctx,ctx->fb);
    assert(test_qr_encodes == encodes);
    test_status.cur_bytes++;
    ctx->now_ms += 3000;
    on_tick(ctx);
    assert(test_qr_encodes == encodes);
    EpdRect toggle = control_rect(3);
    assert(toggle.x+toggle.width < 404 && toggle.y+toggle.height <= 518);
    ui_gesture_event_t ev={.type=UI_GESTURE_PRESS,.x0=toggle.x+2,.y0=toggle.y+2,.x=toggle.x+2,.y=toggle.y+2};
    on_gesture(ctx,&ev);
    ev.type=UI_GESTURE_TAP; ev.x=650;
    on_gesture(ctx,&ev);
    assert(!s_qr_url && test_qr_encodes==encodes);
    tap(ctx,toggle);
    assert(s_qr_url && s_qr_ready && !strcmp(test_qr_payload,test_status.url));
    strcpy(test_status.url,"http://192.168.4.2");
    ctx->now_ms += 3000;
    on_tick(ctx);
    assert(!strcmp(test_qr_payload,test_status.url));
    test_status.network_ready=false;
    ctx->now_ms += 3000;
    on_tick(ctx);
    assert(!s_qr_ready && !test_qr_payload[0]);
    encodes=test_qr_encodes;
    tap(ctx,toggle);
    assert(test_qr_encodes==encodes);
    test_status.network_ready=true;
    test_qr_failure=true;
    ctx->now_ms += 3000;
    on_tick(ctx);
    assert(!s_qr_ready);
    test_qr_failure=false;
    queue_network_start(READ_PICO_TRANSFER_MODE_STA);
    assert(!s_qr_ready && !test_qr_payload[0]);
    test_status.mode=READ_PICO_TRANSFER_MODE_STA;
    strcpy(test_status.url,"http://10.20.30.40");
    on_tick(ctx);
    assert(s_qr_url && s_qr_ready && !strcmp(test_qr_payload,test_status.url));
    encodes=test_qr_encodes;
    tap(ctx,toggle);
    render(ctx,ctx->fb);
    assert(test_qr_encodes==encodes);
    stop_session();
    assert(!s_qr_ready && !test_qr_payload[0]);
    test_status=(read_pico_transfer_status_t){0};
    s_mode=READ_PICO_TRANSFER_MODE_AP;
    test_qr_encodes=0;
    test_configured=true;
    ctx->now_ms=0;
}
int main(void) {
    uint8_t fb = 0;
    app_ctx_t ctx = {.fb = &fb};
    on_enter(&ctx);
    on_tick(&ctx);
    assert(test_qr_encodes == 0);
    qr_regression(&ctx);
    for (int i = 0; i < 47; ++i) {
        EpdRect r = password_control_rect(i);
        assert(r.x >= 0 && r.x + r.width <= UI_LOCK_WIDTH);
        assert(r.y >= 0 && r.y + r.height <= UI_LOCK_HEIGHT);
    }
    on_enter(&ctx);
    test_busy = true;
    tap(&ctx, control_rect(1));
    assert(s_view == TRANSFER_HOME);
    test_busy = false;
    tap(&ctx, control_rect(1));
    assert(s_view == TRANSFER_NETWORKS && s_scan_pending);
    network_ui_tick(&ctx);
    assert(s_network_count == 8 && !s_scan_pending);
    tap(&ctx, ui_row_rect(1, 2, 176, 84));
    assert(test_forget_count == 0);
    tap(&ctx, ui_row_rect(1, 2, 600, UI_BTN_H));
    assert(test_forget_count == 0 && s_saved_configured && !s_forget_confirm);
    tap(&ctx, ui_row_rect(1, 2, 176, 84));
    test_forget_error = ESP_FAIL;
    tap(&ctx, ui_row_rect(0, 2, 600, UI_BTN_H));
    assert(test_forget_count == 1 && s_saved_configured && s_forget_confirm);
    test_forget_error = ESP_OK;
    tap(&ctx, ui_row_rect(0, 2, 600, UI_BTN_H));
    assert(test_forget_count == 2 && !s_saved_configured);
    test_configured = true;
    enter_networks();
    network_ui_tick(&ctx);
    tap(&ctx, network_control_rect(9));
    assert(s_network_page == 1);
    tap(&ctx, network_control_rect(7));
    assert(s_network_page == 0);
    tap(&ctx, network_control_rect(0));
    assert(s_view == TRANSFER_PASSWORD && !s_password[0]);
    tap(&ctx, password_control_rect(46));
    assert(test_save_count == 0 && s_view == TRANSFER_PASSWORD);
    ui_gesture_event_t cancel = {.type = UI_GESTURE_PRESS,.x0 = 42,.y0 = 422,.x = 42,.y = 422};
    on_gesture(&ctx, &cancel);
    cancel.type = UI_GESTURE_TAP;
    cancel.x = 650;
    on_gesture(&ctx, &cancel);
    assert(!s_password[0]);
    bool reachable[128] = {0};
    for (int mode = 0; mode < 3; ++mode) {
        s_keyboard_mode = mode;
        for (int i = 0; i < 40; ++i) reachable[(unsigned char)keyboard_chars()[i]] = true;
    }
    reachable[' '] = true;
    for (int c = 32; c < 127; ++c) assert(reachable[c]);
    s_keyboard_mode = 0;
    for (int i = 0; i < 70; ++i) tap(&ctx, password_control_rect(0));
    assert(strlen(s_password) == 64);
    tap(&ctx, password_control_rect(43));
    assert(strlen(s_password) == 63);
    tap(&ctx, password_control_rect(45));
    assert(s_password_visible);
    test_save_error = ESP_FAIL;
    tap(&ctx, password_control_rect(46));
    assert(s_view == TRANSFER_PASSWORD && strlen(s_password) == 63);
    test_save_error = ESP_OK;
    tap(&ctx, password_control_rect(46));
    assert(s_view == TRANSFER_HOME && s_mode == READ_PICO_TRANSFER_MODE_STA && s_start_pending);
    assert(!s_password[0]);
    assert(strcmp(test_saved_ssid, "中文家庭网络") == 0);
    tap(&ctx, control_rect(1));
    network_ui_tick(&ctx);
    tap(&ctx, network_control_rect(0));
    tap(&ctx, password_control_rect(0));
    tap(&ctx, password_control_rect(44));
    assert(s_view == TRANSFER_NETWORKS && !s_password[0]);
    test_scan_count = 0;
    tap(&ctx, network_control_rect(8));
    network_ui_tick(&ctx);
    assert(!s_network_count && s_network_message[0]);
    test_scan_error = ESP_FAIL;
    tap(&ctx, network_control_rect(8));
    network_ui_tick(&ctx);
    assert(!s_network_count && s_network_message[0]);
    tap(&ctx, network_control_rect(6));
    assert(s_view == TRANSFER_HOME && s_start_pending && s_mode == READ_PICO_TRANSFER_MODE_STA);
    int stops_before = test_stop_count;
    tap(&ctx, control_rect(2));
    assert(test_stop_count == stops_before + 1 && !s_start_pending);
    assert(ctx.request_return && ctx.request_app == NULL);
    on_enter(&ctx);
    on_tick(&ctx);
    test_status.changed_count = 1;
    ctx.now_ms = 3000;
    assert(on_tick(&ctx) == APP_REDRAW_FULL);
    assert(s_status.changed_count == 1);
    transfer_on_exit(&ctx);
    assert(!test_font_suspended);
    assert(test_store_changes == 1);
    on_enter(&ctx);
    on_tick(&ctx);
    s_root.is_flash = false;
    s_free = 123456;
    stops_before = test_stop_count;
    app_transfer.on_media_lost(&ctx);
    assert(test_font_suspended);
    assert(test_stop_count == stops_before + 1);
    assert(s_media_lost && !s_free && !s_qr_ready && !s_root.path[0]);
    assert(!s_start_pending && !s_session_started && s_view == TRANSFER_HOME);
    s_root.is_flash = true;
    app_transfer.on_media_lost(&ctx);
    assert(test_stop_count == stops_before + 1);
    test_busy=true;
    assert(!start_sync(OS_SYNC_JOB_PUSH) && !test_sync_claimed);
    test_busy=false;
    test_start_ok=false;
    assert(!start_sync(OS_SYNC_JOB_PUSH) && !test_sync_claimed);
    test_start_ok=true;
    assert(start_sync(OS_SYNC_JOB_PULL) && test_sync_claimed);
    int cancels=test_sync_cancels;
    cancel_sync();
    assert(test_sync_cancels==cancels+1 && !test_sync_claimed && !test_sync_busy);
    // 两次状态轮询间提交的文件，切入同步时仍须通知书架失效。
    // A file committed between status polls must still invalidate the shelf when entering sync.
    s_status.mode=READ_PICO_TRANSFER_MODE_AP; s_status.changed_count=0;
    test_status.changed_count=1; s_session_started=true; s_any_changed=false;
    assert(start_sync(OS_SYNC_JOB_AUTH) && s_any_changed && !s_session_started);
    cancel_sync();
    test_scan_error=0; test_scan_count=8;
    enter_networks(); network_ui_tick(&ctx);
    snprintf(s_saved_ssid,sizeof(s_saved_ssid),"%s",s_networks[0].ssid); s_saved_configured=true;
    tap(&ctx, network_control_rect(0));
    assert(s_view==TRANSFER_HOME && s_start_pending && s_mode==READ_PICO_TRANSFER_MODE_STA);
    on_enter(&ctx); assert(s_mode==READ_PICO_TRANSFER_MODE_STA);
    queue_network_start(READ_PICO_TRANSFER_MODE_AP); assert(s_mode==READ_PICO_TRANSFER_MODE_AP);
    test_sync_pending=true; test_sync_claimed=true;
    sync_action(&ctx,912);
    assert(!test_sync_pending && !test_sync_claimed);
    test_auto=true; test_start_ok=false; s_sync_pushed=false;
    s_media_lost=false; s_start_pending=false; s_scan_pending=false;
    s_view=TRANSFER_HOME; s_session_started=true; s_mode=READ_PICO_TRANSFER_MODE_STA;
    test_status.mode=READ_PICO_TRANSFER_MODE_STA; test_status.network_ready=true;
    test_status.state=READ_PICO_TRANSFER_READY;
    int starts=test_sync_starts;
    on_tick(&ctx); on_tick(&ctx);
    assert(test_sync_starts==starts+1 && s_sync_pushed);
    puts("transfer_ui_host_test: PASS");
}
