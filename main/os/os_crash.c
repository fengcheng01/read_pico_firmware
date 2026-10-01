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

// 与 esp_reset_reason_t 数值一一对应；跨 IDF 版本保持稳定。
// Mirrors esp_reset_reason_t values; stable across IDF versions.
const char* os_crash_reset_name(int reason) {
    switch (reason) {
        case 0: return "UNKNOWN";
        case 1: return "POWERON";
        case 2: return "SW";
        case 3: return "PANIC";
        case 4: return "INT_WDT";
        case 5: return "TASK_WDT";
        case 6: return "WDT";
        case 7: return "BROWNOUT";
        case 8: return "SDIO";
        default: return "RST?";
    }
}

bool os_crash_reason_abnormal(int reason) {
    // 3..7 = panic、中断看门狗、任务看门狗、其他看门狗、掉电。
    // 3..7 cover panic, interrupt/task/other watchdogs and brownout.
    return reason >= 3 && reason <= 7;
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
