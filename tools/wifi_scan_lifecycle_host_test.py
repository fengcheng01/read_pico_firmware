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
scan_body = source[source.index("static esp_err_t scan_wifi_pass("):source.index("esp_err_t read_pico_transfer_forget_wifi(")]
harness = r'''
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include "transfer_policy.h"
typedef int esp_err_t;
enum { ESP_OK, ESP_ERR_INVALID_ARG, ESP_ERR_INVALID_STATE, ESP_FAIL, ESP_ERR_WIFI_TIMEOUT };
enum { WIFI_STORAGE_RAM, WIFI_MODE_STA, WIFI_SCAN_TYPE_ACTIVE, WIFI_SCAN_TYPE_PASSIVE };
enum { WIFI_AUTH_OPEN, WIFI_AUTH_WEP, WIFI_AUTH_WPA_PSK, WIFI_AUTH_WPA2_PSK,
       WIFI_AUTH_WPA_WPA2_PSK, WIFI_AUTH_WPA3_PSK, WIFI_AUTH_WPA2_WPA3_PSK, WIFI_AUTH_OWE };
enum { WIFI_COUNTRY_POLICY_AUTO, WIFI_COUNTRY_POLICY_MANUAL };
enum { READ_PICO_TRANSFER_STOPPED, READ_PICO_TRANSFER_STARTING, READ_PICO_TRANSFER_READY };
typedef struct { int state; } read_pico_transfer_status_t;
typedef struct { char cc[3]; uint8_t schan, nchan; int policy; } wifi_country_t;
typedef struct { int unused; } wifi_init_config_t;
#define WIFI_INIT_CONFIG_DEFAULT() ((wifi_init_config_t){0})
typedef struct {
    uint8_t *ssid, *bssid; uint8_t channel; bool show_hidden; int scan_type;
    struct { struct { uint32_t min, max; } active; uint32_t passive; } scan_time;
} wifi_scan_config_t;
typedef struct { uint8_t ssid[33]; int8_t rssi; int authmode; } wifi_ap_record_t;
static bool s_wifi; static void *s_http, *s_netif;
static int public_state, initialized, started, own_loop, external_loop;
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
static void read_pico_transfer_get_status(read_pico_transfer_status_t *out) { out->state = public_state; }
static int esp_netif_init(void) { return step(); }
static int esp_event_loop_create_default(void) {
    int err = step(); if (err) return err;
    if (external_loop) return ESP_ERR_INVALID_STATE;
    own_loop = 1; return ESP_OK;
}
static int esp_wifi_init(const wifi_init_config_t *config) {
    assert(config && !initialized); int err = step(); if (!err) initialized = 1; return err;
}
static int esp_wifi_set_storage(int storage) { assert(initialized && storage == WIFI_STORAGE_RAM); return step(); }
static int esp_wifi_set_mode(int mode) { assert(initialized && mode == WIFI_MODE_STA); return step(); }
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
    assert(started); ++stopped; started = 0; return step();
}
static int esp_wifi_deinit(void) {
    assert(initialized && !started); ++deinitialized; initialized = 0; return step();
}
static int esp_event_loop_delete_default(void) {
    assert(own_loop && !external_loop); ++deleted; own_loop = 0; return step();
}
''' + scan_body + r'''
static void reset(int mode) {
    assert(!started && !initialized && !own_loop && !pending);
    s_wifi = false; s_http = s_netif = NULL; public_state = READ_PICO_TRANSFER_STOPPED;
    calls = scans = records = stopped = deinitialized = deleted = cleared = country_reads = 0;
    fail_step = 0; fail_code = ESP_FAIL; scenario = mode; external_loop = 0;
    configured_country = (wifi_country_t){.cc = "01", .schan = 1, .nchan = 11, .policy = WIFI_COUNTRY_POLICY_AUTO};
}
static void clean(void) { assert(!started && !initialized && !own_loop && !pending); }
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
    reset(1); public_state = READ_PICO_TRANSFER_READY;
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
int main(void) { successful_scans(); failures(); puts("Production WiFi scan lifecycle PASS"); }
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
