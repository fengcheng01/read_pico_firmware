/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 *
 * 中文：同步核心的设备桥。HTTP 走 esp_http_client（https 校验证书，不自跳过），
 * 文档标识为 KOReader 部分 MD5；推/拉作用在“最后一本书”的进度记录上。
 * English: Device bridge for the sync core. HTTP uses esp_http_client (https
 * verifies certificates, never skipped), document identity is the KOReader
 * partial MD5, and push/pull act on the last-read book's progress record.
 *
 * 冻结：只在 UI 任务上随传书 STA 会话运行；不做后台轮询；密码仅以 MD5 落盘；
 * 拉取只在文件大小匹配时套用 rp1 精确位置，否则按百分比近似并明示。
 * Frozen: Runs only on the UI task inside transfer STA sessions with no
 * background polling; the password persists only as MD5; pulls apply the exact
 * rp1 position only when file sizes match, otherwise approximating by
 * percentage and saying so.
 */
#include "os_sync.h"
#include "book_progress.h"
#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_rom_md5.h"
#include "settings.h"
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

static const char* TAG = "os_sync";

static int sync_request(const char* method, const char* url, const char* user, const char* key,
                        const char* content_type, const char* body, char* resp, size_t resp_cap) {
    esp_http_client_config_t config = {
        .url = url,
        .method = strcmp(method, "PUT") == 0 ? HTTP_METHOD_PUT :
                  strcmp(method, "POST") == 0 ? HTTP_METHOD_POST : HTTP_METHOD_GET,
        .timeout_ms = 10000,
        // 传书页 STA 会话期间调用；https 需有效证书，绝不跳过校验。
        // Called during the transfer STA session; https needs a valid certificate and never skips verification.
        .crt_bundle_attach = strncmp(url, "https://", 8) == 0 ? esp_crt_bundle_attach : NULL,
        .buffer_size = 1024,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) return -1;
    esp_http_client_set_header(client, "Accept", "application/vnd.koreader.v1+json");
    if (user && key) {
        esp_http_client_set_header(client, "x-auth-user", user);
        esp_http_client_set_header(client, "x-auth-key", key);
    }
    if (content_type) esp_http_client_set_header(client, "Content-Type", content_type);
    int status = -1;
    // open/fetch/read 流程才能读到响应体；perform 会把流消费掉。
    // The open/fetch/read flow is required to read the body; perform consumes the stream.
    if (esp_http_client_open(client, body ? (int)strlen(body) : 0) == ESP_OK) {
        if (body && esp_http_client_write(client, body, (int)strlen(body)) >= 0) {}
        (void)esp_http_client_fetch_headers(client);
        status = esp_http_client_get_status_code(client);
        if (resp && resp_cap) {
            int read = esp_http_client_read(client, resp, (int)resp_cap - 1);
            if (read >= 0) resp[read] = 0;
        }
    }
    esp_http_client_cleanup(client);
    ESP_LOGI(TAG, "%s %s -> %d", method, url, status);
    return status;
}

// KOReader 部分 MD5：12 个偏移各 1024 字节，越过文件尾即停。
// KOReader partial MD5: 1024 bytes at 12 offsets, stopping past EOF.
static bool sync_doc_id(const char* path, char out[33]) {
    static const uint64_t offsets[] = {0, 1024, 4096, 16384, 65536, 262144,
                                       1048576, 4194304, 16777216, 67108864, 268435456, 1073741824};
    FILE* file = fopen(path, "rb");
    if (!file) return false;
    unsigned char digest[16], chunk[1024];
    md5_context_t ctx;
    esp_rom_md5_init(&ctx);
    for (unsigned i = 0; i < sizeof(offsets) / sizeof(offsets[0]); ++i) {
        if (fseek(file, (long)offsets[i], SEEK_SET) != 0) break;
        size_t got = fread(chunk, 1, sizeof(chunk), file);
        if (!got) break;
        esp_rom_md5_update(&ctx, chunk, (uint32_t)got);
        if (got < sizeof(chunk)) break;
    }
    fclose(file);
    esp_rom_md5_final(digest, &ctx);
    for (int i = 0; i < 16; ++i) snprintf(out + i * 2, 3, "%02x", digest[i]);
    return true;
}

static const os_sync_io_t s_io = {
    .request = sync_request,
    .doc_id = sync_doc_id,
};

__attribute__((constructor)) static void os_sync_bind_device(void) { os_sync_bind(&s_io); }

static void load_config(os_sync_config_t* config) {
    snprintf(config->url, sizeof(config->url), "%s", app_settings_sync_url());
    snprintf(config->user, sizeof(config->user), "%s", app_settings_sync_user());
    snprintf(config->key, sizeof(config->key), "%s", app_settings_sync_key());
}

/// 把明文密码转为 MD5 十六进制保存；设备上不留明文。/ Store the password as MD5 hex; no plain text on device.
void os_sync_set_password(const char* plain) {
    if (!plain) return;
    unsigned char digest[16];
    md5_context_t ctx;
    esp_rom_md5_init(&ctx);
    esp_rom_md5_update(&ctx, plain, (uint32_t)strlen(plain));
    esp_rom_md5_final(digest, &ctx);
    char key[33];
    for (int i = 0; i < 16; ++i) snprintf(key + i * 2, 3, "%02x", digest[i]);
    app_settings_set_sync_key(key);
}

/// 上传最后一本书的进度；note 汇总结果给页面显示。/ Push the last book's progress; note carries the page message.
os_sync_result_t os_sync_push_last(char* note, size_t cap) {
    os_sync_config_t config;
    load_config(&config);
    char path[288];
    if (!note || !cap) return OS_SYNC_IO;
    note[0] = 0;
    if (!os_sync_config_ready(&config)) {
        snprintf(note, cap, "请先设置同步账号");
        return OS_SYNC_NO_CONFIG;
    }
    if (!book_progress_last_path(path, sizeof(path)) || !path[0]) {
        snprintf(note, cap, "还没有阅读记录");
        return OS_SYNC_IO;
    }
    struct stat st;
    if (stat(path, &st) || st.st_size <= 0 || st.st_size > 0xFFFFFFFF) {
        snprintf(note, cap, "图书文件不可用");
        return OS_SYNC_IO;
    }
    char doc[33];
    book_progress_t progress = {0};
    if (!sync_doc_id(path, doc) || !book_progress_load(path, (uint32_t)st.st_size, &progress)) {
        snprintf(note, cap, "无法计算本书标识或进度");
        return OS_SYNC_IO;
    }
    char rp1[OS_SYNC_PROGRESS_MAX];
    os_sync_progress_encode(rp1, sizeof(rp1), (uint32_t)st.st_size, progress.chapter,
                            progress.byte_off, progress.px);
    os_sync_result_t result = os_sync_push(&config, doc, rp1, progress.pct > 100 ? 1.0f : progress.pct / 100.0f);
    snprintf(note, cap, "%s", os_sync_result_name(result));
    return result;
}

/// 拉取最后一本书的远端进度；大小匹配用 rp1 精确恢复，否则按百分比近似。
/// / Pull the last book's remote progress; exact rp1 restore on size match, else a percentage approximation.
os_sync_result_t os_sync_pull_last(char* note, size_t cap) {
    os_sync_config_t config;
    load_config(&config);
    char path[288], doc[33], remote[OS_SYNC_PROGRESS_MAX];
    float percent = 0;
    if (!note || !cap) return OS_SYNC_IO;
    note[0] = 0;
    if (!os_sync_config_ready(&config)) {
        snprintf(note, cap, "请先设置同步账号");
        return OS_SYNC_NO_CONFIG;
    }
    if (!book_progress_last_path(path, sizeof(path)) || !path[0]) {
        snprintf(note, cap, "还没有阅读记录");
        return OS_SYNC_IO;
    }
    struct stat st;
    if (stat(path, &st) || st.st_size <= 0 || st.st_size > 0xFFFFFFFF) {
        snprintf(note, cap, "图书文件不可用");
        return OS_SYNC_IO;
    }
    if (!sync_doc_id(path, doc)) {
        snprintf(note, cap, "无法计算本书标识");
        return OS_SYNC_IO;
    }
    os_sync_result_t result = os_sync_pull(&config, doc, remote, sizeof(remote), &percent);
    if (result != OS_SYNC_OK) {
        snprintf(note, cap, "%s", os_sync_result_name(result));
        return result;
    }
    book_progress_t progress = {0};
    uint32_t size = 0, off = 0;
    uint16_t chapter = 0;
    uint8_t px = 48;
    if (os_sync_progress_decode(remote, &size, &chapter, &off, &px) && size == (uint32_t)st.st_size) {
        progress.file_size = size;
        progress.chapter = chapter;
        progress.byte_off = off;
        progress.px = px >= 36 && px <= 72 ? px : 48;
        snprintf(note, cap, "已精确恢复到 %u%%", (unsigned)(percent * 100));
    } else {
        // 远端是其它客户端（如 KOReader）或文件不同：仅按百分比，从头附近换算。
        // Remote came from another client or a different file: percentage only, resolved near the start.
        progress.file_size = (uint32_t)st.st_size;
        progress.chapter = 0;
        progress.byte_off = 0;
        progress.px = app_settings_book_px();
        snprintf(note, cap, "已按百分比恢复到 %u%%（位置为近似）", (unsigned)(percent * 100));
    }
    progress.pct = (uint8_t)(percent * 100 + 0.5f);
    if (progress.pct > 100) progress.pct = 100;
    esp_err_t err = book_progress_save(path, &progress);
    if (err == ESP_OK) err = book_progress_set_last_path(path);
    if (err != ESP_OK) {
        snprintf(note, cap, "进度保存失败，请重试");
        return OS_SYNC_IO;
    }
    return OS_SYNC_OK;
}

os_sync_result_t os_sync_device_auth(char* note, size_t cap) {
    os_sync_config_t config;
    load_config(&config);
    if (!os_sync_config_ready(&config)) {
        if (note && cap) snprintf(note, cap, "请先设置同步账号");
        return OS_SYNC_NO_CONFIG;
    }
    os_sync_result_t result = os_sync_auth(&config);
    if (note && cap) snprintf(note, cap, "%s", os_sync_result_name(result));
    return result;
}

os_sync_result_t os_sync_device_register(char* note, size_t cap) {
    os_sync_config_t config;
    load_config(&config);
    if (!os_sync_config_ready(&config)) {
        if (note && cap) snprintf(note, cap, "请先设置同步账号");
        return OS_SYNC_NO_CONFIG;
    }
    os_sync_result_t result = os_sync_register(&config);
    if (note && cap) snprintf(note, cap, "%s", os_sync_result_name(result));
    return result;
}
