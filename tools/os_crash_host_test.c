/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 *
 * 崩溃日志纯函数测试：原因命名、一行记录、汇总与行界裁剪。
 * Crash-log pure-function tests: reason naming, the record line, the summary
 * and line-boundary trimming.
 */
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "os_crash.h"

int main(void) {
    // 命名与分类。/ Naming and classification.
    assert(strcmp(os_crash_reset_name(3), "PANIC") == 0);
    assert(strcmp(os_crash_reset_name(1), "POWERON") == 0);
    assert(strcmp(os_crash_reset_name(99), "RST?") == 0);
    assert(!os_crash_reason_abnormal(0) && !os_crash_reason_abnormal(1) && !os_crash_reason_abnormal(2));
    assert(os_crash_reason_abnormal(3) && os_crash_reason_abnormal(5) && os_crash_reason_abnormal(7));
    assert(!os_crash_reason_abnormal(8));

    // 一行记录。/ The record line.
    char line[32];
    assert(os_crash_format_record(line, sizeof(line), 1790000000U, 3) == strlen("1790000000 PANIC\n"));
    assert(strcmp(line, "1790000000 PANIC\n") == 0);
    assert(os_crash_format_record(line, sizeof(line), 0, 5) == strlen("0 TASK_WDT\n"));

    // 汇总。/ Summary.
    char summary[64];
    const char* log = "1700000000 PANIC\n1700001000 TASK_WDT\n1700002000 BROWNOUT\n";
    assert(os_crash_format_summary(log, strlen(log), summary, sizeof(summary)));
    assert(strcmp(summary, "3 条 · 最近 BROWNOUT") == 0);
    assert(!os_crash_format_summary("", 0, summary, sizeof(summary)));
    assert(!os_crash_format_summary("\n\n", 2, summary, sizeof(summary)));  // 空行不计。/ Blank lines never count.

    // 裁剪从行首开始。/ Trimming starts at a line boundary.
    const char* text = "aaaa\nbbbb\ncccc\ndddd\n";  // 20 字节 / bytes
    // keep=10 落在 cccc 行首，正好是行界。/ keep=10 lands exactly on the cccc line start.
    size_t at = os_crash_trim_tail(text, strlen(text), 10);
    assert(text[at] == 'c');
    // keep=7 落在行中，起点推进到 dddd 行首。/ keep=7 lands mid-line; the start advances to the dddd line.
    at = os_crash_trim_tail(text, strlen(text), 7);
    assert(text[at] == 'd');
    // 未超限原样返回。/ Within the cap the length is unchanged.
    assert(os_crash_trim_tail(text, strlen(text), 64) == strlen(text));
    printf("os_crash ok\n");
    return 0;
}
