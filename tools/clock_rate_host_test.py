#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""真实网络校时桥与睡眠比例回归，替换网络/PMU/NVS边界。/ Real network time bridge and sleep-rate regressions with network/PMU/NVS boundaries replaced."""
from pathlib import Path
import os
import subprocess
import tempfile
ROOT = Path(__file__).resolve().parents[1]
source = (ROOT/'main/os/os_time_pico.c').read_text()
production = source[source.index('static const char* TAG'):]
harness = r'''
#include "os_time.h"
#include "os_clock_rate.h"
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <sys/time.h>
#define ESP_OK 0
#define ESP_LOGI(...) ((void)TAG)
#define ESP_LOGW(...) ((void)TAG)
#define IP_EVENT_STA_GOT_IP 1
#define PMU_CMD_TIME_SYNC 2
#define pdMS_TO_TICKS(x) (x)
typedef struct { bool start,wait_for_sync; int ip_event_to_renew,num_of_servers; const char* servers[2]; } esp_sntp_config_t;
typedef struct { bool time_ok,status_ok; uint32_t unix_sec; int soc_permille; } pmu_snapshot_t;
static pmu_snapshot_t snapshot;
static int64_t tick_us,network_us;
static int32_t saved_ppm;
static int saved_count;
static bool ignored_write, ntp_event;
static int8_t app_settings_tz_qh(void) {return 32;}
static void app_settings_set_tz_qh(int8_t qh) {(void)qh;}
static int32_t app_settings_sleep_clock_ppm(void) {return saved_ppm;}
static bool app_settings_sleep_clock_valid(void) {return saved_count>0;}
static void app_settings_set_sleep_clock_ppm(int32_t ppm) {if(!saved_count||saved_ppm!=ppm){saved_ppm=ppm;++saved_count;}}
static int64_t esp_timer_get_time(void) {return tick_us;}
static bool read_pico_pmu_ready(void) {return true;}
static int read_pico_pmu_refresh(void) {return ESP_OK;}
static const pmu_snapshot_t* read_pico_pmu_get(void) {return &snapshot;}
static int read_pico_pmu_cmd(int cmd,const uint8_t* p,size_t n) {
 assert(cmd==PMU_CMD_TIME_SYNC && n==4);
 if(!ignored_write){snapshot.time_ok=true;snapshot.unix_sec=p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
 return ESP_OK;
}
static int esp_netif_sntp_init(const esp_sntp_config_t* cfg) {assert(cfg->num_of_servers==2);ntp_event=true;return ESP_OK;}
static void esp_netif_sntp_deinit(void) {}
static int esp_netif_sntp_sync_wait(int wait) {assert(wait==0);bool received=ntp_event;ntp_event=false;return received?ESP_OK:1;}
static time_t test_time(time_t* out) {time_t t=network_us/1000000;if(out)*out=t;return t;}
static int test_gettimeofday(struct timeval* tv,void* zone) {(void)zone;tv->tv_sec=network_us/1000000;tv->tv_usec=network_us%1000000;return 0;}
#define time test_time
#define gettimeofday test_gettimeofday
''' + production + r'''
static void sync(void) {os_time_network(false);os_time_network(true);assert(os_time_recently_synced());}
static void sleep_minutes(int n,int64_t raw_us,int64_t real_us) {
 for(int i=0;i<n;++i){os_time_record_sleep(raw_us);tick_us+=raw_us;network_us+=real_us;os_time_poll(tick_us/1000);}
}
int main(void) {
 const uint32_t base=1791079200;
 network_us=(int64_t)base*1000000+250000;sync();assert(!saved_count);
 sleep_minutes(240,59750000,60000000);
 assert(os_time_info()->unix_utc==base+14340);
 sync();assert(saved_count==1 && saved_ppm==4184);
 sleep_minutes(240,59750000,60000000);
 assert(os_time_info()->unix_utc==base+28800);
 int64_t correction=s_rate.correction_scaled;
 tick_us+=3600000000LL;network_us+=3600000000LL;os_time_poll(tick_us/1000);
 assert(os_time_info()->unix_utc==base+32400 && s_rate.correction_scaled==correction);
 sync();assert(saved_count==1);
 // 失败回读不得学习或标记成功。/ Failed readback must neither learn nor mark success.
 os_time_network(false);ignored_write=true;network_us+=100000000;
 os_time_network(true);assert(!os_time_recently_synced() && saved_count==1);
 ignored_write=false;os_time_network(true);assert(os_time_recently_synced() && saved_count==1);
 os_clock_rate_t c={0};os_clock_rate_sync(&c,0,0);
 os_clock_rate_sleep(&c,14460000000LL);
 assert(os_clock_rate_sync(&c,14400000,14460000) && c.ppm==-4149);
 c=(os_clock_rate_t){0};os_clock_rate_sync(&c,0,0);
 assert(!os_clock_rate_sync(&c,14400000,14340000));
 os_clock_rate_sleep(&c,60000000);
 assert(!os_clock_rate_sync(&c,14460000,14400000));
 c=(os_clock_rate_t){0};os_clock_rate_sync(&c,0,0);os_clock_rate_sleep(&c,14400000000LL);
 assert(!os_clock_rate_sync(&c,15400000,14400000));
 puts("clock rate: actual SNTP bridge learns a 4h/60s slow sleep, preserves awake timing, learns fast rates and rejects outlier/short/failed samples PASS");
}
'''
with tempfile.TemporaryDirectory(prefix='pico-clock-rate-',dir='/tmp') as folder:
    d=Path(folder);(d/'test.c').write_text(harness)
    flags=['-std=gnu11','-Wall','-Wextra','-Werror','-fsanitize=address,undefined']
    if os.uname().sysname=='Darwin': flags+=['-isysroot','/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk']
    subprocess.run([os.environ.get('CC','cc'),*flags,'-Imain/os',str(d/'test.c'),'main/os/os_time.c','-o',str(d/'test')],cwd=ROOT,check=True)
    subprocess.run([str(d/'test')],check=True)
