/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 * 中文：真实解析器与产品状态机的畸形输入、失效与生命周期回归。
 * English: Malformed input, failures and lifecycle regressions for the real decoder and product state machine.
 */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../main/os/os_page_turner.c"
static preferences_t disk, pending;
static uint8_t tries_disk, tries_pending;
static bool have_disk;
static size_t disk_length=sizeof(preferences_t);
static int nvs_failure, starts, stops, connects, start_error, stop_error;
static bool headroom_ok = true;
static int reclaims;
static os_ble_snapshot_t radio;
static os_ble_input_t input[16];
static unsigned head, tail;
esp_err_t nvs_open(const char* ns, int mode, nvs_handle_t* h) { assert(!strcmp(ns,"rp_turner"));(void)mode;*h=1;return nvs_failure==1?ESP_FAIL:ESP_OK; }
void nvs_close(nvs_handle_t h) {assert(h==1);}
esp_err_t nvs_get_blob(nvs_handle_t h,const char* key,void* p,size_t* n){assert(h==1&&!strcmp(key,"prefs"));if(!have_disk)return ESP_ERR_NOT_FOUND;assert(*n>=disk_length);memcpy(p,&disk,disk_length);*n=disk_length;return ESP_OK;}
esp_err_t nvs_set_blob(nvs_handle_t h,const char* key,const void* p,size_t n){assert(h==1&&!strcmp(key,"prefs")&&n==sizeof(pending));pending=*(const preferences_t*)p;return nvs_failure==2?ESP_FAIL:ESP_OK;}
esp_err_t nvs_get_u8(nvs_handle_t h,const char* key,uint8_t* p){assert(h==1&&!strcmp(key,"tries"));*p=tries_disk;return ESP_OK;}
esp_err_t nvs_set_u8(nvs_handle_t h,const char* key,uint8_t p){assert(h==1&&!strcmp(key,"tries"));tries_pending=p;return nvs_failure==2?ESP_FAIL:ESP_OK;}
esp_err_t nvs_commit(nvs_handle_t h){assert(h==1);if(nvs_failure==3)return ESP_FAIL;disk=pending;disk_length=sizeof(disk);have_disk=true;tries_disk=tries_pending;return ESP_OK;}
esp_err_t os_ble_radio_start(void){++starts;radio.phase=start_error?OS_BLE_ERROR:OS_BLE_IDLE;if(start_error)snprintf(radio.error,sizeof(radio.error),"injected startup failure");return start_error?ESP_FAIL:ESP_OK;}
esp_err_t os_ble_radio_stop(void){++stops;if(stop_error)return ESP_FAIL;radio.phase=OS_BLE_OFF;return ESP_OK;}
esp_err_t os_ble_radio_scan(void){radio.phase=OS_BLE_SCANNING;return ESP_OK;}
esp_err_t os_ble_radio_connect(const os_ble_peer_t* peer){++connects;radio.peer=*peer;radio.phase=OS_BLE_CONNECTING;return ESP_OK;}
esp_err_t os_ble_radio_disconnect(void){radio.phase=OS_BLE_IDLE;return ESP_OK;}
esp_err_t os_ble_radio_forget(const os_ble_peer_t* peer){(void)peer;radio.phase=OS_BLE_IDLE;++radio.revision;return ESP_OK;}
void os_ble_radio_snapshot(os_ble_snapshot_t* out){*out=radio;}
bool os_ble_radio_pop(os_ble_input_t* out){if(tail==head)return false;*out=input[tail++%16];return true;}
void os_ble_radio_clear(void){head=tail=0;}
bool os_ble_radio_headroom(void){return headroom_ok;}
static void count_reclaim(void){++reclaims;headroom_ok=true;}
static void key(uint32_t at,uint8_t usage){os_ble_input_t e={.at_ms=at,.len=8};e.bytes[2]=usage;e.key=ble_hid_boot(e.bytes,8);input[head++%16]=e;}
static void raw(uint32_t at,uint8_t value){os_ble_input_t e={.at_ms=at,.len=1,.bytes={value}};input[head++%16]=e;}
// 非标准帧可落在任意通道：学习与触发允许使用不同通道号。/ Raw frames may arrive on any channel: learning and triggering may use distinct indices.
static void raw_ch(uint32_t at,uint8_t value,uint8_t channel){os_ble_input_t e={.at_ms=at,.len=1,.channel=channel,.bytes={value}};input[head++%16]=e;}
static void ready(int64_t at) {radio.phase=OS_BLE_READY;radio.peer=(os_ble_peer_t){.addr={1},.name="Test remote"};os_page_turner_poll(at,false,true);}
static void decoder_tests(void) {
    const uint8_t keyboard[]={0x05,1,0x09,6,0xa1,1,0x05,7,0x19,0xe0,0x29,0xe7,0x15,0,0x25,1,0x75,1,0x95,8,0x81,2,0x95,1,0x75,8,0x81,1,0x95,6,0x75,8,0x15,0,0x25,0x65,0x05,7,0x19,0,0x29,0x65,0x81,0,0xc0};
    ble_hid_map_t map;
    assert(ble_hid_parse(&map,keyboard,sizeof(keyboard)));
    uint8_t data[8]={1,0,0};
    ble_hid_key_t k=ble_hid_decode(&map,0,data,8);assert(k.known&&!k.down&&!k.direction);
    data[2]=0x4e;k=ble_hid_decode(&map,0,data,8);assert(k.down&&k.direction==1);
    data[2]=0x4b;k=ble_hid_decode(&map,0,data,8);assert(k.direction==-1);
    data[3]=0x4e;k=ble_hid_decode(&map,0,data,8);assert(!k.direction);
    assert(!ble_hid_decode(&map,0,data,2).known);
    assert(!ble_hid_decode(&map,9,data,8).known);
    const uint8_t consumer[]={0x05,0x0c,0x09,1,0xa1,1,0x85,3,0x19,0,0x2a,0xff,3,0x15,0,0x26,0xff,3,0x75,16,0x95,1,0x81,0,0xc0};
    assert(ble_hid_parse(&map,consumer,sizeof(consumer)));
    data[0]=0xe9;data[1]=0;k=ble_hid_decode(&map,3,data,2);assert(k.known&&k.direction==-1);
    data[0]=0xea;k=ble_hid_decode(&map,3,data,2);assert(k.direction==1);
    data[0]=0x24;data[1]=2;k=ble_hid_decode(&map,3,data,2);assert(k.down&&!k.direction&&k.usage==0x224);
    const uint8_t bitfield[]={0x05,0x0c,0x09,1,0xa1,1,0x09,0xe9,0x09,0xea,0x75,1,0x95,2,0x81,2,0xc0};
    assert(ble_hid_parse(&map,bitfield,sizeof(bitfield)));data[0]=2;assert(ble_hid_decode(&map,0,data,1).direction==1);
    const uint8_t bad[]={0x75,32,0x96,0xff,0xff,0x81,0};assert(!ble_hid_parse(&map,bad,sizeof(bad)));
    const uint8_t pop[]={0xb4};assert(!ble_hid_parse(&map,pop,sizeof(pop)));
    assert(!ble_hid_parse(&map,(const uint8_t*)"\xfe",1));
    // 随机畸形描述符/报告在 ASAN 与 UBSAN 下执行。/ Fuzz malformed descriptors/reports under ASAN/UBSAN.
    uint32_t random=42;uint8_t fuzz[300];
    for(unsigned run=0;run<20000;++run){for(unsigned i=0;i<sizeof(fuzz);++i){random=random*1664525+1013904223;fuzz[i]=random>>24;}ble_hid_parse(&map,fuzz,run%sizeof(fuzz));for(unsigned i=0;i<4;++i)ble_hid_decode(&map,i,fuzz,run%33);}
}
int main(void) {
    decoder_tests();
    os_page_turner_set_reclaimer(count_reclaim);
    os_page_turner_enter();os_page_turner_poll(1000,true,true);assert(starts==0&&!s_info.enabled);
    for(nvs_failure=1;nvs_failure<=3;++nvs_failure){assert(!os_page_turner_enable(true));assert(!s_info.enabled);}
    nvs_failure=0;assert(os_page_turner_enable(true));os_page_turner_poll(2000,true,true);assert(starts==1);
    ready(3000);assert(s_info.saved&&disk.saved);
    int stops_before = stops, starts_before = starts;
    os_page_turner_leave(); assert(radio.phase==OS_BLE_READY&&stops==stops_before);
    key(3500,0x4e);assert(!os_page_turner_poll(3500,true,true));
    os_page_turner_enter();assert(!os_page_turner_poll(3600,false,false));
    assert(radio.phase==OS_BLE_READY&&starts==starts_before&&stops==stops_before);
    assert(!os_page_turner_poll(3900,true,true));
    key(4000,0x4e);assert(os_page_turner_poll(4000,true,true)==1);
    key(4100,0x4e);assert(os_page_turner_poll(4100,true,true)==0);
    key(5000,0x4e);assert(os_page_turner_poll(5000,true,true)==0);
    key(5100,0);key(5400,0x4e);assert(os_page_turner_poll(5400,true,true)==1);
    key(5500,0);key(5800,0x4b);assert(!os_page_turner_poll(5800,false,true));assert(!os_page_turner_poll(5900,true,true));
    key(6000,0);key(6100,0x4e);assert(!os_page_turner_poll(8000,true,true));
    key(8100,0);key(8500,0x4e);key(8600,0);key(8900,0x4b);assert(os_page_turner_poll(9000,true,true)==1);assert(!os_page_turner_poll(9100,true,true));
    assert(os_page_turner_reverse());key(9200,0);key(9600,0x4e);assert(os_page_turner_poll(9600,true,true)==-1);
    nvs_failure=3;assert(!os_page_turner_reverse()&&s_info.reversed);nvs_failure=0;
    os_page_turner_learn(-1);raw(10000,0x8a);assert(!os_page_turner_poll(10000,false,true));assert(s_info.bound_prev&&!s_info.learning);
    os_page_turner_learn(1);raw(10500,0x8a);os_page_turner_poll(10500,false,true);assert(s_info.learning==1&&!s_info.bound_next);
    nvs_failure=3;raw(11000,0x8b);os_page_turner_poll(11000,false,true);assert(s_info.learning==1&&!s_info.bound_next);nvs_failure=0;
    raw(11500,0x8b);os_page_turner_poll(11500,false,true);assert(s_info.bound_next&&!s_info.learning);
    raw(12000,0x8b);assert(os_page_turner_poll(12000,true,true)==-1);
    raw(12050,0x8b);assert(!os_page_turner_poll(12050,true,true));
    raw(12500,0x8b);assert(os_page_turner_poll(12500,true,true)==-1);
    stop_error=1;assert(!os_page_turner_pause());assert(!os_page_turner_enable(false)&&s_info.enabled);stop_error=0;
    assert(os_page_turner_pause());assert(radio.phase==OS_BLE_OFF);
    os_page_turner_poll(13000,true,true);os_page_turner_poll(13600,true,true);assert(starts==2&&connects==1);
    ready(14000);radio.phase=OS_BLE_IDLE;os_page_turner_poll(25000,false,true);assert(connects==2);
    ready(26000);os_page_turner_disconnect();os_page_turner_poll(50000,false,true);assert(connects==2);
    assert(os_page_turner_forget());os_page_turner_poll(51000,false,true);assert(!s_info.saved&&!disk.saved&&!s_info.bound_next);
    os_page_turner_leave();os_page_turner_poll(60000,true,true);assert(starts==2);
    os_page_turner_enter();start_error=1;os_page_turner_poll(61000,false,true);assert(starts==3);
    os_page_turner_poll(61001,false,true);assert(starts==3);
    os_page_turner_poll(91000,false,true);assert(starts==4);os_page_turner_poll(121000,false,true);assert(starts==5);
    os_page_turner_poll(151000,false,true);assert(starts==5);
    assert(os_page_turner_enable(false));assert(os_page_turner_enable(true));start_error=0;
    os_page_turner_poll(152000,false,true);assert(starts==6);
    ready(153000);
    os_page_turner_learn(OS_PAGE_TURNER_LOCK);
    key(154000,0x4e);assert(!os_page_turner_poll(154000,true,true));assert(s_info.learning==2&&!s_info.bound_lock);
    key(154100,0);key(154500,4);nvs_failure=3;
    assert(!os_page_turner_poll(154500,true,true));assert(!s_info.bound_lock&&s_info.learning==2);nvs_failure=0;
    key(154600,0);key(155000,4);assert(!os_page_turner_poll(155000,true,true));assert(s_info.bound_lock&&!s_info.learning);
    key(155100,0);key(155500,4);assert(!os_page_turner_poll(155500,false,true));
    assert(!os_page_turner_poll(155600,true,true));
    key(155700,0);key(156100,4);assert(os_page_turner_poll(156100,true,true)==OS_PAGE_TURNER_LOCK);
    key(157100,4);assert(!os_page_turner_poll(157100,true,true));
    assert(os_page_turner_reverse());
    key(157200,0);key(157600,4);assert(os_page_turner_poll(157600,true,true)==OS_PAGE_TURNER_LOCK);
    key(157700,0);key(158100,4);assert(!os_page_turner_poll(159700,true,true));
    os_page_turner_learn(-1);key(159200,0);key(159600,4);
    assert(!os_page_turner_poll(159600,false,true));assert(s_info.learning==-1&&!s_info.bound_prev);
    os_page_turner_learn(0);assert(os_page_turner_restore_mapping());assert(!s_info.bound_lock);
    os_page_turner_learn(-1);raw(160100,0x81);os_page_turner_poll(160100,false,true);
    os_page_turner_learn(OS_PAGE_TURNER_LOCK);raw(160600,0x81);os_page_turner_poll(160600,false,true);
    assert(!s_info.bound_lock&&s_info.learning==2);
    raw(161100,0x83);os_page_turner_poll(161100,false,true);assert(s_info.bound_lock);
    s_loaded=false;memset(&s_prefs,0,sizeof(s_prefs));load();assert(s_info.bound_lock&&s_info.bound_prev);
    legacy_preferences_t legacy={.magic=0x31545052,.enabled=1,.reversed=1,.saved=1,.peer={.addr={1},.name="Legacy"}};
    legacy.binding[0]=(binding_t){.len=1,.bytes={0x81}};legacy.binding[1]=(binding_t){.len=1,.bytes={0x82}};
    memset(&disk,0xcc,sizeof(disk));memcpy(&disk,&legacy,sizeof(legacy));disk_length=sizeof(legacy);
    s_loaded=false;memset(&s_prefs,0,sizeof(s_prefs));load();
    assert(s_info.enabled&&s_info.reversed&&s_info.saved&&s_info.bound_prev&&s_info.bound_next&&!s_info.bound_lock);
    assert(disk.magic==0x31545052&&disk_length==sizeof(legacy));
    assert(os_page_turner_reverse());assert(disk.magic==0x32545052&&disk_length==sizeof(preferences_t));
    // 错误相在间隔后自动重连，不再等待用户干预。/ The error phase auto-reconnects after the interval instead of waiting for the user.
    radio.phase=OS_BLE_ERROR;
    int connects_before=connects;
    os_page_turner_poll(300000,false,true);
    assert(connects==connects_before+1&&radio.phase==OS_BLE_CONNECTING);
    // 地址轮换替换保存设备时保留学习按键。/ Replacing the saved peer after address rotation keeps learned keys.
    radio.phase=OS_BLE_READY;radio.peer=(os_ble_peer_t){.addr={7},.name="Rotated"};
    os_page_turner_poll(310000,false,true);
    assert(s_info.saved&&s_info.selected.addr[0]==7&&s_info.bound_prev&&s_info.bound_next);
    assert(disk.saved&&disk.peer.addr[0]==7&&disk.binding[0].bytes[0]==0x81&&disk.binding[1].bytes[0]==0x82);
    // 绑定按帧内容匹配：学习用通道 2，触发用通道 5。/ Bindings match frame content: learn on channel 2, trigger on channel 5.
    os_page_turner_learn(-1);raw_ch(320000,0x71,2);os_page_turner_poll(320000,false,true);
    assert(s_info.bound_prev&&!s_info.learning);
    raw_ch(320500,0,2);raw_ch(321500,0x71,5);
    assert(os_page_turner_poll(321500,true,true)==-1);
    // 排队 1.3 秒的按键仍有效，1.9 秒的过期。/ A press queued 1.3 s still turns; 1.9 s expires.
    raw_ch(322000,0,5);raw_ch(322500,0x71,5);
    assert(os_page_turner_poll(323800,true,true)==-1);
    raw_ch(324000,0,5);raw_ch(324500,0x71,5);
    assert(!os_page_turner_poll(326400,true,true));
    // 启动内存不足先请求一次回收再启动；余量充足时不打扰缓存。/ Short start headroom reclaims once before starting; ample headroom never disturbs caches.
    assert(reclaims==0);
    assert(os_page_turner_pause());
    os_page_turner_enter();
    headroom_ok=false;
    os_page_turner_poll(330000,true,true);
    assert(reclaims==1&&starts==7);
    os_page_turner_set_reclaimer(NULL);
    puts("BLE HID decoder, malformed input, persistence faults, learning, input gates and reconnect lifecycle: PASS");
}
