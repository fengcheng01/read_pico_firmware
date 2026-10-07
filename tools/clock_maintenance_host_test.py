#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 mindreset
# SPDX-License-Identifier: Apache-2.0
"""真实锁屏网络维护与时间桥回归。/ Production lock-network maintenance and time-bridge regressions."""
from pathlib import Path
import os
import subprocess
import tempfile
ROOT=Path(__file__).resolve().parents[1]
original=ROOT/'tools/clock_rate_host_test.py'
scope={'__file__':str(original)}
exec(original.read_text().split('with tempfile.TemporaryDirectory')[0],scope)
harness=scope['harness'].split('static void sleep_minutes')[0]
harness=harness.replace('static int esp_netif_sntp_sync_wait(int wait) {assert(wait==0);bool received=ntp_event;ntp_event=false;return received?ESP_OK:1;}',
 'static bool ntp_available=true; static int esp_netif_sntp_sync_wait(int wait) {assert(wait==0);bool received=ntp_event&&ntp_available;if(received)ntp_event=false;return received?ESP_OK:1;}')
harness+=r'''
#include "read_pico_transfer.h"
static bool auto_on=true,configured=true,claimed,claim_allowed=true;
static unsigned starts,stops,polls;
static read_pico_transfer_status_t status;
static bool app_settings_clock_auto(void) {return auto_on;}
void read_pico_transfer_get_status(read_pico_transfer_status_t* out) {*out=status;}
esp_err_t read_pico_transfer_get_saved_wifi(char ssid[33],bool* out) {ssid[0]=0;*out=configured;return ESP_OK;}
bool read_pico_transfer_claim_sync(void) {assert(!claimed);if(!claim_allowed)return false;claimed=true;return true;}
void read_pico_transfer_release_sync(void) {assert(claimed);claimed=false;}
esp_err_t read_pico_transfer_start_saved_network(void) {starts++;polls=0;status.mode=READ_PICO_TRANSFER_MODE_STA;status.state=READ_PICO_TRANSFER_STARTING;status.network_ready=false;return ESP_OK;}
void read_pico_transfer_stop(void) {assert(claimed);stops++;status=(read_pico_transfer_status_t){0};}
void read_pico_transfer_service_poll(void) {if(++polls>=2){status.state=READ_PICO_TRANSFER_READY;status.network_ready=true;}}
''' + 'static bool s_active'+(ROOT/'main/os/os_time_maintenance.c').read_text().split('static bool s_active')[1]
harness+=r'''
static void lock_minute(void) {
 os_time_record_sleep(59583333);tick_us+=59583333;network_us+=60000000;os_time_poll(tick_us/1000);
 for(int i=0;os_time_lock_sync_tick(tick_us/1000);++i) {
  assert(i<1000);tick_us+=50000;network_us+=50000;
 }
}
int main(void) {
 network_us=1791079200250000LL;sync();
 for(int i=0;i<26*60;++i)lock_minute();
 assert(saved_ppm==6993&&saved_count==1&&starts>=4&&starts==stops&&!claimed);
 os_time_poll(tick_us/1000);
 int64_t error=(int64_t)os_time_info()->unix_utc-network_us/1000000;
 assert(error>=-1&&error<=1);
 printf("26h real bridge + automatic lock sessions: learned %+d ppm, residual %llds, %u bounded sessions PASS\n",saved_ppm,(long long)error,starts);
 unsigned before=starts;
 auto_on=false;tick_us+=21600000000LL;network_us+=21600000000LL;
 assert(!os_time_lock_sync_tick(tick_us/1000)&&starts==before);
 auto_on=true;configured=false;assert(!os_time_lock_sync_tick(tick_us/1000)&&starts==before);
 configured=true;assert(!os_time_lock_sync_tick(tick_us/1000)&&starts==before);
 tick_us+=3600000000LL;network_us+=3600000000LL;
 ntp_available=false;assert(os_time_lock_sync_tick(tick_us/1000)&&claimed);
 tick_us+=30000000;network_us+=30000000;assert(!os_time_lock_sync_tick(tick_us/1000)&&!claimed&&starts==stops);
 assert(!os_time_lock_sync_tick(tick_us/1000));
 tick_us+=3600000000LL;network_us+=3600000000LL;
 assert(os_time_lock_sync_tick(tick_us/1000));os_time_lock_sync_cancel();assert(!claimed&&starts==stops);
 int saved_before=saved_count;
 // 短校时保留学习起点。/ Frequent short syncs retain the learning anchor.
 os_clock_rate_t c={0};os_clock_rate_sync(&c,0,0);
 for(int i=1;i<=6;++i){os_clock_rate_sleep(&c,600000000);bool ok=os_clock_rate_sync(&c,i*604000,i*600000);assert(ok==(i==6));}
 assert(c.ppm==6666&&saved_count==saved_before);
 puts("disabled/missing WiFi/backoff/timeout/cancel and frequent-anchor learning PASS");
}
'''
with tempfile.TemporaryDirectory(prefix='pico-clock-maint-',dir='/tmp') as folder:
 d=Path(folder);(d/'test.c').write_text(harness)
 flags=['-std=gnu11','-Wall','-Wextra','-Werror','-fsanitize=address,undefined']
 if os.uname().sysname=='Darwin':flags+=['-isysroot','/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk']
 subprocess.run([os.environ.get('CC','cc'),*flags,'-Imain/os','-Itools/book_storage_stubs','-Icomponents/read_pico_transfer/include',str(d/'test.c'),'main/os/os_time.c','-o',str(d/'test')],cwd=ROOT,check=True)
 subprocess.run([str(d/'test')],check=True)
