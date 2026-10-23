/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 * 配网解析与连接时限的真实策略回归。/ Regression of actual provisioning parsing and connection deadlines.
 */
#include <assert.h>
#include <stdio.h>
#include "../components/read_pico_transfer/transfer_policy.h"

static int64_t body_time;
static int body_reads, body_cancel_at, body_step = 4000, body_failure;
static bool body_stopped(void *ctx) { (void)ctx; return body_cancel_at && body_reads >= body_cancel_at; }
static int64_t body_now(void *ctx) { (void)ctx; return body_time; }
static int body_receive(void *ctx, char *out, size_t cap) {
    (void)ctx; assert(cap); *out = 'a'; ++body_reads; body_time += body_step;
    return body_failure ? body_failure : 1;
}
static void test_body_deadline(void) {
    char body[257] = {0};
    body_time = body_reads = 0; body_cancel_at = 1;
    assert(!receive_credentials_body(body, 1, body_receive, body_stopped, body_now, NULL));
    assert(body_reads == 1);
    body_time = body_reads = body_cancel_at = 0;
    assert(!receive_credentials_body(body, 256, body_receive, body_stopped, body_now, NULL));
    assert(body_reads == 4);
    body_time = body_reads = 0;
    assert(receive_credentials_body(body, 2, body_receive, body_stopped, body_now, NULL));
    assert(body_reads == 2);
    body_time = body_reads = 0; body_cancel_at = -1;
    assert(!receive_credentials_body(body, 2, body_receive, body_stopped, body_now, NULL));
    assert(body_reads == 0);
    body_cancel_at = 0; body_step = 15000;
    assert(!receive_credentials_body(body, 1, body_receive, body_stopped, body_now, NULL));
    body_time = body_reads = 0; body_step = 1; body_failure = -1;
    assert(!receive_credentials_body(body, 2, body_receive, body_stopped, body_now, NULL));
    body_time = body_reads = 0; body_failure = 3;
    assert(!receive_credentials_body(body, 2, body_receive, body_stopped, body_now, NULL));
    body_failure = 0;
}

// 故障注入只替换NVS边界；生产读写函数原样执行。/ Inject faults only at NVS boundaries; run production storage helpers unchanged.
#define READ_PICO_TRANSFER_NVS_TEST
typedef int esp_err_t;
typedef unsigned nvs_handle_t;
enum { ESP_OK, ESP_ERR_NVS_NOT_FOUND, ESP_ERR_INVALID_STATE, ESP_ERR_INVALID_ARG, ESP_FAIL, ESP_ERR_NOT_FOUND };
enum { NVS_READONLY, NVS_READWRITE };
static transfer_credentials_t legacy_committed, legacy_staged;
static transfer_credentials_store_t profiles_committed, profiles_staged;
static bool legacy_present, legacy_pending, profiles_present, profiles_pending;
static int fail_open, fail_write, fail_commit, handles;
static esp_err_t nvs_open(const char *name, int mode, nvs_handle_t *handle) {
    assert(!strcmp(name, "rp_wifi")); (void)mode;
    if (fail_open) return ESP_FAIL;
    ++handles; *handle = 1;
    legacy_staged = legacy_committed; legacy_pending = legacy_present;
    profiles_staged = profiles_committed; profiles_pending = profiles_present;
    return ESP_OK;
}
static esp_err_t nvs_get_blob(nvs_handle_t handle, const char *key, void *out, size_t *size) {
    assert(handle == 1);
    if (!strcmp(key, "profiles")) {
        if (!profiles_present) return ESP_ERR_NVS_NOT_FOUND;
        assert(*size >= sizeof(profiles_committed)); *size = sizeof(profiles_committed);
        memcpy(out, &profiles_committed, *size); return ESP_OK;
    }
    assert(!strcmp(key, "credentials"));
    if (!legacy_present) return ESP_ERR_NVS_NOT_FOUND;
    assert(*size >= sizeof(legacy_committed)); *size = sizeof(legacy_committed);
    memcpy(out, &legacy_committed, *size); return ESP_OK;
}
static esp_err_t nvs_set_blob(nvs_handle_t handle, const char *key, const void *in, size_t size) {
    assert(handle == 1 && !strcmp(key, "profiles") && size == sizeof(profiles_staged));
    if (fail_write) return ESP_FAIL;
    memcpy(&profiles_staged, in, size); profiles_pending = true; return ESP_OK;
}
static esp_err_t nvs_erase_key(nvs_handle_t handle, const char *key) {
    assert(handle == 1);
    if (fail_write) return ESP_FAIL;
    if (!strcmp(key, "profiles")) {
        bool existed = profiles_pending; profiles_pending = false;
        return existed ? ESP_OK : ESP_ERR_NVS_NOT_FOUND;
    }
    assert(!strcmp(key, "credentials"));
    bool existed = legacy_pending; legacy_pending = false;
    return existed ? ESP_OK : ESP_ERR_NVS_NOT_FOUND;
}
static esp_err_t nvs_commit(nvs_handle_t handle) {
    assert(handle == 1);
    if (fail_commit) return ESP_FAIL;
    legacy_committed = legacy_staged; legacy_present = legacy_pending;
    profiles_committed = profiles_staged; profiles_present = profiles_pending;
    return ESP_OK;
}
static void nvs_close(nvs_handle_t handle) {
    assert(handle == 1); --handles;
    clear_secret(&legacy_staged, sizeof(legacy_staged));
    clear_secret(&profiles_staged, sizeof(profiles_staged));
}
#include "../components/read_pico_transfer/transfer_credentials_store.h"

static bool parse(const char *json, transfer_credentials_t *c) { return parse_credentials(json, strlen(json), c); }
int main(void) {
    test_body_deadline();
    transfer_credentials_store_t ranked = {0};
    transfer_credentials_t saved;
    assert(credentials_from_text("recent", "12345678", &saved));assert(credentials_store_upsert(&ranked,&saved));
    assert(credentials_from_text("strong", "abcdefgh", &saved));assert(credentials_store_upsert(&ranked,&saved));
    assert(credentials_from_text("open", "", &saved));assert(credentials_store_upsert(&ranked,&saved));
    transfer_wifi_candidates_t candidates={0};
    read_pico_transfer_network_t seen={.ssid="recent",.rssi=-80,.supported=true,.requires_password=true};
    credentials_offer_candidate(&ranked,&candidates,&seen);
    strcpy(seen.ssid,"strong");seen.rssi=-30;credentials_offer_candidate(&ranked,&candidates,&seen);
    strcpy(seen.ssid,"recent");seen.rssi=-20;credentials_offer_candidate(&ranked,&candidates,&seen);
    assert(candidates.count==2&&candidates.order[0]==2&&candidates.rssi[0]==-20);
    seen.rssi=-90;credentials_offer_candidate(&ranked,&candidates,&seen);assert(candidates.rssi[0]==-20);
    seen.rssi=-20;strcpy(seen.ssid,"strong");credentials_offer_candidate(&ranked,&candidates,&seen);
    assert(candidates.order[0]==1&&candidates.order[1]==2);
    strcpy(seen.ssid,"open");credentials_offer_candidate(&ranked,&candidates,&seen);assert(candidates.count==2);
    seen.requires_password=false;seen.supported=false;credentials_offer_candidate(&ranked,&candidates,&seen);assert(candidates.count==2);
    seen.supported=true;credentials_offer_candidate(&ranked,&candidates,&seen);assert(candidates.count==3&&candidates.order[0]==0);
    strcpy(seen.ssid,"unknown");credentials_offer_candidate(&ranked,&candidates,&seen);assert(candidates.count==3);
    assert(transfer_wifi_disconnect_classify(15) == TRANSFER_WIFI_DISCONNECT_PASSWORD);
    assert(transfer_wifi_disconnect_classify(23) == TRANSFER_WIFI_DISCONNECT_PASSWORD);
    assert(transfer_wifi_disconnect_classify(202) == TRANSFER_WIFI_DISCONNECT_PASSWORD);
    assert(transfer_wifi_disconnect_classify(204) == TRANSFER_WIFI_DISCONNECT_PASSWORD);
    assert(transfer_wifi_disconnect_classify(201) == TRANSFER_WIFI_DISCONNECT_NOT_FOUND);
    assert(transfer_wifi_disconnect_classify(210) == TRANSFER_WIFI_DISCONNECT_NOT_FOUND);
    assert(transfer_wifi_disconnect_classify(1) == TRANSFER_WIFI_DISCONNECT_OTHER);
    bool stopping = false, uploading = false;
    assert(admission_begin(stopping, &uploading));
    assert(!admission_stop(&stopping, uploading) && !stopping);
    uploading = false;
    assert(admission_stop(&stopping, uploading) && stopping);
    assert(!admission_begin(stopping, &uploading) && !uploading);
    stopping = false;
    assert(admission_stop(&stopping, uploading));
    assert(!admission_begin(stopping, &uploading));
    transfer_credentials_t c;
    assert(credentials_from_text("device", "12345678", &c));
    assert(!credentials_from_text("device", "short", &c) && c.version == 0);
    assert(!credentials_from_text("", "12345678", &c));
    assert(!credentials_from_text("123456789012345678901234567890123", "12345678", &c));
    read_pico_transfer_network_t networks[READ_PICO_TRANSFER_SCAN_MAX] = {0};
    size_t count = 0;
    read_pico_transfer_network_t ap = {.ssid = "same", .rssi = -70, .requires_password = true, .supported = true};
    scan_offer(networks, &count, &ap); ap.rssi = -40; scan_offer(networks, &count, &ap);
    assert(count == 1 && networks[0].rssi == -40);
    ap.ssid[0] = 0; scan_offer(networks, &count, &ap); assert(count == 1);
    for (int i = 0; i < 25; ++i) {
        snprintf(ap.ssid, sizeof(ap.ssid), "network-%02d", i); ap.rssi = (int8_t)(-100 + i);
        scan_offer(networks, &count, &ap);
    }
    assert(count == READ_PICO_TRANSFER_SCAN_MAX && networks[0].rssi == -40);
    for (size_t i = 1; i < count; ++i) assert(networks[i - 1].rssi >= networks[i].rssi);
    strcpy(ap.ssid, "network-24"); ap.rssi = -20; ap.requires_password = false;
    scan_offer(networks, &count, &ap);
    assert(count == READ_PICO_TRANSFER_SCAN_MAX && networks[0].rssi == -20 && !networks[0].requires_password);
    assert(parse("{\"ssid\":\"家庭 WiFi\",\"password\":\"12345678\"}", &c));
    assert(!strcmp(c.ssid, "家庭 WiFi"));
    assert(parse("{\"ssid\":\"open\",\"password\":\"\"}", &c));
    assert(store_credentials(&c) == ESP_OK && profiles_present && !legacy_present && !handles);
    transfer_credentials_t old;
    assert(load_credentials(&old) == ESP_OK && !strcmp(old.ssid, "open"));
    strcpy(c.ssid, "replacement"); fail_write = 1;
    assert(store_credentials(&c) == ESP_FAIL && !handles);
    assert(load_credentials(&old) == ESP_OK && !strcmp(old.ssid, "open"));
    fail_write = 0; fail_commit = 1;
    assert(store_credentials(&c) == ESP_FAIL && !handles);
    assert(load_credentials(&old) == ESP_OK && !strcmp(old.ssid, "open"));
    assert(store_credentials(NULL) == ESP_FAIL && profiles_present && !handles);
    fail_commit = 0; fail_open = 1;
    assert(store_credentials(&c) == ESP_FAIL && !handles);
    assert(load_credentials(&old) == ESP_FAIL && old.version == 0);
    fail_open = 0;
    assert(store_credentials(NULL) == ESP_OK && !profiles_present && !legacy_present && !handles);
    assert(load_credentials(&old) == ESP_OK && old.version == 0);
    assert(store_credentials(NULL) == ESP_OK && !handles);
    // 旧单网络记录可读；第一次新增网络时迁移并保留旧密码。
    // A legacy single-network record remains readable and migrates on the first added network.
    assert(credentials_from_text("legacy", "12345678", &legacy_committed)); legacy_present = true;
    assert(load_credentials(&old) == ESP_OK && !strcmp(old.ssid, "legacy"));
    assert(credentials_from_text("office", "abcdefgh", &c));
    assert(store_credentials(&c) == ESP_OK && profiles_present && !legacy_present);
    transfer_credentials_store_t store;
    assert(load_credentials_store(&store) == ESP_OK && store.count == 2);
    assert(!strcmp(store.profiles[0].ssid, "office") && !strcmp(store.profiles[1].ssid, "legacy"));
    // 更新、晋升、六项上限与逐项遗忘。/ Update, promotion, six-entry bound and per-entry forgetting.
    for (int i = 0; i < 7; ++i) {
        char ssid[16]; snprintf(ssid, sizeof(ssid), "wifi-%d", i);
        assert(credentials_from_text(ssid, "12345678", &c));
        assert(store_credentials(&c) == ESP_OK);
    }
    assert(load_credentials_store(&store) == ESP_OK && store.count == TRANSFER_WIFI_PROFILE_MAX);
    assert(!strcmp(store.profiles[0].ssid, "wifi-6") && credentials_store_find(&store, "office") < 0);
    assert(promote_credentials("wifi-2") == ESP_OK);
    assert(load_credentials(&old) == ESP_OK && !strcmp(old.ssid, "wifi-2"));
    assert(remove_credentials("wifi-2", &old) == ESP_OK && !strcmp(old.ssid, "wifi-6"));
    assert(promote_credentials("missing") == ESP_ERR_NOT_FOUND);
    clear_secret(&store, sizeof(store));
    const char *bad[] = {"{}", "[]", "null", "{\"ssid\":\"\",\"password\":\"12345678\"}",
        "{\"ssid\":\"home\",\"password\":12345678}", "{\"ssid\":\"home\",\"password\":\"short\"}",
        "{\"ssid\":\"home\",\"password\":\"12345678\"}x", "{\"ssid\":\"home\\u0000x\",\"password\":\"12345678\"}",
        "{\"ssid\":\"home\",\"ssid\":\"evil\",\"password\":\"12345678\"}",
        "{\"ssid\":\"home\",\"password\":\"12345678\",\"unknown\":1}"};
    for (size_t i = 0; i < sizeof(bad)/sizeof(*bad); ++i) assert(!parse(bad[i], &c));
    assert(!parse("{\"ssid\":[],\"password\":\"12345678\"}", &c));
    assert(parse("{\"ssid\":\"home[{}]\",\"password\":\"12345678\"}", &c));
    const char embedded[] = "{\"ssid\":\"home\",\"password\":\"12345678\"}\0junk";
    assert(!parse_credentials(embedded, sizeof(embedded) - 1, &c));
    memset(&c, 0, sizeof(c)); c.version = 1; memset(c.ssid, 'a', 32); memset(c.password, 'a', 64);
    assert(credentials_valid(&c)); c.password[0] = 'g'; assert(!credentials_valid(&c));
    c.password[0] = 'a'; c.ssid[32] = 'b'; assert(!credentials_valid(&c));
    clear_secret(&c, sizeof(c)); for (size_t i = 0; i < sizeof(c); ++i) assert(((unsigned char *)&c)[i] == 0);
    transfer_connection_t net;
    connection_begin(&net, 100);
    assert(connection_poll(&net, 100) == 1 && net.attempts == 1);
    assert(connection_poll(&net, 30099) == 0);
    assert(connection_poll(&net, 30100) == -1 && net.failed);
    assert(connection_poll(&net, 40000) == 0);
    // 关联成功后单独留出 DHCP 窗口，不受已消耗的关联时间挤压。
    // Association opens a separate DHCP window instead of sharing the time already spent associating.
    connection_begin(&net, 0);
    assert(connection_poll(&net, 0) == 1);
    connection_associated(&net, 29000);
    assert(net.associated && connection_poll(&net, 48999) == 0);
    assert(connection_poll(&net, 49000) == -1 && net.failed);
    connection_begin(&net, 0);
    assert(connection_poll(&net, 0) == 1);
    connection_associated(&net, 5000);
    connection_lost(&net, 6000);
    assert(!net.associated && connection_poll(&net, 6999) == 0);
    assert(connection_poll(&net, 7000) == 1 && net.attempts == 2);
    connection_begin(&net, 0);
    for (int i = 0; i < 3; ++i) {
        assert(connection_poll(&net, i * 1000) == 1);
        connection_lost(&net, i * 1000);
    }
    assert(connection_poll(&net, 3000) == -1);
    connection_begin(&net, 0); net.online = true;
    assert(connection_poll(&net, 100000) == 0);
    connection_lost(&net, 100000);
    assert(connection_poll(&net, 100999) == 0);
    assert(connection_poll(&net, 101000) == 1 && net.attempts == 1);
    // 反复连上又掉线：四次掉线即终止本轮，不再无限重置看门狗。/ Flapping connect-drop
    // cycles terminate after four drops instead of resetting the watchdog forever.
    connection_begin(&net, 0);
    for (int i = 0; i < 4; ++i) {
        net.online = true;
        connection_lost(&net, i * 1000);
    }
    assert(net.drops == 4 && connection_poll(&net, 3500) == -1 && net.failed);
    puts("transfer WiFi policy tests passed");
}
