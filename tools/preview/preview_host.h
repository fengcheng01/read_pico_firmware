/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 * 中文：电脑预览的硬件与 RTOS 边界，供真实页面和字体代码编译。
 * English: Hardware and RTOS boundary for compiling real pages and fonts on the host.
 * 冻结：为验证后台封面与预绘制生命周期，任务使用 pthread；仍不模拟真实刷新耗时或硬件电源。
 * Frozen: Use pthread tasks to verify cover/prepaint lifecycles; never simulate physical refresh timing or power.
 */
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>

typedef int esp_err_t;
#define ESP_OK 0
#define ESP_FAIL -1
#define ESP_ERR_NO_MEM 0x101
#define ESP_ERR_INVALID_ARG 0x102
#define ESP_ERR_INVALID_STATE 0x103
#define ESP_ERR_NOT_FOUND 0x105
#define ESP_ERR_INVALID_RESPONSE 0x108
#define ESP_ERR_INVALID_SIZE 0x104
#define ESP_ERR_NOT_FINISHED 0x109
#define ESP_ERR_NOT_SUPPORTED 0x106
const char* esp_err_to_name(esp_err_t err);
int64_t esp_timer_get_time(void);
#define ESP_LOGI(tag, ...) do { (void)(tag); } while (0)
#define ESP_LOGW(tag, ...) do { (void)(tag); } while (0)
#define ESP_LOGE(tag, ...) do { fprintf(stderr, "%s: ", tag); fprintf(stderr, __VA_ARGS__); fputc('\n', stderr); } while (0)

#define MALLOC_CAP_SPIRAM 1
#define MALLOC_CAP_8BIT 2

typedef void* SemaphoreHandle_t;
typedef void* TaskHandle_t;
#define portMAX_DELAY UINT32_MAX
#define pdTRUE 1
#define pdPASS 1
#define pdMS_TO_TICKS(ms) (ms)


typedef struct { int x, y, width, height; } EpdRect;
typedef struct { int width, height; } EpdDisplay_t;
typedef struct { int unused; } EpdWaveform;
typedef struct { uint8_t* front_fb; } EpdiyHighlevelState;
enum EpdDrawError { EPD_DRAW_SUCCESS = 0 };
enum EpdDrawMode { MODE_DU = 1, MODE_GC16 = 2, MODE_GL16 = 5 };
enum EpdRotation { EPD_ROT_LANDSCAPE, EPD_ROT_PORTRAIT, EPD_ROT_INVERTED_LANDSCAPE, EPD_ROT_INVERTED_PORTRAIT };
enum EpdFontFlags { EPD_DRAW_BACKGROUND = 1, EPD_DRAW_ALIGN_LEFT = 2, EPD_DRAW_ALIGN_RIGHT = 4, EPD_DRAW_ALIGN_CENTER = 8 };
extern const EpdWaveform E0470_WAVEFORM, E0470_FULL_WAVEFORM, E0470_GRAY8_WAVEFORM;
extern const EpdWaveform E0470_FOLLOW_WAVEFORM;
extern const EpdWaveform E0470_NAVIGATION_WAVEFORM;
int epd_width(void);
int epd_height(void);
int epd_rotated_display_width(void);
int epd_rotated_display_height(void);
enum EpdRotation epd_get_rotation(void);
void epd_set_rotation(enum EpdRotation rotation);
void epd_clear_area(EpdRect area);
void epd_draw_pixel(int x, int y, uint8_t color, uint8_t* fb);
void epd_draw_hline(int x, int y, int n, uint8_t color, uint8_t* fb);
void epd_draw_vline(int x, int y, int n, uint8_t color, uint8_t* fb);
void epd_draw_rect(EpdRect rect, uint8_t color, uint8_t* fb);
void epd_fill_rect(EpdRect rect, uint8_t color, uint8_t* fb);
void epd_fill_circle_helper(int x, int y, int radius, int corners, int delta, uint8_t color, uint8_t* fb);
void epd_fill_triangle(int x0, int y0, int x1, int y1, int x2, int y2, uint8_t color, uint8_t* fb);
void epd_draw_rotated_image(EpdRect rect, const uint8_t* image, uint8_t* fb);
uint8_t* epd_hl_get_framebuffer(EpdiyHighlevelState* hl);
enum EpdDrawError update_display_mode(EpdiyHighlevelState* hl, enum EpdDrawMode mode);
enum EpdDrawError update_display_full(EpdiyHighlevelState* hl);
enum EpdDrawError update_display_text_turn(EpdiyHighlevelState* hl, bool white_on_black);
enum EpdDrawError update_display_text_direct(EpdiyHighlevelState* hl, bool white_on_black);
enum EpdDrawError update_display_white(EpdiyHighlevelState* hl);
enum EpdDrawError update_display_with(EpdiyHighlevelState*, const EpdWaveform*, enum EpdDrawMode);
enum EpdDrawError update_display_from_white(EpdiyHighlevelState* hl);
enum EpdDrawError update_display_from_white_with(EpdiyHighlevelState* hl, const EpdWaveform* wave, enum EpdDrawMode mode);
enum EpdDrawError update_display_area_with(EpdiyHighlevelState* hl, const EpdWaveform* wave, enum EpdDrawMode mode, EpdRect area);
enum EpdDrawError update_display_area_quiet(EpdiyHighlevelState* hl, EpdRect area);
void display_set_bulk_io(bool bulk);
void display_request_navigation_settle(void);
size_t heap_caps_get_free_size(uint32_t caps);
#define MALLOC_CAP_INTERNAL 1
#define MALLOC_CAP_8BIT 2
void guard_draw_result(EpdiyHighlevelState* hl, enum EpdDrawError err);
void display_hold_white_exit(bool hold);
bool display_take_white_exit(void);

typedef void* sc7a20h_handle_t;

/* ---- ESP 仿真层声明（esp_host.c 实现）----
   / ESP emulation declarations implemented in esp_host.c. */
typedef void* nvs_handle_t;
typedef struct { char namespace_name[16]; char key[16]; uint8_t type; } nvs_entry_info_t;
typedef struct nvs_iterator* nvs_iterator_t;
#define NVS_READONLY 0
#define NVS_READWRITE 1
#define ESP_ERR_NVS_NOT_FOUND 0x1102
#define ESP_ERR_NVS_INVALID_LENGTH 0x1103
#define ESP_ERR_NVS_NO_FREE_PAGES 0x1100
#define ESP_ERR_NVS_NEW_VERSION_FOUND 0x1101
#define ESP_ERR_INVALID_CRC 0x109
#define NVS_TYPE_U8 1
#define NVS_TYPE_U16 0
#define NVS_TYPE_I8 2
#define NVS_TYPE_U32 4
#define NVS_TYPE_BLOB 5
#define TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF 1
#define TINFL_FLAG_PARSE_ZLIB_HEADER 2
esp_err_t nvs_flash_init(void);
esp_err_t nvs_flash_erase(void);
esp_err_t nvs_open(const char* namespace_, int mode, nvs_handle_t* out);
void nvs_close(nvs_handle_t handle);
esp_err_t nvs_commit(nvs_handle_t handle);
esp_err_t nvs_get_u8(nvs_handle_t handle, const char* key, uint8_t* out);
esp_err_t nvs_get_i8(nvs_handle_t handle, const char* key, int8_t* out);
esp_err_t nvs_get_u16(nvs_handle_t handle, const char* key, uint16_t* out);
esp_err_t nvs_get_u32(nvs_handle_t handle, const char* key, uint32_t* out);
esp_err_t nvs_get_blob(nvs_handle_t handle, const char* key, void* out, size_t* cap);
esp_err_t nvs_get_str(nvs_handle_t handle, const char* key, char* out, size_t* cap);
esp_err_t nvs_set_u8(nvs_handle_t handle, const char* key, uint8_t value);
esp_err_t nvs_set_i8(nvs_handle_t handle, const char* key, int8_t value);
esp_err_t nvs_set_u16(nvs_handle_t handle, const char* key, uint16_t value);
esp_err_t nvs_set_u32(nvs_handle_t handle, const char* key, uint32_t value);
esp_err_t nvs_set_blob(nvs_handle_t handle, const char* key, const void* value, size_t len);
esp_err_t nvs_set_str(nvs_handle_t handle, const char* key, const char* value);
esp_err_t nvs_erase_key(nvs_handle_t handle, const char* key);
esp_err_t nvs_entry_find(const char* namespace_, const char* match, int type, nvs_iterator_t* out);
esp_err_t nvs_entry_next(nvs_iterator_t* it);
void nvs_entry_info(nvs_iterator_t it, nvs_entry_info_t* info);
void nvs_release_iterator(nvs_iterator_t it);
void* xSemaphoreCreateMutex(void);
void* xSemaphoreCreateBinary(void);
int xSemaphoreTake(void* semaphore, int ticks);
int xSemaphoreGive(void* semaphore);
void vSemaphoreDelete(void* semaphore);
int xTaskCreatePinnedToCore(void (*entry)(void*), const char* name, int stack, void* arg,
                            int priority, void** handle, int core);
void vTaskDelete(void* handle);
unsigned ulTaskNotifyTake(int clear, int wait);
void xTaskNotifyGive(void* handle);
void* heap_caps_malloc(size_t size, uint32_t caps);
void* heap_caps_realloc(void* pointer, size_t size, uint32_t caps);
void* heap_caps_calloc(size_t count, size_t size, uint32_t caps);
void* heap_caps_aligned_alloc(size_t alignment, size_t size, uint32_t caps);
void heap_caps_free(void* pointer);
size_t heap_caps_get_largest_free_block(uint32_t caps);

/* ROM miniz 的最小声明；实现见 miniz_host.c。/ Minimal ROM-miniz declarations; implemented in miniz_host.c. */
typedef unsigned char mz_uint8;
typedef struct { int m_state; } tinfl_decompressor;
typedef enum { TINFL_STATUS_FAILED = -1, TINFL_STATUS_DONE = 0 } tinfl_status;
#define tinfl_init(r) ((r)->m_state = 0)
tinfl_status tinfl_decompress(tinfl_decompressor* state, const mz_uint8* input, size_t* in_size,
                              mz_uint8* output, mz_uint8* out_buf_mark, size_t* out_size, unsigned flags);
// 仅供完整设备书页语法检查，不链接或运行传感器。/ Used only for device book-page syntax checks; sensors are neither linked nor run.
typedef struct { int odr, fs; } sc7a20h_sensor_config_t;
#define SC7A20H_ODR_100 4
#define SC7A20H_FS_2G 0
typedef struct { int unused; } sc7a20h_aoi_cfg_t;
typedef struct { uint8_t aoi2_src; } sc7a20h_events_t;
#define SC7A20H_AOI2 2
const sc7a20h_sensor_config_t* sc7a20h_get_config(sc7a20h_handle_t h);
esp_err_t sc7a20h_apply_config(sc7a20h_handle_t h, const sc7a20h_sensor_config_t* config);
esp_err_t sc7a20h_activity_config(sc7a20h_handle_t h, int threshold, int duration);
esp_err_t sc7a20h_aoi_config(sc7a20h_handle_t h, int index, const sc7a20h_aoi_cfg_t* config);
bool sc7a20h_powered(sc7a20h_handle_t h);
esp_err_t sc7a20h_read_events(sc7a20h_handle_t h, sc7a20h_events_t* events);
void read_pico_sensor_sleep(sc7a20h_handle_t h);
bool read_pico_search_match(const char* name, const char* query);
typedef void* cst836u_handle_t;
typedef struct { bool active; uint16_t x, y; } cst836u_point_t;
typedef struct { bool touched; uint8_t count; uint16_t x, y; cst836u_point_t points[2]; } cst836u_touch_t;
typedef struct { int unused; } cst836u_info_t;
typedef struct { bool present, mounted; uint64_t capacity_bytes, free_bytes; } read_pico_sd_info_t;
esp_err_t read_pico_sd_get_info(read_pico_sd_info_t* out);
void read_pico_sd_start_probe(void);
esp_err_t read_pico_sd_sync(void);
esp_err_t read_pico_sd_remount(void);

void epd_draw_line(int x0, int y0, int x1, int y1, uint8_t c, uint8_t* fb);

// 后台任务有界等待。/ Yield during bounded worker joins.
void vTaskDelay(int ms);
