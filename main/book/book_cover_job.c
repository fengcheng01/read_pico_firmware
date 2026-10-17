/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 * 中文：单任务封面提取；跨根页面保留进行中的工作，UI 只领取完整结果。
 * English: Single-worker cover extraction; keep work across root tabs and publish only complete results.
 * 用户批准：跨根页复用最多八张成品封面；修订旧任务丢弃策略，文件身份或库代次改变后不得复用。
 * User-approved: Reuse at most eight prepared covers across roots; revise result disposal, rejecting changed file identities or library revisions.
 * 修订：只为确认无封面的书生成图；提取失败最多重试三次，不缓存假封面。
 * Revision: Generate artwork only for confirmed absence; retry extraction at most three times, never caching false covers.
 * 冻结：不接触 framebuffer；开书、传书、媒体丢失和睡眠前汇合，串行保护解码器与文件。
 * Frozen: Never touch the framebuffer; join before reading, transfer, media loss or sleep to serialize decoder and file access.
 */
#include "book_cover.h"
#include "book_store.h"
#include "book_cover_auto.h"
#include "esp_heap_caps.h"
#include <sys/stat.h>
#include "app_sleep_hooks.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>

static char s_path[BOOK_STORE_PATH_MAX];
static uint8_t* s_gray;
static unsigned s_revision;
static bool s_pending, s_registered;
static atomic_bool s_done;
static bool s_source_valid;
static struct stat s_source;
#define COVER_BYTES ((size_t)BOOK_COVER_W * BOOK_COVER_H)
#define COVER_REUSE_SLOTS 8
static struct {
    char path[BOOK_STORE_PATH_MAX];
    uint8_t* gray;
    struct stat source;
    unsigned revision;
    uint64_t age;
} s_reuse[COVER_REUSE_SLOTS];
static uint64_t s_age;

static bool source_same(const struct stat* a, const struct stat* b) {
    return a->st_size == b->st_size && a->st_mtime == b->st_mtime;
}
static void reuse_clear(void) {
    for (unsigned i = 0; i < COVER_REUSE_SLOTS; ++i) {
        free(s_reuse[i].gray); memset(&s_reuse[i], 0, sizeof(s_reuse[i]));
    }
}
static uint8_t* reuse_take(const char* path) {
    for (unsigned i = 0; i < COVER_REUSE_SLOTS; ++i) {
        if (!s_reuse[i].gray || strcmp(path, s_reuse[i].path)) continue;
        struct stat now;
        if (s_reuse[i].revision != book_store_revision() || stat(path, &now) || !source_same(&now, &s_reuse[i].source)) {
            free(s_reuse[i].gray); memset(&s_reuse[i], 0, sizeof(s_reuse[i])); return NULL;
        }
        uint8_t* copy = heap_caps_malloc(COVER_BYTES, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (!copy) return NULL;
        memcpy(copy, s_reuse[i].gray, COVER_BYTES); s_reuse[i].age = ++s_age;
        return copy;
    }
    return NULL;
}
static void reuse_store(void) {
    if (!s_gray || !s_source_valid || s_revision != book_store_revision()) return;
    unsigned slot = 0;
    for (unsigned i = 0; i < COVER_REUSE_SLOTS; ++i) {
        if (!s_reuse[i].gray || !strcmp(s_path, s_reuse[i].path)) { slot = i; break; }
        if (s_reuse[i].age < s_reuse[slot].age) slot = i;
    }
    free(s_reuse[slot].gray); memset(&s_reuse[slot], 0, sizeof(s_reuse[slot]));
    if (heap_caps_get_free_size(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT) < COVER_BYTES + 1024u * 1024u) return;
    uint8_t* copy = heap_caps_malloc(COVER_BYTES, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!copy) return;
    memcpy(copy, s_gray, COVER_BYTES);
    strcpy(s_reuse[slot].path, s_path); s_reuse[slot].gray = copy;
    s_reuse[slot].source = s_source; s_reuse[slot].revision = s_revision; s_reuse[slot].age = ++s_age;
}

static void extract(void* arg) {
    (void)arg;
    s_source_valid = stat(s_path, &s_source) == 0 && S_ISREG(s_source.st_mode);
    if (s_source_valid) {
        book_cover_status_t status = BOOK_COVER_RETRY;
        for (unsigned attempt = 0; attempt < 3 && status == BOOK_COVER_RETRY; ++attempt) {
            if (attempt) vTaskDelay(pdMS_TO_TICKS(100));
            status = book_cover_read(s_path, &s_gray);
        }
        if (status == BOOK_COVER_ABSENT) s_gray = book_cover_auto_alloc(s_path);
        struct stat after;
        s_source_valid = stat(s_path, &after) == 0 && source_same(&s_source, &after);
        if (!s_source_valid) { free(s_gray); s_gray = NULL; }
    }
    atomic_store(&s_done, true);
    vTaskDelete(NULL);
}

void book_cover_join(void) {
    if (s_pending) {
        while (!atomic_load(&s_done)) vTaskDelay(pdMS_TO_TICKS(1));
        free(s_gray); s_gray = NULL; s_pending = false;
    }
    reuse_clear();
}

static bool prepare_sleep(void) { book_cover_join(); return true; }

bool book_cover_poll(const char* path, uint8_t** gray) {
    if (!path || !gray) return false;
    *gray = reuse_take(path);
    if (*gray) return true;
    if (s_pending) {
        if (!atomic_load(&s_done)) return false;
        struct stat now;
        bool valid = s_source_valid && s_revision == book_store_revision() &&
            stat(s_path, &now) == 0 && source_same(&s_source, &now);
        if (valid) reuse_store();
        bool match = !strcmp(path, s_path) && s_revision == book_store_revision();
        if (match && valid) { *gray = s_gray; s_gray = NULL; }
        free(s_gray); s_gray = NULL; s_pending = false;
        if (match) return true;
    }
    if (!*path || strlen(path) >= sizeof(s_path)) return true;
    if (!s_registered) {
        if (!app_sleep_prepare_register(prepare_sleep)) return true;
        s_registered = true;
    }
    strcpy(s_path, path);
    s_revision = book_store_revision();
    atomic_store(&s_done, false);
    s_pending = true;
    // 解码链（EPUB 定位 + ZIP 解压 + PNG/JPEG 解码）的局部缓冲按主任务同标准配栈；
    // 8KB 会在 PNG 封面上溢出，见 sdkconfig 对主任务栈的同类说明。
    // The decode chain (EPUB lookup + ZIP inflate + PNG/JPEG decode) needs the
    // same stack as the main task; 8KB overflows on PNG covers (see the
    // matching sdkconfig note for the main task stack).
    if (xTaskCreatePinnedToCore(extract, "book_cover", 16384, NULL, 1, NULL, 0) != pdPASS) {
        s_pending = false;
        return true;
    }
    return false;
}
