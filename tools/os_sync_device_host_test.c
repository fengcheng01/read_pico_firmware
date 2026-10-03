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
#include "esp_rom_md5.h"
#include "freertos/semphr.h"
#include <assert.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
static pthread_t ui, worker;
static char path[288];
static book_progress_t progress={.file_size=100,.chapter=1,.byte_off=20,.px=48,.pct=20,.last_open_s=1};
static unsigned saves;
static atomic_bool pause_read, link_ready;
static unsigned network_polls, http_calls;
static const char* response;
static size_t received;
struct test_sem {pthread_mutex_t lock;pthread_cond_t changed;bool ready;};
SemaphoreHandle_t xSemaphoreCreateBinary(void) {SemaphoreHandle_t s=calloc(1,sizeof(*s));pthread_mutex_init(&s->lock,NULL);pthread_cond_init(&s->changed,NULL);return s;}
int xSemaphoreGive(SemaphoreHandle_t s) {pthread_mutex_lock(&s->lock);s->ready=true;pthread_cond_signal(&s->changed);pthread_mutex_unlock(&s->lock);return 1;}
int xSemaphoreTake(SemaphoreHandle_t s,uint32_t timeout) {pthread_mutex_lock(&s->lock);while(!s->ready && timeout)pthread_cond_wait(&s->changed,&s->lock);bool ok=s->ready;if(ok)s->ready=false;pthread_mutex_unlock(&s->lock);return ok;}
static void* run_task(void* arg) {((void(*)(void*))arg)(NULL);return NULL;}
int xTaskCreate(void(*fn)(void*),const char* name,unsigned size,void* arg,int priority,void* handle) {(void)name;(void)size;(void)arg;(void)priority;(void)handle;return !pthread_create(&worker,NULL,run_task,(void*)fn);}
void vTaskDelete(void* arg) {(void)arg;pthread_exit(NULL);}
int64_t esp_timer_get_time(void) {struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return (int64_t)t.tv_sec*1000000+t.tv_nsec/1000;}
void os_time_force_poll(void) {}
void os_time_poll(int64_t now) {(void)now;}
void os_time_network(bool online) {(void)online;}
int esp_http_client_get_errno(esp_http_client_handle_t c) {(void)c;return 0;}
esp_err_t esp_http_client_get_and_clear_last_tls_error(esp_http_client_handle_t c,int* tls,int* flags) {(void)c;*tls=*flags=0;return ESP_OK;}
void esp_crt_bundle_attach(void) {}
esp_http_client_handle_t esp_http_client_init(const esp_http_client_config_t* cfg) {assert(!pthread_equal(ui,pthread_self()));assert(cfg->timeout_ms==15000 && atomic_load(&link_ready));++http_calls;received=0;return (void*)1;}
esp_err_t esp_http_client_set_header(esp_http_client_handle_t c,const char* a,const char* b) {(void)c;(void)a;(void)b;return ESP_OK;}
esp_err_t esp_http_client_open(esp_http_client_handle_t c,int size) {(void)c;(void)size;return ESP_OK;}
int esp_http_client_write(esp_http_client_handle_t c,const char* text,int size) {(void)c;(void)text;return size>3?3:size;}
int esp_http_client_read(esp_http_client_handle_t c,char* text,int cap) {(void)c;if(atomic_load(&pause_read)){usleep(2000);return -1;}size_t n=strlen(response)-received;if(n>5)n=5;if(n>(size_t)cap)n=cap;memcpy(text,response+received,n);received+=n;return (int)n;}
int esp_http_client_fetch_headers(esp_http_client_handle_t c) {(void)c;return 0;}
int esp_http_client_get_status_code(esp_http_client_handle_t c) {(void)c;return 200;}
bool esp_http_client_is_complete_data_received(esp_http_client_handle_t c) {(void)c;return received==strlen(response);}
esp_err_t esp_http_client_cleanup(esp_http_client_handle_t c) {(void)c;return ESP_OK;}
void esp_rom_md5_init(md5_context_t* ctx) {ctx->sum=0;}
void esp_rom_md5_update(md5_context_t* ctx,const void* in,uint32_t n) {const unsigned char* p=in;for(uint32_t i=0;i<n;i++)ctx->sum+=p[i];}
void esp_rom_md5_final(unsigned char out[16],md5_context_t* ctx) {memset(out,ctx->sum&255,16);}
const char* app_settings_sync_url(void) {assert(pthread_equal(ui,pthread_self()));return "https://example.test";}
const char* app_settings_sync_user(void) {return "reader";}
const char* app_settings_sync_key(void) {return "0123456789abcdef0123456789abcdef";}
void app_settings_set_sync_key(const char* key) {(void)key;}
int app_settings_book_px(void) {return 48;}
bool book_progress_last_path(char* out,size_t cap) {assert(pthread_equal(ui,pthread_self()));snprintf(out,cap,"%s",path);return true;}
bool book_progress_load(const char* name,uint32_t size,book_progress_t* out) {(void)name;assert(pthread_equal(ui,pthread_self()));assert(size==100);*out=progress;return true;}
esp_err_t book_progress_save(const char* name,const book_progress_t* p) {(void)name;assert(pthread_equal(ui,pthread_self()));progress=*p;++saves;return ESP_OK;}
esp_err_t book_progress_set_last_path(const char* name) {(void)name;assert(pthread_equal(ui,pthread_self()));return ESP_OK;}
static bool claimed;
static read_pico_transfer_status_t net;
static unsigned net_starts, net_stops;
void vTaskDelay(uint32_t ticks) { usleep(ticks * 1000); }
bool read_pico_transfer_claim_sync(void) { assert(pthread_equal(ui,pthread_self())); if(claimed)return false;claimed=true;return true; }
void read_pico_transfer_release_sync(void) { assert(pthread_equal(ui,pthread_self()));claimed=false; }
void read_pico_transfer_get_status(read_pico_transfer_status_t* out) { *out=net; out->network_ready=atomic_load(&link_ready); }
void read_pico_transfer_stop(void) { assert(pthread_equal(ui,pthread_self()));net.state=READ_PICO_TRANSFER_STOPPED;atomic_store(&link_ready,false);++net_stops; }
bool read_pico_transfer_try_stop_if_idle(void) { read_pico_transfer_stop();return true; }
esp_err_t read_pico_transfer_start_saved_network(void) { assert(pthread_equal(ui,pthread_self()));net.mode=READ_PICO_TRANSFER_MODE_STA;net.state=READ_PICO_TRANSFER_STARTING;atomic_store(&link_ready,false);network_polls=0;++net_starts;return ESP_OK; }
void read_pico_transfer_service_poll(void) { assert(pthread_equal(ui,pthread_self()));if(++network_polls>=3)atomic_store(&link_ready,true); }
static void finish(char* note) {for(int i=0;i<2000;i++){if(os_sync_job_poll(note,128)){pthread_join(worker,NULL);return;}usleep(1000);}assert(!"worker timeout");}
int main(void) {
    ui=pthread_self();char note[128];snprintf(path,sizeof(path),"/tmp/pico-sync-%ld.txt",(long)getpid());FILE* f=fopen(path,"wb");assert(f);for(int i=0;i<100;i++)fputc('a',f);fclose(f);
    response="{\"progress\":\"rp1|100|2|50|52\",\"percentage\":0.5}";
    assert(os_sync_job_start(OS_SYNC_JOB_PULL,note,sizeof(note)));
    assert(os_sync_job_busy() && !os_sync_job_start(OS_SYNC_JOB_PUSH,note,sizeof(note)));
    finish(note);assert(net_starts==1 && net.state==READ_PICO_TRANSFER_STOPPED && claimed);assert(os_sync_pull_pending() && saves==0 && progress.pct==20);
    assert(os_sync_pull_confirm(false,note,sizeof(note)) && saves==0 && !claimed);
    assert(os_sync_job_start(OS_SYNC_JOB_PULL,note,sizeof(note)));finish(note);
    assert(os_sync_pull_confirm(true,note,sizeof(note)) && saves==1 && progress.pct==50 && progress.byte_off==50);
    assert(os_sync_job_start(OS_SYNC_JOB_PULL,note,sizeof(note)));finish(note);
    progress.last_open_s++;assert(!os_sync_pull_confirm(true,note,sizeof(note)) && saves==1);
    assert(os_sync_job_start(OS_SYNC_JOB_PULL,note,sizeof(note)));finish(note);
    f=fopen(path,"r+b");fputc('b',f);fclose(f);
    assert(!os_sync_pull_confirm(true,note,sizeof(note)) && saves==1);
    atomic_store(&pause_read,true);assert(os_sync_job_start(OS_SYNC_JOB_AUTH,note,sizeof(note)));
    os_sync_job_cancel();pthread_join(worker,NULL);assert(!os_sync_job_busy() && !os_sync_pull_pending() && saves==1);
    atomic_store(&pause_read,false);assert(os_sync_job_start(OS_SYNC_JOB_PUSH,note,sizeof(note)));finish(note);assert(!os_sync_job_busy());
    assert(!claimed && net.state==READ_PICO_TRANSFER_STOPPED && net_starts >= 6 && net_stops >= net_starts);
    response="{\"progress\":\"/body/p[9]\",\"percentage\":0.73}";
    assert(os_sync_job_start(OS_SYNC_JOB_PULL,note,sizeof(note))); finish(note);
    assert(strstr(note,"近似位置") && os_sync_pull_confirm(true,note,sizeof(note)));
    assert(progress.approximate && progress.pct==73 && progress.chapter==0 && progress.byte_off==0);
    unsigned starts_before=net_starts, stops_before=net_stops;
    net.mode=READ_PICO_TRANSFER_MODE_STA;net.state=READ_PICO_TRANSFER_READY;atomic_store(&link_ready,true);
    assert(os_sync_job_start(OS_SYNC_JOB_AUTH,note,sizeof(note)));finish(note);
    assert(net_starts==starts_before && net_stops==stops_before && !claimed && net.state==READ_PICO_TRANSFER_READY);
    net.state=READ_PICO_TRANSFER_STOPPED;atomic_store(&link_ready,false);
    unsigned calls_before=http_calls;
    assert(os_sync_job_start(OS_SYNC_JOB_AUTH,note,sizeof(note)));
    os_sync_job_cancel();pthread_join(worker,NULL);
    assert(http_calls==calls_before && !claimed && net.state==READ_PICO_TRANSFER_STOPPED);
    unlink(path);puts("os_sync_device: worker-only HTTP, UI-only NVS, staged pulls, cancel, reuse and conflicts passed");
}
