/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 *
 * 中文：崩溃日志的纯文本部分：复位原因命名、一行记录、汇总与有界裁剪。
 * English: The pure text half of the crash log: reset-reason naming, the one
 * line record, the summary and bounded trimming.
 */
#include "os_crash.h"

#include <stdio.h>
#include <string.h>

// 对齐 IDF v6.1 完整枚举；设备桥在编译时核对，避免漏项导致后续原因错位。
// Match the complete IDF v6.1 enum, checked by the device bridge at compile time to prevent shifted names after omissions.
const char* os_crash_reset_name(int reason) {
    switch (reason) {
        case 0: return "UNKNOWN";
        case 1: return "POWERON";
        case 2: return "EXT";
        case 3: return "SW";
        case 4: return "PANIC";
        case 5: return "INT_WDT";
        case 6: return "TASK_WDT";
        case 7: return "WDT";
        case 8: return "DEEPSLEEP";
        case 9: return "BROWNOUT";
        case 10: return "SDIO";
        case 11: return "USB";
        case 12: return "JTAG";
        case 13: return "EFUSE";
        case 14: return "PWR_GLITCH";
        case 15: return "CPU_LOCKUP";
        default: return "RST?";
    }
}

bool os_crash_reason_abnormal(int reason) {
    // 软件重启与深睡唤醒不计异常；错误、掉电与CPU锁死才记。
    // Software resets and deep-sleep wakes are normal; record faults, brownouts and CPU lockups.
    return (reason >= 4 && reason <= 7) || reason == 9 || (reason >= 13 && reason <= 15);
}

size_t os_crash_format_record(char* out, size_t cap, uint32_t unix_utc, int reason) {
    if (!out || !cap) return 0;
    return (size_t)snprintf(out, cap, "%u %s\n", (unsigned)unix_utc, os_crash_reset_name(reason));
}

bool os_crash_format_summary(const char* log_text, size_t len, char* out, size_t cap) {
    if (!out || !cap || !log_text || !len) return false;
    unsigned count = 0;
    char latest_name[12] = "";
    size_t line_start = 0;
    for (size_t i = 0; i <= len; ++i) {
        if (i != len && log_text[i] != '\n') continue;
        size_t line_len = i - line_start;
        if (line_len) {
            ++count;
            // 行格式 "<unix> <NAME>"，汇总只取最近一条的名字。/ Line layout "<unix> <NAME>"; the summary keeps the latest name only.
            unsigned long unix_value = 0;
            char name[12] = "";
            // %lu 写 8 字节；宿主上 uint32_t 是 4 字节，不能直接取址。
            // %lu writes 8 bytes; host uint32_t is 4 bytes and cannot be addressed directly.
            if (sscanf(log_text + line_start, "%lu %11s", &unix_value, name) >= 1)
                snprintf(latest_name, sizeof(latest_name), "%s", name);
        }
        line_start = i + 1;
    }
    if (!count) return false;
    snprintf(out, cap, "%u 条 · 最近 %s", count, latest_name[0] ? latest_name : "?");
    return true;
}

size_t os_crash_trim_tail(const char* text, size_t len, size_t keep) {
    if (!text || len <= keep) return len;
    size_t at = len - keep;
    // 从行首边界开始，避免半行开头。/ Start at a line boundary; never begin mid-line.
    while (at < len && text[at - 1] != '\n') ++at;
    return at >= len ? len : at;
}
