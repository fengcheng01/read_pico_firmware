#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 mindreset
# SPDX-License-Identifier: Apache-2.0
"""真实时间桥的RTC有效性和离线快钟边界。/ RTC validity and offline fast-clock boundaries of the production time bridge."""
from pathlib import Path
import argparse
import os
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
fixture = ROOT / "tools/clock_rate_host_test.py"
scope = {"__file__": str(fixture)}
exec(fixture.read_text().split("with tempfile.TemporaryDirectory")[0], scope)
harness = scope["harness"].split("static void sleep_minutes")[0]
harness = harness.replace("static int read_pico_pmu_refresh(void) {return ESP_OK;}",
    "static bool failed_time_read; static int read_pico_pmu_refresh(void) {if(failed_time_read)snapshot.time_ok=false;return ESP_OK;}")
# 测试不得设置宿主时钟；网络与RTC仍保持独立。/ Never set the host clock; network and RTC remain independent.
harness = harness.replace("#define time test_time", "static int test_settimeofday(const struct timeval* tv, const void* zone) {(void)tv;(void)zone;return 0;}\n#define settimeofday test_settimeofday\n#define time test_time")
harness += r'''
static void reset_boot(void) {
    os_time_network(false);
    s_last_poll_ms=-OS_TIME_POLL_MS; s_recently_synced=false;
    s_anchored=false; s_force_reanchor=false; s_anchor_sec=0; s_anchor_ms=0;
    s_rate=(os_clock_rate_t){0}; s_rate_loaded=false; s_rate_learned=false; s_last_sync_ms=-1;
    tick_us=0; network_us=1791400000000000LL; saved_ppm=0; saved_count=0;
    ignored_write=false; ntp_event=false; failed_time_read=false; snapshot=(pmu_snapshot_t){0}; os_time_apply(0,32);
}
static void valid_read_after_unset(void) {
    const uint32_t invalid[]={0, OS_TIME_UNIX_MIN-1, OS_TIME_UNIX_MAX, UINT32_MAX};
    for (unsigned i=0;i<sizeof(invalid)/sizeof(invalid[0]);++i) {
        reset_boot(); tick_us=120000000; snapshot.time_ok=true; snapshot.unix_sec=invalid[i];
        os_time_poll(tick_us/1000);
        assert(!s_anchored && os_time_info()->state!=OS_TIME_VALID);
        tick_us+=16000000; snapshot.unix_sec=1791400000;
        os_time_poll(tick_us/1000);
        assert(s_anchored && os_time_info()->unix_utc==1791400000 && s_anchor_ms==136000);
    }
    puts("RTC range: zero/out-of-range reads cannot latch an anchor; later valid read recovers without WiFi PASS");
}
static void awake_source_isolation(void) {
    reset_boot(); tick_us=120000000; snapshot.time_ok=true; snapshot.unix_sec=1791400120;
    os_time_poll(tick_us/1000); assert(os_time_info()->unix_utc==1791400120);
    tick_us+=43200000000LL; network_us+=43200000000LL;
    snapshot.unix_sec+=44064;
    os_time_poll(tick_us/1000);
    assert(os_time_info()->unix_utc==1791400120+43200);
    snapshot.time_ok=false; tick_us+=61000000; os_time_poll(tick_us/1000);
    assert(os_time_info()->unix_utc==1791400120+43261);
    snapshot.time_ok=true; snapshot.unix_sec=0; tick_us+=16000000; os_time_poll(tick_us/1000);
    assert(os_time_info()->unix_utc==1791400120+43277 && s_anchored);
    puts("Clock sources: boot inherits PMU offset; awake monotonic timing never follows faster/invalid/failed PMU reads PASS");
}
static void offline_fast_sleep(void) {
    reset_boot(); network_us+=250000; sync();
    for (int i=0;i<240;++i) {
        os_time_record_sleep(60250000); tick_us+=60250000; network_us+=60000000;
        os_time_poll(tick_us/1000);
    }
    assert((int64_t)os_time_info()->unix_utc-network_us/1000000==60);
    sync(); assert(saved_count==1 && saved_ppm==-4149);
    os_time_network(false);
    for (int i=0;i<1440;++i) {
        os_time_record_sleep(60250000); tick_us+=60250000; network_us+=60000000;
        os_time_poll(tick_us/1000);
        int64_t delta=(int64_t)os_time_info()->unix_utc-network_us/1000000;
        assert(delta>=-2 && delta<=2);
    }
    assert(saved_count==1);
    puts("Fast sleep: two trusted samples learn negative ppm; a following 24h offline model stays within 2s without extra network PASS");
}
static void stale_write_verification(void) {
    reset_boot();sync();os_time_network(false);
    tick_us+=3600000000LL;network_us+=3600000000LL;
    int64_t previous_sync=s_last_sync_ms,previous_sample=s_rate.sample_tick_ms;
    // 旧秒数恰好等于网络时刻，整体refresh成功也不能掩盖TIME_GET失败。
    // Even when stale seconds match network time, overall refresh success must not hide TIME_GET failure.
    snapshot.unix_sec=(uint32_t)(network_us/1000000);snapshot.time_ok=true;
    ignored_write=true;failed_time_read=true;os_time_network(true);
    assert(!os_time_recently_synced() && saved_count==0);
    assert(s_last_sync_ms==previous_sync && s_rate.sample_tick_ms==previous_sample);
    ignored_write=false;failed_time_read=false;os_time_network(true);
    assert(os_time_recently_synced() && s_last_sync_ms==tick_us/1000);
    puts("Time sync: partial refresh success with stale matching seconds cannot verify a write or advance learning; fresh retry recovers PASS");
}
int main(void) {
    valid_read_after_unset(); awake_source_isolation(); offline_fast_sleep(); stale_write_verification();
    puts("Production RTC restore/source-isolation/fast-sleep boundaries PASS; models do not measure oscillator drift");
}
'''
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--verify-regression", action="store_true")
args = parser.parse_args()
flags = ["-std=gnu11", "-Wall", "-Wextra", "-Werror", "-fsanitize=address,undefined"]
if os.uname().sysname == "Darwin":
    flags += ["-isysroot", "/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk"]
with tempfile.TemporaryDirectory(prefix="pico-clock-sources-", dir="/tmp") as folder:
    d = Path(folder)
    source = d / "test.c"
    source.write_text(harness)
    command = [os.environ.get("CC", "cc"), *flags, "-Imain/os", str(source), "main/os/os_time.c", "-o", str(d / "test")]
    subprocess.run(command, cwd=ROOT, check=True)
    subprocess.run([str(d / "test")], check=True)
    if args.verify_regression:
        fixed = "if (!pmu->time_ok || pmu->unix_sec < OS_TIME_UNIX_MIN || pmu->unix_sec >= OS_TIME_UNIX_MAX) return;"
        assert harness.count(fixed) == 1
        source.write_text(harness.replace(fixed, "if (!pmu->time_ok) return;"))
        subprocess.run(command, cwd=ROOT, check=True)
        result = subprocess.run([str(d / "test")], capture_output=True, text=True)
        assert result.returncode != 0 and "!s_anchored" in result.stderr
        print("Former transport-only RTC guard fails invalid-time recovery (negative control) PASS")
