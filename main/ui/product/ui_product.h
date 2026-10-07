/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 * 中文：产品页的纯绘图、共同触区与显式导航请求；不访问设备 IO。
 * English: Pure product drawing, shared hit geometry and explicit navigation requests; no device IO.
 * 冻结：导航请求只能在输入回调调用，render 仅调用绘图接口。
 * Frozen: Navigation requests run only in input callbacks; render calls drawing APIs only.
 */
#pragma once
#include "ui_kit.h"
#include "ui_gesture.h"
#include "book_cover.h"
#include "os_catalog.h"
#include "app.h"

#define UI_PRODUCT_SHELF_ROWS 3

/// 最多绘制指定行数，超长文本 UTF-8 边界省略；用于文件名回退。/ Draw bounded lines, ellipsizing at UTF-8 boundaries; for filename fallbacks.
void ui_product_title(uint8_t* fb, EpdRect rect, const char* title, int px, int max_lines);
/// 首页底部书架与导入；菜单把手独立保留。/ Home library/import footer; retain the independent menu handle.
void ui_product_home_bar(uint8_t* fb, bool transfer_enabled);
/// 返回稳定 ID，禁用控件不产生动作。/ Return a stable ID; disabled controls produce no action.
os_app_id_t ui_product_home_bar_hit(uint16_t x, uint16_t y, bool transfer_enabled);
/// 统一产品页标题；不读取硬件或文件。/ Shared product header without hardware or file reads.
void ui_product_header(uint8_t* fb, const char* title, const char* detail);
/// 子页返回触区与绘制共用。/ Shared subpage back geometry and drawing.
EpdRect ui_product_back_rect(void);
/// 绘制子页返回按钮。/ Draw a subpage back button.
void ui_product_back(uint8_t* fb, const char* label);
/// 四根入口导航及命中；把手触区独立保留。/ Four-root navigation and hit testing; retain a separate handle target.
void ui_product_root_bar(uint8_t* fb, os_app_id_t active);
os_app_id_t ui_product_root_hit(uint16_t x, uint16_t y);
/// 请求产品根页；书架为明确进入请求，不触发默认续读。/ Request a root page; Library uses an explicit shelf entry without default resume.
bool ui_product_navigate(app_ctx_t* ctx, os_app_id_t id);
/// 按下即导航底栏 tab（PRESS 事件传入）；命中并已发起切换返回 true。
/// Navigate a bottom-bar tab on press (pass PRESS events); true when a target was hit.
bool ui_product_root_press(app_ctx_t* ctx, const ui_gesture_event_t* ev);
/// 文件名排版封面，不假称 EPUB 真实封面。/ Typographic filename cover, never advertised as an extracted EPUB cover.
void ui_product_cover(uint8_t* fb, EpdRect rect, const char* title, int px);
/// 三行书封列表与翻页管理行的共享绘图/触区。/ Shared three-row cover list and paging/management geometry.
EpdRect ui_product_shelf_rect(int row);
EpdRect ui_product_shelf_nav_rect(int index);
/// cover 为 146×188 灰度位图（BOOK_COVER_W/H），NULL 回退文件名排版。
/// / cover is a 146×188 grayscale bitmap (BOOK_COVER_W/H); NULL falls back to the typographic cover.
void ui_product_shelf_card(uint8_t* fb, EpdRect rect, const char* title, const char* meta,
                           unsigned percent, bool has_progress, bool pressed, const uint8_t* cover);
/// 阅读工具与字号面板共用六项触区；夜间模式在更多设置。/ Tools and size panels share six targets; night mode lives in grouped settings.
EpdRect ui_product_tool_rect(int index);
void ui_product_reader_tools(uint8_t* fb, const char* title, int px, bool night, int pressed);
/// 字号子面板，复用工具触区。/ Size subpanel sharing toolbar hit geometry.
void ui_product_reader_sizes(uint8_t* fb, const char* title, int px, int pressed);
/// 正文与缺字告知/页脚共用几何；status 为左侧可选时钟/电量，bar 控制百分比文字。
/// Shared body/font notice/footer geometry; status is the optional clock/battery cluster and bar toggles percentage text.
EpdRect ui_product_reader_body(bool missing_glyphs);
void ui_product_reader_chrome(uint8_t* fb, const char* title, unsigned page, unsigned pages,
                             unsigned percent, bool missing_glyphs, const char* status, bool bar);
/// 锁屏密码键盘绘制/命中；digits 为已输位数，back 额外画返回按钮，命中 0..9 数字、10 清空、11 退格。
/// Lock-PIN keypad draw/hit; digits are entered dots, back adds a return button, hits return 0..9 digits, 10 clear, 11 backspace.
void ui_product_lock_keypad(uint8_t* fb, const char* title, const char* message, unsigned digits, bool back);
int ui_product_lock_keypad_hit(uint16_t x, uint16_t y);
/// 单键几何与重绘；pressed 为按下灰底态，key 取值与命中一致。
/// Single-key geometry and repaint; pressed paints the gray bed, key matches the hit codes.
EpdRect ui_product_lock_key_rect(int key);
void ui_product_lock_key(uint8_t* fb, int key, bool pressed);
/// 只画圆点、键位与提示，不清屏不画标题；页面自带头部时使用。
/// Dots, keys and hint only, without clearing or the title; for pages with their own header.
void ui_product_lock_keypad_body(uint8_t* fb, const char* message, unsigned digits);

/// 缩放内存封面，保留灰阶和比例；不读取文件。/ Scale an in-memory cover preserving grayscale and aspect; never read files.
void ui_product_cover_bitmap(uint8_t* fb, EpdRect rect, const uint8_t* gray);
