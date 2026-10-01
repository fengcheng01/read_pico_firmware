/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 *
 * 中文：崩溃日志设备桥。开机读复位原因，内置存储挂载后追加到有界日志；
 * 不读反栈（IDF v6 无公开 panic 钩子），不写 TF 卡。
 * English: Crash-log device bridge. Boot reads the reset reason and appends to
 * the bounded log once the internal FAT mounts; no backtrace (IDF v6 exposes
 * no panic hook) and never touches the TF card.
 *
 * 冻结：只经 book_store_read_roots 的免格式化挂载路径写 /flash；失败保留
 * 待写状态到下次调用，不在其他线程补偿。
 * Frozen: Write /flash only through the format-free mount of
 * book_store_read_roots; failures keep the pending record for the next call
 * and never retry from other threads.
 */
#include "os_crash.h"

#include <stdio.h>
#include <string.h>

#include "book_store.h"
#include "esp_log.h"
#include "esp_system.h"
#include "os_time.h"

static const char* TAG = "os_crash";

// boot_check 暂存的待写记录；written 标记本次开机已完成。
// The stashed record from boot_check; written marks this boot as done.
static bool s_pending, s_written;

void os_crash_boot_check(void) {
    if (s_pending || s_written) return;
    int reason = (int)esp_reset_reason();
    if (!os_crash_reason_abnormal(reason)) {
        s_written = true;  // 正常开机也记账，避免之后误判。/ Normal boots count too, preventing later misuse.
        return;
    }
    s_pending = true;
    ESP_LOGW(TAG, "previous reset was abnormal: %s", os_crash_reset_name(reason));
}

void os_crash_flush(void) {
    if (!s_pending || s_written) return;
    // 免格式化挂载内置分区；TF 卡状态与本模块无关。/ Format-free flash mount; the TF card is irrelevant here.
    book_store_root_t roots[2];
    int count = 0;
    if (book_store_read_roots(roots, &count) != ESP_OK) return;
    static char log[OS_CRASH_LOG_MAX + 64];
    size_t len = 0;
    FILE* file = fopen(OS_CRASH_LOG_PATH, "rb");
    if (file) {
        len = fread(log, 1, OS_CRASH_LOG_MAX, file);
        fclose(file);
        if (len > OS_CRASH_LOG_MAX) len = OS_CRASH_LOG_MAX;
    }
    os_time_invalidate();
    os_time_poll(0);
    char record[32];
    int reason = (int)esp_reset_reason();
    size_t record_len = os_crash_format_record(record, sizeof(record),
                                               os_time_info()->state == OS_TIME_VALID ? os_time_info()->unix_utc : 0,
                                               reason);
    if (len + record_len > OS_CRASH_LOG_MAX) {
        size_t at = os_crash_trim_tail(log, len, OS_CRASH_LOG_KEEP);
        memmove(log, log + at, len - at);
        len -= at;
    }
    if (len + record_len > sizeof(log)) return;
    memcpy(log + len, record, record_len);
    len += record_len;
    file = fopen(OS_CRASH_LOG_PATH, "wb");
    if (!file) return;
    bool ok = fwrite(log, 1, len, file) == len;
    ok = fclose(file) == 0 && ok;
    if (ok) {
        s_pending = false;
        s_written = true;
        ESP_LOGW(TAG, "crash log updated: %s", record);
    }
}

bool os_crash_summary(char* out, size_t cap) {
    if (!out || !cap) return false;
    FILE* file = fopen(OS_CRASH_LOG_PATH, "rb");
    if (!file) return false;
    static char log[OS_CRASH_LOG_MAX];
    size_t len = fread(log, 1, sizeof(log), file);
    fclose(file);
    return os_crash_format_summary(log, len, out, cap);
}
