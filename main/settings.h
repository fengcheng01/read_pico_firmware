/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 *
 * NVS 里的用户设置：睡眠档、字体路径、上次唤醒/开机原因、拿起唤醒开关。
 *
 * User settings in NVS: sleep mode, font path, last wake/boot reason,
 * pickup-wake switch.
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/// 默认深睡。浅睡：按键回原页；拿起唤醒默认关。软睡：SOFT_SLEEP 拉低 EN，再短按开机。关机：EN=0，长按开机。
/// Default is deep. Light: key returns to the page; pickup-wake defaults off. Soft sleep: SOFT_SLEEP drops EN, then a short press boots. Off: EN=0, long-press to boot.
typedef enum {
    APP_SLEEP_LIGHT = 0,
    APP_SLEEP_DEEP = 1,
    APP_SLEEP_OFF = 2,
} app_sleep_mode_t;

void app_settings_init(void);
app_sleep_mode_t app_settings_sleep_mode(void);
void app_settings_set_sleep_mode(app_sleep_mode_t mode);
const char* app_sleep_mode_name(app_sleep_mode_t mode);
/// 空路径表示固件内建字体；非空为 SD 上的 TTF。/ Empty path is the built-in font; non-empty is a TTF on the SD card.
const char* app_settings_font_path(void);
void app_settings_set_font_path(const char* path);
/// 上次浅睡唤醒源（app_wake_source_t），掉电也保留。/ Last light-sleep wake source (app_wake_source_t); kept across power loss.
uint8_t app_settings_last_wake(void);
void app_settings_set_last_wake(uint8_t src);
/// 最近一次非 0 的 PMU wake_reason。STATUS 报 0 时用这个回显。/ Last non-zero PMU wake_reason. Used when STATUS reports 0.
uint8_t app_settings_last_boot(void);
void app_settings_set_last_boot(uint8_t reason);
/// 浅睡拿起唤醒。默认关；有加速度计也不会自动开。/ Light-sleep pickup wake. Defaults off; an accelerometer does not turn it on.
bool app_settings_pickup_wake(void);
void app_settings_set_pickup_wake(bool on);
/// 阅读默认字号，36..72、步长 4，默认 48。/ Default reading size, 36..72 in steps of 4, initially 48.
uint8_t app_settings_book_px(void);
/// 无效字号恢复 48。/ Invalid sizes fall back to 48.
void app_settings_set_book_px(uint8_t px);
/// 实验性晃动翻页，默认关闭。/ Experimental shake page turn, off by default.
bool app_settings_book_shake(void);
/// 保存实验性晃动翻页开关。/ Persist the experimental shake page-turn switch.
void app_settings_set_book_shake(bool on);
/// 本地时区偏移，四分之一小时为单位，默认 +32（UTC+8）。/ Local timezone offset in quarter-hours, default +32 (UTC+8).
int8_t app_settings_tz_qh(void);
/// 保存时区；越界值被拒绝。/ Persist the timezone; out-of-range values are rejected.
void app_settings_set_tz_qh(int8_t qh);
/// 正文行距加成 0/15/30%，默认 0。/ Body extra leading of 0/15/30 percent, initially 0.
uint8_t app_settings_book_leading(void);
void app_settings_set_book_leading(uint8_t percent);
/// 正文左右边距档 0/1/2，每档 16px，默认 0。/ Body side-margin tier 0/1/2, 16 px each, initially 0.
uint8_t app_settings_book_margin(void);
void app_settings_set_book_margin(uint8_t tier);
/// 行辅助线 0=关 1=实线 2=虚线，每行文字下方，默认关。/ Per-line guide rule 0=off 1=solid 2=dashed under each text line, initially off.
uint8_t app_settings_book_guide(void);
void app_settings_set_book_guide(uint8_t style);
/// 段首行缩进两字符，默认开。/ Two-em first-line indent, on by default.
bool app_settings_book_indent(void);
void app_settings_set_book_indent(bool on);
/// 段落间距档 0=标准 1=加大，默认标准。/ Paragraph gap tier 0=standard 1=relaxed, initially standard.
uint8_t app_settings_book_para(void);
void app_settings_set_book_para(uint8_t tier);
/// 自动翻页 0=关 1=20秒 2=40秒 3=90秒，默认关。/ Auto page turn 0=off 1=20 s 2=40 s 3=90 s, initially off.
uint8_t app_settings_book_auto(void);
void app_settings_set_book_auto(uint8_t tier);
/// 正文左右分区点击翻页，默认开。/ Body left/right tap-zone page turns, on by default.
bool app_settings_book_tap(void);
void app_settings_set_book_tap(bool on);
/// 阅读画面夜间模式：正文、边距、页脚及阅读覆盖层统一反色，默认关；目录/设置等其它页面不反色。
/// Reader night mode inverts body, margins, footer and reading overlays together, off by default; TOC/settings and other pages keep their palette.
bool app_settings_book_night(void);
void app_settings_set_book_night(bool on);
/// 锁屏样式 0=静态 1=时钟 2=日历 3=黄历，默认静态。/ Lock style 0=static 1=clock 2=calendar 3=almanac, initially static.
uint8_t app_settings_lock_style(void);
void app_settings_set_lock_style(uint8_t style);
/// 正文对齐 0=左 1=居中 2=两端对齐（段末行除外），默认左。/ Body alignment 0=left 1=center 2=justified (last line excepted), initially left.
uint8_t app_settings_book_align(void);
void app_settings_set_book_align(uint8_t align);
/// 阅读页脚状态栏：时钟/电量百分比/进度条，默认只开进度条。/ Reader footer status bar: clock/battery percent/progress bar, bar-only by default.
bool app_settings_footer_clock(void);
void app_settings_set_footer_clock(bool on);
bool app_settings_footer_battery(void);
void app_settings_set_footer_battery(bool on);
bool app_settings_footer_bar(void);
void app_settings_set_footer_bar(bool on);
/// 空闲自动锁屏分钟数，0=关，可选 0/5/10/30，默认关。/ Idle auto-lock minutes, 0=off with 0/5/10/30 choices, initially off.
uint8_t app_settings_idle_lock_min(void);
void app_settings_set_idle_lock_min(uint8_t minutes);
/// 阅读同步：服务器基址，默认 kosync 官方。/ Progress sync: server base URL, defaulting to the official kosync.
const char* app_settings_sync_url(void);
void app_settings_set_sync_url(const char* url);
/// 阅读同步用户名。/ Progress-sync username.
const char* app_settings_sync_user(void);
void app_settings_set_sync_user(const char* user);
/// 阅读同步密钥（密码 MD5 十六进制），不存明文。/ Progress-sync key (MD5 hex of the password), never plain text.
const char* app_settings_sync_key(void);
void app_settings_set_sync_key(const char* key);
/// 传书 STA 会话期间自动上传进度，默认关。/ Auto-push progress during STA transfer sessions, off by default.
bool app_settings_sync_auto(void);
void app_settings_set_sync_auto(bool on);
/// 读取锁屏密码到 out（4 位数字或空串）；返回是否设有密码。/ Copy the lock PIN (4 digits or empty) to out; true when armed.
bool app_settings_lock_pin(char* out, size_t cap);
/// 保存或清除（空串）锁屏密码；非法输入返回 false。/ Save or clear (empty) the lock PIN; invalid input returns false.
bool app_settings_set_lock_pin(const char* pin);
/// 解锁验证频率：true 每次解锁都验证，false 仅开机验证（浅睡唤醒免输）。默认 true。
/// Unlock challenge cadence: true verifies every unlock, false boot-only (light-sleep wake skips it). True by default.
bool app_settings_lock_pin_wake(void);
void app_settings_set_lock_pin_wake(bool on);
/// 清残影周期：通用页及成功夜间正文翻页使用，日间正文/控件不计数；0=关，档位0/3/5/10/14/20/30，新装默认5。
/// Cleanup interval for generic pages and successful night body turns, excluding day body/controls; 0=off, tiers 0/3/5/10/14/20/30, fresh default 5.
/// 夜间两模式共用计数，第N次用厂家GC16；失败不计数，任何成功整屏GC或开机清白重置，关闭后的成功夜间翻页清除未完成周期。
/// Both night profiles share a count and use vendor GC16 on turn N; failures do not count, any successful full-screen GC or boot white clear resets it, and successful disabled night turns clear an unfinished interval.
uint8_t app_settings_gc_every(void);
void app_settings_set_gc_every(uint8_t every);
/// 可选厂家黑白DU直刷，真实目标0/15、不保留灰阶抗锯齿，默认关；已解码图片章节保留标准GL16。
/// Optional vendor black/white DU direct with actual targets 0/15 and no gray antialiasing, off by default; decoded-image chapters retain standard GL16.
bool app_settings_book_direct(void);
void app_settings_set_book_direct(bool on);

enum {
    BOOK_NIGHT_PROFILE_CURRENT = 0, ///< 当前清理 / Current cleanup
    BOOK_NIGHT_PROFILE_CROSSMUX = 1, ///< Crossmux Pico 对照 / Crossmux Pico comparison
};
/// 夜间刷新方案，默认当前清理；Crossmux对照仅纯文字夜间使用黑白DU和周期单次GC，不移植其它固件功能。
/// Night refresh profile defaults to current cleanup; the Crossmux comparison uses BW DU and one periodic GC for night text only, without transplanting other firmware features.
uint8_t app_settings_book_night_profile(void);
/// 保存有效方案，拒绝未知值；独立于已撤回的实验设置。/ Persist a valid profile, rejecting unknown values; independent of the withdrawn experiment setting.
void app_settings_set_book_night_profile(uint8_t profile);

enum {
    BOOK_TAP_ACTION_NONE = 0, ///< 无操作 / None
    BOOK_TAP_ACTION_PREV = 1, ///< 上一页 / Previous page
    BOOK_TAP_ACTION_NEXT = 2, ///< 下一页 / Next page
    BOOK_TAP_ACTION_MENU = 3, ///< 菜单 / Menu toolbar
};

/// 点击布局：左右、右手、左手、上下，取值 0..3。/ Tap layouts: sides, right hand, left hand, vertical; 0..3.
uint8_t app_settings_book_tap_layout(void);
void app_settings_set_book_tap_layout(uint8_t layout);
/// 九宫格各区域动作：0=无、1=上一页、2=下一页、3=菜单。/ 9-grid zone action: 0=none, 1=prev, 2=next, 3=menu.
uint8_t app_settings_book_tap_zone(uint8_t zone_idx);
void app_settings_set_book_tap_zone(uint8_t zone_idx, uint8_t action);
void app_settings_reset_book_tap_zones_default(void);

/// 联网实测的睡眠走时补偿ppm，默认0。/ Network-measured sleep correction in ppm, default zero.
int32_t app_settings_sleep_clock_ppm(void);
/// 保存可信测量，范围±10000ppm。/ Persist trusted measurements within ±10000ppm.
void app_settings_set_sleep_clock_ppm(int32_t ppm);
/// 是否有成功持久化的实测比例，包括零补偿。/ Whether a measured rate was persisted successfully, including zero.
bool app_settings_sleep_clock_valid(void);
/// 动态锁屏低频联网维护走时，默认开启；仅使用已保存WiFi。/ Infrequent dynamic-lock clock maintenance, enabled by default using saved WiFi only.
bool app_settings_clock_auto(void);
/// 保存锁屏自动校时开关。/ Persist the lock clock maintenance switch.
void app_settings_set_clock_auto(bool on);
