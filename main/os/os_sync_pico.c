/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 * 冻结：同步联网前确认蓝牙完整停栈，停止失败直接报告，不启动 WiFi。
 * Frozen: Confirm full BLE teardown before sync networking; report stop failure without starting WiFi.
 *
 * 中文：同步核心的设备桥。HTTP 走 esp_http_client（https 校验证书，不自跳过），
 * 文档标识为 KOReader 部分 MD5；推/拉作用在“最后一本书”的进度记录上。
 * English: Device bridge for the sync core. HTTP uses esp_http_client (https
 * verifies certificates, never skipped), document identity is the KOReader
 * partial MD5, and push/pull act on the last-read book's progress record.
 *
 * 冻结：用户批准非阻塞同步；UI 准备/应用快照，后台不读写NVS；离页取消并收齐；密码仅以 MD5 落盘；
 * 用户反馈后由 UI 按需启动已保存 WiFi，后台等到 STA 可用才请求；独占联网不启动传书 HTTP，结果收齐后释放。
 * 用户要求提高KOReader定位：后台可独立只读解析EPUB真实祖先、文本节点与Unicode偏移；旧段落仍兼容，rp1需文件大小匹配，其余明示百分比近似。
 * 拉取只应用位置，字号保留本机记录，不恢复远端或全局默认档位。
 * Frozen: User-approved nonblocking sync: UI prepares/applies snapshots, the worker never accesses NVS, and exit cancels/joins; no
 * periodic background polling; UI owns on-demand saved-WiFi sessions, the worker waits for STA and network-only sessions never start upload HTTP; the password persists only as MD5; pulls apply the exact
 * rp1 position only when file sizes match; for user-requested accuracy, the worker independently reads EPUB ancestry, text nodes and Unicode offsets, retaining legacy paragraphs and clearly approximating unsupported positions by percentage;
 * pulls apply positions only while font tiers stay local, never restoring remote or global defaults.
 * 冻结修订：用户要求首页显式同步先查远端，按 0.01% 精度保留更远位置；远端无记录或不更远才上传，手动下载仍需确认。
 * Frozen revision: Explicit home sync checks remote first and keeps the farther 0.01%-precision position; push only when remote is absent or not ahead, while manual pulls still require confirmation.
 */
#include "os_sync.h"
#include "os_page_turner.h"
#include "os_time.h"
#include "os_sync_http.h"
#include "book_progress.h"
#include "book_epub.h"
#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_rom_md5.h"
#include "settings.h"
#include "read_pico_transfer.h"
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <stdatomic.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

static const char* TAG = "os_sync";

static bool sync_cancelled(void);
static char s_transport_error[128];
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
    s_transport_error[0] = 0;
    esp_http_client_config_t config = {
        .url = url,
        .method = strcmp(method, "PUT") == 0 ? HTTP_METHOD_PUT :
                  strcmp(method, "POST") == 0 ? HTTP_METHOD_POST : HTTP_METHOD_GET,
        .timeout_ms = 15000,
        .crt_bundle_attach = strncmp(url, "https://", 8) == 0 ? esp_crt_bundle_attach : NULL,
        .buffer_size = 2048,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) { snprintf(s_transport_error, sizeof(s_transport_error), "同步内存不足，请退出图书后重试"); return -1; }
    sync_stream_ctx_t ctx = {client, esp_timer_get_time() + 20000000};
    os_sync_stream_t io = {stream_write, stream_read, stream_complete, stream_active, &ctx};
    int status = -1;
    bool headers_ok = esp_http_client_set_header(client, "Accept", "application/vnd.koreader.v1+json") == ESP_OK;
    if (user && key) {
        headers_ok &= esp_http_client_set_header(client, "x-auth-user", user) == ESP_OK;
        headers_ok &= esp_http_client_set_header(client, "x-auth-key", key) == ESP_OK;
    }
    if (content_type) headers_ok &= esp_http_client_set_header(client, "Content-Type", content_type) == ESP_OK;
    headers_ok &= esp_http_client_set_header(client, "Connection", "close") == ESP_OK;
    const char* stage = "headers";
    esp_err_t opened = ESP_OK;
    if (headers_ok && stream_active(&ctx)) {
        stage = "connect";
        opened = esp_http_client_open(client, body ? (int)strlen(body) : 0);
        if (opened == ESP_OK) {
            stage = "write";
            if (os_sync_http_write(&io, body) && stream_active(&ctx)) {
                stage = "response";
                if (esp_http_client_fetch_headers(client) >= 0) {
                    stage = "body";
                    if (os_sync_http_read(&io, resp, resp_cap)) status = esp_http_client_get_status_code(client);
                }
            }
        }
    }
    if (status < 0) {
        int tls = 0, flags = 0;
        esp_http_client_get_and_clear_last_tls_error(client, &tls, &flags);
        int socket_error = esp_http_client_get_errno(client);
        snprintf(s_transport_error, sizeof(s_transport_error), "%s E%x TLS%x/%x net%d", stage,
                 (unsigned)opened, (unsigned)tls, (unsigned)flags, socket_error);
        ESP_LOGW(TAG, "sync transport: %s", s_transport_error);
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

/* ---- 单请求快照 / Single-request snapshot ---- */
static SemaphoreHandle_t s_done;
static TaskHandle_t s_worker_task;
static StaticTask_t s_worker_tcb;
static StackType_t* s_worker_stack;
#define SYNC_STACK_BYTES 12288
static bool s_running, s_pending, s_network_owned, s_claimed;
static void release_session(void) {
    if (s_claimed) read_pico_transfer_release_sync();
    s_claimed = false;
    if (s_network_owned) { os_time_network(false); read_pico_transfer_stop(); }
    s_network_owned = false;
}
static bool prepare_network(char* note, size_t cap) {
    if (!os_page_turner_pause()) { snprintf(note, cap, "请先关闭蓝牙再同步"); return false; }
    char ssid[33];
    bool configured = false;
    if (read_pico_transfer_get_saved_wifi(ssid, &configured) != ESP_OK || !configured) {
        snprintf(note, cap, "请先到设置配置 WiFi");
        return false;
    }
    if (!read_pico_transfer_claim_sync()) { snprintf(note, cap, "正在上传或删除，请完成后再同步"); return false; }
    s_claimed = true;
    read_pico_transfer_status_t status;
    read_pico_transfer_get_status(&status);
    if (status.state != READ_PICO_TRANSFER_STOPPED && status.mode == READ_PICO_TRANSFER_MODE_STA &&
        status.state != READ_PICO_TRANSFER_ERROR) return true;
    if (!read_pico_transfer_try_stop_if_idle()) { release_session(); return false; }
    esp_err_t err = read_pico_transfer_start_saved_network();
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "saved WiFi start failed: %s", esp_err_to_name(err));
        snprintf(note, cap, "%s", err == ESP_ERR_NO_MEM ? "同步联网内存不足，请稍后重试" : "WiFi 启动失败，请稍后重试");
        release_session(); return false;
    }
    s_network_owned = true;
    return true;
}
static atomic_bool s_cancel, s_link_ready;
static os_sync_job_t s_job;
static os_sync_config_t s_config;
static char s_path[288], s_doc[33], s_position[OS_SYNC_PROGRESS_MAX];
static char s_remote_position[OS_SYNC_PROGRESS_MAX];
static book_progress_t s_local, s_remote;
static bool s_has_local;
static uint32_t s_size;
static float s_percent, s_remote_percent;
static float s_confirmed_percent;
static os_sync_result_t s_result;
static bool s_remote_ahead, s_merge_apply;
static bool s_paragraph;
static uint16_t s_paragraph_chapter;
static uint32_t s_paragraph_byte;
static bool sync_cancelled(void) { return atomic_load(&s_cancel); }
static unsigned percent_points(float value) {
    unsigned points = (unsigned)(value * 10000 + 0.5f);
    return points > 10000 ? 10000 : points;
}

static void sync_worker(void* arg) {
    (void)arg;
    for (;;) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        // 关联路由器和 DHCP 各有独立时限；后台等待覆盖完整建链窗口。
        // Router association and DHCP have separate deadlines; cover the complete link window here.
        int64_t deadline = esp_timer_get_time() + 55000000;
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
        const char* ext = strrchr(s_path, '.');
        bool epub = ext && !strcasecmp(ext, ".epub");
        if ((s_job == OS_SYNC_JOB_PUSH || s_job == OS_SYNC_JOB_MERGE) &&
            s_has_local && epub && !s_local.approximate) {
            char position[OS_SYNC_PROGRESS_MAX];
            if (book_epub_sync_encode(s_path, s_local.chapter, s_local.byte_off, position, sizeof(position)))
                memcpy(s_position, position, strlen(position) + 1);
        }
        if (sync_cancelled()) { s_result = OS_SYNC_OFFLINE; goto done; }
        if (s_job == OS_SYNC_JOB_AUTH) s_result = os_sync_auth(&s_config);
        else if (s_job == OS_SYNC_JOB_REGISTER) s_result = os_sync_register(&s_config);
        else if (s_job == OS_SYNC_JOB_PUSH) {
            s_result = os_sync_push(&s_config, s_doc, s_position, s_percent);
            if (s_result == OS_SYNC_OK && !sync_cancelled()) {
                char confirmed_position[OS_SYNC_PROGRESS_MAX];
                float confirmed = 0;
                os_sync_result_t readback = os_sync_pull(&s_config, s_doc, confirmed_position,
                                                          sizeof(confirmed_position), &confirmed);
                if (readback != OS_SYNC_OK || confirmed + 0.00005f < s_percent) {
                    ESP_LOGW(TAG, "push readback failed result=%d sent=%.4f confirmed=%.4f",
                             readback, (double)s_percent, (double)confirmed);
                    s_result = OS_SYNC_VERIFY;
                } else {
                    s_confirmed_percent = confirmed;
                    s_remote_ahead = confirmed > s_percent + 0.00005f;
                    ESP_LOGI(TAG, "push confirmed doc=%s sent=%.4f remote=%.4f",
                             s_doc, (double)s_percent, (double)confirmed);
                }
            }
        }
        else if (s_job == OS_SYNC_JOB_MERGE) {
            s_result = os_sync_pull(&s_config, s_doc, s_remote_position,
                                    sizeof(s_remote_position), &s_remote_percent);
            if (s_result == OS_SYNC_OK) {
                s_merge_apply = !s_has_local || percent_points(s_remote_percent) > percent_points(s_percent);
            } else if (s_result != OS_SYNC_NOT_FOUND) goto done;
            if (!s_merge_apply) {
                if (!s_has_local) { s_result = OS_SYNC_NOT_FOUND; goto done; }
                if (sync_cancelled()) { s_result = OS_SYNC_OFFLINE; goto done; }
                s_result = os_sync_push(&s_config, s_doc, s_position, s_percent);
                if (s_result != OS_SYNC_OK || sync_cancelled()) goto done;
                s_result = os_sync_pull(&s_config, s_doc, s_remote_position,
                                        sizeof(s_remote_position), &s_remote_percent);
                if (s_result != OS_SYNC_OK || percent_points(s_remote_percent) < percent_points(s_percent)) {
                    ESP_LOGW(TAG, "merge push readback failed result=%d sent=%.4f confirmed=%.4f",
                             s_result, (double)s_percent, (double)s_remote_percent);
                    s_result = OS_SYNC_VERIFY; goto done;
                }
                s_merge_apply = percent_points(s_remote_percent) > percent_points(s_percent);
                s_confirmed_percent = s_remote_percent;
            }
            if (s_merge_apply && epub && !sync_cancelled())
                s_paragraph = book_epub_sync_decode(s_path, s_remote_position,
                                                    &s_paragraph_chapter, &s_paragraph_byte);
        }
        else s_result = os_sync_pull(&s_config, s_doc, s_position, sizeof(s_position), &s_percent);
        if (s_job == OS_SYNC_JOB_PULL && s_result == OS_SYNC_OK && epub && !sync_cancelled())
            s_paragraph = book_epub_sync_decode(s_path, s_position, &s_paragraph_chapter, &s_paragraph_byte);
        // 信号发出后直到下次通知都不访问快照，UI 可以安全接收或离页。
        // After signaling, snapshots stay untouched until the next notification so UI may receive or exit safely.
done:
        xSemaphoreGive(s_done);
    }
}

static bool ensure_worker(char* note, size_t cap) {
    if (s_worker_task) return true;
    s_worker_stack = heap_caps_malloc(SYNC_STACK_BYTES, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!s_worker_stack) { snprintf(note, cap, "同步任务内存不足，请重试"); return false; }
    s_worker_task = xTaskCreateStatic(sync_worker, "progress_sync", SYNC_STACK_BYTES, NULL, 3,
                                      s_worker_stack, &s_worker_tcb);
    if (!s_worker_task) {
        heap_caps_free(s_worker_stack);
        s_worker_stack = NULL;
        snprintf(note, cap, "无法启动同步，请重试");
        return false;
    }
    return true;
}
bool os_sync_job_busy(void) { return s_running; }
bool os_sync_pull_pending(void) { return s_pending; }
static bool start_job(os_sync_job_t job, const char* path, char* note, size_t cap) {
    if (!note || !cap || s_running || s_pending || job < OS_SYNC_JOB_AUTH || job > OS_SYNC_JOB_MERGE) return false;
    load_config(&s_config);
    if (!os_sync_config_ready(&s_config)) { snprintf(note, cap, "请先设置同步账号"); return false; }
    if (!s_done) s_done = xSemaphoreCreateBinary();
    if (!s_done) { snprintf(note, cap, "内存不足，请重试"); return false; }
    if (!ensure_worker(note, cap)) return false;
    if (!prepare_network(note, cap)) return false;
    s_job = job;
    s_paragraph = false;
    s_remote_ahead = false;
    s_merge_apply = false;
    s_remote_percent = 0;
    s_confirmed_percent = 0;
    if (job == OS_SYNC_JOB_PUSH || job == OS_SYNC_JOB_PULL || job == OS_SYNC_JOB_MERGE) {
        struct stat st;
        if (path) {
            int copied = snprintf(s_path, sizeof(s_path), "%s", path);
            if (!path[0] || copied < 0 || (size_t)copied >= sizeof(s_path)) {
                snprintf(note, cap, "图书路径不可用"); release_session(); return false;
            }
        } else if (!book_progress_last_path(s_path, sizeof(s_path))) {
            snprintf(note, cap, "图书文件或阅读记录不可用"); release_session(); return false;
        }
        if (!s_path[0] ||
            stat(s_path, &st) || st.st_size <= 0 || st.st_size > UINT32_MAX || !sync_doc_id(s_path, s_doc)) {
            snprintf(note, cap, "图书文件或阅读记录不可用"); release_session(); return false;
        }
        s_size = (uint32_t)st.st_size;
        memset(&s_local, 0, sizeof(s_local));
        s_has_local = book_progress_load(s_path, s_size, &s_local);
        if (job == OS_SYNC_JOB_PUSH && !s_has_local) { snprintf(note, cap, "还没有阅读记录"); release_session(); return false; }
        if (job == OS_SYNC_JOB_PUSH || job == OS_SYNC_JOB_MERGE) {
            if (s_has_local && s_local.approximate) snprintf(s_position, sizeof(s_position), "percentage");
            else if (s_has_local && !os_sync_progress_encode(s_position, sizeof(s_position), s_size, s_local.chapter, s_local.byte_off, s_local.px)) { release_session(); return false; }
            unsigned basis_points = (unsigned)s_local.pct * 100 + s_local.pct_frac;
            s_percent = basis_points > 10000 ? 1.0f : basis_points / 10000.0f;
        }
    }
    s_transport_error[0] = 0;
    os_time_force_poll(); os_time_poll(esp_timer_get_time() / 1000);
    atomic_store(&s_cancel, false);
    atomic_store(&s_link_ready, false);
    s_running = true;
    xTaskNotifyGive(s_worker_task);
    snprintf(note, cap, "%s", "正在连接已保存 WiFi 并同步…");
    return true;
}
bool os_sync_job_start(os_sync_job_t job, char* note, size_t cap) {
    return start_job(job, NULL, note, cap);
}
bool os_sync_job_start_book(const char* path, char* note, size_t cap) {
    return start_job(OS_SYNC_JOB_MERGE, path, note, cap);
}

static void stage_remote(const char* position, float percent, bool preserve_local_stats) {
    uint32_t size, off;
    uint16_t chapter;
    uint8_t px;
    bool exact = os_sync_progress_decode(position, &size, &chapter, &off, &px) && size == s_size;
    s_remote = preserve_local_stats && s_has_local ? s_local : (book_progress_t){0};
    s_remote.file_size = s_size;
    s_remote.approximate = !exact && !s_paragraph;
    // 字号是本机排版参数，不随进度同步。/ Font size is local layout and never syncs.
    s_remote.px = s_has_local ? s_local.px : app_settings_book_px();
    s_remote.chapter = exact ? chapter : s_paragraph ? s_paragraph_chapter : 0;
    s_remote.byte_off = exact ? off : s_paragraph ? s_paragraph_byte : 0;
    unsigned points = percent_points(percent);
    s_remote.pct = (uint8_t)(points / 100);
    s_remote.pct_frac = (uint8_t)(points % 100);
}

static bool snapshot_unchanged(void) {
    struct stat st;
    char doc[33];
    book_progress_t current = {0};
    bool has = book_progress_load(s_path, s_size, &current);
    bool unchanged = has == s_has_local && (!has || (current.last_open_s == s_local.last_open_s &&
        current.chapter == s_local.chapter && current.byte_off == s_local.byte_off &&
        current.pct == s_local.pct && current.pct_frac == s_local.pct_frac));
    return !stat(s_path, &st) && st.st_size == s_size && sync_doc_id(s_path, doc) &&
           !strcmp(doc, s_doc) && unchanged;
}
bool os_sync_job_poll(char* note, size_t cap) {
    if (!s_running) return false;
    read_pico_transfer_service_poll();
    read_pico_transfer_status_t network;
    read_pico_transfer_get_status(&network);
    os_time_network(network.network_ready);
    if (xSemaphoreTake(s_done, 0) != pdTRUE) return false;
    s_running = false;
    if (sync_cancelled()) s_result = OS_SYNC_OFFLINE;
    if (s_result != OS_SYNC_OK || (s_job != OS_SYNC_JOB_PULL && s_job != OS_SYNC_JOB_MERGE)) {
        release_session();
        const char* result = s_result == OS_SYNC_OFFLINE ?
            (atomic_load(&s_link_ready) ? (s_transport_error[0] ? s_transport_error : "服务器连接未完成，请重试") : "WiFi 连接失败，请检查已保存网络") : os_sync_result_name(s_result);
        if (s_result == OS_SYNC_OK && s_job == OS_SYNC_JOB_PUSH)
            snprintf(note, cap, s_remote_ahead ? "服务器已保留较新进度 %.2f%%" : "服务器已确认上传 %.2f%%",
                     (double)(s_confirmed_percent * 100));
        else if (s_job == OS_SYNC_JOB_MERGE && s_result == OS_SYNC_NOT_FOUND && !s_has_local)
            snprintf(note, cap, "远端暂无此书进度，本地也没有阅读记录");
        else snprintf(note, cap, "%s", result);
        return true;
    }
    const char* remote_position = s_job == OS_SYNC_JOB_MERGE ? s_remote_position : s_position;
    float remote_percent = s_job == OS_SYNC_JOB_MERGE ? s_remote_percent : s_percent;
    stage_remote(remote_position, remote_percent, s_job == OS_SYNC_JOB_MERGE);
    if (s_job == OS_SYNC_JOB_MERGE) {
        if (s_merge_apply) {
            if (!snapshot_unchanged()) snprintf(note, cap, "图书或本地进度已变化，请重新同步");
            else if (book_progress_save(s_path, &s_remote) != ESP_OK ||
                     book_progress_set_last_path(s_path) != ESP_OK)
                snprintf(note, cap, "进度保存失败，请重新同步");
            else snprintf(note, cap, "已同步远端进度 %u.%02u%% 到本机",
                          (unsigned)s_remote.pct, (unsigned)s_remote.pct_frac);
        } else snprintf(note, cap, "已将本地进度同步到远端 %u.%02u%%",
                        (unsigned)s_local.pct, (unsigned)s_local.pct_frac);
        release_session();
        return true;
    }
    uint32_t size, off;
    uint16_t chapter;
    uint8_t px;
    bool exact = os_sync_progress_decode(s_position, &size, &chapter, &off, &px) && size == s_size;
    if (s_network_owned) { os_time_network(false); read_pico_transfer_stop(); }
    s_network_owned = false;
    s_pending = true;
    snprintf(note, cap, "本地 %u.%02u%% → 远端 %u.%02u%% · %s",
             (unsigned)s_local.pct, (unsigned)s_local.pct_frac,
             (unsigned)s_remote.pct, (unsigned)s_remote.pct_frac,
             exact ? "精确位置" : s_paragraph ? (strstr(s_position,"/text()")?"文本位置":"段落位置") : "近似位置");
    return true;
}
static bool finish_pull_confirmation(bool apply, char* note, size_t cap) {
    if (!s_pending) return false;
    s_pending = false;
    if (!apply) { snprintf(note, cap, "已保留本地阅读位置"); return true; }
    if (!snapshot_unchanged()) {
        snprintf(note, cap, "图书或本地进度已变化，请重新下载"); return false;
    }
    if (book_progress_save(s_path, &s_remote) != ESP_OK || book_progress_set_last_path(s_path) != ESP_OK) {
        snprintf(note, cap, "进度保存失败，请重新下载"); return false;
    }
    snprintf(note, cap, "已应用远端位置 %u.%02u%%", (unsigned)s_remote.pct, (unsigned)s_remote.pct_frac);
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
