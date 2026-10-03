/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 *
 * 中文：时间服务的设备桥。在 UI 任务上节流刷新 PMU RTC 快照；已有 WiFi 传书
 * 会话期间用 SNTP 校准并写回 PMU TIME_SYNC。
 * English: Device bridge for the time service. Throttled PMU RTC refresh on the
 * UI task; SNTP calibration during STA transfer sessions writes PMU TIME_SYNC.
 *
 * 冻结：对 PMU 只允许 TIME_SYNC 一条写命令；SNTP 只在传书 STA 上行存在时运行，
 * 每会话成功校准一次后释放；读失败保持旧缓存并显示未校时。
 * Frozen: TIME_SYNC is the only PMU write; SNTP runs only while the transfer STA
 * uplink exists, one successful calibration per session then released; read
 * failures keep the old cache and an uncalibrated display.
 */
#include "os_time.h"
#include "esp_log.h"
#include "esp_netif_sntp.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "read_pico_pmu.h"
#include "settings.h"
#include <time.h>
#include <sys/time.h>

static const char* TAG = "os_time";
#define OS_TIME_POLL_MS 15000

static int64_t s_last_poll_ms = -OS_TIME_POLL_MS;
static bool s_recently_synced;

static void refresh_cache(void) {
    const pmu_snapshot_t* pmu = read_pico_pmu_get();
    os_time_apply(pmu->time_ok ? pmu->unix_sec : 0, app_settings_tz_qh());
    // PMU 时钟也恢复 libc 时钟，HTTPS 和界面使用同一 UTC 起点。
    // Seed libc from a valid PMU clock so HTTPS and the UI share the UTC baseline.
    if (pmu->time_ok && pmu->unix_sec >= OS_TIME_UNIX_MIN && pmu->unix_sec < OS_TIME_UNIX_MAX && time(NULL) < OS_TIME_UNIX_MIN) {
        struct timeval now = {.tv_sec = pmu->unix_sec, .tv_usec = 0};
        settimeofday(&now, NULL);
    }
}

// UI 任务节流调用；与主循环的 PMU 轮询同任务串行，无需加锁。
// Throttled on the UI task; serialized with the loop's PMU poll on the same task.
void os_time_poll(int64_t now_ms) {
    if (now_ms - s_last_poll_ms < OS_TIME_POLL_MS) return;
    s_last_poll_ms = now_ms;
    if (read_pico_pmu_ready() && read_pico_pmu_refresh() == ESP_OK) refresh_cache();
}

void os_time_invalidate(void) {
    s_last_poll_ms = -OS_TIME_POLL_MS;
    s_recently_synced = false;
}

void os_time_force_poll(void) {
    // 仅重置节流，保留校准状态；锁屏画面刷新用这个。/ Reset throttling only, keeping the sync state; lock faces use this.
    s_last_poll_ms = -OS_TIME_POLL_MS;
}

int os_time_battery_permille(void) {
    const pmu_snapshot_t* pmu = read_pico_pmu_ready() ? read_pico_pmu_get() : NULL;
    // 电量与时间同快照：refresh 由 poll 节流刷新，这里零 IO。
    // Battery shares the time snapshot: poll refreshes it throttled, this stays IO-free.
    return pmu && pmu->status_ok && pmu->soc_permille <= 1000 ? pmu->soc_permille : -1;
}

bool os_time_recently_synced(void) { return s_recently_synced; }

void os_time_set_tz(int16_t qh) {
    app_settings_set_tz_qh((int8_t)qh);
    refresh_cache();
}

// 传书页 tick 驱动：sta_uplink 表示 STA 已取得网络地址。成功校准一次即写 PMU、
// 刷新缓存并释放 SNTP；无路由时静默等待传书会话结束。
// Driven by the transfer page tick; sta_uplink means STA has a network address.
// One successful calibration writes the PMU, refreshes the cache and releases
// SNTP; without a route it quietly waits for the session end.
void os_time_network(bool sta_uplink) {
    static bool s_sntp_up;
    if (!sta_uplink) {
        if (s_sntp_up) {
            esp_netif_sntp_deinit();
            s_sntp_up = false;
        }
        s_recently_synced = false;
        return;
    }
    if (s_recently_synced) return;  // 本会话已校准一次：不再重初始化/重写 PMU / calibrated once this session
    if (!s_sntp_up) {
        // 官方宏在 -Werror=missing-braces 下不过；先零初始化再逐字段赋值。
        // The official macro trips -Werror=missing-braces; zero-init then assign fields.
        esp_sntp_config_t cfg = {0};
        cfg.start = true;
        cfg.wait_for_sync = true;
        cfg.ip_event_to_renew = IP_EVENT_STA_GOT_IP;
        cfg.num_of_servers = 2;
        cfg.servers[0] = "ntp.aliyun.com";
        cfg.servers[1] = "pool.ntp.org";
        if (esp_netif_sntp_init(&cfg) != ESP_OK) {
            ESP_LOGW(TAG, "sntp init failed");
            return;
        }
        s_sntp_up = true;
    }
    if (esp_netif_sntp_sync_wait(pdMS_TO_TICKS(0)) != ESP_OK) return;
    time_t now = time(NULL);
    if ((uint32_t)now < OS_TIME_UNIX_MIN || (uint32_t)now >= OS_TIME_UNIX_MAX) return;
    uint8_t payload[4] = {
        (uint8_t)((uint32_t)now & 0xFF), (uint8_t)(((uint32_t)now >> 8) & 0xFF),
        (uint8_t)(((uint32_t)now >> 16) & 0xFF), (uint8_t)(((uint32_t)now >> 24) & 0xFF),
    };
    if (read_pico_pmu_cmd(PMU_CMD_TIME_SYNC, payload, sizeof(payload)) != ESP_OK) {
        ESP_LOGW(TAG, "TIME_SYNC failed");
        return;
    }
    esp_netif_sntp_deinit();
    s_sntp_up = false;
    s_recently_synced = true;
    s_last_poll_ms = -OS_TIME_POLL_MS;
    os_time_poll(0);
    ESP_LOGI(TAG, "time calibrated from NTP");
}
