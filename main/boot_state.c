/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 *
 * 中文：固定大小且校验的 NVS 启动记录。开书中断后保留原进度，只屏蔽自动重试。
 * English: Fixed-size checked NVS startup state. Interrupted book opening retains progress and blocks automatic retries only.
 *
 * 冻结：不擦除分区、不动态分配、不逐页写 Flash；本地开机流向（阅读优先）是既有产品决策，本模块不改变开机落点。
 * Frozen: No partition erasure, allocations or per-page Flash writes. The reading-first boot target is an existing product decision this module never changes.
 */

#include "boot_state.h"

#include <stddef.h>
#include <string.h>

#include "esp_log.h"
#include "nvs.h"

#define BOOT_PATH_MAX 288
#define BOOT_MAGIC UINT32_C(0x50425331)

typedef struct {
    uint32_t magic;
    uint8_t starting, reserved[3];
    char opening[BOOT_PATH_MAX], blocked[BOOT_PATH_MAX];
    uint32_t checksum;
} boot_record_t;

static boot_record_t s_record;
static bool s_recovery, s_writable, s_interrupted;

static uint32_t checksum(const boot_record_t* r) {
    const uint8_t* p = (const uint8_t*)r;
    uint32_t hash = 2166136261u;
    for (size_t i = 0; i < offsetof(boot_record_t, checksum); ++i) hash = (hash ^ p[i]) * 16777619u;
    return hash;
}

// 书路径只收 TF 卡与内置存储两个根，逐段拒绝空段、点段、控制字符与反斜杠。
// Book paths accept only the SD and internal-storage roots; each segment rejects empties, dot segments, control characters and backslashes.
static bool path_valid(const char* path, bool empty) {
    size_t n = strnlen(path, BOOT_PATH_MAX);
    if (!n) return empty;
    if (n == BOOT_PATH_MAX || (strncmp(path, "/sdcard/", 8) && strncmp(path, "/flash/", 7))) return false;
    for (const char* p = path + 1; *p;) {
        const char* end = strchr(p, '/');
        size_t len = end ? (size_t)(end - p) : strlen(p);
        if (!len || (len == 1 && p[0] == '.') || (len == 2 && p[0] == '.' && p[1] == '.')) return false;
        for (size_t i = 0; i < len; ++i) if ((unsigned char)p[i] < 32 || p[i] == '\\') return false;
        if (!end) break;
        p = end + 1;
        if (!*p) return false;
    }
    return true;
}

static esp_err_t store(void) {
    if (!s_writable) return ESP_ERR_INVALID_STATE;
    s_record.checksum = checksum(&s_record);
    nvs_handle_t h;
    esp_err_t err = nvs_open("pico_startup", NVS_READWRITE, &h);
    if (err != ESP_OK) return err;
    err = nvs_set_blob(h, "state", &s_record, sizeof(s_record));
    if (err == ESP_OK) err = nvs_commit(h);
    nvs_close(h);
    if (err != ESP_OK) ESP_LOGW("boot_state", "checkpoint: %s", esp_err_to_name(err));
    return err;
}

void pico_boot_init(bool abnormal_reset) {
    memset(&s_record, 0, sizeof(s_record));
    s_interrupted = false;
    s_recovery = abnormal_reset;
    nvs_handle_t h;
    esp_err_t err = nvs_open("pico_startup", NVS_READWRITE, &h);
    s_writable = err == ESP_OK;
    if (s_writable) {
        size_t size = sizeof(s_record);
        err = nvs_get_blob(h, "state", &s_record, &size);
        nvs_close(h);
        bool valid = err == ESP_OK && size == sizeof(s_record) && s_record.magic == BOOT_MAGIC &&
            s_record.starting <= 1 && path_valid(s_record.opening, true) &&
            path_valid(s_record.blocked, true) && s_record.checksum == checksum(&s_record);
        if (!valid) {
            if (err != ESP_ERR_NVS_NOT_FOUND) s_recovery = true;
            memset(&s_record, 0, sizeof(s_record));
        }
    } else s_recovery = true;
    if (s_record.starting || s_record.opening[0]) s_recovery = true;
    if (s_record.opening[0]) {
        s_interrupted = true;
        memcpy(s_record.blocked, s_record.opening, sizeof(s_record.blocked));
        s_record.opening[0] = 0;
        char book[BOOT_PATH_MAX];
        if (pico_boot_interrupted_book(book, sizeof(book)))
            ESP_LOGW("boot_state", "上次开书中断：%s；已停止自动重提", book);
    }
    s_record.magic = BOOT_MAGIC;
    s_record.starting = 1;
    if (store() != ESP_OK) s_recovery = true;
    ESP_LOGI("boot_state", "startup recovery=%d interrupted=%d", s_recovery, s_interrupted);
}

void pico_boot_ready(void) {
    s_record.starting = 0;
    (void)store();
}

bool pico_boot_recovery(void) { return s_recovery; }

// 本地没有恢复模式页面：只屏蔽中断的那本书，正常界面照常运行（s_recovery 仅记日志）。
// No recovery page exists here: gate exactly the interrupted book while normal UI keeps running (s_recovery only feeds logs).
bool pico_boot_asset_allowed(const char* path) {
    return path && (!s_record.blocked[0] || strcmp(path, s_record.blocked));
}

esp_err_t pico_boot_book_begin(const char* path) {
    if (!path || !path_valid(path, false)) return ESP_ERR_INVALID_ARG;
    memset(s_record.opening, 0, sizeof(s_record.opening));
    memcpy(s_record.opening, path, strlen(path));
    return store();
}

void pico_boot_book_end(bool success) {
    if (success && !strcmp(s_record.blocked, s_record.opening)) s_record.blocked[0] = 0;
    s_record.opening[0] = 0;
    (void)store();
}

bool pico_boot_interrupted_book(char* path, size_t capacity) {
    if (!s_interrupted || !path || strlen(s_record.blocked) >= capacity) return false;
    memcpy(path, s_record.blocked, strlen(s_record.blocked) + 1);
    return true;
}

void pico_boot_forget_interrupted_book(void) {
    s_interrupted = false;
    s_record.blocked[0] = 0;
    (void)store();
}
