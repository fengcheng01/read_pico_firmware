/* SPDX-License-Identifier: Apache-2.0
 * 中文：单解码器、三槽LRU与文件身份检查；回调不触碰页面或帧缓冲。
 * English: One decoder, three LRU slots and source checks; workers never touch pages or frames.
 * 冻结：不并发调用图片解码器，不写原图；所有发布与淘汰都由UI任务执行。
 * Frozen: Never run decoders concurrently or write originals; the UI task publishes and evicts results.
 */
#include "ui_image_cache.h"
#include "ui_image.h"
#include "app_sleep_hooks.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define SLOTS 3
#define PATH_CAP 192
typedef struct {
    char path[PATH_CAP];
    struct stat source;
    uint8_t* gray;
    uint16_t width, height;
    uint64_t age;
} image_slot_t;
static image_slot_t s_slots[SLOTS], s_job;
static atomic_bool s_done;
static bool s_pending, s_registered;
static int s_pinned = -1;
static uint64_t s_age;

static bool unchanged(const image_slot_t* slot) {
    struct stat now;
    return !stat(slot->path, &now) && S_ISREG(now.st_mode) &&
        now.st_size == slot->source.st_size && now.st_mtime == slot->source.st_mtime;
}
static void discard(image_slot_t* slot) { free(slot->gray); memset(slot, 0, sizeof(*slot)); }
static int find(const char* path) {
    for (int i = 0; i < SLOTS; ++i) {
        if (!strcmp(path, s_slots[i].path)) {
            if (unchanged(&s_slots[i])) return i;
            discard(&s_slots[i]);
            if (s_pinned == i) s_pinned = -1;
        }
    }
    return -1;
}
static int oldest(void) {
    int slot = -1;
    for (int i = 0; i < SLOTS; ++i) {
        if (i == s_pinned) continue;
        if (!s_slots[i].path[0]) return i;
        if (slot < 0 || s_slots[i].age < s_slots[slot].age) slot = i;
    }
    return slot;
}
static void collect(void) {
    if (!s_pending || !atomic_load(&s_done)) return;
    int slot = oldest();
    if (slot >= 0 && unchanged(&s_job)) {
        discard(&s_slots[slot]); s_slots[slot] = s_job; s_slots[slot].age = ++s_age;
        memset(&s_job, 0, sizeof(s_job));
    } else discard(&s_job);
    s_pending = false;
}
static void decode(void* arg) {
    (void)arg;
    if (!stat(s_job.path, &s_job.source) && S_ISREG(s_job.source.st_mode)) {
        (void)ui_image_load_path(s_job.path, &s_job.gray, &s_job.width, &s_job.height);
        if (!unchanged(&s_job)) discard(&s_job);
    }
    atomic_store(&s_done, true); vTaskDelete(NULL);
}
static bool prepare_sleep(void) {
    // 唤醒会直接重绘当前页面，保留其借用的成品；其他输出和在途任务在睡眠前汇合。
    // Wake repaints the current page directly, so retain its borrowed output while joining and dropping everything else.
    if (s_pending) while (!atomic_load(&s_done)) vTaskDelay(pdMS_TO_TICKS(1));
    discard(&s_job); s_pending = false;
    for (int i = 0; i < SLOTS; ++i) if (i != s_pinned) discard(&s_slots[i]);
    return true;
}
static bool start(const char* path) {
    if (s_pending || !path || !*path || strlen(path) >= PATH_CAP) return false;
    if (!s_registered) {
        if (!app_sleep_prepare_register(prepare_sleep)) return false;
        s_registered = true;
    }
    // 内存偏低时先释放未展示的成品，前台解码仍可尽力执行。
    // Release undisplayed outputs under pressure while still allowing foreground decode.
    if (heap_caps_get_free_size(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT) < 2u * 1024u * 1024u)
        for (int i = 0; i < SLOTS; ++i) if (i != s_pinned) discard(&s_slots[i]);
    memset(&s_job, 0, sizeof(s_job)); strcpy(s_job.path, path);
    atomic_store(&s_done, false); s_pending = true;
    if (xTaskCreatePinnedToCore(decode, "image_preview", 16384, NULL, 1, NULL, 0) != pdPASS) {
        s_pending = false; memset(&s_job, 0, sizeof(s_job)); return false;
    }
    return true;
}
bool ui_image_cache_poll(const char* path, uint8_t** gray, uint16_t* width, uint16_t* height) {
    if (!gray || !width || !height) return true;
    *gray = NULL; *width = *height = 0;
    if (!path || !*path) return true;
    collect();
    int i = find(path);
    if (i >= 0) {
        s_pinned = i; s_slots[i].age = ++s_age;
        *gray = s_slots[i].gray; *width = s_slots[i].width; *height = s_slots[i].height; return true;
    }
    // 已绘制帧保留在EPD缓冲，加载期间页面不再借用旧灰度输出。
    // The EPD buffer retains the painted frame; loading pages no longer borrow the old gray output.
    s_pinned = -1;
    if (s_pending) return false;
    struct stat source;
    if (stat(path, &source) || !S_ISREG(source.st_mode)) return true;
    return !start(path);
}
void ui_image_cache_prefetch(const char* path) {
    collect();
    if (!path || !*path || s_pending || find(path) >= 0 ||
        heap_caps_get_free_size(MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT) < 3u * 1024u * 1024u) return;
    (void)start(path);
}
void ui_image_cache_retry(const char* path) {
    collect();
    int i = path ? find(path) : -1;
    if (i >= 0 && !s_slots[i].gray) discard(&s_slots[i]);
}
void ui_image_cache_clear(void) {
    if (s_pending) while (!atomic_load(&s_done)) vTaskDelay(pdMS_TO_TICKS(1));
    discard(&s_job); s_pending = false; s_pinned = -1;
    for (int i = 0; i < SLOTS; ++i) discard(&s_slots[i]);
    if (s_registered) app_sleep_prepare_unregister(prepare_sleep);
    s_registered = false;
}
