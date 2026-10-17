/* SPDX-License-Identifier: Apache-2.0 */
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "esp_err.h"
#define portMUX_TYPE int
#define portMUX_INITIALIZER_UNLOCKED 0
#define portENTER_CRITICAL(x) ((void)(x))
#define portEXIT_CRITICAL(x) ((void)(x))
#define ESP_LOGW(tag,...) do { (void)(tag); if (0) printf(__VA_ARGS__); } while (0)
#define MALLOC_CAP_INTERNAL 1
#define MALLOC_CAP_8BIT 2
#define MALLOC_CAP_SPIRAM 4
#define CONFIG_BT_NIMBLE_HOST_TASK_STACK_SIZE 8192
#define CONFIG_BT_NIMBLE_PINNED_TO_CORE 0
#define configMAX_PRIORITIES 25
#define pdPASS 1
#define pdMS_TO_TICKS(x) (x)
#define BLE_HS_CONN_HANDLE_NONE 65535
#define BLE_ERR_REM_USER_CONN_TERM 19
#define BLE_HS_ETIMEOUT 13
#define BLE_HS_ENOENT 14
#define BLE_HS_ENOMEM 15
#define BLE_HS_EINVAL 16
#define BLE_HS_EALREADY 17
#define BLE_HS_ENOTSUP 18
#define BLE_HS_EDONE 19
#define BLE_HS_ATT_ERR(x) (0x100+(x))
#define BLE_ATT_ERR_INSUFFICIENT_AUTHEN 5
#define BLE_ATT_ERR_INSUFFICIENT_ENC 15
#define BLE_SM_IO_CAP_NO_IO 0
#define BLE_SM_PAIR_KEY_DIST_ENC 1
#define BLE_SM_PAIR_KEY_DIST_ID 2
#define BLE_GATT_CHR_PROP_NOTIFY 16
#define BLE_GAP_REPEAT_PAIRING_IGNORE 0
enum {BLE_GAP_EVENT_DISC=1,BLE_GAP_EVENT_DISC_COMPLETE,BLE_GAP_EVENT_CONNECT,BLE_GAP_EVENT_ENC_CHANGE,
    BLE_GAP_EVENT_REPEAT_PAIRING,BLE_GAP_EVENT_PASSKEY_ACTION,BLE_GAP_EVENT_NOTIFY_RX,BLE_GAP_EVENT_DISCONNECT};
typedef struct {uint16_t value;} ble_uuid_t;
typedef struct {ble_uuid_t u;uint16_t value;} ble_uuid16_t;
typedef struct {uint8_t type,val[6];} ble_addr_t;
struct os_mbuf {uint8_t* data;size_t len;struct os_mbuf* next;};
size_t fake_mbuf_len(struct os_mbuf*);
#define OS_MBUF_PKTLEN(om) fake_mbuf_len(om)
int os_mbuf_copydata(struct os_mbuf*,int,int,void*);
struct ble_gap_event {
    int type;
    struct {ble_addr_t addr;const uint8_t* data;uint8_t length_data;int8_t rssi;} disc;
    struct {int status;uint16_t conn_handle;} connect,enc_change;
    struct {uint16_t conn_handle,attr_handle;struct os_mbuf* om;} notify_rx;
};
struct ble_gap_conn_desc {ble_addr_t peer_id_addr;};
struct ble_gap_disc_params {uint16_t itvl,window;uint8_t passive,filter_duplicates;};
struct ble_hs_adv_fields {const uint8_t* name;uint8_t name_len,num_uuids16;ble_uuid16_t* uuids16;};
struct ble_gatt_error {int status;};
struct ble_gatt_attr {struct os_mbuf* om;uint16_t offset;};
struct ble_gatt_dsc {struct {ble_uuid_t u;}uuid;uint16_t handle;};
struct ble_gatt_chr {struct {ble_uuid_t u;}uuid;uint16_t def_handle,val_handle;uint8_t properties;};
struct ble_gatt_svc {struct {ble_uuid_t u;}uuid;uint16_t start_handle,end_handle;};
struct ble_npl_event {void(*fn)(struct ble_npl_event*);};
struct ble_npl_callout {void(*fn)(struct ble_npl_event*);bool active;};
struct ble_npl_eventq {int unused;};
typedef int (*attr_cb_t)(uint16_t,const struct ble_gatt_error*,struct ble_gatt_attr*,void*);
typedef int (*dsc_cb_t)(uint16_t,const struct ble_gatt_error*,uint16_t,const struct ble_gatt_dsc*,void*);
typedef int (*chr_cb_t)(uint16_t,const struct ble_gatt_error*,const struct ble_gatt_chr*,void*);
typedef int (*svc_cb_t)(uint16_t,const struct ble_gatt_error*,const struct ble_gatt_svc*,void*);
typedef int (*gap_cb_t)(struct ble_gap_event*,void*);
extern struct {void(*sync_cb)(void);void(*reset_cb)(int);int(*store_status_cb)(int,void*);int sm_io_cap,sm_bonding,sm_mitm,sm_sc,sm_our_key_dist,sm_their_key_dist;} ble_hs_cfg;
int64_t esp_timer_get_time(void);
size_t heap_caps_get_free_size(int);
size_t heap_caps_get_largest_free_block(int);
void vTaskDelay(unsigned);
void vTaskDelete(void*);
int xTaskCreatePinnedToCore(void(*)(void*),const char*,unsigned,void*,int,void*,int);
int nimble_port_init(void);
int nimble_port_deinit(void);
int nimble_port_stop(void);
void nimble_port_run(void);
struct ble_npl_eventq* nimble_port_get_dflt_eventq(void);
void ble_npl_event_init(struct ble_npl_event*,void(*)(struct ble_npl_event*),void*);
void ble_npl_event_deinit(struct ble_npl_event*);
void ble_npl_eventq_put(struct ble_npl_eventq*,struct ble_npl_event*);
void ble_npl_callout_init(struct ble_npl_callout*,struct ble_npl_eventq*,void(*)(struct ble_npl_event*),void*);
void ble_npl_callout_stop(struct ble_npl_callout*);
void ble_npl_callout_reset(struct ble_npl_callout*,unsigned);
void ble_npl_callout_deinit(struct ble_npl_callout*);
unsigned ble_npl_time_ms_to_ticks32(unsigned);
int ble_gap_connect(uint8_t,const ble_addr_t*,int,void*,gap_cb_t,void*);
int ble_gap_disc(uint8_t,int,const struct ble_gap_disc_params*,gap_cb_t,void*);
int ble_gap_disc_cancel(void);
int ble_gap_conn_cancel(void);
int ble_gap_terminate(uint16_t,int);
int ble_gap_conn_find(uint16_t,struct ble_gap_conn_desc*);
int ble_gap_security_initiate(uint16_t);
int ble_hs_util_ensure_addr(int);
int ble_hs_id_infer_auto(int,uint8_t*);
int ble_store_util_delete_peer(const ble_addr_t*);
int ble_store_util_status_rr(int,void*);
uint16_t ble_uuid_u16(const ble_uuid_t*);
int ble_hs_adv_parse_fields(struct ble_hs_adv_fields*,const uint8_t*,uint8_t);
int ble_gattc_write_flat(uint16_t,uint16_t,const void*,uint16_t,attr_cb_t,void*);
int ble_gattc_read(uint16_t,uint16_t,attr_cb_t,void*);
int ble_gattc_read_long(uint16_t,uint16_t,uint16_t,attr_cb_t,void*);
int ble_gattc_disc_all_dscs(uint16_t,uint16_t,uint16_t,dsc_cb_t,void*);
int ble_gattc_disc_all_chrs(uint16_t,uint16_t,uint16_t,chr_cb_t,void*);
int ble_gattc_disc_all_svcs(uint16_t,svc_cb_t,void*);
