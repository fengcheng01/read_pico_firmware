/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 * 中文：设备桥夹具——只仿真存储策略层：书根指向真实夹具目录，容量为静态值。
 * 上方的书架扫描、解析、进度、统计全部是真实固件代码。
 * English: Device-bridge fixtures faking only the storage-policy layer: book
 * roots point at real fixture directories with static capacities. Everything
 * above — shelf scanning, parsing, progress, stats — is real firmware code.
 * 冻结：夹具不格式化不写卡、不删除文件；夹具切换换目录并推进版本号。
 * Frozen: Fixtures never format, write or delete files; switching repoints the
 * directories and bumps the revision.
 */
#include "book_store.h"
#include "book_progress.h"
#include "os_crash.h"
#include "read_pico_sd.h"
#include <sys/stat.h>
#include <stdio.h>
#include <string.h>

static int s_fixture = 1;
static unsigned s_revision;

void preview_fixture(int value) {
    if (value < 0 || value > 2) value = 1;
    if (value == s_fixture) return;
    s_fixture = value;
    ++s_revision;
}

static const char* root_dir(void) {
    return s_fixture == 0 ? "build-host/desktop-preview/fx/e"
                          : "build-host/desktop-preview/fx/f";
}

esp_err_t book_store_roots(book_store_root_t out[2], int* n) {
    out[0].is_flash = false;
    snprintf(out[0].path, sizeof(out[0].path), "%s", root_dir());
    out[1].is_flash = true;
    snprintf(out[1].path, sizeof(out[1].path), "%s", "build-host/desktop-preview/fx/h");
    *n = 2;
    return ESP_OK;
}
esp_err_t book_store_read_roots(book_store_root_t out[2], int* n) { return book_store_roots(out, n); }
bool book_store_roots_degraded(void) { return s_fixture == 2; }
esp_err_t book_store_upload_root(book_store_root_t* out) {
    book_store_root_t roots[2];
    int count = 0;
    esp_err_t err = book_store_roots(roots, &count);
    if (err == ESP_OK && out) *out = roots[0];
    return err;
}
size_t book_store_file_limit(const book_store_root_t* root) {
    return root && root->is_flash ? BOOK_STORE_FLASH_FILE_MAX : (size_t)-1;
}
uint64_t book_store_free_bytes(const book_store_root_t* root) {
    return root && root->is_flash ? 3u * 1024 * 1024 + 400 * 1024 : 14ull * 1024 * 1024 * 1024;
}
bool book_store_flash_ready(void) { return true; }
esp_err_t book_store_delete(const char* path, bool* removed) {
    // 预览不删除真实夹具文件；报告未删除，由页面按真实语义提示。
    // Preview never deletes real fixture files; report not removed so pages explain per real semantics.
    (void)path;
    if (removed) *removed = false;
    return ESP_ERR_NOT_SUPPORTED;
}
void book_store_notify_changed(void) { ++s_revision; }
unsigned book_store_revision(void) { return s_revision; }

// 探测立即完成、无卡：os_storage_probe_complete 走真实 os_device_pico。
// Probing completes at once with no card; os_storage_probe_complete runs the real os_device_pico.
void read_pico_sd_start_probe(void) {}

// 夹具切换：换目录并推进版本，真实首页/书架会随版本重扫。
// Fixture switching: repoint directories and bump the revision for a real rescan.
// 满夹具播种真实进度：路径与文件大小都取自真实夹具文件，走真 book_progress。
// The full fixture seeds real progress: paths and sizes come from the real
// fixture files through the real book_progress module.
static void seed_progress(const char* name, uint8_t pct, uint32_t sequence) {
    char path[BOOK_STORE_PATH_MAX];
    snprintf(path, sizeof(path), "build-host/desktop-preview/fx/f/%s", name);
    struct stat st;
    if (stat(path, &st) || st.st_size <= 0) return;
    book_progress_t progress = {
        .file_size = (uint32_t)st.st_size, .chapter = 0, .byte_off = 0,
        .px = 48, .pct = pct, .last_open_s = sequence,
    };
    if (book_progress_save(path, &progress) == ESP_OK && sequence >= 300)
        (void)book_progress_set_last_path(path);
}

void preview_home_fixture(int value) {
    preview_fixture(value);
    seed_progress("日常阅读.txt", 38, 300);
    seed_progress("纸上的时间.txt", 48, 200);
    seed_progress("阅读记录.txt", 58, 100);
}
