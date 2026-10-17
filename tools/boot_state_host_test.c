/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 * 中文：用内存 NVS 替身验证启动事务、开书事务与中断书屏蔽。
 * English: Verify the startup transaction, book-open transaction and interrupted-book gating with an in-memory NVS fake.
 * 冻结：只用于宿主测试。/ Frozen: Host testing only.
 */
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "boot_state.h"
#include "nvs.h"

// ---- 内存 NVS 替身：一个命名空间两个键足够。/ In-memory NVS fake: one namespace, one key suffices. ----
static unsigned char nvs_store[1024];
static size_t nvs_store_len;
static bool nvs_fail_writes;
static int nvs_handle_counter;
esp_err_t nvs_open(const char* ns, int mode, nvs_handle_t* h) {
    (void)ns; (void)mode;
    *h = ++nvs_handle_counter;
    return ESP_OK;
}
void nvs_close(nvs_handle_t h) { (void)h; }
esp_err_t nvs_get_blob(nvs_handle_t h, const char* key, void* out, size_t* len) {
    (void)h;
    if (strcmp(key, "state")) return ESP_ERR_NVS_NOT_FOUND;
    if (!nvs_store_len) return ESP_ERR_NVS_NOT_FOUND;
    assert(*len >= nvs_store_len);
    memcpy(out, nvs_store, nvs_store_len);
    *len = nvs_store_len;
    return ESP_OK;
}
esp_err_t nvs_set_blob(nvs_handle_t h, const char* key, const void* data, size_t len) {
    (void)h;
    if (strcmp(key, "state")) return ESP_ERR_NVS_NOT_FOUND;
    if (nvs_fail_writes) return ESP_ERR_NVS_NOT_ENOUGH_SPACE;
    assert(len <= sizeof(nvs_store));
    memcpy(nvs_store, data, len);
    nvs_store_len = len;
    return ESP_OK;
}
esp_err_t nvs_commit(nvs_handle_t h) { (void)h; return ESP_OK; }

static void reset_nvs(void) { nvs_store_len = 0; nvs_fail_writes = false; }

int main(void) {
    // 全新设备：无记录，正常启动并写入 starting=1。/ Fresh device: no record, normal start writes starting=1.
    reset_nvs();
    pico_boot_init(false);
    assert(!pico_boot_recovery());
    pico_boot_ready();
    // 模拟下一次正常开机：starting 已清、无 opening，不进恢复。/ Next normal boot: starting cleared, no opening, no recovery.
    pico_boot_init(false);
    assert(!pico_boot_recovery() && !pico_boot_interrupted_book((char[8]){0}, 8));
    assert(pico_boot_asset_allowed("/sdcard/books/a.txt"));

    // 开书事务：begin 落盘，end 成功清空并解除屏蔽。/ Open transaction: begin persists, a successful end clears both.
    assert(pico_boot_book_begin("/sdcard/books/大书.epub") == ESP_OK);
    pico_boot_book_end(true);
    // 模拟开书中断：begin 后直接重启。/ Simulate an interrupted open: reboot right after begin.
    assert(pico_boot_book_begin("/sdcard/books/坏书.epub") == ESP_OK);
    pico_boot_init(false);
    char book[288];
    assert(pico_boot_recovery());
    assert(pico_boot_interrupted_book(book, sizeof(book)));
    assert(!strcmp(book, "/sdcard/books/坏书.epub"));
    // 中断书被屏蔽，其他书不受影响；手动成功打开后解除。/ The interrupted book is gated, others are not; a successful manual open clears it.
    assert(!pico_boot_asset_allowed("/sdcard/books/坏书.epub"));
    assert(pico_boot_asset_allowed("/sdcard/books/别的书.txt"));
    assert(pico_boot_book_begin("/sdcard/books/坏书.epub") == ESP_OK);
    pico_boot_book_end(true);
    assert(pico_boot_asset_allowed("/sdcard/books/坏书.epub"));
    // 优雅失败不重建屏蔽（用户已看到错误，非中断）；再次中断才重建，forget 显式解除。
    // A graceful failure never rebuilds the gate (the user saw the error, not an interruption); only another interruption does, and forget releases it.
    assert(pico_boot_book_begin("/sdcard/books/坏书.epub") == ESP_OK);
    pico_boot_book_end(false);
    assert(pico_boot_asset_allowed("/sdcard/books/坏书.epub"));
    assert(pico_boot_book_begin("/sdcard/books/坏书.epub") == ESP_OK);
    pico_boot_init(false);
    assert(!pico_boot_asset_allowed("/sdcard/books/坏书.epub"));
    pico_boot_forget_interrupted_book();
    assert(pico_boot_asset_allowed("/sdcard/books/坏书.epub"));
    assert(!pico_boot_interrupted_book(book, sizeof(book)));

    // 半途启动（starting 残留）判定为恢复。/ A half-finished startup (starting left set) counts as recovery.
    reset_nvs();
    pico_boot_init(false);
    pico_boot_init(false);
    assert(pico_boot_recovery());

    // 记录损坏：校验不过即重置并进恢复，不清阅读分区。/ Corrupt record: checksum failure resets and enters recovery.
    reset_nvs();
    pico_boot_init(false);
    pico_boot_ready();
    nvs_store[3] ^= 0x80;
    pico_boot_init(false);
    assert(pico_boot_recovery());

    // 路径白名单与逐段校验。/ Path allowlist and per-segment checks.
    assert(pico_boot_book_begin(NULL) == ESP_ERR_INVALID_ARG);
    assert(pico_boot_book_begin("/flash/books/x.txt") == ESP_OK);
    assert(pico_boot_book_begin("/sdcard/../etc/passwd") == ESP_ERR_INVALID_ARG);
    assert(pico_boot_book_begin("/sdcard//double") == ESP_ERR_INVALID_ARG);
    assert(pico_boot_book_begin("relative/path") == ESP_ERR_INVALID_ARG);

    // NVS 写失败：begin 拒绝开书，init 落盘失败进恢复。/ NVS write failure: begin refuses the open; a failing init store enters recovery.
    reset_nvs();
    nvs_fail_writes = true;
    assert(pico_boot_book_begin("/sdcard/b.txt") != ESP_OK);
    pico_boot_init(false);
    assert(pico_boot_recovery());
    puts("boot_state_host_test: PASS");
    return 0;
}
