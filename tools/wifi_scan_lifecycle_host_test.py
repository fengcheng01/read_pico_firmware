#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 mindreset
# SPDX-License-Identifier: Apache-2.0
"""真实扫描入口与SDK边界故障回归；不模拟实际射频。/ Production scan entry with SDK-boundary faults, without simulating RF."""
import argparse
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
source = (ROOT / "components/read_pico_transfer/read_pico_transfer.c").read_text()
scan_body = source[source.index("static wifi_init_config_t wifi_init_for_session("):source.index("esp_err_t read_pico_transfer_forget_wifi(")]
stop_body = source[source.index("bool read_pico_transfer_try_stop_if_idle("):source.index("esp_err_t read_pico_transfer_start(")]
start_body = source[source.index("esp_err_t read_pico_transfer_start("):source.rindex("\n#endif")]
harness = r'''
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/stat.h>
#include "transfer_policy.h"
typedef int esp_err_t;
enum { ESP_OK, ESP_ERR_INVALID_ARG, ESP_ERR_INVALID_STATE, ESP_FAIL, ESP_ERR_WIFI_TIMEOUT,
       ESP_ERR_NO_MEM, ESP_ERR_WIFI_NOT_INIT, ESP_ERR_WIFI_NOT_STARTED, ESP_ERR_NOT_FOUND };
enum { WIFI_STORAGE_RAM, WIFI_MODE_STA, WIFI_MODE_AP, WIFI_IF_STA, WIFI_IF_AP, WIFI_SCAN_TYPE_ACTIVE, WIFI_SCAN_TYPE_PASSIVE };
enum { WIFI_AUTH_OPEN, WIFI_AUTH_WEP, WIFI_AUTH_WPA_PSK, WIFI_AUTH_WPA2_PSK,
       WIFI_AUTH_WPA_WPA2_PSK, WIFI_AUTH_WPA3_PSK, WIFI_AUTH_WPA2_WPA3_PSK, WIFI_AUTH_OWE };
enum { WIFI_COUNTRY_POLICY_AUTO, WIFI_COUNTRY_POLICY_MANUAL };
enum { READ_PICO_TRANSFER_STOPPED, READ_PICO_TRANSFER_STARTING, READ_PICO_TRANSFER_READY, READ_PICO_TRANSFER_ERROR };
enum { READ_PICO_TRANSFER_MODE_AP, READ_PICO_TRANSFER_MODE_STA };
typedef struct { int state, mode, last_error; unsigned sta_count; bool network_ready; char url[64], ssid[33]; } read_pico_transfer_status_t;
typedef struct {
    bool network_only, is_flash; int mode; const char *root_dir, *font_dir;
    uint64_t (*free_bytes_cb)(void *); void *free_bytes_ctx;
} read_pico_transfer_cfg_t;

typedef struct { char cc[3]; uint8_t schan, nchan; int policy; } wifi_country_t;
typedef struct { int static_rx_buf_num, static_tx_buf_num, dynamic_rx_buf_num, rx_ba_win, cache_tx_buf_num, tx_buf_type, magic; } wifi_init_config_t;
#define WIFI_INIT_CONFIG_DEFAULT() ((wifi_init_config_t){16,16,32,16,32,0,0x12345678})
typedef struct {
    uint8_t *ssid, *bssid; uint8_t channel; bool show_hidden; int scan_type;
    struct { struct { uint32_t min, max; } active; uint32_t passive; } scan_time;
} wifi_scan_config_t;
typedef struct { uint8_t ssid[33]; int8_t rssi; int authmode; } wifi_ap_record_t;
static bool s_wifi, s_started, s_loop_owned, s_stopping, s_upload_active;
static void *s_http, *s_netif, *s_events, *s_ip_events;
static char *s_buffer; static char s_root[160], s_font_root[160];
static int s_lock;
static read_pico_transfer_status_t s_status;
static read_pico_transfer_cfg_t s_cfg;
static transfer_connection_t s_connection;
static int initialized, started, own_loop, external_loop;
static size_t heap_budget, driver_bytes;
static wifi_init_config_t used_config;
#define portENTER_CRITICAL(lock) ((void)(lock))
#define portEXIT_CRITICAL(lock) ((void)(lock))
enum { MALLOC_CAP_INTERNAL=1, MALLOC_CAP_8BIT=2, MALLOC_CAP_DMA=4, MALLOC_CAP_SPIRAM=8 };
static size_t heap_caps_get_free_size(unsigned caps) { (void)caps; return heap_budget - driver_bytes; }
static size_t heap_caps_get_largest_free_block(unsigned caps) { return heap_caps_get_free_size(caps); }
static void read_pico_transfer_stop(void);

static int calls, scans, records, stopped, deinitialized, deleted, cleared;
static int fail_step, fail_code, scenario, pending, record_pos, country_reads;
static wifi_country_t configured_country;
static wifi_scan_config_t scan_config[2];
static wifi_ap_record_t results[32];
static int step(void) { return ++calls == fail_step ? fail_code : ESP_OK; }
static void log_line(const char *tag, const char *format, ...) { (void)tag; (void)format; }
#define ESP_LOGI log_line
#define ESP_LOGW log_line
static const char *esp_err_to_name(int err) { return err ? "error" : "ok"; }
static void read_pico_transfer_get_status(read_pico_transfer_status_t *out) { *out = s_status; }
static int esp_netif_init(void) { return step(); }
static int esp_event_loop_create_default(void) {
    int err = step(); if (err) return err;
    if (external_loop) return ESP_ERR_INVALID_STATE;
    own_loop = 1; return ESP_OK;
}
static int esp_wifi_init(const wifi_init_config_t *config) {
    assert(config && !initialized && config->magic == 0x12345678);
    assert(config->tx_buf_type == 0 && config->dynamic_rx_buf_num >= config->static_rx_buf_num);
    assert(config->rx_ba_win <= config->static_rx_buf_num);
    used_config = *config; int err = step(); if (err) return err;
    size_t demand = (size_t)(config->static_rx_buf_num + config->static_tx_buf_num) * 1600;
    if (demand > heap_budget) return ESP_ERR_NO_MEM;
    driver_bytes = demand; initialized = 1; return ESP_OK;
}
static int esp_wifi_set_storage(int storage) { assert(initialized && storage == WIFI_STORAGE_RAM); return step(); }
static int esp_wifi_set_mode(int mode) { assert(initialized && (mode == WIFI_MODE_STA || mode == WIFI_MODE_AP)); return step(); }
static int esp_wifi_get_country(wifi_country_t *country) {
    ++country_reads; *country = configured_country; return ESP_OK;
}
static int esp_wifi_start(void) {
    assert(initialized && !started); int err = step(); if (!err) started = 1; return err;
}
// 边界模型检查驻留策略：100ms会错过迟至150ms的信标，未设被动时间会丢高信道样本。
// The boundary model checks dwell: 100ms misses a 150ms beacon and unset passive dwell loses high-channel fixtures.
static int esp_wifi_scan_start(const wifi_scan_config_t *scan, bool block) {
    assert(initialized && started && block && scans < 2);
    assert(!scan->ssid && !scan->bssid && !scan->channel && !scan->show_hidden);
    assert(!pending);
    scan_config[scans] = *scan; ++scans;
    int err = step(); if (err) return err;
    record_pos = 0;
    if (scenario == 1) {
        bool dwell = !scan->scan_time.active.min && scan->scan_time.active.max >= 150;
        pending = scan->scan_type == WIFI_SCAN_TYPE_PASSIVE || dwell ? 1 : 0;
    } else if (scenario == 2) {
        pending = scan->scan_time.passive >= 360 ? 1 : 0;
    } else if (scenario == 3) {
        pending = scan->scan_type == WIFI_SCAN_TYPE_PASSIVE ? 1 : 0;
    } else if (scenario == 4) {
        pending = 25;
    } else if (scenario == 5) {
        pending = scans == 1 ? 2 : 0;
    } else pending = 0;
    memset(results, 0, sizeof(results));
    for (int i = 0; i < pending; ++i) {
        snprintf((char *)results[i].ssid, sizeof(results[i].ssid), "AP-%02d", i);
        results[i].rssi = (int8_t)(-90 + i); results[i].authmode = WIFI_AUTH_WPA2_PSK;
    }
    if (scenario == 5) results[0].ssid[0] = results[1].ssid[0] = 0;
    return ESP_OK;
}
static int esp_wifi_scan_get_ap_num(uint16_t *out) { int err = step(); if (!err) *out = (uint16_t)pending; return err; }
static int esp_wifi_scan_get_ap_record(wifi_ap_record_t *out) {
    assert(pending > 0); ++records; int err = step(); if (err) return err;
    *out = results[record_pos++]; --pending; return ESP_OK;
}
static int esp_wifi_scan_stop(void) { assert(started); return ESP_OK; }
static int esp_wifi_clear_ap_list(void) { ++cleared; pending = 0; return step(); }
static int esp_wifi_stop(void) {
    assert(started); ++stopped; int err = step(); if (!err) started = 0; return err;
}
static int esp_wifi_deinit(void) {
    assert(initialized && !started); ++deinitialized; int err = step(); if (!err) { initialized = 0; driver_bytes = 0; } return err;
}
static int esp_event_loop_delete_default(void) {
    assert(own_loop && !external_loop); ++deleted; int err = step(); if (!err) own_loop = 0; return err;
}
''' + scan_body + r'''
static int httpd_stop(void *handle) { assert(handle == s_http); return step(); }
static int esp_event_handler_instance_unregister(int base, int id, void *instance) { (void)base; (void)id; assert(instance); return step(); }
static void esp_netif_destroy_default_wifi(void *netif) { assert(netif == s_netif); }
enum { WIFI_EVENT, IP_EVENT, ESP_EVENT_ANY_ID };
''' + stop_body + r'''
// SDK边界只建模生命周期和内存预算，不模拟真实射频或DMA分配器。
// SDK boundaries model ownership and a memory budget, not actual RF or DMA allocation.
typedef struct {
    struct { uint8_t ssid[33]; char password[65]; size_t ssid_len; int channel, max_connection, authmode; } ap;
    struct { uint8_t ssid[33], password[65]; struct { int authmode; } threshold; struct { bool capable; } pmf_cfg; int sae_pwe_h2e; } sta;
} wifi_config_t;
enum { ESP_MAC_WIFI_SOFTAP, WPA3_SAE_PWE_BOTH, HTTP_GET, HTTP_PUT, HTTP_DELETE, HTTP_POST };
#define READ_PICO_TRANSFER_PASSWORD "readpico"
#define READ_PICO_TRANSFER_URL "http://192.168.4.1"
typedef struct { unsigned stack_size, max_uri_handlers, recv_wait_timeout; bool lru_purge_enable; } httpd_config_t;
#define HTTPD_DEFAULT_CONFIG() ((httpd_config_t){0})
typedef struct { const char *uri; int method; int (*handler)(void *); void *user_ctx; } httpd_uri_t;
static int load_credentials(transfer_credentials_t *saved) {
    *saved = (transfer_credentials_t){.version = 1}; strcpy(saved->ssid, "fixture"); return ESP_OK;
}
static void publish_credentials(const transfer_credentials_t *saved) { (void)saved; }
static int cleanup_interrupted(const char *path, unsigned *removed, unsigned *restored) {
    (void)path; *removed = *restored = 0; return 0;
}
static int netif_tokens[2];
static void *esp_netif_create_default_wifi_sta(void) { return &netif_tokens[0]; }
static void *esp_netif_create_default_wifi_ap(void) { return &netif_tokens[1]; }
static int esp_read_mac(uint8_t mac[6], int type) { (void)type; memset(mac, 0, 6); return step(); }
static void wifi_event(void *arg, int base, int id, void *data) { (void)arg; (void)base; (void)id; (void)data; }
static void ip_event(void *arg, int base, int id, void *data) { wifi_event(arg,base,id,data); }
static int esp_event_handler_instance_register(int base, int id, void (*handler)(void *,int,int,void *), void *arg, void **instance) {
    (void)id; (void)handler; (void)arg; int err = step(); if (!err) *instance = &netif_tokens[base]; return err;
}
static int esp_wifi_set_config(int mode, const wifi_config_t *config) { (void)mode; (void)config; return step(); }
static void *heap_caps_malloc(size_t size, unsigned caps) { assert(caps == (MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)); return malloc(size); }
static int httpd_start(void **server, const httpd_config_t *config) {
    assert(config->stack_size == 12288 && config->max_uri_handlers == 10); int err = step(); if (!err) *server = &netif_tokens[0]; return err;
}
static int httpd_register_uri_handler(void *server, const httpd_uri_t *route) { (void)server; (void)route; return step(); }
static int index_handler(void *req) { (void)req; return ESP_OK; }
#define info_handler index_handler
#define upload_handler index_handler
#define font_info_handler index_handler
#define books_handler index_handler
#define wifi_handler index_handler
static int64_t esp_timer_get_time(void) { return 100000; }
static void read_pico_transfer_service_poll(void) { assert(s_started); }
static void set_error(int err) { s_status.last_error = err; s_status.state = err ? READ_PICO_TRANSFER_ERROR : READ_PICO_TRANSFER_READY; }
''' + start_body + r'''
static void reset(int mode) {
    assert(!started && !initialized && !own_loop && !pending);
    s_wifi = s_started = s_loop_owned = s_stopping = s_upload_active = false;
    s_http = s_netif = s_events = s_ip_events = NULL; s_buffer = NULL;
    memset(&s_status, 0, sizeof(s_status));
    heap_budget = 65536; driver_bytes = 0;
    calls = scans = records = stopped = deinitialized = deleted = cleared = country_reads = 0;
    fail_step = 0; fail_code = ESP_FAIL; scenario = mode; external_loop = 0;
    configured_country = (wifi_country_t){.cc = "01", .schan = 1, .nchan = 11, .policy = WIFI_COUNTRY_POLICY_AUTO};
}
static void clean(void) {
    if (s_wifi || s_started || s_loop_owned) {
        fail_step = 0; assert(read_pico_transfer_try_stop_if_idle());
    }
    assert(!started && !initialized && !own_loop && !pending && !driver_bytes);
    assert(!s_wifi && !s_started && !s_loop_owned);
}
static void successful_scans(void) {
    read_pico_transfer_network_t out[READ_PICO_TRANSFER_SCAN_MAX]; size_t count;
    for (int mode = 0; mode <= 5; ++mode) {
        reset(mode); wifi_country_t original = configured_country;
        assert(read_pico_transfer_scan_wifi(out, &count) == ESP_OK);
        clean(); assert(stopped == 1 && deinitialized == 1 && deleted == 1 && country_reads == 1);
        assert(!memcmp(&configured_country, &original, sizeof(original)));
        assert(scans == (mode == 0 || mode == 3 || mode == 5 ? 2 : 1));
        assert(scan_config[0].scan_type == WIFI_SCAN_TYPE_ACTIVE);
        if (scans == 2) assert(scan_config[1].scan_type == WIFI_SCAN_TYPE_PASSIVE);
        assert(count == (mode == 0 || mode == 5 ? 0 : mode == 4 ? 16 : 1));
        if (mode == 4) {
            assert(records == 25 && !strcmp(out[0].ssid, "AP-24"));
            for (size_t i = 1; i < count; ++i) assert(out[i-1].rssi >= out[i].rssi);
        }
    }
    reset(2); configured_country = (wifi_country_t){.cc = "US", .schan = 1, .nchan = 11, .policy = WIFI_COUNTRY_POLICY_MANUAL};
    wifi_country_t original = configured_country;
    assert(read_pico_transfer_scan_wifi(out, &count) == ESP_OK);
    assert(!memcmp(&configured_country, &original, sizeof(original))); clean();
    reset(1); external_loop = 1;
    assert(read_pico_transfer_scan_wifi(out, &count) == ESP_OK);
    clean(); assert(!deleted && external_loop); external_loop = 0;
    puts("Scan: complete active dwell, AUTO high-channel passive dwell, one empty passive retry, bounded sorting and unchanged country PASS");
}
static void failures(void) {
    read_pico_transfer_network_t out[READ_PICO_TRANSFER_SCAN_MAX]; size_t count;
    for (int mode = 0; mode <= 4; ++mode) {
        reset(mode); assert(read_pico_transfer_scan_wifi(out, &count) == ESP_OK); int total_calls = calls;
        for (int fault = 1; fault <= total_calls; ++fault) {
            reset(mode); fail_step = fault;
            int result = read_pico_transfer_scan_wifi(out, &count);
            // 最终清空列表是幂等清理；其错误不掩盖已经取出的成功结果。
            // Final AP-list clearing is idempotent cleanup; its error does not hide a completed result.
            if (result != ESP_OK) {
                assert(!count);
                for (size_t i = 0; i < sizeof(out); ++i) assert(!((uint8_t *)out)[i]);
            }
            clean();
        }
    }
    reset(1); fail_step = 7; fail_code = ESP_ERR_WIFI_TIMEOUT;
    assert(read_pico_transfer_scan_wifi(out, &count) == ESP_ERR_WIFI_TIMEOUT && !count && scans == 1);
    clean();
    reset(1); s_status.state = READ_PICO_TRANSFER_READY;
    assert(read_pico_transfer_scan_wifi(out, &count) == ESP_ERR_INVALID_STATE && !count && !calls);
    reset(1); s_wifi = true;
    assert(read_pico_transfer_scan_wifi(out, &count) == ESP_ERR_INVALID_STATE && !count && !calls);
    s_wifi = false; s_http = out;
    assert(read_pico_transfer_scan_wifi(out, &count) == ESP_ERR_INVALID_STATE && !calls);
    s_http = NULL; s_netif = out;
    assert(read_pico_transfer_scan_wifi(out, &count) == ESP_ERR_INVALID_STATE && !calls);
    s_netif = NULL;
    assert(read_pico_transfer_scan_wifi(NULL, &count) == ESP_ERR_INVALID_ARG && !calls);
    assert(read_pico_transfer_scan_wifi(out, NULL) == ESP_ERR_INVALID_ARG && !calls);
    puts("Lifecycle: all SDK failures, partial lists, timeout, no duplicate scan after error, foreign loop and active-service exclusion PASS");
}
static void low_memory_scans(void) {
    read_pico_transfer_network_t out[READ_PICO_TRANSFER_SCAN_MAX]; size_t count;
    reset(1); heap_budget = 32000;
    wifi_init_config_t full = wifi_init_for_session(false);
    assert(esp_wifi_init(&full) == ESP_ERR_NO_MEM && !initialized);
    assert(read_pico_transfer_scan_wifi(out, &count) == ESP_OK && count == 1);
    assert(used_config.static_rx_buf_num == 6 && used_config.static_tx_buf_num == 4);
    assert(used_config.dynamic_rx_buf_num == 12 && used_config.rx_ba_win == 6 && used_config.cache_tx_buf_num == 4);
    clean();
    puts("Low-memory boundary: default 16+16 buffers fail, scan 6+4 succeeds and returns memory PASS");
}

static uint64_t free_bytes(void *ctx) { (void)ctx; return 100000000; }
static void lock_network_then_scan(void) {
    read_pico_transfer_network_t out[READ_PICO_TRANSFER_SCAN_MAX]; size_t count;
    for (int iteration = 0; iteration < 200; ++iteration) {
        reset(1); heap_budget = 32000;
        assert(read_pico_transfer_start_saved_network() == ESP_OK);
        assert(s_started && s_wifi && s_netif && own_loop && s_loop_owned && driver_bytes == 16000);
        assert(!s_http && !s_buffer && used_config.static_rx_buf_num == 6 && used_config.static_tx_buf_num == 4);
        assert(read_pico_transfer_start_saved_network() == ESP_ERR_INVALID_STATE);
        read_pico_transfer_stop(); clean();
        assert(s_status.state == READ_PICO_TRANSFER_STOPPED && !s_netif && !s_events && !s_ip_events);
        assert(read_pico_transfer_scan_wifi(out, &count) == ESP_OK && count == 1); clean();
    }
    reset(1);
    const read_pico_transfer_cfg_t full = {.mode=READ_PICO_TRANSFER_MODE_AP, .root_dir="/tmp", .free_bytes_cb=free_bytes};
    assert(read_pico_transfer_start(&full) == ESP_OK);
    wifi_init_config_t defaults = WIFI_INIT_CONFIG_DEFAULT();
    assert(!memcmp(&used_config, &defaults, sizeof(defaults)) && s_http && s_buffer);
    assert(read_pico_transfer_try_stop_if_idle()); clean();
    const read_pico_transfer_cfg_t full_sta = {.mode=READ_PICO_TRANSFER_MODE_STA, .root_dir="/tmp", .free_bytes_cb=free_bytes};
    assert(read_pico_transfer_start(&full_sta) == ESP_OK);
    assert(!memcmp(&used_config, &defaults, sizeof(defaults)) && s_http && s_buffer);
    assert(read_pico_transfer_try_stop_if_idle()); clean();
    reset(1); assert(read_pico_transfer_start_saved_network() == ESP_OK); int total_calls = calls;
    read_pico_transfer_stop(); clean();
    for (int fault = 1; fault <= total_calls; ++fault) {
        reset(1); fail_step = fault;
        int err = read_pico_transfer_start_saved_network();
        if (err != ESP_OK) assert(s_status.state == READ_PICO_TRANSFER_ERROR);
        fail_step = 0; read_pico_transfer_stop(); clean();
        assert(!s_netif && !s_events && !s_ip_events && !s_http && !s_buffer);
    }
    puts("Real network-only start/stop -> scan: 200 low-memory cycles, SDK startup faults and unchanged full AP buffers PASS");
}
static void failed_release_retains_ownership(void) {
    read_pico_transfer_network_t out[READ_PICO_TRANSFER_SCAN_MAX]; size_t count;
    for (int fault = 1; fault <= 3; ++fault) {
        reset(1); assert(read_pico_transfer_start_saved_network() == ESP_OK);
        // stop边界先清AP列表，再停驱动，再deinit；每一步只注入一次失败。
        // Stop clears the AP list, stops the driver, then deinitializes; inject one failure at each boundary.
        fail_step = calls + fault; fail_code = ESP_ERR_NO_MEM;
        read_pico_transfer_stop();
        if (fault > 1) {
            assert(s_status.state == READ_PICO_TRANSFER_ERROR && s_status.last_error == ESP_ERR_NO_MEM);
            assert(s_stopping && s_wifi && s_loop_owned && s_netif && initialized && driver_bytes);
            assert(s_started == (fault == 2));
            assert(read_pico_transfer_scan_wifi(out, &count) == ESP_ERR_INVALID_STATE);
        }
        fail_step = 0; assert(read_pico_transfer_try_stop_if_idle()); clean();
        assert(read_pico_transfer_scan_wifi(out, &count) == ESP_OK && count == 1); clean();
    }
    reset(1); assert(read_pico_transfer_start_saved_network() == ESP_OK);
    fail_step = calls + 6; fail_code = ESP_ERR_NO_MEM; read_pico_transfer_stop();
    assert(!s_wifi && !s_started && s_loop_owned && own_loop && !driver_bytes);
    assert(s_status.state == READ_PICO_TRANSFER_ERROR && s_stopping);
    assert(read_pico_transfer_scan_wifi(out, &count) == ESP_ERR_INVALID_STATE);
    fail_step = 0; assert(read_pico_transfer_try_stop_if_idle()); clean();
    reset(1);
    const read_pico_transfer_cfg_t full = {.mode=READ_PICO_TRANSFER_MODE_AP, .root_dir="/tmp", .free_bytes_cb=free_bytes};
    assert(read_pico_transfer_start(&full) == ESP_OK);
    fail_step = calls + 1; fail_code = ESP_ERR_NO_MEM;
    assert(!read_pico_transfer_try_stop_if_idle() && s_http && s_wifi && s_started && s_netif && s_loop_owned);
    assert(s_status.state == READ_PICO_TRANSFER_ERROR && s_stopping);
    fail_step = 0; assert(read_pico_transfer_try_stop_if_idle()); clean();
    puts("Failed HTTP/stop/deinit/event deletion retains ownership; later stop retries before another scan PASS");
}
int main(void) {
    successful_scans(); failures(); low_memory_scans(); lock_network_then_scan(); failed_release_retains_ownership();
    puts("Production WiFi scan lifecycle PASS");
}
'''
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--verify-regression", action="store_true")
args = parser.parse_args()
flags = ["-std=gnu11", "-Wall", "-Wextra", "-Werror", "-Wno-unused-function", "-fsanitize=address,undefined"]
if os.uname().sysname == "Darwin":
    flags += ["-isysroot", "/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk"]
with tempfile.TemporaryDirectory(prefix="pico-wifi-scan-", dir="/tmp") as directory:
    temp = Path(directory); test = temp / "test.c"; test.write_text(harness)
    command = [os.environ.get("CC", "cc"), *flags, "-Icomponents/read_pico_transfer",
               "-Imanaged_components/espressif__cjson/cJSON", str(test), "-o", str(temp / "test")]
    subprocess.run(command, cwd=ROOT, check=True); subprocess.run([str(temp / "test")], check=True)
    if args.verify_regression:
        fixed = ".active = {.min = 0, .max = 200}, .passive = 360"
        assert harness.count(fixed) == 1
        test.write_text(harness.replace(fixed, ".active = {.min = 100, .max = 200}, .passive = 0"))
        subprocess.run(command, cwd=ROOT, check=True)
        result = subprocess.run([str(temp / "test")], capture_output=True, text=True)
        assert result.returncode != 0 and "scans ==" in result.stderr
        print("Former short dwell misses delayed beacon fixture (negative control) PASS")
        controls = [
            ("wifi_init_for_session(true)", "wifi_init_for_session(false)", "low_memory_scans", "Former scan buffers exhaust constrained DMA budget"),
            ("wifi_init_for_session(cfg->network_only)", "wifi_init_for_session(false)", "lock_network_then_scan", "Former lock-sync buffers exhaust constrained DMA budget"),
        ]
        for fixed_config, old_config, check, description in controls:
            assert harness.count(fixed_config) == 1
            controlled = harness.replace(fixed_config, old_config)
            controlled = controlled[:controlled.rindex("int main(void) {")] + f"int main(void) {{ {check}(); }}\n"
            test.write_text(controlled); subprocess.run(command, cwd=ROOT, check=True)
            result = subprocess.run([str(temp / "test")], capture_output=True, text=True)
            assert result.returncode != 0 and "== ESP_OK" in result.stderr
            print(description + " (negative control) PASS")
        fixed_release = "if (err != ESP_OK && err != ESP_ERR_WIFI_NOT_INIT) return err;\n        *initialized = false;"
        old_release = "*initialized = false;\n        if (err != ESP_OK && err != ESP_ERR_WIFI_NOT_INIT) return err;"
        assert harness.count(fixed_release) == 1
        controlled = harness.replace(fixed_release, old_release)
        controlled = controlled[:controlled.rindex("int main(void) {")] + "int main(void) { failed_release_retains_ownership(); }\n"
        test.write_text(controlled); subprocess.run(command, cwd=ROOT, check=True)
        result = subprocess.run([str(temp / "test")], capture_output=True, text=True)
        assert result.returncode != 0 and "s_stopping && s_wifi" in result.stderr
        print("Clearing ownership before failed deinit loses allocated driver (negative control) PASS")

    ui_source = (ROOT / "tools/transfer_ui_host_test.c").read_text()
    app_source = (ROOT / "main/apps/app_transfer.c").read_text()
    ui_app = temp / "app_transfer.c"; ui_app.write_text(app_source)
    ui_test = temp / "ui.c"
    ui_test.write_text(ui_source.replace('#include "../main/apps/app_transfer.c"', f'#include "{ui_app}"'))
    ui_command = [os.environ.get("CC", "cc"), *flags, "-Itools/transfer_ui_stubs", "-Icomponents/read_pico_transfer/include",
                  "-Imain/os", "-Imain/ui/product", "-Imain/book", "-Imain/app", "-Imain", str(ui_test), "-o", str(temp / "ui")]
    subprocess.run(ui_command, cwd=ROOT, check=True); subprocess.run([str(temp / "ui")], check=True)
    if args.verify_regression:
        fixed_ui = '"扫描失败：%s", esp_err_to_name(err)'
        assert app_source.count(fixed_ui) == 1
        ui_app.write_text(app_source.replace(fixed_ui, '"扫描失败，请点重新扫描"'))
        subprocess.run(ui_command, cwd=ROOT, check=True)
        result = subprocess.run([str(temp / "ui")], capture_output=True, text=True)
        assert result.returncode != 0 and "strstr(s_network_message" in result.stderr
        print("Former scan failure UI hides actual SDK error (negative control) PASS")
