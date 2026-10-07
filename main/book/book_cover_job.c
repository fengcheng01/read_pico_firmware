/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 * 中文：单任务封面提取；跨根页面保留进行中的工作，UI 只领取完整结果。
 * English: Single-worker cover extraction; keep work across root tabs and publish only complete results.
 * 冻结：不接触 framebuffer；开书、传书、媒体丢失和睡眠前汇合，串行保护解码器与文件。
 * Frozen: Never touch the framebuffer; join before reading, transfer, media loss or sleep to serialize decoder and file access.
 */
#include "book_cover.h"
#include "book_store.h"
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

static void extract(void* arg) {
    (void)arg;
    (void)book_cover_load(s_path, &s_gray);
    atomic_store(&s_done, true);
    vTaskDelete(NULL);
}

void book_cover_join(void) {
    if (!s_pending) return;
    while (!atomic_load(&s_done)) vTaskDelay(pdMS_TO_TICKS(1));
    free(s_gray);
    s_gray = NULL;
    s_pending = false;
}

static bool prepare_sleep(void) { book_cover_join(); return true; }

bool book_cover_poll(const char* path, uint8_t** gray) {
    if (!path || !gray) return false;
    *gray = NULL;
    if (s_pending) {
        if (!atomic_load(&s_done)) return false;
        bool match = !strcmp(path, s_path) && s_revision == book_store_revision();
        if (match) { *gray = s_gray; s_gray = NULL; }
        book_cover_join();
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
