/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 *
 * 中文：产品时间模型。UTC 来自 PMU RTC，本地显示按用户时区折算；本文件只做
 * 纯换算与缓存，设备侧读取/校时在 os_time_pico.c。
 * English: Product time model. UTC comes from the PMU RTC and local display
 * applies the user timezone; this file is pure conversion plus cache, while
 * device reads/syncing live in os_time_pico.c.
 *
 * 冻结：不伪造时间；RTC 未校准必须显式可见；时区只支持整数与半小时偏移。
 * Frozen: Never invent time; an uncalibrated RTC stays visible; timezones are
 * whole- and half-hour offsets only.
 */
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
    OS_TIME_UNKNOWN = 0, ///< 尚未读取 / Not read yet
    OS_TIME_UNSYNCED, ///< RTC 从未校准 / RTC never calibrated
    OS_TIME_VALID, ///< UTC 可信 / Trusted UTC
} os_time_state_t;

/// 合法 UTC 窗口：早于 2020 或晚于 2099 一律视为未校准。/ Valid UTC window: before 2020 or after 2099 counts as uncalibrated.
#define OS_TIME_UNIX_MIN 1577836800U
#define OS_TIME_UNIX_MAX 4102444800U
/// 时区范围：±11 小时 45 分，步进 15 分钟。/ Timezone range: ±11h45m in 15-minute steps.
#define OS_TIME_TZ_MIN (-47)
#define OS_TIME_TZ_MAX 47

typedef struct {
    /// 缓存状态。/ Cached state.
    os_time_state_t state;
    /// 最近一次读取的 UTC 秒。/ Last read UTC seconds.
    uint32_t unix_utc;
    /// 生效时区，四分之一小时为单位。/ Active timezone in quarter-hours.
    int16_t tz_qh;
    /// 本地日历字段；weekday 0=周日。/ Local calendar fields; weekday 0=Sunday.
    uint16_t year;
    uint8_t month, day, hour, minute, weekday;
} os_time_info_t;

/// 解码并替换缓存；入参由设备侧或宿主夹具提供。/ Decode and replace the cache; callers feed device or host-fixture inputs.
void os_time_apply(uint32_t unix_utc, int16_t tz_qh);
/// 只读缓存快照。/ Read-only cached snapshot.
const os_time_info_t* os_time_info(void);
/// 时钟文本：有效为 "HH:MM"，否则 "--:--"。/ Clock text: "HH:MM" when valid, else "--:--".
void os_time_format_clock(char* out, size_t cap);
/// 日期文本：有效为 "2026年9月30日 周三"，否则 "时间未校时"。/ Date text when valid, else the uncalibrated notice.
void os_time_format_date(char* out, size_t cap);
/// 时区文本，如 "UTC+8:00"。/ Timezone text such as "UTC+8:00".
void os_time_format_tz(int16_t tz_qh, char* out, size_t cap);
/// 本地日期键 YYYYMMDD；缓存无效返回 false。/ Local date key YYYYMMDD; false when the cache is invalid.
bool os_time_date_key(uint32_t* yyyymmdd);
/// 纯日期平移，用于近 N 日汇总；非法输入原样返回。/ Pure date shift for recent-day totals; invalid input returns it unchanged.
uint32_t os_time_date_shift(uint32_t yyyymmdd, int days);
/// 时区步进合法性。/ Timezone step validity.
bool os_time_tz_valid(int16_t tz_qh);

/*
 * 设备桥接口：实现于 os_time_pico.c；宿主预览/测试用各自夹具替换。
 * Device bridge: implemented in os_time_pico.c; host previews and tests substitute fixtures.
 */
/// UI 任务节流刷新 PMU RTC 缓存。/ Throttled PMU RTC cache refresh on the UI task.
void os_time_poll(int64_t now_ms);
/// 强制下次 poll 立即刷新。/ Force the next poll to refresh immediately.
void os_time_invalidate(void);
/// 仅强制下次 poll 刷新，保留校准状态。/ Force the next poll to refresh while keeping the sync state.
void os_time_force_poll(void);
/// 保存并应用新时区。/ Persist and apply a new timezone.
void os_time_set_tz(int16_t qh);
/// 传书会话驱动 SNTP 校时；sta_uplink 表示 STA 已有网络地址。/ Transfer-session SNTP calibration; sta_uplink means STA has an address.
void os_time_network(bool sta_uplink);
/// 电量千分比（0-1000），无有效快照返回 -1；只读缓存不做 IO。/ Battery permille (0-1000), -1 without a valid snapshot; cached read, no IO.
int os_time_battery_permille(void);
/// 最近一次 STA 会话内是否成功校时；会话结束或失效后复位。/ Whether the last STA session calibrated time; cleared when it ends or on invalidate.
bool os_time_recently_synced(void);
