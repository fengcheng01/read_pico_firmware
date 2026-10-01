/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 * 中文：单线程预览调度器，把原页面回调的 framebuffer 输出为竖屏灰度。
 * English: Single-thread preview dispatcher exporting real page callbacks as portrait grayscale.
 * 冻结：只运行已适配页面；所有硬件推屏为零耗时记录，不伪造传感器数值。
 * Frozen: Adapted pages only; presents record zero physical time and do not fabricate sensor values.
 */
#include "preview_host.h"
#include "app.h"
#include "app_registry.h"
#include "os_catalog.h"
#include "settings.h"
#include "ttf_font.h"
#include "ui_kit.h"
#include "ui_menu.h"
#include "ui_gesture.h"
#include "book_entry.h"
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
static bool menu_open = true, white_exit;
static int menu_leaf, unsupported = -1, asset = -1, refresh_mode, presents;
static char font_path[TTF_FONT_PATH_MAX];
void preview_home_fixture(int value);
void preview_fixture(int value);

const EpdWaveform E0470_WAVEFORM = {0}, E0470_FULL_WAVEFORM = {1}, E0470_GRAY8_WAVEFORM = {2};
const EpdWaveform E0470_FOLLOW_WAVEFORM = {3};

int64_t esp_timer_get_time(void) {
    struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t);
    return (int64_t)t.tv_sec * 1000000 + t.tv_nsec / 1000;
}
static int64_t s_time_bias_ms;
static int64_t preview_now_ms(void) { return esp_timer_get_time() / 1000 + s_time_bias_ms; }

const char* esp_err_to_name(esp_err_t err) { return err == ESP_OK ? "ESP_OK" : "HOST_ERROR"; }
esp_err_t read_pico_sd_get_info(read_pico_sd_info_t* out) { *out = (read_pico_sd_info_t){0}; return ESP_OK; }
uint8_t* epd_hl_get_framebuffer(EpdiyHighlevelState* state) { return state->front_fb; }
void epd_clear_area(EpdRect area) { (void)area; }
void guard_draw_result(EpdiyHighlevelState* state, enum EpdDrawError err) { (void)state; (void)err; }
void display_hold_white_exit(bool hold) { white_exit = hold; }
bool display_take_white_exit(void) { bool result = white_exit; white_exit = false; return result; }

static enum EpdDrawError record_refresh(enum EpdDrawMode mode) {
    refresh_mode = mode; presents++; return EPD_DRAW_SUCCESS;
}
enum EpdDrawError update_display_mode(EpdiyHighlevelState* state, enum EpdDrawMode mode) {
    (void)state; return record_refresh(mode);
}
enum EpdDrawError update_display_full(EpdiyHighlevelState* state) { (void)state; return record_refresh(MODE_GC16); }
enum EpdDrawError update_display_white(EpdiyHighlevelState* state) {
    memset(state->front_fb, 0xff, sizeof(fb)); return record_refresh(MODE_GC16);
}
enum EpdDrawError update_display_from_white(EpdiyHighlevelState* state) { return update_display_full(state); }
enum EpdDrawError update_display_from_white_with(EpdiyHighlevelState* state, const EpdWaveform* wave, enum EpdDrawMode mode) {
    (void)state; (void)wave; return record_refresh(mode);
}
enum EpdDrawError update_display_area_with(EpdiyHighlevelState* state, const EpdWaveform* wave, enum EpdDrawMode mode, EpdRect area) {
    (void)state; (void)wave; (void)area; return record_refresh(mode);
}

static void present(app_redraw_t redraw) {
    if (redraw == APP_REDRAW_NONE || redraw == APP_REDRAW_DONE) return;
    if (current->present && current->present(&ctx, redraw)) return;
    if (redraw != APP_REDRAW_AREA && current->render) current->render(&ctx, fb);
    record_refresh(redraw == APP_REDRAW_FULL ? MODE_GC16 : redraw == APP_REDRAW_AREA ? MODE_DU : MODE_GL16);
}

static void draw_menu(void) { ui_draw_menu_page(fb, current, menu_leaf); record_refresh(MODE_GC16); }
static void choose_page(int index) {
    const app_desc_t* next = app_at(index);
    if (!next) return;
    if (!next->render) {
        book_entry_request_t entry;
        if (book_entry_take(&entry)) book_entry_finish(BOOK_ENTRY_CANCELLED);
        unsupported = index; asset = -1; menu_open = true; draw_menu(); return;
    }
    if (current && current->on_exit) current->on_exit(&ctx);
    s_prev_page = current ? app_index_of(current) : -1;
    s_prev_menu = menu_open;
    s_prev_leaf = menu_leaf;
    current = next; ctx.leaf = 0; asset = -1; unsupported = -1; menu_open = false;
    menu_leaf = ui_menu_leaf_for_app(current);
    if (current->on_enter) current->on_enter(&ctx);
    present(APP_REDRAW_FULL);
}

static void requests(void) {
    const app_desc_t* next = ctx.request_app;
    bool menu = ctx.request_menu;
    bool back = ctx.request_return;
    ctx.request_app = NULL; ctx.request_menu = false; ctx.request_return = false;
    if (next) choose_page(app_index_of(next));
    else if (back) {
        // 与设备同语义：回到最近切页来源，菜单来源恢复菜单位置，无历史开当前菜单。
        // Device semantics: return to the latest origin, restoring menu state; without history open the current menu.
        if (s_prev_page >= 0) {
            choose_page(s_prev_page);
            if (s_prev_menu) { menu_open = true; menu_leaf = s_prev_leaf; draw_menu(); }
        } else { menu_open = true; draw_menu(); }
    }
    else if (menu) { menu_open = true; draw_menu(); }
}

static void tap(int x, int y) {
    if (x < 0 || x >= 684 || y < 0 || y >= 1216) return;
    if (asset >= 0) { asset = -1; menu_open = true; draw_menu(); return; }
    if (ui_menu_handle_hit_test((uint16_t)x, (uint16_t)y)) {
        if (menu_open && current) { menu_open = false; present(APP_REDRAW_FULL); }
        else { menu_open = true; draw_menu(); }
        return;
    }
    if (menu_open) {
        int hit = ui_menu_hit_test((uint16_t)x, (uint16_t)y, menu_leaf);
        if (hit == UI_MENU_HIT_PREV) menu_leaf--;
        else if (hit == UI_MENU_HIT_NEXT) menu_leaf++;
        else if (hit >= 0) { choose_page(hit); return; }
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
        if (key == UI_KEY_1 && menu_leaf > 0) menu_leaf--;
        draw_menu();
    } else if (current) {
        if (key == UI_KEY_2 && !current->owns_keys) present(APP_REDRAW_FULL);
        else if (current->on_key) present(current->on_key(&ctx, key));
        requests();
    }
}

static bool load_asset(int which) {
    const char* path = which == 0 ? "main/assets/loading_4bpp.bin" : "main/assets/lock_4bpp.bin";
    FILE* f = fopen(path, "rb");
    if (!f) return false;
    uint8_t* image = malloc(sizeof(fb));
    if (!image) { fclose(f); return false; }
    bool ok = fread(image, 1, sizeof(fb), f) == sizeof(fb) && fgetc(f) == EOF;
    fclose(f);
    if (ok) { ui_draw_full_image(fb, image); asset = which; unsupported = -1; menu_open = false; }
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
        printf("{\"page\":%d,\"menu\":%s,\"menu_leaf\":%d,\"leaf\":%d,\"asset\":%d,\"unsupported\":%d,\"refresh_mode\":%d,\"presents\":%d,\"reading\":%s}\n",
               app_index_of(current), menu_open ? "true" : "false", menu_leaf, ctx.leaf, asset, unsupported, refresh_mode, presents,
               current == app_by_id(OS_APP_LIBRARY) ? "true" : "false");
        fflush(stdout);
        if (!fgets(command, sizeof(command), stdin)) break;
        int value, x, y;
        unsupported = -1;
        if (sscanf(command, "page %d", &value) == 1) choose_page(value);
        else if (sscanf(command, "tap %d %d", &x, &y) == 2) tap(x, y);
        else if (sscanf(command, "key %d", &value) == 1 && value >= 0 && value < 3) key(value);
        else if (sscanf(command, "asset %d", &value) == 1 && (value == 0 || value == 1)) {
            if (!load_asset(value)) { fprintf(stderr, "Asset unavailable\n"); return 1; }
        } else if (strncmp(command, "menu", 4) == 0) { asset = -1; menu_open = true; draw_menu(); }
        else if (sscanf(command, "hold %d %d", &x, &y) == 2) {
            // 合成长按：按下→推进 600ms→抬起，走真实识别器的 LONG_PRESS。
            // Synthesize a long press: press, advance 600 ms, release through the real recognizer.
            if (!menu_open && asset < 0 && current && current->on_gesture) {
                ui_gesture_reset(&s_swipe_gesture);
                cst836u_touch_t touch = {.touched = true, .count = 1, .x = (uint16_t)x, .y = (uint16_t)y};
                ui_gesture_event_t event;
                ctx.now_ms = preview_now_ms();
                ctx.touch = &touch; ctx.pressed = true; ctx.released = false;
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
        else if (sscanf(command, "fixture %d", &value) == 1 && value >= 0 && value <= 2) {
            nvs_flash_erase();
            preview_home_fixture(value);
            choose_page(app_index_of(app_home_page()));
            ctx.now_ms = preview_now_ms();
            present(current->on_tick(&ctx));
        }
        else if (sscanf(command, "swipe %d %d %d %d", &x, &y, &value, &y) == 4) {
            // 用真实识别器合成一次滑动：按下→移动→抬起，事件走 on_gesture。
            // Synthesize a swipe through the real recognizer: press, move, release into on_gesture.
            int x1 = value, y1 = y;
            ui_gesture_reset(&s_swipe_gesture);
            cst836u_touch_t touch = {.touched = true, .count = 1};
            ctx.now_ms = preview_now_ms();
            ui_gesture_event_t event;
            touch.x = (uint16_t)x; touch.y = (uint16_t)y;
            ctx.touch = &touch; ctx.pressed = true; ctx.released = false;
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
