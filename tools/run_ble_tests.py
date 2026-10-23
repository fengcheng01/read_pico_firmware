#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 mindreset
# SPDX-License-Identifier: Apache-2.0
"""真实 BLE 产品状态机与解析器回归。/ Real BLE product-state and decoder regressions."""
from pathlib import Path
import os
import subprocess
import tempfile
ROOT = Path(__file__).resolve().parents[1]
for config_name in ("sdkconfig.defaults", "sdkconfig.ci"):
    config = (ROOT / config_name).read_text()
    assert "CONFIG_BT_CTRL_MODEM_SLEEP=y" in config
    assert "CONFIG_BT_CTRL_LPCLK_SEL_MAIN_XTAL=y" in config
OUT = ROOT / "build-host/ble"
OUT.mkdir(parents=True, exist_ok=True)
flags = ["-std=gnu11", "-g", "-O1", "-Wall", "-Wextra", "-Werror", "-fsanitize=address,undefined", "-fno-omit-frame-pointer"]
subprocess.run([os.environ.get("CC", "cc"), *flags, "-Itools/ble_stubs", "-Imain/os",
    "tools/ble_page_turner_host_test.c", "main/os/ble_hid.c", "-o", str(OUT / "test")], cwd=ROOT, check=True)
subprocess.run([str(OUT / "test")], cwd=ROOT, check=True)
with tempfile.TemporaryDirectory(prefix="radio-", dir=OUT) as folder:
    include = Path(folder)
    for header in ("esp_heap_caps.h", "esp_log.h", "esp_timer.h", "freertos/FreeRTOS.h", "freertos/task.h",
        "host/ble_gap.h", "host/ble_gatt.h", "host/ble_hs.h", "host/ble_hs_adv.h", "host/ble_sm.h", "host/ble_store.h",
        "host/util/util.h", "nimble/nimble_port.h", "os/os_mbuf.h"):
        p = include / header
        p.parent.mkdir(parents=True, exist_ok=True)
        p.write_text('#include "radio_port.h"\n')
    subprocess.run([os.environ.get("CC", "cc"), *flags, "-Wno-unused-variable", f"-I{include}",
        "-Itools/ble_stubs", "-Imain/os", "tools/ble_radio_host_test.c", "main/os/ble_hid.c",
        "-o", str(OUT / "radio")], cwd=ROOT, check=True)
    subprocess.run([str(OUT / "radio")], cwd=ROOT, check=True)
sleep = (ROOT / "main/sleep.c").read_text()
gate = sleep[sleep.index("lock_again:") + len("lock_again:"):sleep.index("uint8_t* framebuffer", sleep.index("lock_again:"))]
source = '''#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
static bool stopped;static unsigned paused,prepared;
static bool os_page_turner_pause(void){++paused;return stopped;}
static void app_sleep_prepare_run(void){++prepared;}
static void gate(void){''' + gate + '''}
int main(void){gate();assert(paused==1&&!prepared);stopped=true;gate();assert(paused==2&&prepared==1);puts("BLE sleep guard rejects sleep until the radio stops: PASS");}
'''
(OUT / "sleep_gate.c").write_text(source)
subprocess.run([os.environ.get("CC", "cc"), *flags, str(OUT / "sleep_gate.c"), "-o", str(OUT / "sleep_gate")], cwd=ROOT, check=True)
subprocess.run([str(OUT / "sleep_gate")], cwd=ROOT, check=True)
