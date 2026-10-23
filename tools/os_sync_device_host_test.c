/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 * 中文：真实设备同步桥的线程、取消、确认和冲突回归；网络与 MD5 使用替身。
 * English: Real device-bridge thread, cancellation, confirmation and conflict regressions with fake network/MD5.
 */
#include "os_sync.h"
#include "read_pico_transfer.h"
#include "os_sync_http.h"
#include "book_progress.h"
#include "esp_http_client.h"
#include "esp_heap_caps.h"
#include "esp_rom_md5.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include <assert.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
static bool test_ble_stopped = true;
bool os_page_turner_pause(void) { return test_ble_stopped; }
static pthread_t ui, worker;
static char path[288];
static book_progress_t progress={.file_size=100,.chapter=1,.byte_off=20,.px=76,.pct=20,.pct_frac=37,.last_open_s=1};
static unsigned saves;
static bool paragraph_mapping;
static const char* paragraph_path="/body/DocFragment[3]/body/div[1]/section[1]/div[2]/p[19]/text()[2].137";
bool book_epub_sync_encode(const char* name,uint16_t ch,uint32_t off,char* out,size_t cap) {
    (void)name;(void)ch;(void)off;
    assert(!pthread_equal(ui,pthread_self()));
    if(!paragraph_mapping)return false;
    snprintf(out,cap,"%s",paragraph_path);return true;
}
bool book_epub_sync_decode(const char* name,const char* p,uint16_t* ch,uint32_t* off) {
    (void)name;assert(!pthread_equal(ui,pthread_self()));
    if(!paragraph_mapping||strcmp(p,paragraph_path))return false;
    *ch=2;*off=37;return true;
}
static atomic_bool pause_read, link_ready;
static unsigned network_polls, http_calls;
static const char* response;
static const char* active_response;
static const char* first_get_response;
static int first_get_status, active_status;
static unsigned pull_calls, push_calls, last_path_calls, last_path_saves;
static size_t received;
static char request_body[4096];
static size_t sent;
static int request_method;
static unsigned task_creates;
static int worker_stack_caps;
static pthread_mutex_t notify_lock=PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t notify_changed=PTHREAD_COND_INITIALIZER;
static unsigned notifications;
struct test_sem {pthread_mutex_t lock;pthread_cond_t changed;bool ready;};
SemaphoreHandle_t xSemaphoreCreateBinary(void) {SemaphoreHandle_t s=calloc(1,sizeof(*s));pthread_mutex_init(&s->lock,NULL);pthread_cond_init(&s->changed,NULL);return s;}
int xSemaphoreGive(SemaphoreHandle_t s) {pthread_mutex_lock(&s->lock);s->ready=true;pthread_cond_signal(&s->changed);pthread_mutex_unlock(&s->lock);return 1;}
int xSemaphoreTake(SemaphoreHandle_t s,uint32_t timeout) {pthread_mutex_lock(&s->lock);while(!s->ready && timeout)pthread_cond_wait(&s->changed,&s->lock);bool ok=s->ready;if(ok)s->ready=false;pthread_mutex_unlock(&s->lock);return ok;}
static void* run_task(void* arg) {((void(*)(void*))arg)(NULL);return NULL;}
int xTaskCreate(void(*fn)(void*),const char* name,unsigned size,void* arg,int priority,void* handle) {(void)name;(void)size;(void)arg;(void)priority;(void)handle;return !pthread_create(&worker,NULL,run_task,(void*)fn);}
TaskHandle_t xTaskCreateStatic(void(*fn)(void*),const char* name,unsigned size,void* arg,int priority,StackType_t* stack,StaticTask_t* tcb) {
    (void)name;(void)arg;(void)priority;assert(size==12288&&stack&&tcb);++task_creates;
    assert(!pthread_create(&worker,NULL,run_task,(void*)fn));return (TaskHandle_t)1;
}
void vTaskDelete(void* arg) {(void)arg;pthread_exit(NULL);}
unsigned ulTaskNotifyTake(int clear,uint32_t ticks) {(void)clear;(void)ticks;pthread_mutex_lock(&notify_lock);while(!notifications)pthread_cond_wait(&notify_changed,&notify_lock);--notifications;pthread_mutex_unlock(&notify_lock);return 1;}
void xTaskNotifyGive(TaskHandle_t task) {(void)task;pthread_mutex_lock(&notify_lock);++notifications;pthread_cond_signal(&notify_changed);pthread_mutex_unlock(&notify_lock);}
void* heap_caps_malloc(size_t size,int caps) {worker_stack_caps=caps;return malloc(size);}
void heap_caps_free(void* ptr) {free(ptr);}
int64_t esp_timer_get_time(void) {struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return (int64_t)t.tv_sec*1000000+t.tv_nsec/1000;}
void os_time_force_poll(void) {}
void os_time_poll(int64_t now) {(void)now;}
void os_time_network(bool online) {(void)online;}
int esp_http_client_get_errno(esp_http_client_handle_t c) {(void)c;return 0;}
esp_err_t esp_http_client_get_and_clear_last_tls_error(esp_http_client_handle_t c,int* tls,int* flags) {(void)c;*tls=*flags=0;return ESP_OK;}
void esp_crt_bundle_attach(void) {}
esp_http_client_handle_t esp_http_client_init(const esp_http_client_config_t* cfg) {
    assert(!pthread_equal(ui,pthread_self()));assert(cfg->timeout_ms==15000 && atomic_load(&link_ready));
    ++http_calls;received=0;request_method=cfg->method;active_status=200;active_response=response;
    if(request_method==HTTP_METHOD_GET){
        ++pull_calls;
        if(first_get_status){active_status=first_get_status;first_get_status=0;active_response="";}
        else if(first_get_response){active_response=first_get_response;first_get_response=NULL;}
    } else if(request_method==HTTP_METHOD_PUT) ++push_calls;
    return (void*)1;
}
esp_err_t esp_http_client_set_header(esp_http_client_handle_t c,const char* a,const char* b) {(void)c;(void)a;(void)b;return ESP_OK;}
esp_err_t esp_http_client_open(esp_http_client_handle_t c,int size) {(void)c;assert(size<(int)sizeof(request_body));if(request_method==HTTP_METHOD_PUT){sent=0;request_body[0]=0;}return ESP_OK;}
int esp_http_client_write(esp_http_client_handle_t c,const char* text,int size) {(void)c;int n=size>3?3:size;assert(sent+n<sizeof(request_body));memcpy(request_body+sent,text,n);sent+=n;request_body[sent]=0;return n;}
int esp_http_client_read(esp_http_client_handle_t c,char* text,int cap) {(void)c;if(atomic_load(&pause_read)){usleep(2000);return -1;}size_t n=strlen(active_response)-received;if(n>5)n=5;if(n>(size_t)cap)n=cap;memcpy(text,active_response+received,n);received+=n;return (int)n;}
int esp_http_client_fetch_headers(esp_http_client_handle_t c) {(void)c;return 0;}
int esp_http_client_get_status_code(esp_http_client_handle_t c) {(void)c;return active_status;}
bool esp_http_client_is_complete_data_received(esp_http_client_handle_t c) {(void)c;return received==strlen(active_response);}
esp_err_t esp_http_client_cleanup(esp_http_client_handle_t c) {(void)c;return ESP_OK;}
void esp_rom_md5_init(md5_context_t* ctx) {ctx->sum=0;}
void esp_rom_md5_update(md5_context_t* ctx,const void* in,uint32_t n) {const unsigned char* p=in;for(uint32_t i=0;i<n;i++)ctx->sum+=p[i];}
void esp_rom_md5_final(unsigned char out[16],md5_context_t* ctx) {memset(out,ctx->sum&255,16);}
const char* app_settings_sync_url(void) {assert(pthread_equal(ui,pthread_self()));return "https://example.test";}
const char* app_settings_sync_user(void) {return "reader";}
const char* app_settings_sync_key(void) {return "0123456789abcdef0123456789abcdef";}
void app_settings_set_sync_key(const char* key) {(void)key;}
int app_settings_book_px(void) {return 48;}
bool book_progress_last_path(char* out,size_t cap) {assert(pthread_equal(ui,pthread_self()));++last_path_calls;snprintf(out,cap,"%s",path);return true;}
static bool has_progress=true;
bool book_progress_load(const char* name,uint32_t size,book_progress_t* out) {(void)name;assert(pthread_equal(ui,pthread_self()));assert(size==100);if(!has_progress)return false;*out=progress;return true;}
esp_err_t book_progress_save(const char* name,const book_progress_t* p) {(void)name;assert(pthread_equal(ui,pthread_self()));progress=*p;++saves;return ESP_OK;}
esp_err_t book_progress_set_last_path(const char* name) {(void)name;assert(pthread_equal(ui,pthread_self()));++last_path_saves;return ESP_OK;}
static bool claimed;
static read_pico_transfer_status_t net;
static unsigned net_starts, net_stops;
static bool wifi_configured = true;
static esp_err_t wifi_start_error;
void vTaskDelay(uint32_t ticks) { usleep(ticks * 1000); }
esp_err_t read_pico_transfer_get_saved_wifi(char ssid[33], bool* configured) {
    assert(pthread_equal(ui,pthread_self()));
    snprintf(ssid,33,"fixture-wifi");*configured=wifi_configured;return ESP_OK;
}
bool read_pico_transfer_claim_sync(void) { assert(pthread_equal(ui,pthread_self())); if(claimed)return false;claimed=true;return true; }
void read_pico_transfer_release_sync(void) { assert(pthread_equal(ui,pthread_self()));claimed=false; }
void read_pico_transfer_get_status(read_pico_transfer_status_t* out) { *out=net; out->network_ready=atomic_load(&link_ready); }
void read_pico_transfer_stop(void) { assert(pthread_equal(ui,pthread_self()));net.state=READ_PICO_TRANSFER_STOPPED;atomic_store(&link_ready,false);++net_stops; }
bool read_pico_transfer_try_stop_if_idle(void) { read_pico_transfer_stop();return true; }
esp_err_t read_pico_transfer_start_saved_network(void) { assert(pthread_equal(ui,pthread_self()));++net_starts;if(wifi_start_error)return wifi_start_error;net.mode=READ_PICO_TRANSFER_MODE_STA;net.state=READ_PICO_TRANSFER_STARTING;atomic_store(&link_ready,false);network_polls=0;return ESP_OK; }
void read_pico_transfer_service_poll(void) { assert(pthread_equal(ui,pthread_self()));if(++network_polls>=3)atomic_store(&link_ready,true); }
static void finish(char* note) {for(int i=0;i<2000;i++){if(os_sync_job_poll(note,128))return;usleep(1000);}assert(!"worker timeout");}
int main(void) {
    ui=pthread_self();char note[128];snprintf(path,sizeof(path),"/tmp/pico-sync-%ld.txt",(long)getpid());FILE* f=fopen(path,"wb");assert(f);for(int i=0;i<100;i++)fputc('a',f);fclose(f);
    response="{\"progress\":\"rp1|100|2|50|52\",\"percentage\":0.5037}";
    test_ble_stopped=false;
    assert(!os_sync_job_start(OS_SYNC_JOB_PULL,note,sizeof(note)) && !net_starts && !claimed);
    test_ble_stopped=true;
    wifi_configured=false;
    assert(!os_sync_job_start(OS_SYNC_JOB_PULL,note,sizeof(note)) && strstr(note,"配置 WiFi") && !net_starts && !claimed);
    wifi_configured=true;wifi_start_error=ESP_ERR_NO_MEM;
    assert(!os_sync_job_start(OS_SYNC_JOB_PULL,note,sizeof(note)) && strstr(note,"内存不足") && net_starts==1 && !claimed);
    wifi_start_error=ESP_OK;
    assert(os_sync_job_start(OS_SYNC_JOB_PULL,note,sizeof(note)));
    assert(os_sync_job_busy() && !os_sync_job_start(OS_SYNC_JOB_PUSH,note,sizeof(note)));
    finish(note);assert(net_starts==2 && net.state==READ_PICO_TRANSFER_STOPPED && claimed);assert(os_sync_pull_pending() && saves==0 && progress.pct==20);
    assert(os_sync_pull_confirm(false,note,sizeof(note)) && saves==0 && !claimed);
    assert(os_sync_job_start(OS_SYNC_JOB_PULL,note,sizeof(note)));finish(note);
    // 远端 rp1 带 px=52、全局桩是 48：应用后必须保留本地 76 档。
    // The remote rp1 carries px=52 and the stubbed global is 48: applying must keep the local tier 76.
    assert(os_sync_pull_confirm(true,note,sizeof(note)) && saves==1 && progress.pct==50 && progress.pct_frac==37 && progress.byte_off==50 && progress.px==76);
    assert(os_sync_job_start(OS_SYNC_JOB_PULL,note,sizeof(note)));finish(note);
    progress.last_open_s++;assert(!os_sync_pull_confirm(true,note,sizeof(note)) && saves==1);
    assert(os_sync_job_start(OS_SYNC_JOB_PULL,note,sizeof(note)));finish(note);
    f=fopen(path,"r+b");fputc('b',f);fclose(f);
    assert(!os_sync_pull_confirm(true,note,sizeof(note)) && saves==1);
    atomic_store(&pause_read,true);assert(os_sync_job_start(OS_SYNC_JOB_AUTH,note,sizeof(note)));
    os_sync_job_cancel();assert(!os_sync_job_busy() && !os_sync_pull_pending() && saves==1);
    atomic_store(&pause_read,false);assert(os_sync_job_start(OS_SYNC_JOB_PUSH,note,sizeof(note)));finish(note);assert(!os_sync_job_busy());
    assert(strstr(note,"服务器已确认上传")&&strstr(request_body,"\"percentage\":0.5037"));
    response="{\"progress\":\"rp1|100|1|40|76\",\"percentage\":0.4}";
    assert(os_sync_job_start(OS_SYNC_JOB_PUSH,note,sizeof(note)));finish(note);
    assert(strstr(note,"服务器未确认进度"));
    assert(!claimed && net.state==READ_PICO_TRANSFER_STOPPED && net_starts >= 6 && net_stops >= net_starts);
    response="{\"progress\":\"/body/p[9]\",\"percentage\":0.73}";
    assert(os_sync_job_start(OS_SYNC_JOB_PULL,note,sizeof(note))); finish(note);
    assert(strstr(note,"近似位置") && os_sync_pull_confirm(true,note,sizeof(note)));
    assert(progress.approximate && progress.pct==73 && progress.chapter==0 && progress.byte_off==0 && progress.px==76);
    unsigned starts_before=net_starts, stops_before=net_stops;
    net.mode=READ_PICO_TRANSFER_MODE_STA;net.state=READ_PICO_TRANSFER_READY;atomic_store(&link_ready,true);
    assert(os_sync_job_start(OS_SYNC_JOB_AUTH,note,sizeof(note)));finish(note);
    assert(net_starts==starts_before && net_stops==stops_before && !claimed && net.state==READ_PICO_TRANSFER_READY);
    net.state=READ_PICO_TRANSFER_STOPPED;atomic_store(&link_ready,false);
    unsigned calls_before=http_calls;
    assert(os_sync_job_start(OS_SYNC_JOB_AUTH,note,sizeof(note)));
    os_sync_job_cancel();
    assert(http_calls==calls_before && !claimed && net.state==READ_PICO_TRANSFER_STOPPED);
    char epub_path[288];snprintf(epub_path,sizeof(epub_path),"/tmp/pico-sync-%ld.epub",(long)getpid());
    assert(rename(path,epub_path)==0);snprintf(path,sizeof(path),"%s",epub_path);
    paragraph_mapping=true;progress.approximate=false;
    response="{\"progress\":\"/body/DocFragment[3]/body/div[1]/section[1]/div[2]/p[19]/text()[2].137\",\"percentage\":0.81}";
    assert(os_sync_job_start(OS_SYNC_JOB_PUSH,note,sizeof(note)));finish(note);
    assert(strstr(request_body,paragraph_path));
    assert(os_sync_job_start(OS_SYNC_JOB_PULL,note,sizeof(note)));finish(note);
    unsigned saves_before=saves;
    assert(strstr(note,"文本位置")&&saves==saves_before);
    assert(os_sync_pull_confirm(true,note,sizeof(note))&&saves==saves_before+1);
    assert(progress.chapter==2&&progress.byte_off==37&&!progress.approximate&&progress.px==76);
    // 首页自动同步只比较 0.01% 精度：远端更远就应用，不更远才上传。
    // Home auto-sync compares hundredths of a percent: apply a farther remote, otherwise push.
    paragraph_mapping=false;
    progress=(book_progress_t){.file_size=100,.chapter=1,.byte_off=42,.px=76,
                               .pct=42,.pct_frac=15,.last_open_s=9,.read_ms=1234,.pages=7};
    response="{\"progress\":\"rp1|100|3|65|48\",\"percentage\":0.6523}";
    unsigned pushes_before=push_calls, pulls_before=pull_calls, paths_before=last_path_calls;
    saves_before=saves;
    assert(os_sync_job_start_book(path,note,sizeof(note)));finish(note);
    assert(pull_calls==pulls_before+1&&push_calls==pushes_before&&last_path_calls==paths_before);
    assert(strstr(note,"已同步远端进度")&&saves==saves_before+1&&last_path_saves>=2);
    assert(progress.pct==65&&progress.pct_frac==23&&progress.chapter==3&&progress.byte_off==65);
    assert(progress.px==76&&progress.read_ms==1234&&progress.pages==7);

    progress.pct=75;progress.pct_frac=13;progress.chapter=4;progress.byte_off=75;
    first_get_response="{\"progress\":\"rp1|100|3|65|48\",\"percentage\":0.6523}";
    response="{\"progress\":\"rp1|100|4|75|76\",\"percentage\":0.7513}";
    pushes_before=push_calls;pulls_before=pull_calls;saves_before=saves;
    assert(os_sync_job_start_book(path,note,sizeof(note)));finish(note);
    assert(pull_calls==pulls_before+2&&push_calls==pushes_before+1&&saves==saves_before);
    assert(strstr(note,"已将本地进度同步到远端")&&strstr(request_body,"\"percentage\":0.7513"));

    // 相等仍推本地；404 才推，解析/网络失败绝不盲推。
    // Equality still pushes local; 404 pushes, while parse/network errors never push blindly.
    pushes_before=push_calls;saves_before=saves;
    assert(os_sync_job_start_book(path,note,sizeof(note)));finish(note);
    assert(push_calls==pushes_before+1&&saves==saves_before);
    first_get_status=404;
    pushes_before=push_calls;
    assert(os_sync_job_start_book(path,note,sizeof(note)));finish(note);
    assert(push_calls==pushes_before+1&&strstr(note,"已将本地进度同步到远端"));
    first_get_response="not-json";
    pushes_before=push_calls;
    assert(os_sync_job_start_book(path,note,sizeof(note)));finish(note);
    assert(push_calls==pushes_before&&strstr(note,"解析失败"));
    first_get_status=503;
    pushes_before=push_calls;
    assert(os_sync_job_start_book(path,note,sizeof(note)));finish(note);
    assert(push_calls==pushes_before&&strstr(note,"服务器错误"));

    // 远端在上传与回读之间变得更远，仍自动应用较大值。
    // If remote advances between push and readback, apply that larger value too.
    first_get_response="{\"progress\":\"rp1|100|4|75|76\",\"percentage\":0.7513}";
    response="{\"progress\":\"rp1|100|5|90|52\",\"percentage\":0.9001}";
    pushes_before=push_calls;saves_before=saves;
    assert(os_sync_job_start_book(path,note,sizeof(note)));finish(note);
    assert(push_calls==pushes_before+1&&saves==saves_before+1);
    assert(progress.pct==90&&progress.pct_frac==1&&progress.px==76);

    // 无本地记录时可接受远端；双方都没有记录时不创建空进度。
    // With no local record, accept remote; with neither record, create no empty progress.
    has_progress=false;
    response="{\"progress\":\"rp1|100|5|95|52\",\"percentage\":0.9501}";
    pushes_before=push_calls;saves_before=saves;
    assert(os_sync_job_start_book(path,note,sizeof(note)));finish(note);
    assert(push_calls==pushes_before&&saves==saves_before+1&&progress.pct==95&&progress.px==48);
    has_progress=false;
    response="{\"progress\":\"rp1|100|0|0|52\",\"percentage\":0.0}";
    saves_before=saves;
    assert(os_sync_job_start_book(path,note,sizeof(note)));finish(note);
    assert(saves==saves_before+1&&progress.pct==0&&progress.pct_frac==0);
    has_progress=false;first_get_status=404;
    pushes_before=push_calls;saves_before=saves;
    assert(os_sync_job_start_book(path,note,sizeof(note)));finish(note);
    assert(push_calls==pushes_before&&saves==saves_before&&strstr(note,"本地也没有阅读记录"));
    has_progress=true;

    // 同步期间若本地记录已变化，网络结果不能覆盖较新的本地位置。
    // If local progress changes mid-sync, the network result must not overwrite it.
    response="{\"progress\":\"rp1|100|6|99|52\",\"percentage\":0.9901}";
    saves_before=saves;
    assert(os_sync_job_start_book(path,note,sizeof(note)));
    ++progress.last_open_s;
    finish(note);
    assert(saves==saves_before&&strstr(note,"本地进度已变化"));
    assert(task_creates==1&&worker_stack_caps==(MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT));
    unlink(path);puts("os_sync_device: persistent PSRAM worker, verified push, pull-first home merge, precise percentage, UI-only NVS, staged pulls and cancel passed");
}
