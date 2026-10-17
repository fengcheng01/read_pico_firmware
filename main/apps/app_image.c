/*
 * SPDX-FileCopyrightText: 2026 mindreset
 *
 * 中文：锁屏图片页：分页浏览 TF 卡 pictures/images 目录的 JPG/PNG，整页 GC16
 * 查看，可设为锁屏壁纸或清除；样式与路径写入 NVS，锁屏绘制失败回退默认静态图。
 * 查看页提供上一张/下一张按钮（滑动仍可用），无需返回列表重选。
 * English: Lock-image page: paged browsing of TF pictures/images JPG/PNG with a
 * full GC16 viewer; set as lock wallpaper or clear it. Style and path land in NVS
 * while lock drawing falls back to the default static image on failure. The viewer
 * adds previous/next buttons (swipes still work) so browsing needs no list detour.
 *
 * 冻结修订：文件沿解码器50MiB防护；render纯绘制，唯一后台任务准备至多三张预览，
 * 退出/媒体丢失/睡眠前汇合。列表扫描≤96项，失败不阻塞页面。
 * Frozen revision: Retain the decoder50MiB guard and pure render; one background decoder prepares at most three previews,
 * joining on exit/media loss/sleep. Scan at most96 list entries and report errors without blocking.
 */
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>

#include "app.h"
#include "app_registry.h"
#include "display.h"
#include "esp_heap_caps.h"
#include "read_pico_sd.h"
#include "settings.h"
#include "ui_gesture.h"
#include "ui_image.h"
#include "ui_image_cache.h"
#include "book_cover.h"
#include "ui_kit.h"
#include "ui_menu.h"
#include "ui_product.h"
#include "ui_press.h"

#define IMAGE_MAX 96
#define IMAGE_ROWS 6

typedef struct {
    char name[128];
    char path[192];
} image_item_t;
// 图片列表放 PSRAM：与文件列表同理，内部 RAM 留给只能用内部 RAM 的东西。
// The image list lives in PSRAM like the file lists: internal RAM stays for what requires it.
static image_item_t* s_items;
static int s_count, s_selected;
static bool s_viewing, s_loading;
static uint8_t* s_gray;
static uint16_t s_width, s_height;
static char s_message[96];
static ui_press_t s_press;
static bool pressed_id(int id) { return ui_press_active(&s_press) && s_press.id == id; }

static const char* const k_image_dirs[] = {UI_IMAGE_MEDIA_ROOT "/pictures", UI_IMAGE_MEDIA_ROOT "/images"};

static EpdRect item_rect(int row) { return (EpdRect){UI_MARGIN, 230 + row * 100, ui_content_width(), 92}; }
static EpdRect nav_rect(int i) { return ui_row_rect(i, 2, 852, 76); }
static int pages(void) { return (s_count + IMAGE_ROWS - 1) / IMAGE_ROWS; }

static bool image_name(const char* name) {
    if (!name[0] || name[0] == '.') return false; // 跳过 macOS 辅助文件。/ Skip macOS sidecar files.
    const char* ext = strrchr(name, '.');
    if (!ext) return false;
    return !strcasecmp(ext, ".png") || !strcasecmp(ext, ".jpg") || !strcasecmp(ext, ".jpeg");
}

static void scan_images(void) {
    if (!s_items) s_items = heap_caps_calloc(IMAGE_MAX, sizeof(image_item_t), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    s_count = s_selected = 0;
    s_viewing = false;
    if (!s_items) { snprintf(s_message, sizeof(s_message), "%s", "图片列表内存不足，请返回后重试"); return; }
    for (unsigned d = 0; d < sizeof(k_image_dirs) / sizeof(k_image_dirs[0]); ++d) {
        DIR* dir = opendir(k_image_dirs[d]);
        if (!dir) continue;
        struct dirent* ent;
        while (s_count < IMAGE_MAX && (ent = readdir(dir))) {
            if (!image_name(ent->d_name) || strlen(ent->d_name) >= sizeof(s_items[0].name)) continue;
            char path[256];
            if (snprintf(path, sizeof(path), "%s/%s", k_image_dirs[d], ent->d_name) >= (int)sizeof(path)) continue;
            struct stat st;
            if (stat(path, &st) || !S_ISREG(st.st_mode)) continue;
            bool duplicate = false;
            for (int i = 0; i < s_count; ++i) if (!strcmp(s_items[i].path, path)) { duplicate = true; break; }
            if (duplicate) continue;
            strcpy(s_items[s_count].name, ent->d_name);
            strcpy(s_items[s_count].path, path);
            ++s_count;
        }
        closedir(dir);
    }
    if (!s_count) {
        read_pico_sd_info_t sd = {0};
        read_pico_sd_get_info(&sd);
        snprintf(s_message, sizeof(s_message), "%s", sd.mounted
            ? "TF 卡 pictures / images 目录没有图片"
            : "未检测到 TF 卡");
    }
}

// 查看页用 GC16 整屏显示照片；离开后的第一帧列表必须同样 GC16，GL16 保白清不掉
// 照片灰阶，会留残影。一次性标记在 present 里消费。
// The viewer shows photos with a full GC16 pass; the first list frame after leaving
// must settle the same way — GL16 keeps whites and cannot clear photo grays, which
// is exactly the ghosting seen on return. present() consumes this one-shot flag.
static bool s_viewed_gc;
static void free_view(void) {
    if (s_viewing) s_viewed_gc = true;
    s_gray = NULL;
    s_viewing = s_loading = false;
}

static bool load_image(int index) {
    free_view();
    if (!s_items || index < 0 || index >= s_count) return false;
    s_selected = index;
    ui_image_cache_retry(s_items[index].path);
    uint16_t width = 0, height = 0;
    if (!ui_image_cache_poll(s_items[index].path, &s_gray, &width, &height)) {
        s_viewing = s_loading = true; s_message[0] = 0; return true;
    }
    if (!s_gray) { snprintf(s_message, sizeof(s_message), "%s", "图片超过 50 MB 或解码失败"); return false; }
    s_width = width; s_height = height;
    s_viewing = true; s_message[0] = 0; return true;
}

// 查看页上一张/下一张行：主三键上方一整行两枚宽键，首尾张对应键置灰。
// The viewer's previous/next row: two wide keys above the three main bars,
// greyed out at the first and last image.
#define VIEW_TURN_TOP (UI_BAR_TOP - UI_BAR_H - 12)
static EpdRect turn_rect(int i) {
    EpdRect r = ui_bar_rect(i, 2);
    r.y = VIEW_TURN_TOP;
    return r;
}
static void render(app_ctx_t* ctx, uint8_t* fb) {
    ui_clear_page(fb);
    if (s_viewing) {
        // 查看区避开两行按钮：整幅居中裁切。
        // The view area clears both button rows: full cover-crop.
        if (s_loading) {
            ui_product_header(fb, "图片查看", s_items[s_selected].name);
            ui_text_vc(fb, UI_LOCK_WIDTH / 2, 500, 36, "正在加载图片…", EPD_DRAW_ALIGN_CENTER, false);
        } else ui_image_blit(fb, s_gray, s_width, s_height, (EpdRect){0, 0, UI_LOCK_WIDTH, VIEW_TURN_TOP - 8});
        if (pressed_id(0)) ui_draw_pressed_round_rect(fb, turn_rect(0), UI_BTN_RADIUS);
        ui_draw_button(fb, turn_rect(0), "上一张", s_selected > 0);
        if (pressed_id(1)) ui_draw_pressed_round_rect(fb, turn_rect(1), UI_BTN_RADIUS);
        ui_draw_button(fb, turn_rect(1), "下一张", s_selected + 1 < s_count);
        const char* names[3] = {"设为锁屏", "清除壁纸", "返回列表"};
        for (int i = 0; i < 3; ++i) {
            if (pressed_id(2 + i)) ui_draw_pressed_round_rect(fb, ui_bar_rect(i, 3), UI_BTN_RADIUS);
            ui_draw_button(fb, ui_bar_rect(i, 3), names[i], false);
        }
        return;
    }
    char detail[96];
    snprintf(detail, sizeof(detail), "%d 张 · JPG / PNG · 第 %d/%d 页", s_count, ctx->leaf + 1, pages() ? pages() : 1);
    ui_product_header(fb, "锁屏图片", detail);
    if (pressed_id(100)) ui_draw_pressed_round_rect(fb, ui_product_back_rect(), UI_BTN_RADIUS);
    ui_product_back(fb, "返回");
    for (int row = 0; row < IMAGE_ROWS; ++row) {
        int index = ctx->leaf * IMAGE_ROWS + row;
        if (index >= s_count) break;
        bool active = !strcmp(s_items[index].path, app_settings_lock_image());
        EpdRect r = item_rect(row);
        if (pressed_id(10 + row)) ui_draw_pressed_round_rect(fb, r, UI_BTN_RADIUS);
        ui_draw_choice_round_rect(fb, r, UI_BTN_RADIUS, active);
        ui_product_title(fb, (EpdRect){r.x + 16, r.y + 10, r.width - 150, 54}, s_items[index].name, 36, 1);
        ui_text(fb, r.x + 16, r.y + 58, 24, "TF 卡图片", EPD_DRAW_ALIGN_LEFT, false);
        if (active) ui_text_vc(fb, r.x + r.width - 16, r.y + 46, 28, "锁屏", EPD_DRAW_ALIGN_RIGHT, false);
    }
    if (pressed_id(0)) ui_draw_pressed_round_rect(fb, nav_rect(0), UI_BTN_RADIUS);
    ui_draw_button(fb, nav_rect(0), "上一页", ctx->leaf > 0);
    if (pressed_id(1)) ui_draw_pressed_round_rect(fb, nav_rect(1), UI_BTN_RADIUS);
    ui_draw_button(fb, nav_rect(1), "下一页", ctx->leaf + 1 < pages());
    ui_product_title(fb, (EpdRect){UI_MARGIN, 962, ui_content_width(), 42},
                     s_message[0] ? s_message : "选定图片后可设为锁屏壁纸；文件不超过 50 MB", 26, 1);
    if (pressed_id(101)) ui_draw_pressed_round_rect(fb, ui_bar_rect(0, 1), UI_BTN_RADIUS);
    ui_draw_button(fb, ui_bar_rect(0, 1), "返回", false);
    ui_draw_menu_handle(fb, false);
}

static bool present(app_ctx_t* ctx, app_redraw_t redraw) {
    if (s_viewed_gc && (redraw == APP_REDRAW_PAGE || redraw == APP_REDRAW_FULL)) {
        s_viewed_gc = false;
        render(ctx, ctx->fb);
        guard_draw_result(ctx->hl, update_display_full(ctx->hl));
        return true;
    }
    if (!s_viewing || s_loading) return false;
    // 查看页只接管整页呈现；DU 按下反馈等局部推送保持原样。
    // The viewer intercepts whole-page presentations only; local DU pushes such as press feedback stay as-is.
    if (redraw != APP_REDRAW_PAGE && redraw != APP_REDRAW_FULL) return true;
    render(ctx, ctx->fb);
    // 查看整图走 GC16：照片灰阶与抖动需要全灰度迁移。
    // Viewing uses GC16: photo grays and dithering need the full-grayscale migration.
    guard_draw_result(ctx->hl, update_display_full(ctx->hl));
    return true;
}

static void on_enter(app_ctx_t* ctx) {
    // 进页清空上一轮消息，扫描结果重新决定提示。/ Clear last session's message; the fresh scan decides the hint.
    ui_image_cache_clear();
    book_cover_join();
    s_viewed_gc = false;
    s_message[0] = 0;
    scan_images();
    ctx->leaf = 0;
    ui_press_reset(&s_press);
    const char* wallpaper = app_settings_lock_image();
    for (int i = 0; i < s_count; ++i) if (!strcmp(s_items[i].path, wallpaper)) {
        s_selected = i;
        ctx->leaf = i / IMAGE_ROWS;
        break;
    }
}

static void image_on_exit(app_ctx_t* ctx) { (void)ctx; free_view(); ui_image_cache_clear(); }

static void on_media_lost(app_ctx_t* ctx) {
    (void)ctx;
    free_view();
    ui_image_cache_clear();
    scan_images();
}

static app_redraw_t move_image(int delta) {
    int next = s_selected + delta;
    if (next < 0 || next >= s_count) return APP_REDRAW_NONE;
    if (!load_image(next)) { free_view(); return APP_REDRAW_PAGE; }
    return s_loading ? APP_REDRAW_PAGE : APP_REDRAW_FULL;
}

static app_redraw_t on_key(app_ctx_t* ctx, int key) {
    if (key == UI_KEY_1) {
        if (s_viewing) { free_view(); return APP_REDRAW_PAGE; }
        // 返回回进入来源（通常为睡眠设置），不再固定跳睡眠页。/ Back returns to the entry origin instead of a fixed sleep-page jump.
        ctx->request_return = true;
    }
    return APP_REDRAW_NONE;
}

// 命中当前画出的控件；查看页与列表页各用一段编号。/ Hit drawn controls; viewer and list keep separate id ranges.
static int control_at(app_ctx_t* ctx, uint16_t x, uint16_t y, EpdRect* rect) {
    if (s_viewing) {
        if (s_selected > 0 && ui_rect_hit(turn_rect(0), x, y)) { if (rect) *rect = turn_rect(0); return 0; }
        if (s_selected + 1 < s_count && ui_rect_hit(turn_rect(1), x, y)) { if (rect) *rect = turn_rect(1); return 1; }
        for (int i = 0; i < 3; ++i) if ((!s_loading || i != 0) && ui_rect_hit(ui_bar_rect(i, 3), x, y)) {
            if (rect) *rect = ui_bar_rect(i, 3);
            return 2 + i;
        }
        return -1;
    }
    if (ui_rect_hit(ui_product_back_rect(), x, y)) { if (rect) *rect = ui_product_back_rect(); return 100; }
    if (ctx->leaf > 0 && ui_rect_hit(nav_rect(0), x, y)) { if (rect) *rect = nav_rect(0); return 0; }
    if (ctx->leaf + 1 < pages() && ui_rect_hit(nav_rect(1), x, y)) { if (rect) *rect = nav_rect(1); return 1; }
    if (ui_rect_hit(ui_bar_rect(0, 1), x, y)) { if (rect) *rect = ui_bar_rect(0, 1); return 101; }
    for (int row = 0; row < IMAGE_ROWS; ++row) {
        int index = ctx->leaf * IMAGE_ROWS + row;
        if (index >= s_count) break;
        if (ui_rect_hit(item_rect(row), x, y)) { if (rect) *rect = item_rect(row); return 10 + row; }
    }
    return -1;
}

static app_redraw_t on_gesture(app_ctx_t* ctx, const ui_gesture_event_t* ev) {
    EpdRect rect = {0};
    int id = control_at(ctx, ev->x0, ev->y0, &rect);
    if (ev->type == UI_GESTURE_PRESS)
        return id >= 0 ? ui_press_begin(ctx, &s_press, id, rect, render) : APP_REDRAW_NONE;
    if (ev->type == UI_GESTURE_CANCEL) return ui_press_restore(ctx, &s_press, render);
    if (ev->type == UI_GESTURE_SWIPE_L || ev->type == UI_GESTURE_SWIPE_R) {
        app_redraw_t restore = ui_press_restore(ctx, &s_press, render);
        if (s_viewing) return ev->type == UI_GESTURE_SWIPE_L ? move_image(1) : move_image(-1);
        if (ev->type == UI_GESTURE_SWIPE_L && (ctx->leaf + 1) * IMAGE_ROWS < s_count) { ++ctx->leaf; return APP_REDRAW_PAGE; }
        if (ev->type == UI_GESTURE_SWIPE_R && ctx->leaf > 0) { --ctx->leaf; return APP_REDRAW_PAGE; }
        return restore;
    }
    if (ev->type != UI_GESTURE_TAP) return APP_REDRAW_NONE;
    int held = s_press.id;
    EpdRect held_rect = s_press.rect;
    ui_press_forget(&s_press);
    app_redraw_t result = APP_REDRAW_NONE;
    if (id >= 0 && id == control_at(ctx, ev->x, ev->y, NULL)) {
        if (s_viewing) {
            if (id < 2) result = move_image(id ? 1 : -1);
            else if (id == 2) {
                app_settings_set_lock_image(s_items[s_selected].path);
                app_settings_set_lock_style(0);
                // 把查看端已解码的灰度存为侧车：锁屏直读快照，免去阅读中锁屏时的解码内存峰值。
                // Persist the viewer's decoded gray as the sidecar: the lock reads the
                // snapshot and skips the decode memory peak while a book is open.
                struct stat st;
                if (s_gray && !stat(s_items[s_selected].path, &st))
                    (void)ui_image_wallpaper_cache_store(UI_IMAGE_WALLPAPER_CACHE, s_items[s_selected].path,
                                                         st.st_size, st.st_mtime, s_gray, s_width, s_height);
                snprintf(s_message, sizeof(s_message), "%s", "已设为锁屏壁纸");
                free_view();
                result = APP_REDRAW_PAGE;
            } else if (id == 3) {
                // 清壁纸只清路径；图片样式保留，锁屏回退内置图。
                // Clearing removes only the path; the image style stays with the built-in fallback.
                app_settings_set_lock_image("");
                snprintf(s_message, sizeof(s_message), "%s", "已恢复默认锁屏");
                free_view();
                result = APP_REDRAW_PAGE;
            } else {
                free_view();
                result = APP_REDRAW_PAGE;
            }
        } else if (id == 100 || id == 101) {
            // 两处“返回”都回进入来源；查看器内返回仅退回列表。
            // Both back keys return to the entry origin; the viewer's back only exits the view.
            ctx->request_return = true;
            return APP_REDRAW_NONE;
        } else if (id < 2) {
            ctx->leaf += id ? 1 : -1;
            result = APP_REDRAW_PAGE;
        } else {
            int index = ctx->leaf * IMAGE_ROWS + (id - 10);
            if (!load_image(index)) { free_view(); result = APP_REDRAW_PAGE; }
            else result = s_loading ? APP_REDRAW_PAGE : APP_REDRAW_FULL;
        }
    } else if (s_viewing && !ui_press_active(&s_press)) {
        // 查看页边缘两段换图；中段不再误触退出，返回列表只走按钮。
        // Viewer edge thirds turn pages; the middle no longer exits by accident — leaving goes through the button.
        if (ev->x0 < UI_LOCK_WIDTH / 3) result = move_image(-1);
        else if (ev->x0 > UI_LOCK_WIDTH * 2 / 3) result = move_image(1);
    }
    if (result == APP_REDRAW_NONE && held >= 0 && !ctx->request_app && !ctx->request_menu && !ctx->request_return)
        return ui_press_rewind(ctx, &s_press, held_rect, render);
    return result;
}

static app_redraw_t on_tick(app_ctx_t* ctx) {
    ui_press_settle(ctx, &s_press, ctx->now_ms);
    if (!s_viewing || ctx->consumed || ui_press_active(&s_press)) return APP_REDRAW_NONE;
    if (s_loading) {
        if (!ui_image_cache_poll(s_items[s_selected].path, &s_gray, &s_width, &s_height)) return APP_REDRAW_NONE;
        s_loading = false;
        if (!s_gray) {
            free_view(); snprintf(s_message, sizeof(s_message), "%s", "图片超过 50 MB 或解码失败");
            return APP_REDRAW_PAGE;
        }
        return APP_REDRAW_FULL;
    }
    if (s_selected + 1 < s_count) ui_image_cache_prefetch(s_items[s_selected + 1].path);
    if (s_selected > 0) ui_image_cache_prefetch(s_items[s_selected - 1].path);
    return APP_REDRAW_NONE;
}

const app_desc_t app_image = {
    .title = "锁屏图片 Images", .detail = "TF 图片查看 · 设为锁屏", .enter_full = false,
    .on_enter = on_enter, .on_exit = image_on_exit, .on_media_lost = on_media_lost,
    .render = render, .present = present, .on_key = on_key, .on_gesture = on_gesture, .on_tick = on_tick,
};
