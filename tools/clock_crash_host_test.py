#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""真实崩溃/时间桥联测，替换硬件、网络与存储路径。/ Real crash/time bridges with hardware, network and storage-path boundaries replaced."""
from pathlib import Path
import argparse
import os
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]

# 复用真实时间桥的边界夹具，不复用其测试主函数。/ Reuse the real time bridge's boundary fixture, not its test main.
rate_test = ROOT / "tools/clock_rate_host_test.py"
scope = {"__file__": str(rate_test)}
exec(rate_test.read_text().split("with tempfile.TemporaryDirectory")[0], scope)
harness = scope["harness"].split("static void sleep_minutes")[0]

# 两个设备桥放入同一测试单元；只改私有 TAG 名消除重名。/ Put both bridges in one test unit; only rename the private TAG to avoid a collision.
crash_source = (ROOT / "main/os/os_crash_pico.c").read_text()
crash_body = crash_source[crash_source.index("static const char* TAG"):]
crash_body = re.sub(r"\bTAG\b", "CRASH_TAG", crash_body)
harness += r'''
#include "os_crash.h"
#include <errno.h>
#include <string.h>
#include <unistd.h>
#undef ESP_LOGI
#undef ESP_LOGW
#define ESP_LOGI(tag,...) ((void)(tag))
#define ESP_LOGW(tag,...) ((void)(tag))
typedef struct { int unused; } book_store_root_t;
static int reset_reason, mount_result, mount_calls, file_calls;
static bool fail_log_write;
static const char* host_log_path;
static int esp_reset_reason(void) { return reset_reason; }
static int book_store_read_roots(book_store_root_t* roots, int* count) {
 (void)roots; ++mount_calls; *count=0; return mount_result;
}
static FILE* log_fopen(const char* path, const char* mode) {
 assert(strcmp(path,OS_CRASH_LOG_PATH)==0); ++file_calls;
 if(fail_log_write && strcmp(mode,"wb")==0) { errno=EIO; return NULL; }
 return fopen(host_log_path,mode);
}
#define fopen log_fopen
''' + crash_body + r'''
#undef fopen

static void reset_boot(void) {
 os_time_network(false);
 s_last_poll_ms=-OS_TIME_POLL_MS; s_recently_synced=false;
 s_anchored=false; s_force_reanchor=false; s_anchor_sec=0; s_anchor_ms=0;
 s_rate=(os_clock_rate_t){0}; s_rate_loaded=false; s_rate_learned=false; s_last_sync_ms=-1;
 tick_us=0; network_us=0; saved_ppm=0; saved_count=0; ignored_write=false; ntp_event=false;
 snapshot=(pmu_snapshot_t){0}; os_time_apply(0,32);
 s_pending=false; s_written=false; s_boot_abnormal=false;
 reset_reason=3; mount_result=ESP_OK; mount_calls=0; file_calls=0; fail_log_write=false;
 assert(unlink(host_log_path)==0 || errno==ENOENT);
}
static void advance_awake(int seconds) {
 tick_us+=(int64_t)seconds*1000000; network_us+=(int64_t)seconds*1000000;
 os_time_poll(tick_us/1000);
}
static void advance_sleep(int minutes) {
 for(int i=0;i<minutes;++i) {
  os_time_record_sleep(59750000); tick_us+=59750000; network_us+=60000000;
  os_time_poll(tick_us/1000);
 }
}
static void assert_record(uint32_t utc, int reason) {
 char text[128]={0},expected[32]; FILE* file=fopen(host_log_path,"rb"); assert(file);
 size_t len=fread(text,1,sizeof(text)-1,file); assert(fclose(file)==0);
 size_t expected_len=os_crash_format_record(expected,sizeof(expected),utc,reason);
 assert(len==expected_len && memcmp(text,expected,len)==0);
}
static void nonzero_boot_tick(void) {
 const uint32_t base=1791400000;
 for(int reason=3;reason<=7;++reason) {
  reset_boot(); reset_reason=reason; tick_us=120000000;
  network_us=(int64_t)base*1000000; snapshot.time_ok=true; snapshot.unix_sec=base;
  os_crash_boot_check(); assert(os_crash_boot_abnormal() && s_pending && !s_written);
  os_crash_flush(); assert(!s_pending && s_written && s_anchored);
  os_time_poll(tick_us/1000);
  assert(os_time_info()->state==OS_TIME_VALID && os_time_info()->unix_utc==base);
  assert(s_anchor_ms==120000 && s_anchor_sec==base);
  assert_record(base,reason);
  advance_awake(61); assert(os_time_info()->unix_utc==base+61);
  int mounts=mount_calls,files=file_calls; os_crash_flush();
  assert(mount_calls==mounts && file_calls==files);
 }
 puts("crash clock: all five abnormal reasons at boot tick 120s anchor current PMU without adding boot uptime PASS");
}
static void assert_trusted(uint32_t anchor_sec, int64_t anchor_ms, os_clock_rate_t rate, uint32_t utc) {
 assert(os_time_info()->unix_utc==utc && s_anchored && os_time_recently_synced());
 assert(s_anchor_sec==anchor_sec && s_anchor_ms==anchor_ms && !s_force_reanchor);
 assert(s_rate.sampled && s_rate.sample_utc_ms==rate.sample_utc_ms);
 assert(s_rate.sample_tick_ms==rate.sample_tick_ms && s_rate.sample_sleep_us==rate.sample_sleep_us);
 assert(s_rate.sleep_us==rate.sleep_us && s_rate.correction_scaled==rate.correction_scaled);
 assert(s_rate.ppm==rate.ppm && s_last_sync_ms==rate.sample_tick_ms);
}
static void deferred_log_retry_preserves_learning(void) {
 reset_boot(); network_us=1791400000250000LL; sync(); os_crash_boot_check();
 advance_sleep(30); advance_awake(600);
 const uint32_t anchor_sec=s_anchor_sec; const int64_t anchor_ms=s_anchor_ms;
 snapshot.unix_sec=(uint32_t)(network_us/1000000+600);
 fail_log_write=true;
 for(int attempt=0;attempt<2;++attempt) {
  os_clock_rate_t rate=s_rate; uint32_t utc=os_time_info()->unix_utc;
  os_crash_flush(); assert(s_pending && !s_written);
  assert_trusted(anchor_sec,anchor_ms,rate,utc);
  advance_awake(attempt==0?600:300);
 }
 os_clock_rate_t rate=s_rate; uint32_t utc=os_time_info()->unix_utc;
 fail_log_write=false; os_crash_flush(); assert(!s_pending && s_written);
 assert_trusted(anchor_sec,anchor_ms,rate,utc); assert_record(utc,3);
 // 延迟重试后第二网络锚点仍能覆盖之前的睡眠样本。/ The second network anchor still covers sleep samples preceding the delayed retry.
 advance_sleep(45); sync();
 assert(saved_ppm==4184 && saved_count==1 && s_rate.result==OS_CLOCK_LEARNED);
 assert(s_rate.window_sleep_us==75LL*59750000);
 assert(s_rate.window_ms==tick_us/1000);
 assert(os_time_info()->unix_utc==(uint32_t)(network_us/1000000));
 puts("crash clock: failed writes and late retries retain trusted NTP time and the full two-anchor sleep-learning window PASS");
}
static void unavailable_mount_and_time(void) {
 reset_boot(); network_us=1791400000250000LL; sync(); os_crash_boot_check(); advance_awake(600);
 uint32_t utc=os_time_info()->unix_utc,anchor_sec=s_anchor_sec; int64_t anchor_ms=s_anchor_ms;
 os_clock_rate_t rate=s_rate; mount_result=1; os_crash_flush();
 assert(s_pending && !s_written && file_calls==0);
 assert_trusted(anchor_sec,anchor_ms,rate,utc);
 mount_result=ESP_OK; os_crash_flush(); assert(!s_pending && s_written);
 assert_trusted(anchor_sec,anchor_ms,rate,utc); assert_record(utc,3);
 reset_boot(); tick_us=120000000; network_us=1791400000000000LL;
 os_crash_boot_check(); os_crash_flush();
 assert(!s_pending && s_written && !s_anchored && !s_rate.sampled);
 assert(os_time_info()->state!=OS_TIME_VALID); assert_record(0,3);
 puts("crash clock: mount retry preserves trusted time; unavailable PMU records unknown UTC without inventing an anchor PASS");
}
static void normal_boot_is_noop(void) {
 for(int reason=0;reason<=8;++reason) {
  if(os_crash_reason_abnormal(reason)) continue;
  reset_boot(); reset_reason=reason; network_us=1791400000250000LL; sync(); advance_awake(123);
  uint32_t utc=os_time_info()->unix_utc,anchor_sec=s_anchor_sec; int64_t anchor_ms=s_anchor_ms;
  os_clock_rate_t rate=s_rate; os_crash_boot_check(); os_crash_flush();
  assert(!os_crash_boot_abnormal() && !s_pending && s_written && mount_calls==0 && file_calls==0);
  assert_trusted(anchor_sec,anchor_ms,rate,utc);
 }
 puts("crash clock: normal reset reasons do not mount storage or mutate time PASS");
}
int main(int argc,char** argv) {
 assert(argc==2 || argc==3); host_log_path=argv[1];
 if(argc==3) {
  if(strcmp(argv[2],"startup")==0) nonzero_boot_tick();
  else { assert(strcmp(argv[2],"retry")==0); deferred_log_retry_preserves_learning(); }
  return 0;
 }
 nonzero_boot_tick(); deferred_log_retry_preserves_learning(); unavailable_mount_and_time(); normal_boot_is_noop();
 puts("Production os_crash_pico.c + os_time_pico.c + os_clock_rate.h + pure helpers PASS");
}
'''

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--verify-regression", action="store_true",
                    help="验证旧锚点处理在两条真实路径上失败。/ Verify the former anchor handling fails on both real paths.")
args = parser.parse_args()

with tempfile.TemporaryDirectory(prefix="pico-clock-crash-", dir="/tmp") as folder:
    d = Path(folder)
    (d / "test.c").write_text(harness)
    flags = ["-std=gnu11", "-Wall", "-Wextra", "-Werror", "-g", "-fsanitize=address,undefined"]
    if os.uname().sysname == "Darwin":
        flags += ["-isysroot", "/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk"]
    subprocess.run([os.environ.get("CC", "cc"), *flags, "-Imain/os", str(d / "test.c"),
                    "main/os/os_time.c", "main/os/os_crash.c", "-o", str(d / "test")], cwd=ROOT, check=True)
    subprocess.run([str(d / "test"), str(d / "crash.log")],
                   env={**os.environ, "UBSAN_OPTIONS": "halt_on_error=1"}, check=True)
    if args.verify_regression:
        # 只在临时夹具恢复旧的两行，不改工作区源码。/ Restore the former two lines only in the temporary fixture, never in workspace sources.
        fixed = "os_time_force_poll();\n    os_time_poll(esp_timer_get_time() / 1000);"
        assert harness.count(fixed) == 1
        former = harness.replace(fixed, "os_time_invalidate();\n    os_time_poll(0);")
        (d / "former.c").write_text(former)
        subprocess.run([os.environ.get("CC", "cc"), *flags, "-Imain/os", str(d / "former.c"),
                        "main/os/os_time.c", "main/os/os_crash.c", "-o", str(d / "former")], cwd=ROOT, check=True)
        for case in ("startup", "retry"):
            result = subprocess.run([str(d / "former"), str(d / "former.log"), case],
                                    env={**os.environ, "UBSAN_OPTIONS": "halt_on_error=1"},
                                    text=True, capture_output=True)
            assert result.returncode != 0 and "Assertion" in result.stderr and "failed" in result.stderr
            assert "os_time_info()->" in result.stderr and "AddressSanitizer" not in result.stderr
            print(f"Former crash-log handling fails the {case} time assertion as expected PASS")
