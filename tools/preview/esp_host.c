/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 * 中文：ESP-IDF 系统仿真层：内存 NVS、pthread 版 FreeRTOS、内存堆与传感器空实现。
 * 预览链接真实产品代码，只在系统调用边界仿真；不按功能分叉夹具。
 * English: ESP-IDF emulation layer: in-memory NVS, pthread FreeRTOS, memory heap
 * and sensor no-ops. The preview links real product code and fakes only the
 * syscall boundary, never per-feature fixtures.
 * 冻结：NVS 仅进程内存，不落盘；任务/信号量映射 pthread；传感器永远未上电。
 * Frozen: NVS is process-memory only; tasks/semaphores map onto pthreads; sensors stay powered off.
 */
#include "preview_host.h"
#include <pthread.h>
#include <stdatomic.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

/* ---- NVS：命名空间内存键值，语义对齐 esp NVS（类型区分、commit 后可见）----
   / In-memory typed key-value namespaces matching esp NVS semantics. */
typedef enum { NVS_KIND_U8, NVS_KIND_I8, NVS_KIND_U16, NVS_KIND_U32, NVS_KIND_STR, NVS_KIND_BLOB } nvs_kind_t;
typedef struct nvs_entry {
    char namespace_[16];
    char key[16];
    nvs_kind_t kind;
    union {
        uint8_t u8;
        int8_t i8;
        uint16_t u16;
        uint32_t u32;
    };
    char* str;
    size_t blob_len;
    struct nvs_entry* next;
} nvs_entry_t;
static nvs_entry_t* s_nvs;
static pthread_mutex_t s_nvs_lock = PTHREAD_MUTEX_INITIALIZER;

static nvs_entry_t* nvs_find(const char* namespace_, const char* key) {
    for (nvs_entry_t* e = s_nvs; e; e = e->next)
        if (!strcmp(e->namespace_, namespace_) && !strcmp(e->key, key)) return e;
    return NULL;
}
static nvs_entry_t* nvs_upsert(const char* namespace_, const char* key, nvs_kind_t kind) {
    nvs_entry_t* e = nvs_find(namespace_, key);
    if (e && e->kind != kind) {
        free(e->str);
        e->str = NULL;
        e->kind = kind;
    }
    if (e) return e;
    e = calloc(1, sizeof(*e));
    e->kind = kind;
    snprintf(e->namespace_, sizeof(e->namespace_), "%s", namespace_);
    snprintf(e->key, sizeof(e->key), "%s", key);
    e->next = s_nvs;
    return s_nvs = e;
}

esp_err_t nvs_flash_init(void) { return ESP_OK; }
esp_err_t nvs_flash_erase(void) {
    pthread_mutex_lock(&s_nvs_lock);
    while (s_nvs) {
        nvs_entry_t* next = s_nvs->next;
        free(s_nvs->str);
        free(s_nvs);
        s_nvs = next;
    }
    pthread_mutex_unlock(&s_nvs_lock);
    return ESP_OK;
}
esp_err_t nvs_open(const char* namespace_, int mode, nvs_handle_t* out) {
    (void)mode;
    if (!namespace_ || !out || strlen(namespace_) >= 16) return ESP_ERR_INVALID_ARG;
    // 句柄即命名串；条目全局存储，与设备一致跨句柄可见。
    // The handle is the namespace string; entries stay global like the device.
    char* handle = strdup(namespace_);
    *out = handle;
    return handle ? ESP_OK : ESP_ERR_NO_MEM;
}
void nvs_close(nvs_handle_t handle) { free(handle); }
esp_err_t nvs_commit(nvs_handle_t handle) { (void)handle; return ESP_OK; }

esp_err_t nvs_get_u8(nvs_handle_t h, const char* key, uint8_t* out) {
    pthread_mutex_lock(&s_nvs_lock);
    const nvs_entry_t* e = nvs_find(h, key);
    esp_err_t err = e && e->kind == NVS_KIND_U8 ? (*out = e->u8, ESP_OK) : ESP_ERR_NVS_NOT_FOUND;
    pthread_mutex_unlock(&s_nvs_lock);
    return err;
}
esp_err_t nvs_get_i8(nvs_handle_t h, const char* key, int8_t* out) {
    pthread_mutex_lock(&s_nvs_lock);
    const nvs_entry_t* e = nvs_find(h, key);
    esp_err_t err = e && e->kind == NVS_KIND_I8 ? (*out = e->i8, ESP_OK) : ESP_ERR_NVS_NOT_FOUND;
    pthread_mutex_unlock(&s_nvs_lock);
    return err;
}
esp_err_t nvs_get_u16(nvs_handle_t h, const char* key, uint16_t* out) {
    pthread_mutex_lock(&s_nvs_lock);
    const nvs_entry_t* e = nvs_find(h, key);
    esp_err_t err = e && e->kind == NVS_KIND_U16 ? (*out = e->u16, ESP_OK) : ESP_ERR_NVS_NOT_FOUND;
    pthread_mutex_unlock(&s_nvs_lock);
    return err;
}
esp_err_t nvs_get_u32(nvs_handle_t h, const char* key, uint32_t* out) {
    pthread_mutex_lock(&s_nvs_lock);
    const nvs_entry_t* e = nvs_find(h, key);
    esp_err_t err = e && e->kind == NVS_KIND_U32 ? (*out = e->u32, ESP_OK) : ESP_ERR_NVS_NOT_FOUND;
    pthread_mutex_unlock(&s_nvs_lock);
    return err;
}
esp_err_t nvs_set_u32(nvs_handle_t h, const char* key, uint32_t value) {
    pthread_mutex_lock(&s_nvs_lock);
    nvs_upsert(h, key, NVS_KIND_U32)->u32 = value;
    pthread_mutex_unlock(&s_nvs_lock);
    return ESP_OK;
}
esp_err_t nvs_get_blob(nvs_handle_t h, const char* key, void* out, size_t* cap) {
    pthread_mutex_lock(&s_nvs_lock);
    const nvs_entry_t* e = nvs_find(h, key);
    esp_err_t err = ESP_ERR_NVS_NOT_FOUND;
    if (e && e->kind == NVS_KIND_BLOB && e->str) {
        if (*cap >= e->blob_len) {
            memcpy(out, e->str, e->blob_len);
            err = ESP_OK;
        } else err = ESP_ERR_NVS_INVALID_LENGTH;
        *cap = e->blob_len;
    }
    pthread_mutex_unlock(&s_nvs_lock);
    return err;
}
esp_err_t nvs_set_blob(nvs_handle_t h, const char* key, const void* value, size_t len) {
    if (!value && len) return ESP_ERR_INVALID_ARG;
    pthread_mutex_lock(&s_nvs_lock);
    nvs_entry_t* e = nvs_upsert(h, key, NVS_KIND_BLOB);
    char* copy = len ? malloc(len) : (char*)1;
    if (copy) {
        if (len) memcpy(copy, value, len);
        free(e->str);
        e->str = copy;
        e->blob_len = len;
    }
    pthread_mutex_unlock(&s_nvs_lock);
    return copy ? ESP_OK : ESP_ERR_NO_MEM;
}
esp_err_t nvs_get_str(nvs_handle_t h, const char* key, char* out, size_t* cap) {
    pthread_mutex_lock(&s_nvs_lock);
    const nvs_entry_t* e = nvs_find(h, key);
    esp_err_t err = ESP_ERR_NVS_NOT_FOUND;
    if (e && e->kind == NVS_KIND_STR && e->str) {
        size_t need = strlen(e->str) + 1;
        if (*cap >= need) {
            memcpy(out, e->str, need);
            err = ESP_OK;
        } else err = ESP_ERR_NVS_INVALID_LENGTH;
        *cap = need;
    }
    pthread_mutex_unlock(&s_nvs_lock);
    return err;
}
esp_err_t nvs_set_u8(nvs_handle_t h, const char* key, uint8_t value) {
    pthread_mutex_lock(&s_nvs_lock);
    nvs_upsert(h, key, NVS_KIND_U8)->u8 = value;
    pthread_mutex_unlock(&s_nvs_lock);
    return ESP_OK;
}
esp_err_t nvs_set_i8(nvs_handle_t h, const char* key, int8_t value) {
    pthread_mutex_lock(&s_nvs_lock);
    nvs_upsert(h, key, NVS_KIND_I8)->i8 = value;
    pthread_mutex_unlock(&s_nvs_lock);
    return ESP_OK;
}
esp_err_t nvs_set_u16(nvs_handle_t h, const char* key, uint16_t value) {
    pthread_mutex_lock(&s_nvs_lock);
    nvs_upsert(h, key, NVS_KIND_U16)->u16 = value;
    pthread_mutex_unlock(&s_nvs_lock);
    return ESP_OK;
}
esp_err_t nvs_set_str(nvs_handle_t h, const char* key, const char* value) {
    pthread_mutex_lock(&s_nvs_lock);
    nvs_entry_t* e = nvs_upsert(h, key, NVS_KIND_STR);
    char* copy = strdup(value);
    if (copy) {
        free(e->str);
        e->str = copy;
    }
    pthread_mutex_unlock(&s_nvs_lock);
    return copy ? ESP_OK : ESP_ERR_NO_MEM;
}
esp_err_t nvs_erase_key(nvs_handle_t h, const char* key) {
    pthread_mutex_lock(&s_nvs_lock);
    for (nvs_entry_t** at = &s_nvs; *at; at = &(*at)->next) {
        if (!strcmp((*at)->namespace_, h) && !strcmp((*at)->key, key)) {
            nvs_entry_t* dead = *at;
            *at = dead->next;
            free(dead->str);
            free(dead);
            break;
        }
    }
    pthread_mutex_unlock(&s_nvs_lock);
    return ESP_OK;
}

// 迭代器为堆分配的不透明指针，对齐 IDF v6：find 四参返回 esp_err_t、next 改指针。
// Heap opaque iterator matching IDF v6: four-arg find returning esp_err_t, pointer-based next.
typedef struct {
    char namespace_[16];
    char key[16];
    uint8_t type;
    bool used;
} nvs_iter_item_t;
typedef struct nvs_iterator {
    nvs_iter_item_t items[256];
    unsigned count, at;
} nvs_iterator_impl_t;
esp_err_t nvs_entry_find(const char* namespace_, const char* match, int type, nvs_iterator_t* out) {
    (void)match;
    if (!out) return ESP_ERR_INVALID_ARG;
    *out = NULL;
    nvs_iterator_t it = calloc(1, sizeof(*it));
    if (!it) return ESP_ERR_NO_MEM;
    pthread_mutex_lock(&s_nvs_lock);
    for (nvs_entry_t* e = s_nvs; e && it->count < 256; e = e->next) {
        if (namespace_ && strcmp(e->namespace_, namespace_)) continue;
        if ((type == 1 && e->kind != NVS_KIND_U8) || (type == 3 && e->kind != NVS_KIND_STR) ||
            (type == 0 && e->kind != NVS_KIND_U16) || (type == 5 && e->kind != NVS_KIND_BLOB)) continue;
        snprintf(it->items[it->count].namespace_, 16, "%s", e->namespace_);
        snprintf(it->items[it->count].key, 16, "%s", e->key);
        it->items[it->count].type = (uint8_t)e->kind;
        it->items[it->count++].used = true;
    }
    pthread_mutex_unlock(&s_nvs_lock);
    if (!it->count) {
        free(it);
        return ESP_ERR_NVS_NOT_FOUND;
    }
    *out = it;
    return ESP_OK;
}
esp_err_t nvs_entry_next(nvs_iterator_t* it) {
    if (!it || !*it) return ESP_ERR_INVALID_ARG;
    if (++(*it)->at >= (*it)->count) {
        free(*it);
        *it = NULL;
        return ESP_ERR_NVS_NOT_FOUND;
    }
    return ESP_OK;
}
void nvs_entry_info(nvs_iterator_t it, nvs_entry_info_t* info) {
    if (!info || !it || it->at >= it->count) return;
    snprintf(info->namespace_name, sizeof(info->namespace_name), "%s", it->items[it->at].namespace_);
    snprintf(info->key, sizeof(info->key), "%s", it->items[it->at].key);
}
void nvs_release_iterator(nvs_iterator_t it) { free(it); }

/* ---- FreeRTOS：互斥/二值信号量与预渲染任务映射 pthread ----
   / FreeRTOS: mutexes, binary semaphores and the prep task over pthreads. */
void vTaskDelay(int ms) { usleep((useconds_t)ms * 1000); }
void* xSemaphoreCreateMutex(void) {
    pthread_mutex_t* mutex = malloc(sizeof(*mutex));
    return mutex && pthread_mutex_init(mutex, NULL) == 0 ? mutex : (free(mutex), NULL);
}
void* xSemaphoreCreateBinary(void) {
    pthread_mutex_t* mutex = xSemaphoreCreateMutex();
    if (mutex) pthread_mutex_lock(mutex);  // 二值信号量初始为空。/ Binary semaphores start empty.
    return mutex;
}
int xSemaphoreTake(void* semaphore, int ticks) {
    (void)ticks;
    return semaphore && pthread_mutex_lock((pthread_mutex_t*)semaphore) == 0 ? 1 : 0;
}
int xSemaphoreGive(void* semaphore) {
    return semaphore && pthread_mutex_unlock((pthread_mutex_t*)semaphore) == 0 ? 1 : 0;
}
void vSemaphoreDelete(void* semaphore) {
    if (semaphore) {
        pthread_mutex_destroy((pthread_mutex_t*)semaphore);
        free(semaphore);
    }
}
typedef struct {
    void (*entry)(void*);
    void* arg;
} task_spawn_t;
static void* task_trampoline(void* raw) {
    task_spawn_t spawn = *(task_spawn_t*)raw;
    free(raw);
    spawn.entry(spawn.arg);
    return NULL;
}
int xTaskCreatePinnedToCore(void (*entry)(void*), const char* name, int stack, void* arg,
                            int priority, void** handle, int core) {
    (void)name; (void)stack; (void)priority; (void)core;
    task_spawn_t* spawn = malloc(sizeof(*spawn));
    if (!spawn) return -1;
    spawn->entry = entry;
    spawn->arg = arg;
    pthread_t thread;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    int err = pthread_create(&thread, &attr, task_trampoline, spawn);
    pthread_attr_destroy(&attr);
    if (err) {
        free(spawn);
        return -1;
    }
    // 预览句柄只作非空标记；vTaskDelete 通过取消线程实现。
    // The preview handle is a non-null marker; vTaskDelete cancels the thread.
    if (handle) *handle = (void*)thread;
    return 0;
}
void vTaskDelete(void* handle) {
    if (handle) pthread_cancel((pthread_t)handle);
}
// 任务通知映射进程级条件变量：预渲染任务是唯一的等待者。
// Task notifications map to a process-wide condvar: the prep task is the only waiter.
static pthread_mutex_t s_notify_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t s_notify_cond = PTHREAD_COND_INITIALIZER;
static atomic_uint s_notify_pending;
unsigned ulTaskNotifyTake(int clear, int wait) {
    (void)clear; (void)wait;
    pthread_mutex_lock(&s_notify_lock);
    while (!s_notify_pending) pthread_cond_wait(&s_notify_cond, &s_notify_lock);
    s_notify_pending = 0;
    pthread_mutex_unlock(&s_notify_lock);
    return 1;
}
void xTaskNotifyGive(void* handle) {
    (void)handle;
    pthread_mutex_lock(&s_notify_lock);
    s_notify_pending = 1;
    pthread_cond_signal(&s_notify_cond);
    pthread_mutex_unlock(&s_notify_lock);
}

/* ---- 内存堆与杂项 / Heap and misc ---- */
void* heap_caps_malloc(size_t size, uint32_t caps) { (void)caps; return malloc(size); }
void* heap_caps_realloc(void* pointer, size_t size, uint32_t caps) { (void)caps; return realloc(pointer, size); }
void* heap_caps_calloc(size_t count, size_t size, uint32_t caps) { (void)caps; return calloc(count, size); }
void* heap_caps_aligned_alloc(size_t alignment, size_t size, uint32_t caps) {
    (void)caps;
    void* pointer = NULL;
    return posix_memalign(&pointer, alignment, size) == 0 ? pointer : NULL;
}
void heap_caps_free(void* pointer) { free(pointer); }

void display_set_bulk_io(bool active) { (void)active; }

/* ---- 传感器空实现：永远未上电，摇动实验不可触发 ----
   / Sensor no-ops: never powered, the shake experiment cannot fire. */
const sc7a20h_sensor_config_t* sc7a20h_get_config(sc7a20h_handle_t handle) {
    (void)handle;
    static sc7a20h_sensor_config_t config;
    return &config;
}
esp_err_t sc7a20h_apply_config(sc7a20h_handle_t handle, const sc7a20h_sensor_config_t* config) {
    (void)handle; (void)config;
    return ESP_ERR_INVALID_STATE;
}
esp_err_t sc7a20h_activity_config(sc7a20h_handle_t handle, int threshold, int duration) {
    (void)handle; (void)threshold; (void)duration;
    return ESP_ERR_INVALID_STATE;
}
esp_err_t sc7a20h_aoi_config(sc7a20h_handle_t handle, int index, const sc7a20h_aoi_cfg_t* config) {
    (void)handle; (void)index; (void)config;
    return ESP_ERR_INVALID_STATE;
}
bool sc7a20h_powered(sc7a20h_handle_t handle) { (void)handle; return false; }
esp_err_t sc7a20h_read_events(sc7a20h_handle_t handle, sc7a20h_events_t* events) {
    (void)handle; (void)events;
    return ESP_ERR_INVALID_STATE;
}
void read_pico_sensor_sleep(sc7a20h_handle_t handle) { (void)handle; }

size_t heap_caps_get_largest_free_block(uint32_t caps) { (void)caps; return 64u * 1024 * 1024; }
void app_loop_stay_awake(void) { /* 预览无空闲锁屏；声明与设备一致。/ No idle lock in preview; declaration matches the device. */ }
