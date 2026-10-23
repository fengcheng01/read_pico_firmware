/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 * 中文：保留真实无线回调和生命周期，只替换 IDF 无线/RTOS API。
 * English: Retain real radio callbacks and lifecycle, substituting only IDF radio/RTOS APIs.
 */
#include <assert.h>
#include "../main/os/os_ble_radio_pico.c"
__typeof__(ble_hs_cfg) ble_hs_cfg;
static int64_t clock_us;
static int init_count,deinit_count,stop_count,init_error,task_error,stop_error,connect_count,security_count,forget_error,deinit_error;
static int scan_count, cancel_count, cancel_error;
static size_t free_internal=160*1024,largest_internal=64*1024;
static struct ble_npl_event* queued;
static struct ble_npl_eventq queue;
static gap_cb_t gap_callback;
static svc_cb_t svc_callback;
static chr_cb_t chr_callback;
static dsc_cb_t dsc_callback;
static attr_cb_t attr_callback;
static void* callback_arg;
static uint16_t requested_handle,requested_end;
static struct ble_hs_adv_fields advertisement;
static ble_addr_t forgotten;
int64_t esp_timer_get_time(void){return clock_us;}
size_t heap_caps_get_free_size(int caps){return caps==MALLOC_CAP_SPIRAM?1024*1024:free_internal;}
size_t heap_caps_get_largest_free_block(int caps){(void)caps;return largest_internal;}
void vTaskDelay(unsigned ms){clock_us+=ms*1000;}
void vTaskDelete(void* t){(void)t;}
int xTaskCreatePinnedToCore(void(*fn)(void*),const char* name,unsigned stack,void* arg,int priority,void* task,int core){
    assert(fn&&name&&stack==8192&&!arg&&priority==21&&!task&&core==0);return task_error?0:pdPASS;
}
int nimble_port_init(void){++init_count;return init_error;}
int nimble_port_deinit(void){++deinit_count;return deinit_error;}
int nimble_port_stop(void){++stop_count;if(!stop_error)atomic_store(&s_exited,true);return stop_error;}
void nimble_port_run(void){}
struct ble_npl_eventq* nimble_port_get_dflt_eventq(void){return &queue;}
void ble_npl_event_init(struct ble_npl_event* e,void(*fn)(struct ble_npl_event*),void* arg){assert(!arg);e->fn=fn;}
void ble_npl_event_deinit(struct ble_npl_event* e){assert(e);queued=NULL;}
void ble_npl_eventq_put(struct ble_npl_eventq* q,struct ble_npl_event* e){assert(q==&queue);queued=e;}
void ble_npl_callout_init(struct ble_npl_callout* c,struct ble_npl_eventq* q,void(*fn)(struct ble_npl_event*),void* arg){assert(q==&queue&&!arg);c->fn=fn;c->active=false;}
void ble_npl_callout_stop(struct ble_npl_callout* c){c->active=false;}
void ble_npl_callout_reset(struct ble_npl_callout* c,unsigned ms){assert(ms==18000);c->active=true;}
void ble_npl_callout_deinit(struct ble_npl_callout* c){c->active=false;}
unsigned ble_npl_time_ms_to_ticks32(unsigned x){return x;}
int ble_gap_connect(uint8_t type,const ble_addr_t* peer,int ms,void* params,gap_cb_t cb,void* arg){assert(!type&&peer&&ms==10000&&!params&&!arg);++connect_count;gap_callback=cb;return 0;}
int ble_gap_disc(uint8_t type,int ms,const struct ble_gap_disc_params* params,gap_cb_t cb,void* arg){assert(!type&&ms==12000&&params&&!arg);++scan_count;gap_callback=cb;return 0;}
int ble_gap_disc_cancel(void){++cancel_count;return cancel_error;}
int ble_gap_conn_cancel(void){return 0;}
int ble_gap_terminate(uint16_t conn,int reason){assert(conn==7&&reason==19);return 0;}
int ble_gap_conn_find(uint16_t conn,struct ble_gap_conn_desc* desc){assert(conn==7);desc->peer_id_addr=(ble_addr_t){.type=0,.val={1,2,3,4,5,6}};return 0;}
int ble_gap_security_initiate(uint16_t conn){assert(conn==7);++security_count;return 0;}
int ble_hs_util_ensure_addr(int value){assert(!value);return 0;}
int ble_hs_id_infer_auto(int value,uint8_t* type){assert(!value);*type=0;return 0;}
int ble_store_util_delete_peer(const ble_addr_t* peer){forgotten=*peer;return forget_error;}
int ble_store_util_status_rr(int event,void* arg){(void)event;(void)arg;return 0;}
uint16_t ble_uuid_u16(const ble_uuid_t* uuid){return uuid->value;}
int ble_hs_adv_parse_fields(struct ble_hs_adv_fields* fields,const uint8_t* data,uint8_t len){(void)data;(void)len;*fields=advertisement;return 0;}
int ble_gattc_write_flat(uint16_t conn,uint16_t handle,const void* data,uint16_t n,attr_cb_t cb,void* arg){assert(conn==7&&n==2&&((const uint8_t*)data)[0]==1);requested_handle=handle;attr_callback=cb;callback_arg=arg;return 0;}
int ble_gattc_read(uint16_t conn,uint16_t handle,attr_cb_t cb,void* arg){assert(conn==7);requested_handle=handle;attr_callback=cb;callback_arg=arg;return 0;}
int ble_gattc_read_long(uint16_t conn,uint16_t handle,uint16_t offset,attr_cb_t cb,void* arg){assert(conn==7&&!offset);requested_handle=handle;attr_callback=cb;callback_arg=arg;return 0;}
int ble_gattc_disc_all_dscs(uint16_t conn,uint16_t handle,uint16_t end,dsc_cb_t cb,void* arg){assert(conn==7);requested_handle=handle;requested_end=end;dsc_callback=cb;callback_arg=arg;return 0;}
int ble_gattc_disc_all_chrs(uint16_t conn,uint16_t start,uint16_t end,chr_cb_t cb,void* arg){assert(conn==7&&start==1&&end==30&&!arg);chr_callback=cb;return 0;}
int ble_gattc_disc_all_svcs(uint16_t conn,svc_cb_t cb,void* arg){assert(conn==7&&!arg);svc_callback=cb;return 0;}
size_t fake_mbuf_len(struct os_mbuf* om){size_t n=0;for(;om;om=om->next)n+=om->len;return n;}
int os_mbuf_copydata(struct os_mbuf* om,int off,int len,void* out){
    uint8_t* target=out;for(;om&&off>=(int)om->len;om=om->next)off-=om->len;
    for(;len&&om;om=om->next){int n=(int)om->len-off;if(n>len)n=len;memcpy(target,om->data+off,n);target+=n;len-=n;off=0;}return len?-1:0;
}
static void run_command(void){assert(queued);struct ble_npl_event* e=queued;queued=NULL;e->fn(e);}
static const struct ble_gatt_error ok={0},done={BLE_HS_EDONE};
static void emit_char(uint16_t def,uint16_t value,uint16_t uuid,uint8_t properties){
    struct ble_gatt_chr chr={.def_handle=def,.val_handle=value,.properties=properties,.uuid.u.value=uuid};chr_callback(7,&ok,&chr,NULL);
}
static void emit_dsc(uint16_t handle,uint16_t uuid){struct ble_gatt_dsc dsc={.handle=handle,.uuid.u.value=uuid};dsc_callback(7,&ok,requested_handle,&dsc,callback_arg);}
static void attr_value(uint8_t* data,size_t n){struct os_mbuf om={.data=data,.len=n};struct ble_gatt_attr attr={.om=&om};attr_callback(7,&ok,&attr,callback_arg);}
static void disconnect_event(void){struct ble_gap_event event={.type=BLE_GAP_EVENT_DISCONNECT};gap_callback(&event,NULL);}
int main(void){
    free_internal=1;assert(os_ble_radio_start()==ESP_ERR_NO_MEM&&init_count==0&&s_status.phase==OS_BLE_ERROR);
    assert(os_ble_radio_stop()==ESP_OK&&s_status.phase==OS_BLE_OFF);free_internal=160*1024;
    init_error=ESP_FAIL;assert(os_ble_radio_start()==ESP_FAIL&&s_status.phase==OS_BLE_ERROR);
    assert(os_ble_radio_stop()==ESP_OK&&s_status.phase==OS_BLE_OFF);init_error=0;
    task_error=1;assert(os_ble_radio_start()==ESP_ERR_NO_MEM&&deinit_count==1&&s_status.phase==OS_BLE_ERROR);
    assert(os_ble_radio_stop()==ESP_OK&&s_status.phase==OS_BLE_OFF);task_error=0;
    assert(os_ble_radio_start()==ESP_OK&&s_status.phase==OS_BLE_STARTING);assert(os_ble_radio_scan()==ESP_ERR_INVALID_STATE);
    ble_hs_cfg.sync_cb();assert(s_status.phase==OS_BLE_IDLE&&ble_hs_cfg.sm_bonding&&ble_hs_cfg.sm_sc);
    assert(os_ble_radio_scan()==ESP_OK);run_command();assert(s_status.phase==OS_BLE_SCANNING);
    assert(os_ble_radio_scan()==ESP_OK);run_command();assert(scan_count==2&&cancel_count==1&&s_status.phase==OS_BLE_SCANNING);
    struct ble_gap_event advert={.type=BLE_GAP_EVENT_DISC,.disc.addr.val={8},.disc.rssi=-42};
    advertisement=(struct ble_hs_adv_fields){.name=(const uint8_t*)"Remote",.name_len=6};gap_callback(&advert,NULL);assert(s_status.count==1);
    ble_uuid16_t hid={.value=0x1812};advertisement.uuids16=&hid;advertisement.num_uuids16=1;
    gap_callback(&advert,NULL);assert(s_status.count==1&&s_status.peers[0].hid);
    for(unsigned i=0;i<25;++i){advert.disc.addr.val[0]=i+20;gap_callback(&advert,NULL);}assert(s_status.count==OS_BLE_PEERS);
    os_ble_peer_t peer=s_status.peers[0];
    cancel_error=BLE_HS_ETIMEOUT;assert(os_ble_radio_connect(&peer)==ESP_OK);run_command();assert(s_status.phase==OS_BLE_ERROR&&!connect_count&&!s_timeout.active);
    cancel_error=0;assert(os_ble_radio_connect(&peer)==ESP_OK);run_command();assert(!s_pending&&connect_count==1&&s_status.phase==OS_BLE_CONNECTING&&s_timeout.active);
    struct ble_gap_event event={.type=BLE_GAP_EVENT_DISC_COMPLETE};gap_callback(&event,NULL);assert(connect_count==1&&s_status.phase==OS_BLE_CONNECTING);
    s_timeout.fn(NULL);assert(s_status.phase==OS_BLE_ERROR&&!s_timeout.active);
    assert(os_ble_radio_connect(&peer)==ESP_OK);run_command();assert(connect_count==2&&s_status.phase==OS_BLE_CONNECTING);
    event=(struct ble_gap_event){.type=BLE_GAP_EVENT_CONNECT,.connect.conn_handle=7};gap_callback(&event,NULL);
    assert(s_status.phase==OS_BLE_DISCOVERING&&security_count==0&&s_timeout.active);
    struct ble_gatt_svc svc={.uuid.u.value=0x1812,.start_handle=1,.end_handle=30};svc_callback(7,&ok,&svc,NULL);svc_callback(7,&done,NULL,NULL);
    emit_char(9,10,0x2a4b,2);emit_char(19,20,0x2a4d,16);emit_char(24,25,0x2a4d,16);chr_callback(7,&done,NULL,NULL);
    assert(requested_handle==20&&requested_end==23);emit_dsc(21,0x2902);emit_dsc(22,0x2908);dsc_callback(7,&done,20,NULL,callback_arg);
    assert(requested_handle==25&&requested_end==30);emit_dsc(26,0x2902);emit_dsc(27,0x2908);dsc_callback(7,&done,25,NULL,callback_arg);
    assert(requested_handle==10);
    uint8_t map[]={0x05,7,0x75,8,0x95,2,0x81,1,0x95,6,0x81,0};
    struct os_mbuf second={.data=map+4,.len=sizeof(map)-4},first={.data=map,.len=4,.next=&second};
    struct ble_gatt_attr attr={.om=&first};attr_callback(7,&ok,&attr,NULL);attr_callback(7,&done,NULL,NULL);
    assert(requested_handle==22);uint8_t ref[]={0,1};attr_value(ref,2);assert(requested_handle==27);ref[1]=2;attr_value(ref,2);
    assert(requested_handle==21&&s_status.phase!=OS_BLE_READY);attr_callback(7,&ok,NULL,NULL);assert(s_status.phase==OS_BLE_READY&&!s_timeout.active);
    uint8_t keybytes[]={0,0,0x4e,0,0,0,0,0};struct os_mbuf keyom={.data=keybytes,.len=8};
    event=(struct ble_gap_event){.type=BLE_GAP_EVENT_NOTIFY_RX,.notify_rx={.conn_handle=7,.attr_handle=20,.om=&keyom}};
    gap_callback(&event,NULL);os_ble_input_t input;assert(os_ble_radio_pop(&input)&&input.key.direction==1);
    event.notify_rx.attr_handle=25;gap_callback(&event,NULL);assert(!os_ble_radio_pop(&input));
    event.notify_rx.attr_handle=999;gap_callback(&event,NULL);assert(!os_ble_radio_pop(&input));
    event.notify_rx.attr_handle=20;for(unsigned i=0;i<100;++i)gap_callback(&event,NULL);unsigned count=0;while(os_ble_radio_pop(&input))++count;assert(count<INPUTS);
    gap_callback(&event,NULL);disconnect_event();assert(s_status.phase==OS_BLE_IDLE&&!os_ble_radio_pop(&input));
    assert(os_ble_radio_forget(&peer)==ESP_OK);run_command();assert(!memcmp(forgotten.val,peer.addr,6));
    forget_error=42;assert(os_ble_radio_forget(&peer)==ESP_OK);run_command();assert(s_status.phase==OS_BLE_ERROR);
    s_conn=7;s_security_retry=false;assert(secure_if_needed(BLE_HS_ATT_ERR(5))&&security_count==1);assert(!secure_if_needed(BLE_HS_ATT_ERR(5)));
    s_timeout.fn(NULL);assert(s_status.phase==OS_BLE_ERROR);
    stop_error=1;assert(os_ble_radio_stop()==ESP_FAIL&&atomic_load(&s_running));stop_error=0;
    assert(os_ble_radio_stop()==ESP_OK&&!atomic_load(&s_running)&&s_status.phase==OS_BLE_OFF);
    assert(stop_count==2&&deinit_count==2);
    assert(os_ble_radio_start()==ESP_OK);ble_hs_cfg.sync_cb();assert(os_ble_radio_stop()==ESP_OK);
    assert(os_ble_radio_start()==ESP_OK);ble_hs_cfg.sync_cb();deinit_error=1;
    assert(os_ble_radio_stop()!=ESP_OK);int deinit_before=deinit_count;
    assert(os_ble_radio_stop()!=ESP_OK&&deinit_count==deinit_before);
    puts("BLE radio: scan/selection, serial GATT bounds, report references, chained mbuf, input readiness, auth retry, forget/stop faults and restart: PASS");
}
