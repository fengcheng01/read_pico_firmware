/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 * 中文：UI线程低频采样和失败重试，历史及读回工作区放PSRAM。
 * English: Low-rate UI-thread sampling/retries with history and readback work in PSRAM.
 * 冻结：卡切换不混合电量历史；缺卡只记当前会话，读写失败不销毁有效内存数据。
 * Frozen: Never merge battery histories across cards; cardless records are session-only and IO failures retain valid in-memory data.
 */
#include "os_history_service.h"
#include "book_stats.h"
#include "book_stats_archive.h"
#include "esp_heap_caps.h"
#include <stdlib.h>
#include <string.h>

static os_battery_history_t* s_history;
static bool s_mounted, s_loaded, s_pending, s_read_error;
static int s_slot = -1;
static int64_t s_last_poll = -1000, s_retry_at, s_sample_at;
static unsigned s_revision;
static int64_t s_archive_at;
static bool ensure_history(void) {
    if (!s_history) {
        s_history = heap_caps_calloc(1, sizeof(*s_history), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (!s_history) return false;
        os_battery_history_init(s_history);
    }
    return true;
}
void os_history_service_poll(int64_t now, bool mounted) {
    if (mounted != s_mounted) {
        s_mounted = mounted; s_loaded = s_pending = s_read_error = false;
        s_slot = -1; s_retry_at = s_sample_at = 0; ++s_revision;
        book_stats_archive_media(mounted ? os_history_directory() : NULL);
        s_archive_at=0;
        if (s_history) os_battery_history_init(s_history);
    }
    if (now >= s_last_poll && now - s_last_poll < 1000) return;
    s_last_poll = now;
    if (mounted && now >= s_archive_at) {
        book_stats_store_maintain(); book_stats_reconcile();
        s_archive_at=now+60000;
    }
    if (!ensure_history()) return;
    if (mounted && !s_loaded && now >= s_retry_at) {
        os_battery_history_t* work = heap_caps_calloc(1, sizeof(*work), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        os_battery_load_t result = work ? os_battery_history_load(os_history_directory(), s_history, work, &s_slot) : OS_BATTERY_LOAD_ERROR;
        free(work); s_loaded = result != OS_BATTERY_LOAD_ERROR; s_read_error = !s_loaded;
        s_retry_at = s_loaded ? now : now + 60000; ++s_revision;
    }
    os_history_snapshot_t snap = {0}; os_history_snapshot(now, &snap);
    if (snap.valid && (!mounted || s_loaded) && now >= s_sample_at) {
        os_battery_sample_t sample = {.at=snap.utc, .permille=(uint16_t)snap.permille,
            .millivolts=snap.millivolts, .charge_state=snap.charge_state};
        if (os_battery_history_add(s_history, &sample)) { s_pending = true; ++s_revision; }
        s_sample_at = now + (int64_t)OS_BATTERY_INTERVAL * 1000;
    }
    if (mounted && s_loaded && s_pending && now >= s_retry_at) {
        os_battery_history_t* work = heap_caps_calloc(1, sizeof(*work), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        bool saved = work && os_battery_history_save(os_history_directory(), s_history, work, &s_slot);
        free(work); if (saved) { s_pending = false; ++s_revision; }
        s_retry_at = now + 60000;
    }
}
const os_battery_history_t* os_history_battery(void) { return s_history; }
unsigned os_history_revision(void) { return s_revision; }
bool os_history_media_ready(void) { return s_mounted; }
bool os_history_battery_pending(void) { return s_pending; }
bool os_history_battery_read_error(void) { return s_read_error; }
