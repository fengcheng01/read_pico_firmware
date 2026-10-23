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
scan_body = source[source.index("static void wifi_minimal_buffers("):source.index("esp_err_t read_pico_transfer_forget_wifi(")]
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
       ESP_ERR_NO_MEM, ESP_ERR_WIFI_NOT_INIT, ESP_ERR_WIFI_NOT_STARTED, ESP_ERR_NOT_FOUND,
       ESP_ERR_HTTPD_TASK };
enum { WIFI_STORAGE_RAM, WIFI_MODE_STA, WIFI_MODE_AP, WIFI_IF_STA, WIFI_IF_AP, WIFI_SCAN_TYPE_ACTIVE, WIFI_SCAN_TYPE_PASSIVE };
enum { WIFI_AUTH_OPEN, WIFI_AUTH_WEP, WIFI_AUTH_WPA_PSK, WIFI_AUTH_WPA2_PSK,
       WIFI_AUTH_WPA_WPA2_PSK, WIFI_AUTH_WPA3_PSK, WIFI_AUTH_WPA2_WPA3_PSK, WIFI_AUTH_OWE };
enum { WIFI_COUNTRY_POLICY_AUTO, WIFI_COUNTRY_POLICY_MANUAL };
enum { WPA3_SAE_PWE_BOTH };
enum { READ_PICO_TRANSFER_STOPPED, READ_PICO_TRANSFER_STARTING, READ_PICO_TRANSFER_READY, READ_PICO_TRANSFER_ERROR };
enum { READ_PICO_TRANSFER_MODE_AP, READ_PICO_TRANSFER_MODE_STA };
typedef struct { int state, mode, last_error; unsigned sta_count; bool wifi_associated, network_ready; char url[64], ssid[33]; } read_pico_transfer_status_t;
typedef struct {
    bool network_only, is_flash, use_selected_wifi; int mode; const char *root_dir, *font_dir, *image_dir;
    uint64_t (*free_bytes_cb)(void *); void *free_bytes_ctx;
} read_pico_transfer_cfg_t;

typedef struct { char cc[3]; uint8_t schan, nchan; int policy; } wifi_country_t;
typedef struct { int static_rx_buf_num, static_tx_buf_num, dynamic_rx_buf_num, rx_ba_win, cache_tx_buf_num, tx_buf_type, mgmt_sbuf_num, magic; } wifi_init_config_t;
#define WIFI_INIT_CONFIG_DEFAULT() ((wifi_init_config_t){16,16,32,16,32,0,32,0x12345678})
typedef struct {
    uint8_t *ssid, *bssid; uint8_t channel; bool show_hidden; int scan_type;
    struct { struct { uint32_t min, max; } active; uint32_t passive; } scan_time;
} wifi_scan_config_t;
typedef struct { uint8_t ssid[33]; int8_t rssi; int authmode; } wifi_ap_record_t;
typedef struct {
    struct { uint8_t ssid[33]; char password[65]; size_t ssid_len; int channel, max_connection, authmode; } ap;
    struct { uint8_t ssid[33], password[65]; struct { int authmode; } threshold; struct { bool capable; } pmf_cfg; int sae_pwe_h2e; } sta;
} wifi_config_t;
static bool s_wifi, s_started, s_loop_owned, s_stopping, s_upload_active;
static void *s_http, *s_netif, *s_events, *s_ip_events;
static char *s_buffer; static char s_root[160], s_font_root[160], s_image_root[160];
static int s_lock;
static read_pico_transfer_status_t s_status;
static read_pico_transfer_cfg_t s_cfg;
static transfer_connection_t s_connection;
static esp_err_t s_sta_connect_error;
static char s_candidate_ssids[TRANSFER_WIFI_PROFILE_MAX][33];
static uint8_t s_candidate_count, s_candidate_index;
static bool s_profile_promotion_attempted;
static char configured_sta_ssid[33];
static int initialized, started, own_loop, external_loop;
static size_t heap_budget, driver_bytes, driver_fixed_bytes, largest_limit, dma_free_limit, dma_largest_limit;
static size_t http_bytes, dma_unavailable;
static unsigned http_starts, http_stops, http_small_starts, http_routes;
static unsigned fail_http_stack, fail_route;
static bool fail_http_stop, fail_upload_buffer;
static bool enforce_start_reserve;
static unsigned start_calls;
static wifi_init_config_t used_config;
#define portENTER_CRITICAL(lock) ((void)(lock))
#define portEXIT_CRITICAL(lock) ((void)(lock))
enum { MALLOC_CAP_INTERNAL=1, MALLOC_CAP_8BIT=2, MALLOC_CAP_DMA=4, MALLOC_CAP_SPIRAM=8 };
static size_t heap_caps_get_free_size(unsigned caps) {
    assert(heap_budget >= driver_bytes + http_bytes);
    size_t free = heap_budget - driver_bytes - http_bytes;
    if (caps & MALLOC_CAP_DMA) free = free > dma_unavailable ? free - dma_unavailable : 0;
    return (caps & MALLOC_CAP_DMA) && dma_free_limit && dma_free_limit < free ? dma_free_limit : free;
}
static size_t heap_caps_get_largest_free_block(unsigned caps) {
    size_t free = heap_caps_get_free_size(caps);
    if ((caps & MALLOC_CAP_DMA) && dma_largest_limit && dma_largest_limit < free) free = dma_largest_limit;
    return largest_limit && largest_limit < free ? largest_limit : free;
}
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
static int load_credentials_store(transfer_credentials_store_t *store) {
    memset(store, 0, sizeof(*store)); store->version = TRANSFER_CREDENTIALS_STORE_VERSION; store->count = 2;
    store->profiles[0].version = store->profiles[1].version = 1;
    strcpy(store->profiles[0].ssid, "fixture"); strcpy(store->profiles[0].password, "12345678");
    strcpy(store->profiles[1].ssid, "AP-00"); strcpy(store->profiles[1].password, "abcdefgh");
    return ESP_OK;
}
static void publish_credentials(const transfer_credentials_t *saved) { (void)saved; }
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
    size_t demand = (size_t)(config->static_rx_buf_num + config->static_tx_buf_num) * 1600 + driver_fixed_bytes;
    if (demand > heap_budget - http_bytes) return ESP_ERR_NO_MEM;
    driver_bytes = demand; initialized = 1; return ESP_OK;
}
static int esp_wifi_set_storage(int storage) { assert(initialized && storage == WIFI_STORAGE_RAM); return step(); }
static int esp_wifi_set_mode(int mode) { assert(initialized && (mode == WIFI_MODE_STA || mode == WIFI_MODE_AP)); return step(); }
static int esp_wifi_get_country(wifi_country_t *country) {
    ++country_reads; *country = configured_country; return ESP_OK;
}
enum { WIFI_PS_NONE };
static int esp_wifi_set_ps(int mode) {assert(initialized&&started&&mode==WIFI_PS_NONE);return step();}
static int esp_wifi_start(void) {
    ++start_calls;
    if (enforce_start_reserve) {
        assert(heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT) >= 16384);
        assert(heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT) >= 4096);
    }
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
    } else if (scenario == 6) {
        pending = 4;
    } else pending = 0;
    memset(results, 0, sizeof(results));
    for (int i = 0; i < pending; ++i) {
        snprintf((char *)results[i].ssid, sizeof(results[i].ssid), "AP-%02d", i);
        results[i].rssi = (int8_t)(-90 + i); results[i].authmode = WIFI_AUTH_WPA2_PSK;
    }
    if (scenario == 5) results[0].ssid[0] = results[1].ssid[0] = 0;
    if (scenario == 6) {
        strcpy((char*)results[0].ssid,"fixture");results[0].rssi=-80;
        strcpy((char*)results[1].ssid,"AP-00");results[1].rssi=-30;
        strcpy((char*)results[2].ssid,"fixture");results[2].rssi=-70;
        strcpy((char*)results[3].ssid,"AP-00");results[3].rssi=-10;results[3].authmode=WIFI_AUTH_OPEN;
    }
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
    assert(initialized && !started); ++deinitialized; int err = step();
    // 组件把deinit的INVALID_STATE按已释放处理：模型同步回收内存，检验该契约不再闩锁。
    // The component treats deinit INVALID_STATE as released; the model reclaims memory in step so the contract stays unlatched.
    if (!err || err == ESP_ERR_INVALID_STATE) { initialized = 0; driver_bytes = 0; }
    return err;
}
static int esp_event_loop_delete_default(void) {
    assert(own_loop && !external_loop); ++deleted; int err = step();
    // IDF语义：INVALID_STATE表示默认循环已不存在，视为已删除。
    // IDF semantics: INVALID_STATE means the default loop no longer exists; treat it as deleted.
    if (!err || err == ESP_ERR_INVALID_STATE) own_loop = 0;
    return err;
}
''' + scan_body + r'''
static int httpd_stop(void *handle) {
    assert(handle == s_http && http_bytes); ++http_stops;
    int err=step(); if(err || fail_http_stop)return err?err:ESP_FAIL;
    http_bytes=0;return ESP_OK;
}
static int esp_event_handler_instance_unregister(int base, int id, void *instance) { (void)base; (void)id; assert(instance); return step(); }
// httpd 边界供 stop_body 内的启动辅助使用，必须在切片前声明。
// / The httpd boundary feeds the start helper inside stop_body, so it must precede the slice.
enum { ESP_MAC_WIFI_SOFTAP, HTTP_GET, HTTP_PUT, HTTP_DELETE, HTTP_POST };
typedef struct { unsigned stack_size, max_uri_handlers, recv_wait_timeout; bool lru_purge_enable; } httpd_config_t;
#define HTTPD_DEFAULT_CONFIG() ((httpd_config_t){0})
typedef struct { const char *uri; int method; int (*handler)(void *); void *user_ctx; } httpd_uri_t;
static int netif_tokens[2];
static int httpd_start(void **server, const httpd_config_t *config) {
    assert(!http_bytes && (config->stack_size==12288 || config->stack_size==8192) && config->max_uri_handlers==15);
    ++http_starts;if(config->stack_size==8192)++http_small_starts;
    int err=step();if(err)return err;
    if(config->stack_size==fail_http_stack || config->stack_size>heap_budget-driver_bytes)return ESP_ERR_HTTPD_TASK;
    http_bytes=config->stack_size;*server=&netif_tokens[0];return ESP_OK;
}
static int httpd_register_uri_handler(void *server, const httpd_uri_t *route) {
    (void)server;(void)route;++http_routes;int err=step();return err?err:http_routes==fail_route?ESP_FAIL:ESP_OK;
}
static int index_handler(void *req) { (void)req; return ESP_OK; }
#define info_handler index_handler
#define upload_handler index_handler
#define upload_check_handler index_handler
#define font_info_handler index_handler
#define books_handler index_handler
#define fs_handler index_handler
#define wifi_handler index_handler
#define kind_delete_handler index_handler
static void esp_netif_destroy_default_wifi(void *netif) { assert(netif == s_netif); }
enum { WIFI_EVENT, IP_EVENT, ESP_EVENT_ANY_ID };
''' + stop_body + r'''
// SDK边界只建模生命周期和内存预算，不模拟真实射频或DMA分配器。
// SDK boundaries model ownership and a memory budget, not actual RF or DMA allocation.
#define READ_PICO_TRANSFER_PASSWORD "readpico"
#define READ_PICO_TRANSFER_URL "http://192.168.4.1"
static int cleanup_interrupted(const char *path, unsigned *removed, unsigned *restored) {
    (void)path; *removed = *restored = 0; return 0;
}
static void *esp_netif_create_default_wifi_sta(void) { return &netif_tokens[0]; }
static void *esp_netif_create_default_wifi_ap(void) { return &netif_tokens[1]; }
static int esp_read_mac(uint8_t mac[6], int type) { (void)type; memset(mac, 0, 6); return step(); }
static void wifi_event(void *arg, int base, int id, void *data) { (void)arg; (void)base; (void)id; (void)data; }
static void ip_event(void *arg, int base, int id, void *data) { wifi_event(arg,base,id,data); }
static int esp_event_handler_instance_register(int base, int id, void (*handler)(void *,int,int,void *), void *arg, void **instance) {
    (void)id; (void)handler; (void)arg; int err = step(); if (!err) *instance = &netif_tokens[base]; return err;
}
static int esp_wifi_set_config(int mode, const wifi_config_t *config) {
    if (mode == WIFI_IF_STA) snprintf(configured_sta_ssid, sizeof(configured_sta_ssid), "%s", config->sta.ssid);
    return step();
}
static void *heap_caps_malloc(size_t size, unsigned caps) { assert(caps == (MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)); return fail_upload_buffer?NULL:malloc(size); }
static int64_t esp_timer_get_time(void) { return 100000; }
static void read_pico_transfer_service_poll(void) { assert(s_started); }
static void set_error(int err) { s_status.last_error = err; s_status.state = err ? READ_PICO_TRANSFER_ERROR : READ_PICO_TRANSFER_READY; }
''' + start_body + r'''
static void reset(int mode) {
    assert(!started && !initialized && !own_loop && !pending && !http_bytes);
    s_wifi = s_started = s_loop_owned = s_stopping = s_upload_active = false;
    s_http = s_netif = s_events = s_ip_events = NULL; s_buffer = NULL;
    memset(&s_status, 0, sizeof(s_status));
    s_sta_connect_error = ESP_OK;
    memset(configured_sta_ssid, 0, sizeof(configured_sta_ssid));
    heap_budget = 90000; driver_bytes = 0;
    driver_fixed_bytes = largest_limit = dma_free_limit = dma_largest_limit = 0;
    dma_unavailable=0;http_starts=http_stops=http_small_starts=http_routes=0;
    fail_http_stack=fail_route=0;fail_http_stop=fail_upload_buffer=false;
    enforce_start_reserve = false; start_calls = 0;
    calls = scans = records = stopped = deinitialized = deleted = cleared = country_reads = 0;
    fail_step = 0; fail_code = ESP_FAIL; scenario = mode; external_loop = 0;
    configured_country = (wifi_country_t){.cc = "01", .schan = 1, .nchan = 11, .policy = WIFI_COUNTRY_POLICY_AUTO};
}
static void clean(void) {
    if (s_wifi || s_started || s_loop_owned) {
        fail_step = 0; assert(read_pico_transfer_try_stop_if_idle());
    }
    assert(!started && !initialized && !own_loop && !pending && !driver_bytes && !http_bytes);
    assert(!s_wifi && !s_started && !s_loop_owned && !s_http && !s_buffer && !s_netif);
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
    wifi_init_config_t full = WIFI_INIT_CONFIG_DEFAULT();
    assert(esp_wifi_init(&full) == ESP_ERR_NO_MEM && !initialized);
    assert(read_pico_transfer_scan_wifi(out, &count) == ESP_OK && count == 1);
    assert(used_config.static_rx_buf_num == 4 && used_config.static_tx_buf_num == 3);
    assert(used_config.dynamic_rx_buf_num == 8 && used_config.rx_ba_win == 4 && used_config.cache_tx_buf_num == 2 && used_config.mgmt_sbuf_num == 6);
    clean();
    puts("Low-memory boundary: default 16+16 buffers fail, scan 4+3 succeeds and returns memory PASS");
}

static uint64_t free_bytes(void *ctx) { (void)ctx; return 100000000; }
static void lock_network_then_scan(void) {
    read_pico_transfer_network_t out[READ_PICO_TRANSFER_SCAN_MAX]; size_t count;
    for (int iteration = 0; iteration < 200; ++iteration) {
        reset(1); heap_budget = 32000;
        assert(read_pico_transfer_start_saved_network() == ESP_OK);
        assert(!strcmp(configured_sta_ssid, "AP-00") && s_candidate_count == 1);
        assert(s_started && s_wifi && s_netif && own_loop && s_loop_owned && driver_bytes == 11200);
        assert(!s_http && !s_buffer && used_config.static_rx_buf_num == 4 && used_config.static_tx_buf_num == 3);
        assert(read_pico_transfer_start_saved_network() == ESP_ERR_INVALID_STATE);
        read_pico_transfer_stop(); clean();
        assert(s_status.state == READ_PICO_TRANSFER_STOPPED && !s_netif && !s_events && !s_ip_events);
        assert(read_pico_transfer_scan_wifi(out, &count) == ESP_OK && count == 1); clean();
    }
    reset(1);
    const read_pico_transfer_cfg_t full = {.mode=READ_PICO_TRANSFER_MODE_AP, .root_dir="/tmp", .free_bytes_cb=free_bytes};
    wifi_init_config_t defaults = wifi_init_for_session(false);
    assert(defaults.static_rx_buf_num==8&&defaults.static_tx_buf_num==6&&defaults.mgmt_sbuf_num==6);
    assert(read_pico_transfer_start(&full) == ESP_OK);
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
static void automatic_candidates(void) {
    read_pico_transfer_cfg_t cfg={.network_only=true,.mode=READ_PICO_TRANSFER_MODE_STA};
    reset(6);assert(read_pico_transfer_start(&cfg)==ESP_OK);
    assert(!strcmp(configured_sta_ssid,"AP-00")&&s_candidate_count==2);
    assert(!strcmp(s_candidate_ssids[1],"fixture"));read_pico_transfer_stop();clean();
    reset(0);assert(read_pico_transfer_start(&cfg)==ESP_ERR_NOT_FOUND);clean();
    reset(6);cfg.use_selected_wifi=true;assert(read_pico_transfer_start(&cfg)==ESP_OK);
    assert(!strcmp(configured_sta_ssid,"fixture")&&s_candidate_count==1&&!scans);
    read_pico_transfer_stop();clean();
    puts("WiFi candidates: RSSI/auth filtering, duplicate APs, absent-network refusal and explicit choice PASS");
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
static void retained_latch_start_self_heals(void) {
    // 停止失败留下的stopping残留：下一次start必须先重试停止再发起连接，保存新密码不能永远停在“已保存，尚未连接”。
    // Residue with stopping held from a failed stop: the next start must retry the stop before
    // connecting, so a freshly saved password never rests on saved-but-not-connected forever.
    reset(1);
    assert(read_pico_transfer_start_saved_network() == ESP_OK);
    fail_step = calls + 3; fail_code = ESP_ERR_NO_MEM;
    read_pico_transfer_stop();
    assert(s_status.state == READ_PICO_TRANSFER_ERROR && s_stopping && s_wifi && driver_bytes);
    fail_step = 0;
    assert(read_pico_transfer_start_saved_network() == ESP_OK);
    assert(s_started && s_wifi && !s_stopping && s_status.state == READ_PICO_TRANSFER_STARTING);
    read_pico_transfer_stop(); clean();
    // 重试停止仍失败时维持原INVALID_STATE拒绝并保留残留待下次再试。
    // When the retried stop fails again, keep the original INVALID_STATE rejection and residue for a later retry.
    reset(1);
    assert(read_pico_transfer_start_saved_network() == ESP_OK);
    int failed_deinit = calls + 3;
    fail_step = failed_deinit; fail_code = ESP_ERR_NO_MEM; read_pico_transfer_stop();
    fail_step = failed_deinit + 1; fail_code = ESP_ERR_NO_MEM;
    assert(read_pico_transfer_start_saved_network() == ESP_ERR_INVALID_STATE);
    assert(s_stopping && s_wifi && s_status.state == READ_PICO_TRANSFER_ERROR);
    fail_step = 0; assert(read_pico_transfer_try_stop_if_idle()); clean();
    // 正在运行的会话不被自愈拆掉：仍返回INVALID_STATE。
    // A running session is never torn down by self-healing: it still returns INVALID_STATE.
    reset(1);
    assert(read_pico_transfer_start_saved_network() == ESP_OK);
    assert(read_pico_transfer_start_saved_network() == ESP_ERR_INVALID_STATE && s_started);
    read_pico_transfer_stop(); clean();
    puts("Retained-teardown latch: start retries stop once and connects, persistent failure keeps residue, running sessions keep rejection PASS");
}
static void already_gone_release_tolerated(void) {
    // IDF对已删除对象返回INVALID_STATE：视作已释放，停止成功，不再形成闩锁。
    // IDF returns INVALID_STATE for already-deleted objects: treat them as released so stop succeeds without latching.
    reset(1);
    assert(read_pico_transfer_start_saved_network() == ESP_OK);
    fail_step = calls + 3; fail_code = ESP_ERR_INVALID_STATE; read_pico_transfer_stop();
    assert(s_status.state == READ_PICO_TRANSFER_STOPPED && !s_wifi && !s_stopping && !driver_bytes);
    clean();
    reset(1);
    assert(read_pico_transfer_start_saved_network() == ESP_OK);
    fail_step = calls + 5; fail_code = ESP_ERR_INVALID_STATE; read_pico_transfer_stop();
    assert(s_status.state == READ_PICO_TRANSFER_STOPPED && !s_loop_owned && !own_loop);
    clean();
    puts("Already-gone INVALID_STATE from deinit/event deletion clears ownership without latching PASS");
}
static void full_session_low_memory_clamps(void) {
    // 全量会话16+16拿不下时回退小缓冲重试一次：配网连接优先于传书吞吐。
    // When the full 16+16 set cannot be allocated, fall back once to the smaller buffers: provisioning beats throughput.
    reset(1); heap_budget = 40000;
    const read_pico_transfer_cfg_t full = {.mode=READ_PICO_TRANSFER_MODE_STA, .root_dir="/tmp", .free_bytes_cb=free_bytes};
    assert(read_pico_transfer_start(&full) == ESP_OK);
    assert(used_config.static_rx_buf_num == 2 && used_config.static_tx_buf_num == 2);
    assert(s_started && s_http && s_buffer && !s_stopping);
    assert(read_pico_transfer_try_stop_if_idle()); clean();
    puts("Full-session NO_MEM falls back to clamped buffers so provisioning still connects PASS");
}
static void ap_beacon_headroom(void) {
    const read_pico_transfer_cfg_t ap = {.mode=READ_PICO_TRANSFER_MODE_AP, .root_dir="/tmp", .free_bytes_cb=free_bytes};
    // 现场初始化前46071B；注入额外驱动分配，证明init成功也必须检查start余量。
    // The field trace has 46071 bytes before init; inject additional driver allocations to distinguish successful init from safe start headroom.
    reset(1); heap_budget=46071; driver_fixed_bytes=12000; enforce_start_reserve=true;
    assert(read_pico_transfer_start(&ap)==ESP_OK && start_calls==1);
    assert(used_config.static_rx_buf_num==2 && used_config.static_tx_buf_num==2);
    read_pico_transfer_stop(); clean();
    // 充足预算同时包含HTTP任务栈，往返200次仍保留高吞吐配置和启动保护。
    // A roomy budget includes the HTTP stack; retain full-throughput buffers and startup guards across200 cycles.
    reset(1); heap_budget=90000; driver_fixed_bytes=12000; enforce_start_reserve=true;
    for (unsigned cycle=0; cycle<200; ++cycle) {
        assert(read_pico_transfer_start(&ap)==ESP_OK);
        assert(used_config.static_rx_buf_num==8 && used_config.static_tx_buf_num==6);
        read_pico_transfer_stop(); clean();
    }
    // 最小配置仍不足或堆碎片化时不进入驱动启动；释放后可直接再试。
    // Insufficient headroom even with minimal buffers or fragmentation never enters driver start; teardown allows another attempt.
    reset(1); heap_budget=46071; driver_fixed_bytes=24000; enforce_start_reserve=true;
    assert(read_pico_transfer_start(&ap)==ESP_ERR_NO_MEM && !start_calls);
    assert(s_status.state==READ_PICO_TRANSFER_ERROR && !s_http && !s_netif); clean();
    driver_fixed_bytes=0;
    assert(read_pico_transfer_start(&ap)==ESP_OK && start_calls==1);
    read_pico_transfer_stop(); clean();
    reset(1); largest_limit=4095; enforce_start_reserve=true;
    assert(read_pico_transfer_start(&ap)==ESP_ERR_NO_MEM && !start_calls); clean();
    reset(1); dma_free_limit=8191; enforce_start_reserve=true;
    assert(read_pico_transfer_start(&ap)==ESP_ERR_NO_MEM && !start_calls); clean();
    reset(1); dma_largest_limit=2047; enforce_start_reserve=true;
    assert(read_pico_transfer_start(&ap)==ESP_ERR_NO_MEM && !start_calls); clean();
    reset(1); heap_budget=30976; enforce_start_reserve=true;
    assert(read_pico_transfer_start(&ap)==ESP_OK && start_calls==1);
    read_pico_transfer_stop(); clean();
    reset(1); heap_budget=30975; enforce_start_reserve=true;
    assert(read_pico_transfer_start(&ap)==ESP_ERR_NO_MEM && !start_calls); clean();
    puts("AP beacon safety: proactive low-memory profile, post-init exhaustion/fragmentation reject before start, exact reserve boundary and retry PASS");
}
// 仅模拟已分配字节和SDK故障，不把预算模型当作射频、真实堆碎片或任务栈高水位验收。
// Model allocated bytes/SDK faults only, not RF, physical fragmentation or stack high-water acceptance.
static void field_http_budget(void) {
    reset(1);heap_budget=44867;driver_fixed_bytes=12896;dma_unavailable=7740;enforce_start_reserve=true;
    assert(heap_budget-12288-6400-driver_fixed_bytes==13283);
    assert(heap_budget-12288-6400-driver_fixed_bytes-dma_unavailable==5543);
}
static void http_start_headroom_fallback(void) {
    for(int mode=READ_PICO_TRANSFER_MODE_AP;mode<=READ_PICO_TRANSFER_MODE_STA;++mode) {
        const read_pico_transfer_cfg_t cfg={.mode=mode,.root_dir="/tmp",.free_bytes_cb=free_bytes};
        for(unsigned cycle=0;cycle<100;++cycle) {
            field_http_budget();
            assert(read_pico_transfer_start(&cfg)==ESP_OK);
            assert(http_starts==2&&http_small_starts==1&&http_stops==1&&http_routes==30);
            assert(http_bytes==8192&&s_started&&start_calls==1&&initialized);
            assert(heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_8BIT)==17379);
            assert(heap_caps_get_free_size(MALLOC_CAP_INTERNAL|MALLOC_CAP_DMA)==9639);
            assert(read_pico_transfer_start(&cfg)==ESP_ERR_INVALID_STATE&&http_starts==2);
            read_pico_transfer_stop();clean();assert(http_stops==2);
        }
    }
    const read_pico_transfer_cfg_t cfg={.mode=READ_PICO_TRANSFER_MODE_STA,.root_dir="/tmp",.free_bytes_cb=free_bytes};
    field_http_budget();heap_budget-=2000;
    assert(read_pico_transfer_start(&cfg)==ESP_ERR_NO_MEM&&!start_calls&&http_starts==2);clean();
    field_http_budget();dma_unavailable=9500;
    assert(read_pico_transfer_start(&cfg)==ESP_ERR_NO_MEM&&!start_calls&&http_starts==2);clean();
    field_http_budget();largest_limit=4095;
    assert(read_pico_transfer_start(&cfg)==ESP_ERR_NO_MEM&&!start_calls&&http_starts==2);clean();
    field_http_budget();dma_largest_limit=2047;
    assert(read_pico_transfer_start(&cfg)==ESP_ERR_NO_MEM&&!start_calls&&http_starts==2);clean();
    // 首次已回8KiB时不重复重建，也不把失败清理过的内存当作仍持有。
    // Never rebuild again if initial allocation already fell back to8KiB; account successful cleanup exactly once.
    field_http_budget();fail_http_stack=12288;
    assert(read_pico_transfer_start(&cfg)==ESP_OK&&http_starts==2&&http_small_starts==1&&http_stops==0);
    read_pico_transfer_stop();clean();
    reset(1);heap_budget=30975;enforce_start_reserve=true;fail_http_stack=12288;
    assert(read_pico_transfer_start(&cfg)==ESP_ERR_NO_MEM&&!start_calls&&http_starts==2&&http_small_starts==1);clean();
    field_http_budget();fail_http_stop=true;
    assert(read_pico_transfer_start(&cfg)==ESP_FAIL&&!start_calls&&http_starts==1);
    assert(s_http&&http_bytes==12288&&s_wifi&&initialized&&s_stopping);
    fail_http_stop=false;clean();
    field_http_budget();fail_http_stack=8192;
    assert(read_pico_transfer_start(&cfg)==ESP_ERR_HTTPD_TASK&&!start_calls&&http_starts==2);clean();
    field_http_budget();fail_route=16;
    assert(read_pico_transfer_start(&cfg)==ESP_FAIL&&!start_calls&&http_starts==2&&http_stops==2);clean();
    field_http_budget();fail_upload_buffer=true;
    assert(read_pico_transfer_start(&cfg)==ESP_ERR_NO_MEM&&!start_calls&&http_starts==1);clean();
    field_http_budget();assert(read_pico_transfer_start(&cfg)==ESP_OK);int total_calls=calls;
    read_pico_transfer_stop();clean();
    for(int fault=1;fault<=total_calls;++fault) {
        field_http_budget();fail_step=fault;
        int result=read_pico_transfer_start(&cfg);
        if(result!=ESP_OK)assert(s_status.state==READ_PICO_TRANSFER_ERROR);
        fail_step=0;read_pico_transfer_stop();clean();
    }
    puts("HTTP headroom: observed13283/5543 -> modeled17379/9639 via12->8KiB; AP/STA200 cycles, bounded retry, active rejection,8KiB floor, DMA/fragment guards, release/registration/stack/PSRAM and every SDK startup fault PASS");
}
static void minimal_profiles(void) {
    for(int network=0;network<2;++network){
        reset(1);heap_budget=6400;
        assert(wifi_driver_init(network,"test")==ESP_OK);
        assert(initialized&&used_config.static_rx_buf_num==2&&used_config.static_tx_buf_num==2);
        assert(used_config.dynamic_rx_buf_num==4&&used_config.rx_ba_win==2&&used_config.mgmt_sbuf_num==6);
        assert(esp_wifi_deinit()==ESP_OK);clean();
        reset(1);heap_budget=6399;
        assert(wifi_driver_init(network,"test")==ESP_ERR_NO_MEM&&!initialized);
        clean();
    }
    read_pico_transfer_network_t out[READ_PICO_TRANSFER_SCAN_MAX];size_t count;
    reset(1);heap_budget=6400;
    assert(read_pico_transfer_scan_wifi(out,&count)==ESP_ERR_NO_MEM&&!count&&!start_calls);clean();
    reset(1);heap_budget=6399;assert(read_pico_transfer_scan_wifi(out,&count)==ESP_ERR_NO_MEM&&!count);clean();
    puts("Bounded minimal WiFi fallback: scan/compact/full success, exhausted-budget cleanup, static TX and management bounds PASS");
}
int main(void) {
    successful_scans(); failures(); low_memory_scans(); lock_network_then_scan(); failed_release_retains_ownership();
    retained_latch_start_self_heals(); already_gone_release_tolerated(); full_session_low_memory_clamps(); minimal_profiles(); ap_beacon_headroom();automatic_candidates();http_start_headroom_fallback();
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
        for needle,replacement,description in [
            ('if (!wifi_start_has_headroom() && !cfg->network_only && !s_started && s_http && http_stack_bytes > 8192u) {',
             'if (false) {', 'Omitting HTTP compaction reproduces field startup exhaustion'),
            ('http_stack_bytes > 8192u', 'http_stack_bytes >= 8192u', 'Rebuilding an already-small HTTP stack violates bounded fallback'),
            ('err = httpd_stop(s_http); if (err != ESP_OK) goto fail;\n        s_http = NULL;',
             'err = httpd_stop(s_http);\n        s_http = NULL;', 'Forgetting a failed HTTP stop loses live ownership'),
        ]:
            assert harness.count(needle)==1
            controlled=harness.replace(needle,replacement)
            controlled=controlled[:controlled.rindex('int main(void) {')]+'int main(void) { http_start_headroom_fallback(); }\n'
            test.write_text(controlled);subprocess.run(command,cwd=ROOT,check=True)
            result=subprocess.run([str(temp/'test')],capture_output=True,text=True)
            assert result.returncode!=0 and 'assert' in result.stderr.lower()
            print(description+' (negative control) PASS')
        fixed = ".active = {.min = 0, .max = 200}, .passive = 360"
        assert harness.count(fixed) == 2
        test.write_text(harness.replace(fixed, ".active = {.min = 100, .max = 200}, .passive = 0"))
        subprocess.run(command, cwd=ROOT, check=True)
        result = subprocess.run([str(temp / "test")], capture_output=True, text=True)
        assert result.returncode != 0 and "scans ==" in result.stderr
        print("Former short dwell misses delayed beacon fixture (negative control) PASS")
        controls = [
            ('wifi_init_config_t init = wifi_init_for_session(network_only);', 'wifi_init_config_t init = WIFI_INIT_CONFIG_DEFAULT(); (void)network_only;', "low_memory_scans", "Former unbounded WiFi buffers fail the constrained DMA fixture"),
        ]
        for fixed_config, old_config, check, description in controls:
            assert harness.count(fixed_config) == 1
            controlled = harness.replace(fixed_config, old_config).replace("if (err != ESP_ERR_NO_MEM) return err;", "return err;")
            controlled = controlled[:controlled.rindex("int main(void) {")] + f"int main(void) {{ {check}(); }}\n"
            test.write_text(controlled); subprocess.run(command, cwd=ROOT, check=True)
            result = subprocess.run([str(temp / "test")], capture_output=True, text=True)
            assert result.returncode != 0 and "== ESP_OK" in result.stderr
            print(description + " (negative control) PASS")
        fixed_release = "if (err != ESP_OK && err != ESP_ERR_WIFI_NOT_INIT && err != ESP_ERR_INVALID_STATE) return err;\n        *initialized = false;"
        old_release = "*initialized = false;\n        if (err != ESP_OK && err != ESP_ERR_WIFI_NOT_INIT) return err;"
        assert harness.count(fixed_release) == 1
        controlled = harness.replace(fixed_release, old_release)
        controlled = controlled[:controlled.rindex("int main(void) {")] + "int main(void) { failed_release_retains_ownership(); }\n"
        test.write_text(controlled); subprocess.run(command, cwd=ROOT, check=True)
        result = subprocess.run([str(temp / "test")], capture_output=True, text=True)
        assert result.returncode != 0 and "s_stopping && s_wifi" in result.stderr
        print("Clearing ownership before failed deinit loses allocated driver (negative control) PASS")
        for needle, replacement, description in [
            ('if (!network_only && heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT) < 64u * 1024u)',
             'if (false)', 'Waiting for init failure misses later AP allocations'),
            ('if (!wifi_start_has_headroom()) { err = ESP_ERR_NO_MEM; goto fail; }',
             '', 'Starting after init without runtime headroom reaches unsafe driver allocation'),
        ]:
            assert harness.count(needle) == 1
            controlled = harness.replace(needle, replacement)
            controlled = controlled[:controlled.rindex("int main(void) {")] + "int main(void) { ap_beacon_headroom(); }\n"
            test.write_text(controlled); subprocess.run(command, cwd=ROOT, check=True)
            result = subprocess.run([str(temp / "test")], capture_output=True, text=True)
            assert result.returncode != 0 and 'assert' in result.stderr.lower()
            print(description + ' (negative control) PASS')

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
