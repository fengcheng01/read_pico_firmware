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
#include "os/os_page_turner.h"
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
static unsigned s_quote_save_failures;
// 只向摘录命名空间注入一次写失败，验证界面保留记录与重试。
// Inject failures only into excerpt writes to check retained records and UI retry.
void preview_quote_save_fail(unsigned count) { s_quote_save_failures = count; }
esp_err_t nvs_set_blob(nvs_handle_t h, const char* key, const void* value, size_t len) {
    if (!value && len) return ESP_ERR_INVALID_ARG;
    pthread_mutex_lock(&s_nvs_lock);
    if ((!strcmp(h, "rp_quotes") || !strcmp(h,"rp_lockquote")) && s_quote_save_failures) {
        --s_quote_save_failures;
        pthread_mutex_unlock(&s_nvs_lock);
        return ESP_ERR_NO_MEM;
    }
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
static unsigned s_style_save_failures, s_goal_save_failures;
void preview_goal_save_fail(unsigned count) { s_goal_save_failures=count; }
// 锁屏样式/动画档位的独立写失败边界。/ Independent failed-write boundary for lock styles and animation modes.
void preview_style_save_fail(unsigned count) { s_style_save_failures=count; }
esp_err_t nvs_set_u8(nvs_handle_t h, const char* key, uint8_t value) {
    if(!strcmp(h,"read_pico")&&(!strcmp(key,"lk_style")||!strcmp(key,"bk_tanim"))&&s_style_save_failures){--s_style_save_failures;return ESP_ERR_NO_MEM;}
    if(!strcmp(h,"read_pico")&&!strcmp(key,"daily_goal")&&s_goal_save_failures){--s_goal_save_failures;return ESP_ERR_NO_MEM;}
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
esp_err_t nvs_entry_find(const char* partition, const char* namespace_, int type, nvs_iterator_t* out) {
    if (partition && strcmp(partition, "nvs")) return ESP_ERR_NVS_NOT_FOUND;
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
typedef struct {
    pthread_mutex_t lock;
    pthread_cond_t changed;
    unsigned count;
} host_semaphore_t;
static void cleanup_unlock(void* lock) { pthread_mutex_unlock(lock); }
static void* semaphore_create(unsigned initial) {
    host_semaphore_t* sem = calloc(1, sizeof(*sem));
    if (!sem) return NULL;
    pthread_mutex_init(&sem->lock, NULL);
    pthread_cond_init(&sem->changed, NULL);
    sem->count = initial;
    return sem;
}
void* xSemaphoreCreateMutex(void) { return semaphore_create(1); }
void* xSemaphoreCreateBinary(void) { return semaphore_create(0); }
int xSemaphoreTake(void* semaphore, int ticks) {
    host_semaphore_t* sem = semaphore;
    if (!sem) return 0;
    int taken = 0;
    pthread_mutex_lock(&sem->lock);
    pthread_cleanup_push(cleanup_unlock, &sem->lock);
    while (!sem->count && ticks) pthread_cond_wait(&sem->changed, &sem->lock);
    if (sem->count) { sem->count = 0; taken = 1; }
    pthread_cleanup_pop(1);
    return taken;
}
int xSemaphoreGive(void* semaphore) {
    host_semaphore_t* sem = semaphore;
    if (!sem) return 0;
    pthread_mutex_lock(&sem->lock);
    sem->count = 1;
    pthread_cond_signal(&sem->changed);
    pthread_mutex_unlock(&sem->lock);
    return 1;
}
void vSemaphoreDelete(void* semaphore) {
    host_semaphore_t* sem = semaphore;
    if (!sem) return;
    pthread_cond_destroy(&sem->changed);
    pthread_mutex_destroy(&sem->lock);
    free(sem);
}
typedef struct {
    void (*entry)(void*);
    void* arg;
    pthread_t thread;
    pthread_mutex_t lock;
    pthread_cond_t changed;
    unsigned notifications;
    bool detached;
} host_task_t;
static _Thread_local host_task_t* s_current_task;
static void task_cleanup(void* raw) {
    host_task_t* task = raw;
    if (!task->detached) return;
    pthread_cond_destroy(&task->changed);
    pthread_mutex_destroy(&task->lock);
    free(task);
}
static void* task_trampoline(void* raw) {
    host_task_t* task = raw;
    s_current_task = task;
    pthread_cleanup_push(task_cleanup, task);
    task->entry(task->arg);
    pthread_cleanup_pop(1);
    return NULL;
}
int xTaskCreatePinnedToCore(void (*entry)(void*), const char* name, int stack, void* arg,
                            int priority, void** handle, int core) {
    (void)name; (void)stack; (void)priority; (void)core;
    host_task_t* task = calloc(1, sizeof(*task));
    if (!task) return -1;
    task->entry = entry; task->arg = arg; task->detached = !handle;
    pthread_mutex_init(&task->lock, NULL);
    pthread_cond_init(&task->changed, NULL);
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    if (!handle) pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    int err = pthread_create(&task->thread, &attr, task_trampoline, task);
    pthread_attr_destroy(&attr);
    if (err) {
        pthread_cond_destroy(&task->changed); pthread_mutex_destroy(&task->lock); free(task);
        return -1;
    }
    if (handle) *handle = task;
    return pdPASS;
}
void vTaskDelete(void* handle) {
    if (!handle) pthread_exit(NULL);
    host_task_t* task = handle;
    pthread_cancel(task->thread); pthread_join(task->thread, NULL);
    pthread_cond_destroy(&task->changed); pthread_mutex_destroy(&task->lock); free(task);
}
unsigned uxTaskGetStackHighWaterMark(TaskHandle_t task) { (void)task; return 4096; }
// 每个任务独立计数通知，封面与阅读预绘不能互相抢走唤醒。
// Count notifications per task so cover and reader preparation cannot consume each other's wakeups.
unsigned ulTaskNotifyTake(int clear, int wait) {
    (void)wait;
    host_task_t* task = s_current_task;
    unsigned count;
    pthread_mutex_lock(&task->lock);
    pthread_cleanup_push(cleanup_unlock, &task->lock);
    while (!task->notifications) pthread_cond_wait(&task->changed, &task->lock);
    count = task->notifications;
    if (clear) task->notifications = 0; else --task->notifications;
    pthread_cleanup_pop(1);
    return count;
}
void xTaskNotifyGive(void* handle) {
    host_task_t* task = handle;
    pthread_mutex_lock(&task->lock);
    ++task->notifications;
    pthread_cond_signal(&task->changed);
    pthread_mutex_unlock(&task->lock);
}

/* ---- 内存堆与杂项 / Heap and misc ---- */
// 只供宿主的定次分配失败，用来验收产品缩略预览的低内存回退。
// Host-only counted allocation failures validate low-memory fallback for product thumbnail previews.
static atomic_int s_alloc_failures;
void preview_heap_fail(int count) { atomic_store(&s_alloc_failures, count > 0 ? count : 0); }
static bool fail_preview_alloc(void) {
    int count = atomic_load(&s_alloc_failures);
    while (count > 0) if (atomic_compare_exchange_weak(&s_alloc_failures, &count, count - 1)) return true;
    return false;
}
void* heap_caps_malloc(size_t size, uint32_t caps) { (void)caps; return fail_preview_alloc() ? NULL : malloc(size); }
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
unsigned preview_lock_requests;
void app_loop_request_lock(void) { ++preview_lock_requests; os_page_turner_pause(); }
void app_loop_stay_awake(void) { /* 预览无空闲锁屏；声明与设备一致。/ No idle lock in preview; declaration matches the device. */ }
