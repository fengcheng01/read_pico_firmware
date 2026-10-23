#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""真实睡眠外围/CPU/PMU路径的故障注入。/ Fault-inject actual peripheral, CPU and PMU sleep paths."""
from pathlib import Path
import os
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
source = (ROOT / "main/sleep.c").read_text()


def function(name):
    match = re.search(r"^(?:static )?[^\n]+\b" + name + r"\([^;]*?\) \{", source, re.M)
    assert match, name
    at, depth = match.end(), 1
    while depth:
        depth += (source[at] == "{") - (source[at] == "}")
        at += 1
    return source[match.start():at]


unit = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <setjmp.h>
#define CONFIG_PM_ESP_SLEEP_POWER_DOWN_CPU 1
#define ESP_OK 0
#define ESP_FAIL 1
#define ESP_ERR_INVALID_ARG 2
#define ESP_ERR_INVALID_STATE 3
#define ESP_ERR_TIMEOUT 4
#define CST836U_MODE_DEEPSLEEP 1
#define TAG "test"
#define ESP_LOGI(...) ((void)0)
#define ESP_LOGW(...) ((void)0)
#define pdMS_TO_TICKS(ms) (ms)
typedef int esp_err_t;
typedef int cst836u_handle_t;
typedef int sc7a20h_handle_t;
typedef enum {APP_SLEEP_LIGHT,APP_SLEEP_DEEP,APP_SLEEP_OFF} app_sleep_mode_t;
static cst836u_handle_t s_sleep_tp;
static sc7a20h_handle_t s_sleep_acc;
static bool s_touch_resume,s_acc_resume,s_peripherals_parked;
static bool acc_on,buzzer_on,parked,host_parked,rails,pause_ok;
static bool cpu_fail,cut_en;
static int touch_mode,fail_event,pmu_result,pmu_calls,ready_calls,cpu_bytes,cpu_calls;
static int elapsed,events[128],count;
static jmp_buf power_cut;
enum {E_BUZZER=1,E_TOUCH_DOWN,E_ACC_DOWN,E_PARK,E_RESTORE,E_TOUCH_UP,E_ACC_UP,E_OFF,E_JOIN,E_CANCEL,E_PMU,E_READY};
static int event(int value){assert(count<128);events[count++]=value;if(fail_event==value){fail_event=0;return ESP_FAIL;}return ESP_OK;}
static const char* esp_err_to_name(int err){(void)err;return "mock";}
static int read_pico_ioe_sleep(bool off){assert(!rails);int err=event(E_PARK);if(!err){parked=true;host_parked=off;}return err;}
static int read_pico_ioe_wake(void){int err=event(E_RESTORE);if(!err)parked=host_parked=false;return err;}
static int cst836u_get_mode(int tp){assert(tp==1);return touch_mode;}
static int cst836u_set_mode(int tp,int mode){assert(tp==1&&mode==1);touch_mode=mode;return event(E_TOUCH_DOWN);}
static int cst836u_wake(int tp){assert(tp==1&&!parked);int err=event(E_TOUCH_UP);if(!err)touch_mode=0;return err;}
static bool sc7a20h_powered(int acc){assert(acc==2);return acc_on;}
static int sc7a20h_power_down(int acc){assert(acc==2);acc_on=false;return event(E_ACC_DOWN);}
static int sc7a20h_power_up(int acc){assert(acc==2);int err=event(E_ACC_UP);if(!err)acc_on=true;return err;}
static bool read_pico_buzzer_score_busy(void){return buzzer_on;}
static int read_pico_buzzer_score_stop(void){int err=event(E_BUZZER);if(!err)buzzer_on=false;return err;}
static int sleep_cpu_configure(bool enabled){cpu_calls++;cpu_bytes=enabled?4096:0;return enabled&&cpu_fail?ESP_FAIL:ESP_OK;}
static bool os_page_turner_pause(void){return pause_ok;}
static void book_cover_join(void){event(E_JOIN);}
static void os_sync_job_cancel(void){event(E_CANCEL);}
static void epd_poweroff(void){event(E_OFF);rails=false;}
static void pmu_selftest_prepare_powerdown(void){assert(!rails);}
static int read_pico_pmu_report_sleep(void){assert(parked&&host_parked&&!acc_on&&touch_mode==1);event(E_PMU);pmu_calls++;return pmu_result;}
static int read_pico_pmu_power_off(void){return read_pico_pmu_report_sleep();}
static int read_pico_pmu_report_ready(void){assert(!parked);event(E_READY);ready_calls++;return ESP_OK;}
static void vTaskDelay(int ms){assert(ms==200||ms==1500);elapsed+=ms;if(ms==1500&&cut_en)longjmp(power_cut,1);}
'''
unit += "\n".join(function(name) for name in (
    "app_sleep_bind", "sleep_peripherals_resume", "sleep_peripherals_suspend",
    "sleep_cpu_prepare", "sleep_cpu_release", "app_enter_host_sleep"))
unit += r'''
static void reset(void){
 assert(cpu_bytes==0);count=elapsed=pmu_calls=ready_calls=cpu_calls=0;touch_mode=0;
 acc_on=buzzer_on=rails=pause_ok=true;parked=host_parked=cpu_fail=cut_en=false;
 fail_event=0;pmu_result=ESP_FAIL;s_touch_resume=s_acc_resume=s_peripherals_parked=false;
 app_sleep_bind(1,2);
}
int main(void){
 for(int acc=0;acc<2;acc++)for(int touch=0;touch<2;touch++){
  reset();rails=false;acc_on=acc;touch_mode=touch;
  assert(sleep_peripherals_suspend(false)==ESP_OK);
  assert(parked&&!host_parked&&!acc_on&&touch_mode==1&&!buzzer_on);
  assert(sleep_peripherals_resume()==ESP_OK&&!parked&&acc_on==acc&&touch_mode==touch);
 }
 for(int failure=E_BUZZER;failure<=E_PARK;failure++){
  reset();rails=false;fail_event=failure;
  assert(sleep_peripherals_suspend(true)==ESP_FAIL);
  assert(!parked&&acc_on&&touch_mode==0&&!s_peripherals_parked);
 }
 reset();rails=false;assert(!sleep_peripherals_suspend(false));fail_event=E_RESTORE;
 assert(sleep_peripherals_resume()==ESP_FAIL&&parked&&s_touch_resume&&s_acc_resume);
 assert(!sleep_peripherals_resume()&&!parked&&touch_mode==0&&acc_on);
 for(int i=0;i<200;i++){
  reset();rails=false;assert(!sleep_peripherals_suspend(false));
  sleep_cpu_prepare();assert(cpu_bytes==4096);sleep_cpu_release();assert(!cpu_bytes);
  assert(!sleep_peripherals_resume());
 }
 reset();cpu_fail=true;sleep_cpu_prepare();assert(!cpu_bytes&&cpu_calls==2);sleep_cpu_release();assert(!cpu_bytes);
 reset();assert(app_enter_host_sleep(APP_SLEEP_LIGHT)==ESP_ERR_INVALID_ARG&&!count);
 reset();pause_ok=false;assert(app_enter_host_sleep(APP_SLEEP_DEEP)==ESP_ERR_INVALID_STATE&&!count);
 reset();fail_event=E_TOUCH_DOWN;assert(app_enter_host_sleep(APP_SLEEP_DEEP)==ESP_FAIL&&!pmu_calls&&!parked&&touch_mode==0);
 reset();assert(app_enter_host_sleep(APP_SLEEP_DEEP)==ESP_FAIL);
 assert(pmu_calls==3&&elapsed==400&&ready_calls==1&&!parked&&acc_on&&touch_mode==0);
 reset();assert(app_enter_host_sleep(APP_SLEEP_OFF)==ESP_FAIL&&pmu_calls==1&&elapsed==0&&ready_calls==1&&!parked);
 reset();pmu_result=ESP_OK;assert(app_enter_host_sleep(APP_SLEEP_DEEP)==ESP_ERR_TIMEOUT);
 assert(pmu_calls==1&&elapsed==1500&&ready_calls==1&&!parked&&acc_on&&touch_mode==0);
 reset();pmu_result=ESP_OK;cut_en=true;
 if(!setjmp(power_cut)){app_enter_host_sleep(APP_SLEEP_DEEP);assert(false);}
 assert(parked&&host_parked&&!ready_calls&&touch_mode==1&&!acc_on);
 puts("sleep power: peripheral state/rollback, transient wake faults,200 retention release cycles,OOM,PMU errors and ACK-without-power-loss PASS");
}
'''
with tempfile.TemporaryDirectory(prefix="rp-sleep-power-", dir="/tmp") as folder:
    c, binary = Path(folder) / "test.c", Path(folder) / "test"
    c.write_text(unit)
    subprocess.run([os.environ.get("CC", "cc"), "-std=c11", "-Wall", "-Wextra", "-Werror",
                    "-Wno-unused-function", "-g", "-fsanitize=address,undefined", str(c), "-o", str(binary)], check=True)
    subprocess.run([str(binary)], check=True)
