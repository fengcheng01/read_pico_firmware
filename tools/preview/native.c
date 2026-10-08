/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 * 中文：单线程预览调度器，把原页面回调的 framebuffer 输出为竖屏灰度。
 * English: Single-thread preview dispatcher exporting real page callbacks as portrait grayscale.
 * 冻结：只运行已适配页面；所有硬件推屏为零耗时记录，不伪造传感器数值。
 * Frozen: Adapted pages only; presents record zero physical time and do not fabricate sensor values.
 * 冻结：布局入口同步设备的一次导航清理标志；普通重绘不重复，全程不模拟纸屏残影。
 * Frozen: Layout entries mirror the device's one-shot navigation cleanup marker; ordinary redraws do not repeat it, and panel ghosting is never modeled.
 * 冻结：成功夜间正文翻页共用gc_every计数；整屏GC归零，普通控件、局推、页脚和日间翻页不计。
 * Frozen: Successful night body turns share the gc_every count; full-screen GC resets it, and controls, local pushes, footers and day turns never count.
 */
#include "preview_host.h"
#include "display_pixels.h"
#include "app.h"
#include "app_registry.h"
#include "os_catalog.h"
#include "settings.h"
#include "ttf_font.h"
#include "ui_kit.h"
#include "ui_menu.h"
#include "ui_gesture.h"
#include "book_entry.h"
#include "book_quotes.h"
#include "book_home.h"
#include "book_source.h"
#include "os_time.h"
#include <string.h>
#include <time.h>

static uint8_t fb[1216 * 684 / 2];
static EpdiyHighlevelState hl = {.front_fb = fb};
static app_ctx_t ctx = {.hl = &hl, .fb = fb};
static const app_desc_t* current;
static ui_gesture_t s_swipe_gesture;
static int s_prev_page = -1;
static bool s_prev_menu;
static int s_prev_leaf;
static int s_prev_ctx_leaf;
static bool s_navigation_entry;
static bool menu_open = true, white_exit;
static int menu_leaf, unsupported = -1, asset = -1, refresh_mode, presents, gc_presents;
static unsigned s_night_body_turns, body_presents, quiet_presents, night_area_presents;
static int refresh_wave;
static char font_path[TTF_FONT_PATH_MAX];
void preview_home_fixture(int value);
void preview_fixture(int value);
unsigned preview_sync_starts(void);
int preview_sync_job(void);
void preview_time_step(int seconds);
void preview_draw_lock_clock(uint8_t* framebuffer);

const EpdWaveform E0470_WAVEFORM = {0}, E0470_FULL_WAVEFORM = {1}, E0470_GRAY8_WAVEFORM = {2};
const EpdWaveform E0470_FOLLOW_WAVEFORM = {3};
const EpdWaveform E0470_NAVIGATION_WAVEFORM = {4};
const EpdWaveform E0470_TEXTTURN_NIGHT_WAVEFORM = {5};
const EpdWaveform E0470_TEXTTURN_WAVEFORM = {7};

int64_t esp_timer_get_time(void) {
    struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t);
    return (int64_t)t.tv_sec * 1000000 + t.tv_nsec / 1000;
}
static int64_t s_time_bias_ms;
static int64_t preview_now_ms(void) { return esp_timer_get_time() / 1000 + s_time_bias_ms; }

const char* esp_err_to_name(esp_err_t err) { return err == ESP_OK ? "ESP_OK" : "HOST_ERROR"; }
// 卡状态夹具仅验证 UI，不挂载真实卡。/ Card-state fixture verifies UI without real card mounting.
static bool s_sd_fixture;
esp_err_t read_pico_sd_get_info(read_pico_sd_info_t* out) { *out = (read_pico_sd_info_t){.present=s_sd_fixture,.mounted=s_sd_fixture,.capacity_bytes=32ULL*1024*1024*1024,.free_bytes=16ULL*1024*1024*1024}; return ESP_OK; }
uint8_t* epd_hl_get_framebuffer(EpdiyHighlevelState* state) { return state->front_fb; }
void epd_clear_area(EpdRect area) { (void)area; }
void guard_draw_result(EpdiyHighlevelState* state, enum EpdDrawError err) { (void)state; (void)err; }
void display_hold_white_exit(bool hold) { white_exit = hold; }
bool display_take_white_exit(void) { bool result = white_exit; white_exit = false; return result; }

static enum EpdDrawError record_refresh(enum EpdDrawMode mode) {
    refresh_mode = mode; presents++;
    if ((mode & 0xF) == MODE_GC16) gc_presents++;
    return EPD_DRAW_SUCCESS;
}
enum EpdDrawError update_display_mode(EpdiyHighlevelState* state, enum EpdDrawMode mode) {
    if ((mode & 0xF) == MODE_GC16) return update_display_full(state);
    if ((mode & 0xF) == MODE_GL16)
        return update_display_with(state, s_navigation_entry ? &E0470_NAVIGATION_WAVEFORM : &E0470_WAVEFORM, mode);
    return record_refresh(mode);
}
void display_request_navigation_settle(void) { s_navigation_entry = true; }
enum EpdDrawError update_display_with(EpdiyHighlevelState* state, const EpdWaveform* wave, enum EpdDrawMode mode) {
    (void)state;
    refresh_wave = wave->unused;
    // 入口标志只在导航整页提交时提升一次；宿主记录模式，不模拟光学效果。
    // Promote a navigation page once per entry marker; the host records modes without modeling optics.
    if (s_navigation_entry &&
        (wave == &E0470_NAVIGATION_WAVEFORM || wave == &E0470_TEXTTURN_WAVEFORM ||
         wave == &E0470_TEXTTURN_NIGHT_WAVEFORM) &&
        (mode & 0xF) == MODE_GL16) {
        mode = (enum EpdDrawMode)((mode & ~0xF) | MODE_GC16);
        refresh_wave = E0470_FULL_WAVEFORM.unused;
    }
    enum EpdDrawError result = record_refresh(mode);
    if (result == EPD_DRAW_SUCCESS &&
        ((mode & 0xF) == MODE_GC16 || wave == &E0470_NAVIGATION_WAVEFORM || wave == &E0470_TEXTTURN_WAVEFORM ||
         wave == &E0470_TEXTTURN_NIGHT_WAVEFORM))
        s_navigation_entry = false;
    if (result == EPD_DRAW_SUCCESS && (mode & 0xF) == MODE_GC16) s_night_body_turns = 0;
    return result;
}
enum EpdDrawError update_display_full(EpdiyHighlevelState* state) {
    return update_display_with(state, &E0470_FULL_WAVEFORM, MODE_GC16);
}
enum EpdDrawError update_display_white(EpdiyHighlevelState* state) {
    memset(state->front_fb, 0xff, sizeof(fb)); return update_display_full(state);
}
enum EpdDrawError update_display_from_white(EpdiyHighlevelState* state) { return update_display_full(state); }
enum EpdDrawError update_display_from_white_with(EpdiyHighlevelState* state, const EpdWaveform* wave, enum EpdDrawMode mode) {
    enum EpdDrawError result = update_display_with(state, wave, mode);
    if (result == EPD_DRAW_SUCCESS) s_navigation_entry = false;
    return result;
}
enum EpdDrawError update_display_area_with(EpdiyHighlevelState* state, const EpdWaveform* wave, enum EpdDrawMode mode, EpdRect area) {
    (void)state; (void)area; refresh_wave = wave->unused;
    if (wave == &E0470_TEXTTURN_NIGHT_WAVEFORM) night_area_presents++;
    return record_refresh(mode);
}
// 静默局推按 GL16 记账；验证时钟字带不触发全清与 DU。/ Quiet band push records GL16; clock bands stay off full cleans and DU.
enum EpdDrawError update_display_area_quiet(EpdiyHighlevelState* state, EpdRect area) {
    (void)state; (void)area; quiet_presents++; refresh_wave = -1; return record_refresh(MODE_GL16);
}

// 只记录成功正文出口和周期选择，不模拟厂家扫描或真实残影。
// Record successful body calls and periodic mode selection without modeling vendor scans or physical ghosting.
static enum EpdDrawError record_body(bool night, bool direct) {
    unsigned every = night ? app_settings_gc_every() : 0;
    bool clean = s_navigation_entry || (night && every && s_night_body_turns + 1 >= every);
    refresh_wave = clean ? E0470_FULL_WAVEFORM.unused : direct ? 6 : night ? E0470_TEXTTURN_NIGHT_WAVEFORM.unused : 7;
    enum EpdDrawError result = record_refresh(clean ? MODE_GC16 : MODE_GL16);
    if (result == EPD_DRAW_SUCCESS) {
        body_presents++;
        s_navigation_entry = false;
        if (clean) s_night_body_turns = 0;
        else if (night) s_night_body_turns = every ? s_night_body_turns + 1 : 0;
    }
    return result;
}
enum EpdDrawError update_display_text_turn(EpdiyHighlevelState* state, bool white_on_black) {
    (void)state; return record_body(white_on_black, false);
}
enum EpdDrawError update_display_text_direct(EpdiyHighlevelState* state, bool white_on_black) {
    display_prepare_direct_frame(state->front_fb, epd_width(), epd_height(), white_on_black);
    return record_body(white_on_black, true);
}

static void present(app_redraw_t redraw) {
    if (redraw == APP_REDRAW_NONE || redraw == APP_REDRAW_DONE) return;
    if (current->present && current->present(&ctx, redraw)) return;
    if (redraw != APP_REDRAW_AREA && current->render) current->render(&ctx, fb);
    if (redraw == APP_REDRAW_FULL) update_display_full(ctx.hl);
    else if (redraw == APP_REDRAW_AREA) record_refresh(MODE_DU);
    else if (current->clean_page) update_display_with(ctx.hl, &E0470_NAVIGATION_WAVEFORM, MODE_GL16);
    else update_display_mode(ctx.hl, MODE_GL16);
}

static void draw_menu(void) {
    if (display_take_white_exit()) update_display_white(ctx.hl);
    ui_draw_menu_page(fb, current, menu_leaf);
    update_display_with(ctx.hl, &E0470_NAVIGATION_WAVEFORM, MODE_GL16);
}
static void choose_page_entry(int index, bool from_menu) {
    const app_desc_t* next = app_at(index);
    if (!next) return;
    if (!next->render) {
        book_entry_request_t entry;
        if (book_entry_take(&entry)) book_entry_finish(BOOK_ENTRY_CANCELLED);
        unsupported = index; asset = -1;
        if (!menu_open) display_request_navigation_settle();
        menu_open = true; draw_menu(); return;
    }
    bool changed = next != current;
    if (!changed && !(from_menu && next->on_menu_select)) {
        if (from_menu) {
            menu_open = false;
            display_request_navigation_settle();
            present(APP_REDRAW_PAGE);
        }
        return;
    }
    if (changed) {
        s_prev_page = current ? app_index_of(current) : -1;
        s_prev_menu = from_menu;
        s_prev_leaf = menu_leaf;
        s_prev_ctx_leaf = ctx.leaf;
    }
    if (current && current->on_exit) current->on_exit(&ctx);
    current = next; ctx.leaf = 0; asset = -1; unsupported = -1; menu_open = false;
    menu_leaf = ui_menu_leaf_for_app(current);
    if (from_menu && next->on_menu_select) next->on_menu_select(&ctx);
    if (current->on_enter) current->on_enter(&ctx);
    if (!next->enter_full) display_request_navigation_settle();
    present(next->enter_full ? APP_REDRAW_FULL : APP_REDRAW_PAGE);
}

static void choose_page(int index) { choose_page_entry(index, false); }

static void requests(void) {
    const app_desc_t* next = ctx.request_app;
    bool menu = ctx.request_menu;
    bool back = ctx.request_return;
    ctx.request_app = NULL; ctx.request_menu = false; ctx.request_return = false;
    ctx.request_app_rearm_touch = false;
    if (back) {
        // 与设备同语义：回到最近切页来源，菜单来源恢复菜单位置，无历史开当前菜单。
        // Device semantics: return to the latest origin, restoring menu state; without history open the current menu.
        if (s_prev_page >= 0) {
            if (current && current->on_exit) current->on_exit(&ctx);
            current = app_at(s_prev_page);
            s_prev_page = -1;
            if (current->on_enter) current->on_enter(&ctx);
            ctx.leaf = s_prev_ctx_leaf;
            menu_open = s_prev_menu; menu_leaf = s_prev_leaf;
        } else { menu_open = true; menu_leaf = ui_menu_leaf_for_app(current); }
        display_request_navigation_settle();
        if (menu_open) draw_menu(); else present(APP_REDRAW_PAGE);
    }
    else if (next) choose_page(app_index_of(next));
    else if (menu) {
        menu_open = true; menu_leaf = ui_menu_leaf_for_app(current);
        display_request_navigation_settle(); draw_menu();
    }
}

static void tap(int x, int y) {
    if (x < 0 || x >= 684 || y < 0 || y >= 1216) return;
    if (asset >= 0) { asset = -1; menu_open = true; display_request_navigation_settle(); draw_menu(); return; }
    if (ui_menu_handle_hit_test((uint16_t)x, (uint16_t)y)) {
        display_request_navigation_settle();
        if (menu_open && current) { menu_open = false; present(APP_REDRAW_PAGE); }
        else { menu_open = true; menu_leaf = ui_menu_leaf_for_app(current); draw_menu(); }
        return;
    }
    if (menu_open) {
        int hit = ui_menu_hit_test((uint16_t)x, (uint16_t)y, menu_leaf);
        if (hit == UI_MENU_HIT_PREV) { menu_leaf--; display_request_navigation_settle(); }
        else if (hit == UI_MENU_HIT_NEXT) { menu_leaf++; display_request_navigation_settle(); }
        else if (hit >= 0) {
            choose_page_entry(hit, true); return;
        }
        unsupported = -1; draw_menu(); return;
    }
    cst836u_touch_t touch = {.touched = true, .count = 1, .x = x, .y = y};
    ctx.now_ms = preview_now_ms();
    ctx.touch = &touch; ctx.pressed = true; ctx.released = false;
    if (current->on_touch) present(current->on_touch(&ctx, &touch));
    if (current->on_gesture) {
        ui_gesture_event_t event = {.type = UI_GESTURE_PRESS, .x0 = x, .y0 = y, .x = x, .y = y};
        present(current->on_gesture(&ctx, &event));
        event.type = UI_GESTURE_TAP;
        present(current->on_gesture(&ctx, &event));
    }
    touch.touched = false; touch.count = 0; ctx.pressed = false; ctx.released = true;
    if (current->on_tick) present(current->on_tick(&ctx));
    ctx.touch = NULL; ctx.released = false;
    requests();
}

static void key(int key) {
    if (key == UI_KEY_3 && (menu_open || !current || !current->owns_keys)) { tap(612, 1150); return; }
    if (asset >= 0) return;
    if (menu_open) {
        if (key == UI_KEY_2) { ui_draw_menu_page(fb, current, menu_leaf); update_display_full(ctx.hl); return; }
        int next = menu_leaf + (key == UI_KEY_1 ? -1 : 1);
        if (next >= 0 && next < ui_menu_leaf_count()) {
            menu_leaf = next; display_request_navigation_settle(); draw_menu();
        }
    } else if (current) {
        if (key == UI_KEY_2 && !current->owns_keys) present(APP_REDRAW_FULL);
        else if (current->on_key) present(current->on_key(&ctx, key));
        requests();
    }
}

static bool load_asset(int which) {
    const char* path = which == 0 ? "main/assets/loading_4bpp.pack" : "main/assets/lock_4bpp.pack";
    FILE* f = fopen(path, "rb");
    if (!f) return false;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return false; }
    long length = ftell(f);
    if (length <= 0 || length > 3 * 1024 * 1024 || fseek(f, 0, SEEK_SET) != 0) { fclose(f); return false; }
    uint8_t* image = malloc((size_t)length);
    if (!image) { fclose(f); return false; }
    bool ok = fread(image, 1, (size_t)length, f) == (size_t)length && fgetc(f) == EOF;
    fclose(f);
    if (ok) ok = ui_draw_packed_full_image(fb, image, (size_t)length);
    if (ok) { asset = which; unsupported = -1; menu_open = false; }
    free(image); return ok;
}

static bool export_frame(const char* path) {
    FILE* f = fopen(path, "wb");
    if (!f) return false;
    fprintf(f, "P5\n684 1216\n255\n");
    uint8_t row[684];
    for (int y = 0; y < 1216; y++) {
        for (int x = 0; x < 684; x++) {
            int physical_x = y, physical_y = 683 - x;
            uint8_t packed = fb[physical_y * 608 + physical_x / 2];
            row[x] = (physical_x % 2 ? packed >> 4 : packed & 15) * 17;
        }
        if (fwrite(row, 1, sizeof(row), f) != sizeof(row)) { fclose(f); return false; }
    }
    return fclose(f) == 0;
}

int main(int argc, char** argv) {
    if (argc != 2) { fprintf(stderr, "Usage: preview FRAME.pgm\n"); return 1; }
    epd_set_rotation(EPD_ROT_INVERTED_PORTRAIT);
    if (ttf_font_init() != ESP_OK) { fprintf(stderr, "Cannot load built-in font\n"); return 1; }
    choose_page(app_index_of(app_home_page()));
    ctx.now_ms = preview_now_ms();
    present(current->on_tick(&ctx));
    char command[128];
    do {
        if (!export_frame(argv[1])) { perror("frame export"); return 1; }
        static book_quote_t quote_snapshot[BOOK_QUOTES_MAX];
        size_t quote_count = book_quotes_list(quote_snapshot, BOOK_QUOTES_MAX);
        printf("{\"page\":%d,\"menu\":%s,\"menu_leaf\":%d,\"leaf\":%d,\"asset\":%d,\"unsupported\":%d,\"refresh_mode\":%d,\"presents\":%d,\"gc_presents\":%d,\"refresh_wave\":%d,\"night_turns\":%u,\"body_presents\":%u,\"quiet_presents\":%u,\"night_area_presents\":%u,\"reading\":%s,\"sync_starts\":%u,\"sync_job\":%d,\"quote_count\":%u,\"history_page\":%u,\"history_count\":%u}\n",
               app_index_of(current), menu_open ? "true" : "false", menu_leaf, ctx.leaf, asset, unsupported, refresh_mode, presents, gc_presents, refresh_wave, s_night_body_turns, body_presents, quiet_presents, night_area_presents,
               current == app_by_id(OS_APP_LIBRARY) && book_chapter_count() > 0 ? "true" : "false",
               preview_sync_starts(), preview_sync_job(), (unsigned)quote_count, book_home_snapshot()->recent_page, book_home_snapshot()->history_count);
        fflush(stdout);
        if (!fgets(command, sizeof(command), stdin)) break;
        int value, x, y, x1, y1;
        if (sscanf(command, "time_step %d", &value) == 1) {
            preview_time_step(value);
            ctx.now_ms = preview_now_ms();
            if (current && current->on_tick) present(current->on_tick(&ctx));
            continue;
        }
        unsupported = -1;
        if (sscanf(command, "page %d", &value) == 1) choose_page(value);
        else if (sscanf(command, "sd %d", &value) == 1) s_sd_fixture = value == 1;
        else if (sscanf(command, "tap %d %d", &x, &y) == 2) tap(x, y);
        else if (sscanf(command, "clock %d", &x) == 1) {
            os_time_apply((uint32_t)x, 32);
            preview_draw_lock_clock(fb);
        }
        else if (sscanf(command, "glyph %d", &x) == 1) {
            ui_clear_page(fb);
            ui_text(fb, 40, 300, x, "3", EPD_DRAW_ALIGN_LEFT, false);
        }
        else if (sscanf(command, "key %d", &value) == 1 && value >= 0 && value < 3) key(value);
        else if (sscanf(command, "asset %d", &value) == 1 && (value == 0 || value == 1)) {
            if (!load_asset(value)) { fprintf(stderr, "Asset unavailable\n"); return 1; }
        } else if (strncmp(command, "menu", 4) == 0) {
            if (!menu_open || asset >= 0) display_request_navigation_settle();
            asset = -1; menu_open = true; menu_leaf = ui_menu_leaf_for_app(current); draw_menu();
        }
        else if (sscanf(command, "hold %d %d", &x, &y) == 2) {
            // 合成长按：按下→稳定帧→推进 600ms→抬起，走真实识别器的 LONG_PRESS。
            // Synthesize a long press: press, settled sample, advance 600 ms, release
            // through the real recognizer (PRESS now fires on the second sample).
            if (!menu_open && asset < 0 && current && current->on_gesture) {
                ui_gesture_reset(&s_swipe_gesture);
                cst836u_touch_t touch = {.touched = true, .count = 1, .x = (uint16_t)x, .y = (uint16_t)y};
                ui_gesture_event_t event;
                ctx.now_ms = preview_now_ms();
                ctx.touch = &touch; ctx.pressed = true; ctx.released = false;
                if (ui_gesture_feed(&s_swipe_gesture, &ctx, &event))
                    present(current->on_gesture(&ctx, &event));
                ctx.now_ms += 1;
                s_time_bias_ms += 1;
                ctx.pressed = false;
                if (ui_gesture_feed(&s_swipe_gesture, &ctx, &event))
                    present(current->on_gesture(&ctx, &event));
                ctx.now_ms += 600;
                s_time_bias_ms += 600;
                if (ui_gesture_feed(&s_swipe_gesture, &ctx, &event))
                    present(current->on_gesture(&ctx, &event));
                touch.touched = false; touch.count = 0;
                ctx.pressed = false; ctx.released = true;
                if (ui_gesture_feed(&s_swipe_gesture, &ctx, &event))
                    present(current->on_gesture(&ctx, &event));
                ctx.touch = NULL; ctx.released = false;
                requests();
            }
        }
        else if (sscanf(command, "fixture %d", &value) == 1 && value >= 0 && value <= 3) {
            nvs_flash_erase();
            preview_home_fixture(value);
            // 夹具重置主动重入首页；普通同页导航仍按设备语义不重入。
            // Fixture resets explicitly reenter Home; ordinary same-page navigation retains device semantics.
            if (current && current->on_exit) current->on_exit(&ctx);
            current = NULL;
            choose_page(app_index_of(app_home_page()));
            ctx.now_ms = preview_now_ms();
            present(current->on_tick(&ctx));
        }
        else if (sscanf(command, "swipe %d %d %d %d", &x, &y, &x1, &y1) == 4) {
            // 用真实识别器合成一次滑动：按下→移动→抬起，事件走 on_gesture。
            // Synthesize a swipe through the real recognizer: press, move, release into on_gesture.

            ui_gesture_reset(&s_swipe_gesture);
            cst836u_touch_t touch = {.touched = true, .count = 1};
            ctx.now_ms = preview_now_ms();
            ui_gesture_event_t event;
            touch.x = (uint16_t)x; touch.y = (uint16_t)y;
            ctx.touch = &touch; ctx.pressed = true; ctx.released = false;
            if (ui_gesture_feed(&s_swipe_gesture, &ctx, &event) && current->on_gesture)
                present(current->on_gesture(&ctx, &event));
            // 稳定帧：PRESS 在第二帧重锚后再开始移动。/ Settled sample: PRESS re-anchors here before motion.
            ctx.now_ms += 1; s_time_bias_ms += 1; ctx.pressed = false;
            if (ui_gesture_feed(&s_swipe_gesture, &ctx, &event) && current->on_gesture)
                present(current->on_gesture(&ctx, &event));
            for (int step = 1; step <= 4; ++step) {
                ctx.now_ms += 40;
                touch.x = (uint16_t)(x + (x1 - x) * step / 4);
                touch.y = (uint16_t)(y + (y1 - y) * step / 4);
                if (ui_gesture_feed(&s_swipe_gesture, &ctx, &event) && current->on_gesture)
                    present(current->on_gesture(&ctx, &event));
            }
            ctx.now_ms += 40;
            touch.touched = false; touch.count = 0;
            ctx.pressed = false; ctx.released = true;
            if (ui_gesture_feed(&s_swipe_gesture, &ctx, &event) && current->on_gesture)
                present(current->on_gesture(&ctx, &event));
            ctx.touch = NULL; ctx.released = false;
            requests();
        }
        else if (strncmp(command, "tick", 4) == 0 && !menu_open && asset < 0 && current && current->on_tick) {
            s_time_bias_ms += 600;
            ctx.now_ms = preview_now_ms();
            present(current->on_tick(&ctx));
            requests();
        }
    } while (true);
    if (current && current->on_exit) current->on_exit(&ctx);
    ttf_font_unload();
    return 0;
}
