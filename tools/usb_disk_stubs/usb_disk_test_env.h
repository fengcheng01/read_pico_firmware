/* SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 * USB 生命周期宿主替身，不模拟 USB 线协议。/ USB lifecycle host substitutes; not a USB wire-protocol simulation.
 */
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <assert.h>
#include <string.h>
typedef int esp_err_t;
#define ESP_OK 0
#define ESP_FAIL -1
#define ESP_ERR_INVALID_STATE 0x103
#define ESP_ERR_NVS_NOT_FOUND 0x1102
#define NVS_READWRITE 1
typedef int nvs_handle_t;
static uint8_t stored, staged;
static int nvs_error, commit_error, restarts;
static int nvs_open(const char* ns,int mode,nvs_handle_t* h) {(void)ns;(void)mode;*h=1;staged=stored;return nvs_error;}
static int nvs_set_u8(int h,const char* key,uint8_t val) {(void)h;(void)key;staged=val;return 0;}
static int nvs_get_u8(int h,const char* key,uint8_t* val) {(void)h;(void)key;*val=stored;return stored?0:ESP_ERR_NVS_NOT_FOUND;}
static int nvs_erase_key(int h,const char* key) {(void)h;(void)key;staged=0;return 0;}
static int nvs_commit(int h) {(void)h;if(!commit_error)stored=staged;return commit_error;}
static void nvs_close(int h) {(void)h;}
static void esp_restart(void) {++restarts;}
typedef struct { int unused; } EpdiyHighlevelState;
typedef void* cst836u_handle_t;
typedef struct { bool touched; int count,x,y; } cst836u_touch_t;
static int cst836u_read(void* tp,cst836u_touch_t* touch) {(void)tp;*touch=(cst836u_touch_t){0};return 0;}
typedef struct {int x,y,width,height;} EpdRect;
#define UI_MARGIN 40
#define UI_BTN_RADIUS 14
#define UI_GRAY_BLACK 0
#define EPD_DRAW_ALIGN_LEFT 0
static int ui_content_width(void) {return 604;}
static EpdRect ui_row_rect(int i,int n,int y,int h) {(void)n;return(EpdRect){40+i*308,y,296,h};}
static bool ui_rect_hit(EpdRect r,int x,int y) {return x>=r.x&&y>=r.y&&x<r.x+r.width&&y<r.y+r.height;}
static void ui_clear_page(void* fb) {(void)fb;}
static void ui_product_header(void* fb,const char* t,const char* d) {(void)fb;(void)t;(void)d;}
static void ui_product_title(void* fb,EpdRect r,const char* t,int px,int n) {(void)fb;(void)r;(void)t;(void)px;(void)n;}
static void ui_text(void* fb,...) {(void)fb;}
static void ui_draw_button(void* fb,EpdRect r,const char* t,bool enabled) {(void)fb;(void)r;(void)t;(void)enabled;}
static int update_display_full(void* hl) {(void)hl;return 0;}
static void guard_draw_result(void* hl,int err) {(void)hl;(void)err;}
static const char* esp_err_to_name(int err) {(void)err;return "error";}
#define pdMS_TO_TICKS(x) (x)
static void vTaskDelay(int n) {(void)n;}
typedef struct {int unused;} sdmmc_card_t;
static sdmmc_card_t test_card;
static int raw_error, raw_closes;
static bool card_present=true;
static int read_pico_sd_open_raw(sdmmc_card_t** card) {*card=raw_error?NULL:&test_card;return raw_error;}
static void read_pico_sd_close_raw(sdmmc_card_t* card) {if(card)++raw_closes;}
static bool read_pico_sd_present(void) {return card_present;}
typedef void* tinyusb_msc_storage_handle_t;
typedef enum {TINYUSB_MSC_STORAGE_MOUNT_USB,TINYUSB_MSC_STORAGE_MOUNT_APP} tinyusb_msc_mount_point_t;
typedef struct {int id;} tinyusb_msc_event_t;
#define TINYUSB_MSC_EVENT_MOUNT_FAILED 2
#define TINYUSB_MSC_EVENT_FORMAT_REQUIRED 3
typedef struct {void(*callback)(void*,tinyusb_msc_event_t*,void*);} tinyusb_msc_driver_config_t;
typedef struct {struct {sdmmc_card_t* card;} medium;int mount_point;struct {const char* base_path;struct {bool format_if_mount_failed;int max_files;} config;bool do_not_format;} fat_fs;} tinyusb_msc_storage_config_t;
typedef struct {int id;} tinyusb_event_t;
#define TINYUSB_EVENT_ATTACHED 0
#define TINYUSB_EVENT_DETACHED 1
typedef struct {struct {const char** string;int string_count;} descriptor;void(*event_cb)(tinyusb_event_t*,void*);} tinyusb_config_t;
#define TINYUSB_DEFAULT_CONFIG(cb) ((tinyusb_config_t){.event_cb=cb})
static int msc_error, storage_error, usb_error, delete_busy, deletes, usb_stops, disconnects;
static int owner=TINYUSB_MSC_STORAGE_MOUNT_USB;
static int tinyusb_msc_install_driver(const tinyusb_msc_driver_config_t* c) {assert(c->callback);return msc_error;}
static int tinyusb_msc_new_storage_sdmmc(const tinyusb_msc_storage_config_t* c,void** out) {
    assert(c->medium.card==&test_card && c->mount_point==TINYUSB_MSC_STORAGE_MOUNT_USB);
    assert(!c->fat_fs.config.format_if_mount_failed && c->fat_fs.do_not_format);
    *out=storage_error?NULL:&owner;return storage_error;
}
static int tinyusb_driver_install(const tinyusb_config_t* c) {
    assert(c->event_cb && c->descriptor.string_count==5);
    assert(!strcmp(c->descriptor.string[2],"Read Pico SD"));return usb_error;
}
static int tinyusb_msc_get_storage_mount_point(void* h,tinyusb_msc_mount_point_t* p) {(void)h;*p=owner;return 0;}
static int tinyusb_msc_delete_storage(void* h) {(void)h;if(delete_busy){--delete_busy;return ESP_ERR_INVALID_STATE;}++deletes;return 0;}
static int tinyusb_driver_uninstall(void) {++usb_stops;return 0;}
static int tinyusb_msc_uninstall_driver(void) {return 0;}
static void tud_disconnect(void) {++disconnects;}
static int esp_efuse_mac_get_default(uint8_t mac[6]) {memset(mac,0x42,6);return 0;}
typedef void* usb_phy_handle_t;
typedef struct {int controller,target;} usb_phy_config_t;
#define USB_PHY_CTRL_SERIAL_JTAG 1
#define USB_PHY_TARGET_INT 0
static int usb_new_phy(const usb_phy_config_t* c,void** h) {assert(c->controller==1&&c->target==0);*h=&owner;return 0;}

static void (*deferred_fn)(void*);
static void usbd_defer_func(void(*fn)(void*),void* arg,bool isr) { (void)arg;assert(!isr&&!deferred_fn);deferred_fn=fn; }
static void run_deferred(void) { assert(deferred_fn);void(*fn)(void*)=deferred_fn;deferred_fn=NULL;fn(NULL); }
