/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 * 有界多网络凭据读写与旧格式迁移，失败不更新公开快照。
 * Bounded multi-network credential I/O and legacy migration; failures do not update public snapshots.
 */
#pragma once
#ifndef READ_PICO_TRANSFER_NVS_TEST
#include "nvs.h"
#endif
#include "transfer_policy.h"

static esp_err_t load_credentials_store(transfer_credentials_store_t *out) {
    memset(out, 0, sizeof(*out));
    nvs_handle_t handle;
    esp_err_t err = nvs_open("rp_wifi", NVS_READONLY, &handle);
    if (err == ESP_ERR_NVS_NOT_FOUND) return ESP_OK;
    if (err != ESP_OK) return err;
    size_t size = sizeof(*out);
    err = nvs_get_blob(handle, "profiles", out, &size);
    if (err == ESP_OK) {
        nvs_close(handle);
        if (size == sizeof(*out) && credentials_store_valid(out)) return ESP_OK;
        clear_secret(out, sizeof(*out));
        return ESP_ERR_INVALID_STATE;
    }
    if (err != ESP_ERR_NVS_NOT_FOUND) { nvs_close(handle); return err; }

    // 0.6.27及更早版本只保存一个网络；读取时映射为新列表，下次写入再原子迁移。
    // 0.6.27 and older saved one network; map it in memory and migrate atomically on the next write.
    transfer_credentials_t legacy = {0};
    size = sizeof(legacy);
    err = nvs_get_blob(handle, "credentials", &legacy, &size);
    nvs_close(handle);
    if (err == ESP_ERR_NVS_NOT_FOUND) return ESP_OK;
    if (err != ESP_OK || size != sizeof(legacy) || !credentials_valid(&legacy)) {
        clear_secret(&legacy, sizeof(legacy));
        return err == ESP_OK ? ESP_ERR_INVALID_STATE : err;
    }
    out->version = TRANSFER_CREDENTIALS_STORE_VERSION;
    out->count = 1;
    out->profiles[0] = legacy;
    clear_secret(&legacy, sizeof(legacy));
    return ESP_OK;
}

static esp_err_t write_credentials_store(const transfer_credentials_store_t *next) {
    if (next && !credentials_store_valid(next)) return ESP_ERR_INVALID_ARG;
    nvs_handle_t handle;
    esp_err_t err = nvs_open("rp_wifi", NVS_READWRITE, &handle);
    if (err != ESP_OK) return err;
    err = next ? nvs_set_blob(handle, "profiles", next, sizeof(*next)) : nvs_erase_key(handle, "profiles");
    if (!next && err == ESP_ERR_NVS_NOT_FOUND) err = ESP_OK;
    if (err == ESP_OK) {
        esp_err_t legacy_err = nvs_erase_key(handle, "credentials");
        if (legacy_err != ESP_OK && legacy_err != ESP_ERR_NVS_NOT_FOUND) err = legacy_err;
    }
    if (err == ESP_OK) err = nvs_commit(handle);
    nvs_close(handle);
    return err;
}

static esp_err_t load_credentials(transfer_credentials_t *out) {
    memset(out, 0, sizeof(*out));
    transfer_credentials_store_t store;
    esp_err_t err = load_credentials_store(&store);
    if (err == ESP_OK && store.count) *out = store.profiles[0];
    clear_secret(&store, sizeof(store));
    return err;
}

static esp_err_t store_credentials(const transfer_credentials_t *next) {
    if (next && !credentials_valid(next)) return ESP_ERR_INVALID_ARG;
    transfer_credentials_store_t store;
    esp_err_t err = load_credentials_store(&store);
    if (err == ESP_OK && next && !credentials_store_upsert(&store, next)) err = ESP_ERR_INVALID_STATE;
    if (err == ESP_OK) err = write_credentials_store(next ? &store : NULL);
    clear_secret(&store, sizeof(store));
    return err;
}

static esp_err_t promote_credentials(const char *ssid) {
    transfer_credentials_store_t store;
    esp_err_t err = load_credentials_store(&store);
    int found = err == ESP_OK ? credentials_store_find(&store, ssid) : -1;
    if (err == ESP_OK && found < 0) err = ESP_ERR_NOT_FOUND;
    if (err == ESP_OK && found > 0) {
        credentials_store_promote(&store, ssid);
        err = write_credentials_store(&store);
    }
    clear_secret(&store, sizeof(store));
    return err;
}

static esp_err_t remove_credentials(const char *ssid, transfer_credentials_t *preferred) {
    if (preferred) memset(preferred, 0, sizeof(*preferred));
    transfer_credentials_store_t store;
    esp_err_t err = load_credentials_store(&store);
    if (err == ESP_OK && !credentials_store_remove(&store, ssid)) err = ESP_ERR_NOT_FOUND;
    if (err == ESP_OK) err = write_credentials_store(store.count ? &store : NULL);
    if (err == ESP_OK && preferred && store.count) *preferred = store.profiles[0];
    clear_secret(&store, sizeof(store));
    return err;
}
