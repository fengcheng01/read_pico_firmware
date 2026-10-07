/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 *
 * 中文：动态锁屏的低频校时会话，补齐首次走时学习；UI任务推进，无传书服务。
 * English: Infrequent dynamic-lock time sessions complete initial rate learning, driven by the UI without upload services.
 *
 * 冻结：用户要求修复长期慢钟；只用已保存WiFi，每次至多30秒，失败一小时后重试；解锁即取消并断网。
 * Frozen: User-requested long-run drift repair uses saved WiFi for at most 30 seconds, retries failures after an hour and disconnects on unlock.
 */
#include "os_time.h"
#include "settings.h"
#include "read_pico_transfer.h"

static bool s_active, s_claimed;
static int64_t s_started_ms, s_service_ms, s_retry_ms;

void os_time_lock_sync_cancel(void) {
    if (s_active) { os_time_network(false); read_pico_transfer_stop(); }
    s_active = false;
    if (s_claimed) read_pico_transfer_release_sync();
    s_claimed = false;
}
bool os_time_lock_sync_tick(int64_t now_ms) {
    if (!app_settings_clock_auto()) { os_time_lock_sync_cancel(); return false; }
    if (!s_active) {
        if (now_ms < s_retry_ms || !os_time_maintenance_due(now_ms)) return false;
        s_retry_ms = now_ms + 3600000;
        char ssid[33]; bool configured = false;
        read_pico_transfer_status_t status;
        read_pico_transfer_get_status(&status);
        if (status.state != READ_PICO_TRANSFER_STOPPED ||
            read_pico_transfer_get_saved_wifi(ssid, &configured) != ESP_OK || !configured ||
            !read_pico_transfer_claim_sync()) return false;
        s_claimed = true;
        os_time_network(false);
        if (read_pico_transfer_start_saved_network() != ESP_OK) { os_time_lock_sync_cancel(); return false; }
        s_active = true; s_started_ms = s_service_ms = now_ms;
    }
    if (now_ms - s_service_ms >= 500) { read_pico_transfer_service_poll(); s_service_ms = now_ms; }
    read_pico_transfer_status_t status;
    read_pico_transfer_get_status(&status);
    os_time_network(status.mode == READ_PICO_TRANSFER_MODE_STA && status.network_ready);
    if (os_time_recently_synced() || status.state == READ_PICO_TRANSFER_ERROR ||
        status.state == READ_PICO_TRANSFER_STOPPED || now_ms - s_started_ms >= 30000) {
        os_time_lock_sync_cancel(); return false;
    }
    return true;
}
