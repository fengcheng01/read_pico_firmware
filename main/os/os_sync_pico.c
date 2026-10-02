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
 * 冻结：用户批准非阻塞同步；UI 准备/应用快照，单个后台请求只做网络；离页取消并收齐；密码仅以 MD5 落盘；
 * 用户反馈后由 UI 按需启动已保存 WiFi，后台等到 STA 可用才请求；独占联网不启动传书 HTTP，结果收齐后释放。
 * 拉取只在文件大小匹配时套用 rp1 精确位置，否则按百分比近似并明示。
 * Frozen: User-approved nonblocking sync: UI prepares/applies snapshots, one worker only networks, and exit cancels/joins; no
 * periodic background polling; UI owns on-demand saved-WiFi sessions, the worker waits for STA and network-only sessions never start upload HTTP; the password persists only as MD5; pulls apply the exact
 * rp1 position only when file sizes match, otherwise approximating by
 * percentage and saying so.
 */
#include "os_sync.h"
#include "os_sync_http.h"
#include "book_progress.h"
#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_rom_md5.h"
#include "settings.h"
#include "read_pico_transfer.h"
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <stdatomic.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

static const char* TAG = "os_sync";

static bool sync_cancelled(void);
typedef struct { esp_http_client_handle_t client; int64_t deadline; } sync_stream_ctx_t;
static bool stream_active(void* arg) {
    sync_stream_ctx_t* ctx = arg;
    return !sync_cancelled() && esp_timer_get_time() < ctx->deadline;
}
static int stream_write(void* arg, const char* data, size_t size) {
    sync_stream_ctx_t* ctx = arg;
    return esp_http_client_write(ctx->client, data, (int)size);
}
static int stream_read(void* arg, char* data, size_t size) {
    sync_stream_ctx_t* ctx = arg;
    return esp_http_client_read(ctx->client, data, (int)size);
}
static bool stream_complete(void* arg) {
    sync_stream_ctx_t* ctx = arg;
    return esp_http_client_is_complete_data_received(ctx->client);
}
static int sync_request(const char* method, const char* url, const char* user, const char* key,
                        const char* content_type, const char* body, char* resp, size_t resp_cap) {
    if (resp && resp_cap) resp[0] = 0;
    esp_http_client_config_t config = {
        .url = url,
        .method = strcmp(method, "PUT") == 0 ? HTTP_METHOD_PUT :
                  strcmp(method, "POST") == 0 ? HTTP_METHOD_POST : HTTP_METHOD_GET,
        .timeout_ms = 5000,
        .crt_bundle_attach = strncmp(url, "https://", 8) == 0 ? esp_crt_bundle_attach : NULL,
        .buffer_size = 1024,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) return -1;
    sync_stream_ctx_t ctx = {client, esp_timer_get_time() + 20000000};
    os_sync_stream_t io = {stream_write, stream_read, stream_complete, stream_active, &ctx};
    int status = -1;
    bool headers_ok = esp_http_client_set_header(client, "Accept", "application/vnd.koreader.v1+json") == ESP_OK;
    if (user && key) {
        headers_ok &= esp_http_client_set_header(client, "x-auth-user", user) == ESP_OK;
        headers_ok &= esp_http_client_set_header(client, "x-auth-key", key) == ESP_OK;
    }
    if (content_type) headers_ok &= esp_http_client_set_header(client, "Content-Type", content_type) == ESP_OK;
    if (headers_ok && stream_active(&ctx) &&
        esp_http_client_open(client, body ? (int)strlen(body) : 0) == ESP_OK &&
        os_sync_http_write(&io, body) && stream_active(&ctx) &&
        esp_http_client_fetch_headers(client) >= 0 && os_sync_http_read(&io, resp, resp_cap))
        status = esp_http_client_get_status_code(client);
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

/* ---- 单请求快照 / Single-request snapshot ---- */
static SemaphoreHandle_t s_done;
static bool s_running, s_pending, s_network_owned, s_claimed;
static void release_session(void) {
    if (s_claimed) read_pico_transfer_release_sync();
    s_claimed = false;
    if (s_network_owned) read_pico_transfer_stop();
    s_network_owned = false;
}
static bool prepare_network(char* note, size_t cap) {
    if (!read_pico_transfer_claim_sync()) { snprintf(note, cap, "正在上传或删除，请完成后再同步"); return false; }
    s_claimed = true;
    read_pico_transfer_status_t status;
    read_pico_transfer_get_status(&status);
    if (status.state != READ_PICO_TRANSFER_STOPPED && status.mode == READ_PICO_TRANSFER_MODE_STA &&
        status.state != READ_PICO_TRANSFER_ERROR) return true;
    if (!read_pico_transfer_try_stop_if_idle()) { release_session(); return false; }
    if (read_pico_transfer_start_saved_network() != ESP_OK) {
        snprintf(note, cap, "无法连接已保存 WiFi，请先到设置配置网络"); release_session(); return false;
    }
    s_network_owned = true;
    return true;
}
static atomic_bool s_cancel, s_link_ready;
static os_sync_job_t s_job;
static os_sync_config_t s_config;
static char s_path[288], s_doc[33], s_position[OS_SYNC_PROGRESS_MAX];
static book_progress_t s_local, s_remote;
static bool s_has_local;
static uint32_t s_size;
static float s_percent;
static os_sync_result_t s_result;
static bool sync_cancelled(void) { return atomic_load(&s_cancel); }

static void sync_worker(void* arg) {
    (void)arg;
    int64_t deadline = esp_timer_get_time() + 16000000;
    read_pico_transfer_status_t status;
    do {
        read_pico_transfer_get_status(&status);
        if (status.mode == READ_PICO_TRANSFER_MODE_STA && status.network_ready) break;
        if (sync_cancelled() || status.state == READ_PICO_TRANSFER_ERROR ||
            status.state == READ_PICO_TRANSFER_STOPPED || esp_timer_get_time() >= deadline) {
            s_result = OS_SYNC_OFFLINE; goto done;
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    } while (true);
    atomic_store(&s_link_ready, true);
    if (sync_cancelled()) { s_result = OS_SYNC_OFFLINE; goto done; }
    if (s_job == OS_SYNC_JOB_AUTH) s_result = os_sync_auth(&s_config);
    else if (s_job == OS_SYNC_JOB_REGISTER) s_result = os_sync_register(&s_config);
    else if (s_job == OS_SYNC_JOB_PUSH) s_result = os_sync_push(&s_config, s_doc, s_position, s_percent);
    else s_result = os_sync_pull(&s_config, s_doc, s_position, sizeof(s_position), &s_percent);
    // 信号发出后不再访问快照，UI 可以安全接收或离页。/ After signaling, never access snapshots; UI may receive or exit safely.
done:
    xSemaphoreGive(s_done);
    vTaskDelete(NULL);
}
bool os_sync_job_busy(void) { return s_running; }
bool os_sync_pull_pending(void) { return s_pending; }
bool os_sync_job_start(os_sync_job_t job, char* note, size_t cap) {
    if (!note || !cap || s_running || s_pending || job < OS_SYNC_JOB_AUTH || job > OS_SYNC_JOB_PULL) return false;
    load_config(&s_config);
    if (!os_sync_config_ready(&s_config)) { snprintf(note, cap, "请先设置同步账号"); return false; }
    if (!prepare_network(note, cap)) return false;
    s_job = job;
    if (job == OS_SYNC_JOB_PUSH || job == OS_SYNC_JOB_PULL) {
        struct stat st;
        if (!book_progress_last_path(s_path, sizeof(s_path)) || !s_path[0] ||
            stat(s_path, &st) || st.st_size <= 0 || st.st_size > UINT32_MAX || !sync_doc_id(s_path, s_doc)) {
            snprintf(note, cap, "图书文件或阅读记录不可用"); release_session(); return false;
        }
        s_size = (uint32_t)st.st_size;
        memset(&s_local, 0, sizeof(s_local));
        s_has_local = book_progress_load(s_path, s_size, &s_local);
        if (job == OS_SYNC_JOB_PUSH && !s_has_local) { snprintf(note, cap, "还没有阅读记录"); release_session(); return false; }
        if (job == OS_SYNC_JOB_PUSH) {
            if (s_local.approximate) snprintf(s_position, sizeof(s_position), "percentage");
            else if (!os_sync_progress_encode(s_position, sizeof(s_position), s_size, s_local.chapter, s_local.byte_off, s_local.px)) { release_session(); return false; }
            s_percent = s_local.pct > 100 ? 1.0f : s_local.pct / 100.0f;
        }
    }
    if (!s_done) s_done = xSemaphoreCreateBinary();
    if (!s_done) { snprintf(note, cap, "内存不足，请重试"); release_session(); return false; }
    atomic_store(&s_cancel, false);
    atomic_store(&s_link_ready, false);
    s_running = true;
    if (xTaskCreate(sync_worker, "progress_sync", 8192, NULL, 3, NULL) != pdPASS) {
        s_running = false; release_session(); snprintf(note, cap, "无法启动同步，请重试"); return false;
    }
    snprintf(note, cap, "%s", "正在连接已保存 WiFi 并同步…");
    return true;
}
bool os_sync_job_poll(char* note, size_t cap) {
    if (!s_running) return false;
    read_pico_transfer_service_poll();
    if (xSemaphoreTake(s_done, 0) != pdTRUE) return false;
    s_running = false;
    if (sync_cancelled()) s_result = OS_SYNC_OFFLINE;
    if (s_result != OS_SYNC_OK || s_job != OS_SYNC_JOB_PULL) {
        release_session();
        const char* result = s_result == OS_SYNC_OFFLINE ?
            (atomic_load(&s_link_ready) ? "无法访问同步服务器，请检查地址与网络" : "WiFi 连接失败，请检查已保存网络") : os_sync_result_name(s_result);
        snprintf(note, cap, "%s", result); return true;
    }
    memset(&s_remote, 0, sizeof(s_remote));
    uint32_t size, off;
    uint16_t chapter;
    uint8_t px;
    bool exact = os_sync_progress_decode(s_position, &size, &chapter, &off, &px) && size == s_size;
    s_remote.file_size = s_size;
    s_remote.approximate = !exact;
    s_remote.px = app_settings_book_px();
    if (exact) {
        s_remote.chapter = chapter; s_remote.byte_off = off;
        s_remote.px = px >= 36 && px <= 72 ? px : 48;
    }
    s_remote.pct = (uint8_t)(s_percent * 100 + 0.5f);
    if (s_network_owned) read_pico_transfer_stop();
    s_network_owned = false;
    s_pending = true;
    snprintf(note, cap, "本地 %u%% → 远端 %u%% · %s", (unsigned)s_local.pct, (unsigned)s_remote.pct,
             exact ? "精确位置" : "近似位置");
    return true;
}
static bool finish_pull_confirmation(bool apply, char* note, size_t cap) {
    if (!s_pending) return false;
    s_pending = false;
    if (!apply) { snprintf(note, cap, "已保留本地阅读位置"); return true; }
    struct stat st;
    char doc[33];
    book_progress_t current = {0};
    bool has = book_progress_load(s_path, s_size, &current);
    bool unchanged = has == s_has_local && (!has || (current.last_open_s == s_local.last_open_s &&
        current.chapter == s_local.chapter && current.byte_off == s_local.byte_off && current.pct == s_local.pct));
    if (stat(s_path, &st) || st.st_size != s_size || !sync_doc_id(s_path, doc) || strcmp(doc, s_doc) || !unchanged) {
        snprintf(note, cap, "图书或本地进度已变化，请重新下载"); return false;
    }
    if (book_progress_save(s_path, &s_remote) != ESP_OK || book_progress_set_last_path(s_path) != ESP_OK) {
        snprintf(note, cap, "进度保存失败，请重新下载"); return false;
    }
    snprintf(note, cap, "已应用远端位置 %u%%", (unsigned)s_remote.pct);
    return true;
}
bool os_sync_pull_confirm(bool apply, char* note, size_t cap) {
    if (!s_pending) return false;
    bool result = finish_pull_confirmation(apply, note, cap);
    release_session();
    return result;
}
void os_sync_job_request_cancel(void) { atomic_store(&s_cancel, true); }
void os_sync_job_cancel(void) {
    os_sync_job_request_cancel();
    if (s_running) { xSemaphoreTake(s_done, portMAX_DELAY); s_running = false; }
    s_pending = false;
    release_session();
    memset(&s_config, 0, sizeof(s_config));
}
