/*
 * SPDX-FileCopyrightText: 2026 mindreset
 *
 * 中文：时间服务的设备桥。RTC/SNTP 只做锚点，运行时间由本地单调钟插值推进；
 * 节流轮询 PMU RTC 仅诊断；浅睡保留锚点，校时或冷启动才重新锚定。
 * English: Device bridge for the time service. RTC/SNTP only anchor; runtime
 * time advances on the local monotonic clock; PMU polls diagnose drift, and light sleep retains the anchor.
 *
 * 冻结：对 PMU 只允许 TIME_SYNC 一条写命令；SNTP 在传书、按需对时或已启用锁屏维护的 STA 上行运行，
 * 每会话成功校准一次（写入需回读验证）后释放；读失败保持旧锚点并显示未校时。
 * Frozen: TIME_SYNC is the only PMU write; SNTP runs during transfer, on-demand sync or enabled lock maintenance STA
 * uplink exists, one verified calibration per session then released; read
 * failures keep the old anchor and an uncalibrated display.
 * 冻结：针对锁屏走慢，补偿仅由同次开机两次SNTP实测学习并持久化，只应用浅睡累计。
 * Frozen: To address slow lock clocks, persist rates learned from two SNTP samples in one boot and apply them only to accumulated light sleep.
 * 修订原因：用户24小时慢钟测试缺少第二锚点；锁屏可用已保存WiFi低频完成学习并断网，可关闭。
 * Revision: The user's 24-hour slow-clock test lacked a second anchor; optional lock maintenance obtains it with saved WiFi and disconnects.
 * 冻结：RTC通信成功不等于时间有效；只有支持范围内的UTC可成为启动锚点。
 * Frozen: A successful RTC read does not imply valid time; only UTC in the supported range may seed a boot anchor.
 * 修订原因：参考M4先验证RTC再恢复的边界，避免未校准的零值锁住后续有效读数。
 * Revision: Follow M4's validate-before-restore boundary so an unset zero cannot block a later valid RTC read.
 * 冻结：睡眠比例只恢复同一时钟模型的记录，旧无标识值需重新学习，不按反馈估算扣秒。
 * Frozen: Restore rates only for the same clock model; relearn untagged legacy values rather than subtracting estimated drift.
 * 修订原因：用户离线锁屏偏快且保存+4410ppm，补偿来源必须可核验。
 * Revision: Offline lock time gains with a saved +4410ppm, so persisted compensation needs a verifiable model identity.
 */
#include "os_time.h"
#include "os_clock_rate.h"
#include "esp_log.h"
#include "esp_netif_sntp.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "read_pico_pmu.h"
#include "settings.h"
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <sys/time.h>

static const char* TAG = "os_time";
#define OS_TIME_POLL_MS 15000
// RTC 读数与插值的日志容差（秒）；超出也不自动追针。
// Log tolerance between RTC and interpolation (seconds); even larger deltas never step automatically.
#define OS_TIME_STEP_S 10
// TIME_SYNC 写后回读允许的最大偏差（秒），超过视为写入被忽略。
// Maximum readback skew after TIME_SYNC (seconds); larger means the write was ignored.
#define OS_TIME_VERIFY_S 2

static int64_t s_last_poll_ms = -OS_TIME_POLL_MS;
static bool s_recently_synced;
// 锚点 = 校准时刻的 UTC 秒 + 当时的单调毫秒；未锚定时显示未校时。
// 醒着用晶体驱动的单调钟插值；浅睡经过内部RC，精度与墨菲M4的独立RTC不同。
// 墨菲M4每次深睡唤醒从RX8010重锚；本板PMU协议注明LSI约±1–2%，不能视为同等参考源。
// Interpolate with the crystal-driven monotonic clock while awake; light sleep uses internal RC,
// unlike Murphy M4, which re-anchors from its independent RX8010 after each deep-sleep wake.
// This board's PMU protocol notes about ±1–2% LSI tolerance, so its reads are not an equivalent reference.
static bool s_anchored;
static bool s_force_reanchor;
static uint32_t s_anchor_sec;
static int64_t s_anchor_ms;

static os_clock_rate_t s_rate;
static bool s_rate_loaded;
static bool s_rate_learned;
static int64_t s_last_sync_ms = -1;
static void load_rate(void) {
    if (!s_rate_loaded) {
        s_rate.ppm = app_settings_sleep_clock_ppm();
        s_rate_learned = app_settings_sleep_clock_valid(); s_rate_loaded = true;
    }
}
bool os_time_maintenance_due(int64_t now_ms) {
    load_rate();
    return s_last_sync_ms < 0 || now_ms - s_last_sync_ms >= (s_rate_learned ? 21600000 : 3660000);
}
void os_time_clock_status(char* out, size_t cap) {
    load_rate();
    if (s_rate_learned) snprintf(out, cap, "睡眠补偿 %+ld ppm · %s", (long)s_rate.ppm,
        app_settings_sleep_clock_valid() ? "已保存" : "本次生效，保存待重试");
    else snprintf(out, cap, "%s", s_rate.result == OS_CLOCK_OUTLIER ? "走时样本异常 · 下次重新测量" :
        s_rate.result == OS_CLOCK_AWAKE ? "睡眠样本不足 · 锁屏后继续学习" : "走时待学习 · 需两次联网对时");
}
void os_time_record_sleep(int64_t duration_us) {
    load_rate();
    os_clock_rate_sleep(&s_rate, duration_us);
}

static uint32_t interpolated(int64_t now_ms) {
    now_ms = os_clock_rate_ms(&s_rate, now_ms);
    return s_anchor_sec + (uint32_t)((now_ms - s_anchor_ms) / 1000);
}

static void anchor_from(uint32_t unix_sec, int64_t now_ms) {
    s_anchored = true;
    s_anchor_sec = unix_sec;
    load_rate();
    s_anchor_ms = os_clock_rate_ms(&s_rate, now_ms);
    os_time_apply(unix_sec, app_settings_tz_qh());
}

static void seed_libc(uint32_t unix_sec) {
    // PMU 时钟也恢复 libc 时钟，HTTPS 和界面使用同一 UTC 起点。
    // Seed libc from a valid PMU clock so HTTPS and the UI share the UTC baseline.
    if (unix_sec >= OS_TIME_UNIX_MIN && unix_sec < OS_TIME_UNIX_MAX && time(NULL) < OS_TIME_UNIX_MIN) {
        struct timeval now = {.tv_sec = unix_sec, .tv_usec = 0};
        settimeofday(&now, NULL);
    }
}

// UI 任务节流调用；与主循环的 PMU 轮询同任务串行，无需加锁。
// Throttled on the UI task; serialized with the loop's PMU poll on the same task.
void os_time_poll(int64_t now_ms) {
    if (s_anchored) os_time_apply(interpolated(now_ms), app_settings_tz_qh());
    if (now_ms - s_last_poll_ms < OS_TIME_POLL_MS) return;
    s_last_poll_ms = now_ms;
    if (!read_pico_pmu_ready() || read_pico_pmu_refresh() != ESP_OK) return;
    const pmu_snapshot_t* pmu = read_pico_pmu_get();
    if (!pmu->time_ok || pmu->unix_sec < OS_TIME_UNIX_MIN || pmu->unix_sec >= OS_TIME_UNIX_MAX) return;
    seed_libc(pmu->unix_sec);
    if (s_anchored && !s_force_reanchor) {
        // 只记录偏差不追针：RTC 变快会体现在日志里，显式校时才重锚。
        // Record the delta without stepping: a fast RTC appears in logs and explicit calibration re-anchors.
        int64_t delta = (int64_t)pmu->unix_sec - (int64_t)interpolated(now_ms);
        if (delta < -OS_TIME_STEP_S || delta > OS_TIME_STEP_S)
            ESP_LOGI(TAG, "rtc drift %llds ignored until re-anchor", (long long)delta);
        return;
    }
    s_force_reanchor = false;
    if (s_anchored) ESP_LOGI(TAG, "re-anchor, rtc delta %llds",
                             (long long)((int64_t)pmu->unix_sec - (int64_t)interpolated(now_ms)));
    anchor_from(pmu->unix_sec, now_ms);
}

void os_time_rtc_reanchor(void) {
    s_rate.sampled = false;
    s_force_reanchor = true;
    s_last_poll_ms = -OS_TIME_POLL_MS;
}

void os_time_invalidate(void) {
    s_last_poll_ms = -OS_TIME_POLL_MS;
    s_recently_synced = false;
    s_anchored = false;
    s_rate.sampled = false;
}

void os_time_force_poll(void) {
    // 仅重置节流，保留校准状态与锚点；锁屏画面刷新用这个。/ Reset throttling only, keeping the sync state and anchor; lock faces use this.
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
    // 时区只影响换算，不改锚点。/ The timezone changes conversion only, never the anchor.
    if (s_anchored) os_time_apply(interpolated(esp_timer_get_time() / 1000), app_settings_tz_qh());
    else os_time_apply(0, app_settings_tz_qh());
}

// 传书页 tick 驱动：sta_uplink 表示 STA 已取得网络地址。成功校准一次即写 PMU、
// 回读验证、重锚并释放 SNTP；无路由时静默等待传书会话结束。
// Driven by the transfer page tick; sta_uplink means STA has a network address.
// One successful calibration writes the PMU, verifies by readback, re-anchors
// and releases SNTP; without a route it quietly waits for the session end.
void os_time_network(bool sta_uplink) {
    static bool s_sntp_up;
    static bool s_ntp_received;
    if (!sta_uplink) {
        if (s_sntp_up) {
            esp_netif_sntp_deinit();
            s_sntp_up = false;
        }
        s_recently_synced = false;
        s_ntp_received = false;
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
        s_ntp_received = false;
    }
    // 同步信号只消费一次，PMU暂时读写失败后仍可重试同一可信网络结果。
    // Consume the sync signal once but retain it across transient PMU write/readback failures.
    if (!s_ntp_received) {
        if (esp_netif_sntp_sync_wait(pdMS_TO_TICKS(0)) != ESP_OK) return;
        s_ntp_received = true;
    }
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
    // 回读校验：PMU 忽略写入时不能误标已校准，下个会话还能重试。
    // Read back: never mark calibrated when the PMU ignored the write so a later session can retry.
    if (read_pico_pmu_refresh() != ESP_OK || !read_pico_pmu_get()->time_ok) {
        ESP_LOGW(TAG, "TIME_SYNC readback failed");
        return;
    }
    int64_t skew = (int64_t)read_pico_pmu_get()->unix_sec - (int64_t)(uint32_t)now;
    if (skew < 0) skew = -skew;
    if (skew > OS_TIME_VERIFY_S) {
        ESP_LOGW(TAG, "TIME_SYNC verify skew %llds", (long long)skew);
        return;
    }
    esp_netif_sntp_deinit();
    s_sntp_up = false;
    s_recently_synced = true;
    seed_libc((uint32_t)now);
    // 回读验证后重新读取网络时间，避免PMU写入耗时进入锚点偏差。
    // Read network time again after verification so PMU write latency cannot skew the anchor.
    struct timeval verified_time;
    gettimeofday(&verified_time, NULL);
    int64_t tick_ms = esp_timer_get_time() / 1000;
    load_rate();
    if (os_clock_rate_sync(&s_rate, (int64_t)verified_time.tv_sec * 1000 + verified_time.tv_usec / 1000, tick_ms)) {
        s_rate_learned = true;
        app_settings_set_sleep_clock_ppm(s_rate.ppm);
    } else if (s_rate_learned && !app_settings_sleep_clock_valid()) app_settings_set_sleep_clock_ppm(s_rate.ppm);
    s_last_sync_ms = tick_ms;
    ESP_LOGI(TAG, "sleep clock result=%d ppm=%ld elapsed=%lldms sleep=%lldms error=%lldms saved=%d",
        s_rate.result, (long)s_rate.ppm, (long long)s_rate.window_ms, (long long)(s_rate.window_sleep_us / 1000),
        (long long)s_rate.error_ms, app_settings_sleep_clock_valid());
    anchor_from((uint32_t)verified_time.tv_sec, tick_ms);
    // 保留当前秒内相位，分钟显示不因整数秒截断提前跳转。
    // Retain the subsecond phase so minute displays do not advance from truncation.
    s_anchor_ms -= verified_time.tv_usec / 1000;
    ESP_LOGI(TAG, "time calibrated from NTP");
}
