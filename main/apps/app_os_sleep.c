/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 *
 * 中文：设置下的睡眠子页。选择锁屏后的电源模式（浅睡/深睡/关机）、浅睡
 * 拿起唤醒、锁屏样式（时钟/日历/黄历/图片/个人摘录）与锁屏密码入口；图片样式
 * 进入锁屏图片页选择 TF 壁纸，全部写入既有 NVS 设置，原演示睡眠页在诊断目录。
 * English: Sleep subpage under Settings. Picks the post-lock power mode
 * (light/deep/off), light-sleep pickup wake, the lock style (clock/calendar/
 * almanac/image/personal excerpt) and the lock-PIN entry; the image style opens the
 * lock-image page to pick a TF wallpaper. Everything writes the existing NVS
 * settings while the original demo page stays under diagnostics.
 *
 * 摘录修订：用户选择固定摘录副本；按实机要求浅睡每分钟更新时钟，显式关机仍为快照，数据只在回调读取。
 * Excerpt revision: Keep the selected snapshot; hardware feedback enables a minute clock in light sleep, while explicit Off stays static and callbacks alone read data.
 * 第三批修订：电源、唤醒与样式预览分区；动态缩略示意用真实时间，图片回调装载前汇合封面任务，内存失败回退说明，拿起只在实际浅睡可点。
 * Batch-three revision: Separate power/wake from style previews; dynamic sketches use real time, callbacks join the cover worker before loading images, allocation failure gets a fallback note, and pickup activates only for effective light sleep.
 * 冻结：本页只改设置，不直接执行睡眠或关机；普通样式立即保存，摘录预览确认后保存；render 只绘图；
 * 时钟/日历锁屏在未校时时自动回退静态图，不画假时间；壁纸样式的选择发生在图片页。
 * Frozen: This page only changes settings and never sleeps or powers off
 * directly; ordinary styles save at once and excerpts after confirmation; render only paints; clock/calendar faces
 * fall back to the static image while uncalibrated and never fake a time;
 * wallpaper selection happens on the image page.
 */
#include "app.h"
#include "app_registry.h"
#include "os_catalog.h"
#include "settings.h"
#include "ui_gesture.h"
#include "ui_menu.h"
#include "ui_product.h"
#include "ui_press.h"
#include "ui_sheet.h"
#include "ui_image.h"
#include "asset_pack.h"
#include "book_cover.h"
#include "book_quotes.h"
#include "book_stats.h"
#include "esp_heap_caps.h"
#include "os_time.h"
#include "os_lunar.h"
#include "ttf_font.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

static EpdRect back_rect(void) { return ui_product_back_rect(); }
static EpdRect mode_rect(int i) { return ui_row_rect(i, 3, 230, 80); }
static EpdRect pickup_rect(void) { return (EpdRect){UI_MARGIN, 420, ui_content_width(), 72}; }
static EpdRect style_rect(int i) { return ui_row_rect(i, 4, 576, 252); }
static EpdRect quote_style_rect(void) { return (EpdRect){UI_MARGIN, 842, ui_content_width(), 74}; }
static EpdRect pin_rect(void) { return (EpdRect){UI_MARGIN, 956, ui_content_width(), 76}; }
static EpdRect idle_rect(void) { return (EpdRect){UI_MARGIN, 1050, ui_content_width(), 46}; }
// 图片样式的“去挑选”链接：标题行右端。/ The “去挑选” link for the image style: right end of the style header.
static EpdRect pick_link_rect(void) {
    return app_settings_lock_style() == 0 ? (EpdRect){ui_content_right() - 170, 520, 170, 48} : (EpdRect){0, 0, 0, 0};
}

static const struct {
    const char* title;
    const char* detail;
    app_sleep_mode_t mode;
} modes[] = {
    {"浅睡", "保留当前页面；按键或拿起唤醒；按锁屏样式维护时钟", APP_SLEEP_LIGHT},
    {"深睡", "主控断电省电；短按电源键重新开机", APP_SLEEP_DEEP},
    {"关机", "锁屏即关机；长按电源键开机", APP_SLEEP_OFF},
};
static const char* style_names[] = {"时钟", "日历", "黄历"};
// 芯片顺序对应样式 1/2/3，第四枚是图片：选中样式0并进入图片页。
// Chips map to styles 1/2/3, with the fourth for image: select style 0 and open the picker.
static const uint8_t style_values[4] = {1, 2, 3, 0};
static ui_press_t s_press;
static bool pressed_id(int id) { return ui_press_active(&s_press) && s_press.id == id; }
// 空闲锁屏的选择面板：-1 关闭。/ The idle-lock choice sheet; -1 closed.
static int s_idle_sheet = -1;
static const char* const k_idle_labels[] = {"关", "5 分钟", "10 分钟", "30 分钟"};
static const uint8_t k_idle_steps[] = {0, 5, 10, 30};
static int idle_current(void) {
    uint8_t idle = app_settings_idle_lock_min();
    for (int i = 0; i < 4; ++i) if (k_idle_steps[i] == idle) return i;
    return 0;
}

#define PREVIEW_W 100
#define PREVIEW_H 178
extern const uint8_t loading_4bpp_pack_start[] asm("_binary_loading_4bpp_pack_start");
extern const uint8_t loading_4bpp_pack_end[] asm("_binary_loading_4bpp_pack_end");
static uint8_t *s_static_preview, *s_image_preview;
static const char* s_image_note;
static uint32_t s_preview_minute;
static bool s_preview_valid;
static book_quote_t s_selected_quote;
static bool s_has_quote;
static unsigned s_quote_revision;

/* ---- 摘录选择 / Excerpt selection ---- */
// 摘录选择只缓存最近16条；候选与固定副本独立，列表变化不偷换预览对象。
// Cache at most sixteen choices; independent candidate/pinned copies keep list changes from replacing a preview.
static book_quote_t* s_quote_choices;
static book_quote_t s_quote_candidate;
static size_t s_quote_count;
static int s_quote_view, s_quote_page;
static char s_quote_error[96];
static os_time_info_t s_quote_time;
static unsigned s_quote_minutes, s_quote_days;
static EpdRect quote_choice_rect(int row) { return (EpdRect){UI_MARGIN,244+row*196,ui_content_width(),180}; }
static EpdRect quote_current_rect(void) { return (EpdRect){470,188,174,48}; }
static EpdRect quote_preview_button(int index) { return ui_row_rect(index,2,24,80); }
static void quote_info_refresh(void) {
    s_quote_time = *os_time_info();
    uint32_t date = s_quote_time.state==OS_TIME_VALID ? s_quote_time.year*10000u+s_quote_time.month*100u+s_quote_time.day : 0;
    s_quote_minutes = date ? book_stats_minutes(date) : 0; s_quote_days = 0;
    if (date) for (int i=0;i<30;++i) if (book_stats_minutes(os_time_date_shift(date,-i))) ++s_quote_days;
}
static void quote_source(const book_quote_t* q, char out[BOOK_STORE_PATH_MAX]) {
    const char* base = strrchr(q->path,'/');
    snprintf(out,BOOK_STORE_PATH_MAX,"%s",base?base+1:q->path);
    char* ext = strrchr(out,'.');if(ext&&ext!=out)*ext=0;
    // 有章节名快照时附加，旧记录保持仅书名。/ Append the saved chapter title when present; legacy records stay book-only.
    if(q->chapter_title[0]) snprintf(out+strlen(out),BOOK_STORE_PATH_MAX-strlen(out),"· %s",q->chapter_title);
}
static bool quote_matches(const book_quote_t* q) {
    return s_has_quote && q->chapter==s_selected_quote.chapter && q->byte_off==s_selected_quote.byte_off &&
        !strcmp(q->path,s_selected_quote.path) && !strcmp(q->text,s_selected_quote.text);
}
static app_redraw_t quote_picker_open(void) {
    if (!s_quote_choices) s_quote_choices=heap_caps_malloc(sizeof(book_quote_t)*BOOK_QUOTES_MAX,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
    if (!s_quote_choices) { snprintf(s_quote_error,sizeof(s_quote_error),"列表内存不足，请重试");return APP_REDRAW_PAGE; }
    s_quote_count=book_quotes_list(s_quote_choices,BOOK_QUOTES_MAX);
    s_quote_page=0;s_quote_view=1;s_quote_error[0]=0;quote_info_refresh();
    return APP_REDRAW_PAGE;
}
static void draw_quote_picker(uint8_t* fb) {
    ui_clear_page(fb);ui_product_header(fb,"锁屏摘录","先选择一句预览，再确认设为锁屏");
    if (ui_press_active(&s_press) && s_press.id == 100) ui_draw_pressed_round_rect(fb, back_rect(), UI_BTN_RADIUS);
    ui_draw_button(fb,back_rect(),"返回睡眠",false);
    char count[64];snprintf(count,sizeof(count),"共 %u 条 · 每页 4 条",(unsigned)s_quote_count);
    ui_text(fb,UI_MARGIN,198,26,count,EPD_DRAW_ALIGN_LEFT,false);
    if (s_has_quote) ui_draw_button(fb,quote_current_rect(),"已选摘录",false);
    for (int row=0;row<4;++row) {
        size_t at=(size_t)s_quote_page*4+row;if(at>=s_quote_count)break;
        const book_quote_t* q=&s_quote_choices[at];EpdRect r=quote_choice_rect(row);
        if(pressed_id(40+row))ui_draw_pressed_round_rect(fb,r,UI_BTN_RADIUS);
        if(quote_matches(q))ui_draw_selected_round_rect(fb,r,UI_BTN_RADIUS);else ui_draw_round_rect(fb,r,UI_BTN_RADIUS,UI_GRAY_BLACK);
        ui_product_title(fb,(EpdRect){r.x+18,r.y+16,r.width-36,108},q->text,28,3);
        char source[BOOK_STORE_PATH_MAX];quote_source(q,source);
        ui_product_title(fb,(EpdRect){r.x+18,r.y+138,r.width-132,32},source,22,1);
        if(quote_matches(q))ui_text(fb,r.x+r.width-18,r.y+138,22,"已固定",EPD_DRAW_ALIGN_RIGHT,false);
    }
    if(!s_quote_count)ui_product_title(fb,(EpdRect){UI_MARGIN,350,ui_content_width(),200},
        s_has_quote ? "最近摘录列表为空。已固定的摘录仍可从上方「已选摘录」预览。" : "还没有保存摘录。阅读时长按句子，确认保存后再来选择。",32,4);
    unsigned pages=(unsigned)(s_quote_count+3)/4;if(!pages)pages=1;
    char page[96];snprintf(page,sizeof(page),"%d / %u · 新保存的句子不会顶掉锁屏选择",s_quote_page+1,pages);
    ui_product_title(fb,(EpdRect){UI_MARGIN,1038,ui_content_width(),36},page,22,1);
    if(s_quote_page>0)ui_draw_button(fb,ui_bar_rect(0,3),"上一页",false);
    ui_draw_button(fb,ui_bar_rect(1,3),"返回",false);
    if((size_t)(s_quote_page+1)*4<s_quote_count)ui_draw_button(fb,ui_bar_rect(2,3),"下一页",false);
    ui_draw_menu_handle(fb,false);
}
static void draw_quote_preview(uint8_t* fb) {
    ui_product_excerpt_lock(fb,s_quote_candidate.text,s_quote_candidate.path,s_quote_candidate.chapter_title,app_settings_lock_wake_hint(),
                           &s_quote_time,s_quote_minutes,s_quote_days,app_settings_effective_lock_mode() != APP_SLEEP_OFF,true);
    ui_draw_button(fb,quote_preview_button(0),"返回列表",false);
    ui_draw_button(fb,quote_preview_button(1),s_quote_error[0]?"重试保存":"设为锁屏",true);
    if(s_quote_error[0]) {
        ui_clear_rect_fast(fb,(EpdRect){UI_MARGIN,462,ui_content_width(),28});
        ui_product_title(fb,(EpdRect){UI_MARGIN,462,ui_content_width(),28},s_quote_error,20,1);
    }
}
static app_redraw_t quote_picker_action(int id) {
    if(id==100||id==31){s_quote_view=0;s_quote_error[0]=0;return APP_REDRAW_PAGE;}
    if(id==30&&s_quote_page>0){--s_quote_page;return APP_REDRAW_PAGE;}
    if(id==32&&(size_t)(s_quote_page+1)*4<s_quote_count){++s_quote_page;return APP_REDRAW_PAGE;}
    if((id>=40&&id<44)||(id==34&&s_has_quote)){
        size_t at=(size_t)s_quote_page*4+(id-40);
        if(id!=34&&at>=s_quote_count)return APP_REDRAW_NONE;
        s_quote_candidate=id==34?s_selected_quote:s_quote_choices[at];
        s_quote_view=2;s_quote_error[0]=0;quote_info_refresh();return APP_REDRAW_PAGE;
    }
    if(id==35){s_quote_view=1;s_quote_error[0]=0;return APP_REDRAW_PAGE;}
    if(id==36){
        if(book_quotes_lock_set(&s_quote_candidate)!=ESP_OK){snprintf(s_quote_error,sizeof(s_quote_error),"摘录未保存，请重试或返回");return APP_REDRAW_PAGE;}
        s_selected_quote=s_quote_candidate;s_has_quote=true;
        if(!app_settings_set_lock_style(APP_LOCK_STYLE_QUOTE)){snprintf(s_quote_error,sizeof(s_quote_error),"摘录已保存，样式未启用；请重试");return APP_REDRAW_PAGE;}
        s_quote_view=0;s_quote_error[0]=0;return APP_REDRAW_PAGE;
    }
    return APP_REDRAW_NONE;
}

static void free_previews(void) {
    free(s_static_preview); free(s_image_preview);
    s_static_preview = s_image_preview = NULL;
}
// 一次性压到小缩略图并释放大图；render只借用小图，不触TF或解压器。
// Reduce to one small thumbnail and release the full image; render borrows small images without TF access or inflation.
static void prepare_previews(void) {
    free_previews();
    s_image_note = app_settings_lock_image()[0] ? "图片样式暂不可预览，读取失败时使用内置图" : "图片样式未选壁纸，将使用内置图";
    size_t bytes = (size_t)UI_LOCK_WIDTH * UI_LOCK_HEIGHT / 2;
    size_t packed_size = (size_t)(loading_4bpp_pack_end - loading_4bpp_pack_start);
    uint8_t* packed_gray = heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (packed_gray && asset_pack_size(loading_4bpp_pack_start, packed_size) == bytes &&
        asset_pack_unpack(loading_4bpp_pack_start, packed_size, packed_gray, bytes)) {
        s_static_preview = heap_caps_malloc(PREVIEW_W * PREVIEW_H, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (s_static_preview) for (int y = 0; y < PREVIEW_H; ++y) for (int x = 0; x < PREVIEW_W; ++x) {
            int sx = x * UI_LOCK_WIDTH / PREVIEW_W, sy = y * UI_LOCK_HEIGHT / PREVIEW_H;
            uint8_t pixel = packed_gray[(size_t)sy * UI_LOCK_WIDTH / 2 + sx / 2];
            s_static_preview[y * PREVIEW_W + x] = (sx & 1 ? pixel >> 4 : pixel & 15) * 17;
        }
    }
    free(packed_gray);
    const char* path = app_settings_lock_image();
    struct stat st; uint8_t* gray = NULL; uint16_t width = 0, height = 0;
    if (!path[0] || stat(path, &st) || st.st_size <= 0) return;
    if (!ui_image_wallpaper_cache_load(UI_IMAGE_WALLPAPER_CACHE, path, st.st_size, st.st_mtime, &gray, &width, &height) &&
        !ui_image_load_path(path, &gray, &width, &height)) return;
    if (gray && width && height) {
        s_image_preview = heap_caps_malloc(PREVIEW_W * PREVIEW_H, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (s_image_preview) {
            int crop_w = width, crop_h = height;
            if ((int)width * PREVIEW_H > (int)height * PREVIEW_W) crop_w = ((int)height * PREVIEW_W + PREVIEW_H - 1) / PREVIEW_H;
            else crop_h = ((int)width * PREVIEW_H + PREVIEW_W - 1) / PREVIEW_W;
            if (crop_w < 1) crop_w = 1;
            if (crop_h < 1) crop_h = 1;
            for (int y = 0; y < PREVIEW_H; ++y) for (int x = 0; x < PREVIEW_W; ++x) {
                int sx = (width - crop_w) / 2 + x * crop_w / PREVIEW_W;
                int sy = (height - crop_h) / 2 + y * crop_h / PREVIEW_H;
                s_image_preview[y * PREVIEW_W + x] = gray[(size_t)sy * width + sx];
            }
            s_image_note = "图片样式显示所选壁纸，实际锁屏铺满屏幕";
        } else s_image_note = "预览内存不足，实际锁屏仍读取所选图片";
    }
    free(gray);
}
static void draw_style_preview(uint8_t* fb, int index) {
    EpdRect card = style_rect(index);
    EpdRect r = {card.x + (card.width - PREVIEW_W) / 2, card.y + 16, PREVIEW_W, PREVIEW_H};
    uint8_t style = style_values[index];
    const os_time_info_t* info = os_time_info();
    bool valid = info->state == OS_TIME_VALID;
    if (!style || !valid) {
        const uint8_t* gray = !style && s_image_preview ? s_image_preview : s_static_preview;
        if (gray == s_static_preview && gray) {
            for (int y = 0; y < PREVIEW_H; ++y) for (int x = 0; x < PREVIEW_W; ++x)
                epd_draw_pixel(r.x + x, r.y + y, gray[y * PREVIEW_W + x], fb);
        } else if (gray) ui_image_blit(fb, gray, PREVIEW_W, PREVIEW_H, r);
        else ui_product_title(fb, (EpdRect){r.x + 4, r.y + 40, r.width - 8, 90}, "图片预览不可用", 20, 3);
        return;
    }
    char clock[16], date[32]; os_time_format_clock(clock, sizeof(clock));
    snprintf(date, sizeof(date), "%u/%u/%u", info->year, info->month, info->day);
    if (style == 1) {
        int px = 28; while (px > 14 && ttf_text_width_px(px, clock) > r.width - 8) --px;
        ui_text(fb, r.x + r.width / 2, r.y + 58, px, clock, EPD_DRAW_ALIGN_CENTER, false);
        ui_text(fb, r.x + r.width / 2, r.y + 108, 12, date, EPD_DRAW_ALIGN_CENTER, false);
    } else if (style == 2) {
        snprintf(date, sizeof(date), "%u / %u", info->year, info->month);
        ui_text(fb, r.x + r.width / 2, r.y + 10, 16, date, EPD_DRAW_ALIGN_CENTER, false);
        static const char* names[] = {"一", "二", "三", "四", "五", "六", "日"};
        int first = ((int)info->weekday + 6 - (int)(info->day - 1) % 7 + 7) % 7;
        int days = info->month == 2 ? (info->year % 4 == 0 && (info->year % 100 != 0 || info->year % 400 == 0) ? 29 : 28) :
            info->month == 4 || info->month == 6 || info->month == 9 || info->month == 11 ? 30 : 31;
        for (int i = 0; i < 7; ++i) ui_text(fb, r.x + i * 14 + 7, r.y + 40, 10, names[i], EPD_DRAW_ALIGN_CENTER, false);
        for (int day = 1; day <= days; ++day) {
            int slot = first + day - 1, x = r.x + slot % 7 * 14, y = r.y + 62 + slot / 7 * 16;
            char label[4]; snprintf(label, sizeof(label), "%d", day);
            if ((unsigned)day == info->day) epd_draw_rect((EpdRect){x, y - 2, 14, 16}, UI_GRAY_BLACK, fb);
            ui_text(fb, x + 7, y, 10, label, EPD_DRAW_ALIGN_CENTER, false);
        }
        ui_text(fb, r.x + r.width / 2, r.y + 160, 12, clock, EPD_DRAW_ALIGN_CENTER, false);
    } else {
        os_lunar_date_t lunar;
        ui_text(fb, r.x + r.width / 2, r.y + 10, 14, "小纸 Pico", EPD_DRAW_ALIGN_CENTER, false);
        ui_hairline(fb, r.y + 34, r.x + 4, r.width - 8, UI_GRAY_LIGHT);
        if (os_lunar_from_solar(info->year, info->month, info->day, &lunar)) {
            ui_text(fb, r.x + r.width / 2, r.y + 50, 14, "农历", EPD_DRAW_ALIGN_CENTER, false);
            ui_text(fb, r.x + r.width / 2, r.y + 80, 14, os_lunar_day_name(lunar.day), EPD_DRAW_ALIGN_CENTER, false);
        }
        ui_text(fb, r.x + r.width / 2, r.y + 130, 14, "宜读书", EPD_DRAW_ALIGN_CENTER, false);
        ui_text(fb, r.x + r.width / 2, r.y + 158, 14, clock, EPD_DRAW_ALIGN_CENTER, false);
    }
}
static void render(app_ctx_t* ctx, uint8_t* fb) {
    (void)ctx;
    if(s_quote_view==1){draw_quote_picker(fb);return;}
    if(s_quote_view==2){draw_quote_preview(fb);return;}
    ui_clear_page(fb);
    ui_product_header(fb, "睡眠", "电源 · 唤醒 · 锁屏样式");
    if (pressed_id(100)) ui_draw_pressed_round_rect(fb, back_rect(), UI_BTN_RADIUS);
    ui_draw_button(fb, back_rect(), "返回设置", false);
    ui_text(fb, UI_MARGIN, 184, 28, "锁屏后的电源模式", EPD_DRAW_ALIGN_LEFT, false);
    app_sleep_mode_t current = app_settings_sleep_mode(), effective = app_settings_effective_lock_mode();
    for (int i = 0; i < 3; ++i) {
        EpdRect r = mode_rect(i);
        if (pressed_id(i)) ui_draw_pressed_round_rect(fb, r, UI_BTN_RADIUS);
        if (modes[i].mode == current) ui_draw_selected_round_rect(fb, r, UI_BTN_RADIUS);
        else ui_draw_round_rect(fb, r, UI_BTN_RADIUS, UI_GRAY_BLACK);
        ui_text_vc(fb, r.x + r.width / 2, r.y + r.height / 2, 32, modes[i].title, EPD_DRAW_ALIGN_CENTER, false);
    }
    char wake[160];
    snprintf(wake, sizeof(wake), "%s%s%s", current != effective ? "此动态样式保留浅睡；" : "",
             app_settings_lock_wake_hint(), effective == APP_SLEEP_LIGHT ?
                 (app_settings_lock_style() == APP_LOCK_STYLE_QUOTE ? "；每分钟更新时钟" : "；30分钟无操作转深睡") : "，开机恢复来源页面");
    ui_product_title(fb, (EpdRect){UI_MARGIN, 332, ui_content_width(), 76}, wake, 26, 2);
    bool light = effective == APP_SLEEP_LIGHT;
    if (light && pressed_id(3)) ui_draw_pressed_round_rect(fb, pickup_rect(), UI_BTN_RADIUS);
    ui_draw_round_rect(fb, pickup_rect(), UI_BTN_RADIUS, light ? UI_GRAY_BLACK : UI_GRAY_LIGHT);
    ui_text_vc(fb, UI_MARGIN + 18, 456, 28, light ? "拿起唤醒" : "拿起唤醒 · 仅浅睡可用", EPD_DRAW_ALIGN_LEFT, false);
    if (light) ui_draw_choice_round_rect(fb, (EpdRect){ui_content_right() - 68, 430, 52, 52}, 8, app_settings_pickup_wake());
    ui_hairline(fb, 508, UI_MARGIN, ui_content_width(), UI_GRAY_LIGHT);
    ui_text(fb, UI_MARGIN, 532, 28, "锁屏样式 · 缩略示意", EPD_DRAW_ALIGN_LEFT, false);
    if (!app_settings_lock_style()) ui_text_vc(fb, ui_content_right(), 548, 26, "去挑选 ›", EPD_DRAW_ALIGN_RIGHT, false);
    for (int i = 0; i < 4; ++i) {
        EpdRect r = style_rect(i);
        if (pressed_id(4 + i)) ui_draw_pressed_round_rect(fb, r, UI_BTN_RADIUS);
        if (app_settings_lock_style() == style_values[i]) ui_draw_selected_round_rect(fb, r, UI_BTN_RADIUS);
        else ui_draw_round_rect(fb, r, UI_BTN_RADIUS, UI_GRAY_BLACK);
        draw_style_preview(fb, i);
        ui_text_vc(fb, r.x + r.width / 2, r.y + 222, 24, i == 3 ? "图片" : style_names[i], EPD_DRAW_ALIGN_CENTER, false);
    }
    EpdRect quote = quote_style_rect();
    if (pressed_id(12)) ui_draw_pressed_round_rect(fb, quote, UI_BTN_RADIUS);
    if (app_settings_lock_style() == APP_LOCK_STYLE_QUOTE) ui_draw_selected_round_rect(fb, quote, UI_BTN_RADIUS);
    else ui_draw_round_rect(fb, quote, UI_BTN_RADIUS, UI_GRAY_BLACK);
    ui_text(fb, quote.x + 14, quote.y + 8, 28, "个人摘录", EPD_DRAW_ALIGN_LEFT, false);
    ui_text(fb,quote.x+quote.width-14,quote.y+10,24,"选择 ›",EPD_DRAW_ALIGN_RIGHT,false);
    ui_product_title(fb, (EpdRect){quote.x + 14, quote.y + 44, quote.width - 28, 28},
                     s_has_quote ? s_selected_quote.text : "点击选择已保存的摘录，预览后确认", 20, 1);
    const char* note = app_settings_lock_style() == APP_LOCK_STYLE_QUOTE
        ? (s_has_quote ? "已固定所选摘录；浅睡每分钟更新时钟，关机显示关机前时刻。" : "尚未选择摘录，实际锁屏使用内置图片。")
        : os_time_info()->state != OS_TIME_VALID ? "时间未校时：时钟、日历与黄历回退内置图片。" : s_image_note;
    ui_product_title(fb, (EpdRect){UI_MARGIN, 922, ui_content_width(), 28}, s_quote_error[0]?s_quote_error:note, 20, 1);
    char pin[8]; bool armed = app_settings_lock_pin(pin, sizeof(pin));
    if (pressed_id(9)) ui_draw_pressed_round_rect(fb, pin_rect(), UI_BTN_RADIUS);
    ui_text(fb, UI_MARGIN, 962, 28, "锁屏密码", EPD_DRAW_ALIGN_LEFT, false);
    ui_product_title(fb, (EpdRect){UI_MARGIN, 1004, ui_content_width() - 140, 32},
                     armed ? (app_settings_lock_pin_wake() ? "开机与解锁验证；可修改、关闭或清除" : "仅开机验证；唤醒免输，可修改或清除") : "设置后开机需输入密码", 22, 1);
    ui_text_vc(fb, ui_content_right(), 970, 28, armed ? "已设置 ›" : "未设置 ›", EPD_DRAW_ALIGN_RIGHT, false);
    ui_hairline(fb, 1046, UI_MARGIN, ui_content_width(), UI_GRAY_LIGHT);
    uint8_t idle = app_settings_idle_lock_min();
    if (pressed_id(8)) ui_draw_pressed_round_rect(fb, idle_rect(), UI_CHIP_RADIUS);
    ui_text_vc(fb, UI_MARGIN, 1072, 28, "空闲锁屏", EPD_DRAW_ALIGN_LEFT, false);
    ui_text_vc(fb, ui_content_right(), 1072, 26, idle == 0 ? "关" : idle == 5 ? "5 分钟" : idle == 10 ? "10 分钟" : "30 分钟", EPD_DRAW_ALIGN_RIGHT, false);
    if (s_idle_sheet >= 0) ui_draw_choice_sheet(fb, "空闲锁屏", k_idle_labels, 4, idle_current(),
                             pressed_id(20) ? 0 : pressed_id(21) ? 1 : pressed_id(22) ? 2 : pressed_id(23) ? 3 : -1);
    ui_draw_menu_handle(fb, false);
}

static int control_at(uint16_t x, uint16_t y, EpdRect* rect) {
    if(s_quote_view){
        if(s_quote_view==2){
            for(int i=0;i<2;++i)if(ui_rect_hit(quote_preview_button(i),x,y)){if(rect)*rect=quote_preview_button(i);return 35+i;}
            return -1;
        }
        if(ui_rect_hit(back_rect(),x,y)){if(rect)*rect=back_rect();return 100;}
        if(s_has_quote&&ui_rect_hit(quote_current_rect(),x,y)){if(rect)*rect=quote_current_rect();return 34;}
        for(int row=0;row<4;++row)if((size_t)s_quote_page*4+row<s_quote_count&&ui_rect_hit(quote_choice_rect(row),x,y)){if(rect)*rect=quote_choice_rect(row);return 40+row;}
        int hit=ui_bar_hit(x,y,3);
        if(hit>=0&&(hit==1||(hit==0&&s_quote_page>0)||(hit==2&&(size_t)(s_quote_page+1)*4<s_quote_count))){if(rect)*rect=ui_bar_rect(hit,3);return 30+hit;}
        return -1;
    }
    if (s_idle_sheet >= 0) {
        int hit = ui_choice_sheet_hit(x, y, 4);
        if (hit >= 0) { if (rect) *rect = ui_sheet_choice_chip(hit, 4); return 20 + hit; }
        if (hit == UI_SHEET_HIT_OUTSIDE) { if (rect) *rect = ui_sheet_choice_rect(4); return 24; }
        return -1;
    }
    if (ui_rect_hit(back_rect(), x, y)) { if (rect) *rect = back_rect(); return 100; }
    for (unsigned i = 0; i < sizeof(modes) / sizeof(modes[0]); ++i)
        if (ui_rect_hit(mode_rect(i), x, y)) { if (rect) *rect = mode_rect(i); return (int)i; }
    if (app_settings_effective_lock_mode() == APP_SLEEP_LIGHT && ui_rect_hit(pickup_rect(), x, y)) { if (rect) *rect = pickup_rect(); return 3; }
    for (int i = 0; i < 4; ++i)
        if (ui_rect_hit(style_rect(i), x, y)) { if (rect) *rect = style_rect(i); return 4 + i; }
    if (ui_rect_hit(quote_style_rect(), x, y)) { if (rect) *rect = quote_style_rect(); return 12; }
    if (ui_rect_hit(pick_link_rect(), x, y)) { if (rect) *rect = pick_link_rect(); return 11; }
    if (ui_rect_hit(pin_rect(), x, y)) { if (rect) *rect = pin_rect(); return 9; }
    if (ui_rect_hit(idle_rect(), x, y)) { if (rect) *rect = idle_rect(); return 8; }
    return -1;
}

static app_redraw_t gesture(app_ctx_t* ctx, const ui_gesture_event_t* ev) {
    EpdRect rect = {0};
    int id = control_at(ev->x0, ev->y0, &rect);
    if (ev->type == UI_GESTURE_PRESS)
        return id >= 0 ? ui_press_begin(ctx, &s_press, id, rect, render) : APP_REDRAW_NONE;
    if (ev->type != UI_GESTURE_TAP) return ui_press_restore(ctx, &s_press, render);
    int held = s_press.id;
    EpdRect held_rect = s_press.rect;
    ui_press_forget(&s_press);
    app_redraw_t result = APP_REDRAW_NONE;
    if(s_quote_view&&id>=0&&id==control_at(ev->x,ev->y,NULL)) return quote_picker_action(id);
    if (id >= 20 && id <= 24 && id == control_at(ev->x, ev->y, NULL)) {
        // 选择面板：选项即刻生效并收起，面板上方轻点只收起。
        // Choice sheet: an option applies at once and closes; a tap above the sheet only closes.
        if (id <= 23) app_settings_set_idle_lock_min(k_idle_steps[id - 20]);
        s_idle_sheet = -1;
        result = APP_REDRAW_PAGE;
    } else if (id >= 0 && id == control_at(ev->x, ev->y, NULL)) {
        if (id == 100) {
            ctx->request_app = app_by_id(OS_APP_SETTINGS);
            return APP_REDRAW_NONE;
        } else if (id < 3) {
            if (modes[id].mode != app_settings_sleep_mode()) {
                app_settings_set_sleep_mode(modes[id].mode);
                result = APP_REDRAW_PAGE;
            }
        } else if (id == 3) {
            app_settings_set_pickup_wake(!app_settings_pickup_wake());
            result = APP_REDRAW_PAGE;
        } else if (id < 8) {
            int i = id - 4;
            if (app_settings_lock_style() != style_values[i]) {
                app_settings_set_lock_style(style_values[i]);
                result = APP_REDRAW_PAGE;
            }
        } else if (id == 8) {
            s_idle_sheet = 0;
            result = APP_REDRAW_PAGE;
        } else if (id == 12) {
            return quote_picker_open();
        } else if (id == 11) {
            const app_desc_t* image_page = app_by_id(OS_APP_IMAGES);
            if (image_page) ctx->request_app = image_page;
            return APP_REDRAW_NONE;
        } else {
            const app_desc_t* pin_page = app_by_id(OS_APP_PIN);
            if (pin_page) ctx->request_app = pin_page;
            return APP_REDRAW_NONE;
        }
    }
    if (result == APP_REDRAW_NONE && held >= 0 && !ctx->request_app && !ctx->request_menu && !ctx->request_return)
        return ui_press_rewind(ctx, &s_press, held_rect, render);
    return result;
}

static void on_enter(app_ctx_t* ctx) {
    ui_press_reset(&s_press); s_idle_sheet = -1; s_quote_view=0;s_quote_error[0]=0;
    book_cover_join();
    os_time_poll(ctx->now_ms); prepare_previews();
    s_has_quote = book_quotes_lock_get(&s_selected_quote);
    s_quote_revision = book_quotes_revision(); quote_info_refresh();
    s_preview_valid = os_time_info()->state == OS_TIME_VALID;
    s_preview_minute = os_time_info()->unix_utc / 60;
}
static void sleep_settings_exit(app_ctx_t* ctx) { (void)ctx; free_previews();free(s_quote_choices);s_quote_choices=NULL; }
static void on_media_lost(app_ctx_t* ctx) {
    (void)ctx; free(s_image_preview); s_image_preview = NULL;
    s_image_note = "TF 卡已移除，实际锁屏回退内置图片";
}

static app_redraw_t on_tick(app_ctx_t* ctx) {
    ui_press_settle(ctx, &s_press, ctx->now_ms);
    if (!ctx->consumed) {
        if (s_quote_revision != book_quotes_revision()) {
            s_has_quote = book_quotes_lock_get(&s_selected_quote);
            s_quote_revision = book_quotes_revision();
            if(s_quote_view==1&&s_quote_choices){s_quote_count=book_quotes_list(s_quote_choices,BOOK_QUOTES_MAX);if((size_t)s_quote_page*4>=s_quote_count)s_quote_page=0;}
            return APP_REDRAW_PAGE;
        }
        os_time_poll(ctx->now_ms);
        bool valid = os_time_info()->state == OS_TIME_VALID;
        uint32_t minute = os_time_info()->unix_utc / 60;
        if (valid != s_preview_valid || (valid && minute != s_preview_minute)) {
            s_preview_valid = valid; s_preview_minute = minute; quote_info_refresh(); return APP_REDRAW_PAGE;
        }
    }
    return APP_REDRAW_NONE;
}

static app_redraw_t key(app_ctx_t* ctx, int key) {
    if(key==UI_KEY_1&&s_quote_view){s_quote_view=s_quote_view==2?1:0;s_quote_error[0]=0;return APP_REDRAW_PAGE;}
    if (key == UI_KEY_1) ctx->request_app = app_by_id(OS_APP_SETTINGS);
    return APP_REDRAW_NONE;
}

const app_desc_t app_os_sleep = {
    .title = "睡眠与锁屏", .detail = "模式 · 唤醒 · 样式 · 密码", .enter_full = false,
    .render = render, .on_gesture = gesture, .on_key = key, .on_enter = on_enter, .on_exit = sleep_settings_exit, .on_media_lost = on_media_lost, .on_tick = on_tick,
};
