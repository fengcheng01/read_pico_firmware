#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""中文：真实触摸队列，pthread替代FreeRTOS调度边界。/ English: Production touch queue with pthread RTOS seams."""
from pathlib import Path
import os
import subprocess
import tempfile
root = Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix="pico-touch-input-") as directory:
    out = Path(directory)
    (out/"freertos").mkdir()
    header = '''#pragma once
#include "common.h"
#define portMAX_DELAY 0xffffffffu
#define pdPASS 1
typedef void* SemaphoreHandle_t;
typedef void* TaskHandle_t;
void* xSemaphoreCreateMutex(void);
int xSemaphoreTake(void*, unsigned);
int xSemaphoreGive(void*);
int xTaskCreatePinnedToCore(void (*)(void*),const char*,unsigned,void*,unsigned,void**,int);
'''
    for name in ("FreeRTOS.h","semphr.h","task.h"):
        (out/"freertos"/name).write_text(header)
    command=[os.environ.get("CC","cc"),"-std=gnu11","-Wall","-Wextra","-Werror","-g","-fsanitize=address,undefined","-pthread",
             "-I"+str(out),"-Itools/app_loop_stubs","-Imain/app","-Imain/ui","-Imain/os","-Itools/ui_gesture_stubs",
             "tools/touch_input_host_test.c","main/app/app_touch_input.c","main/ui/ui_gesture.c","-o",str(out/"test")]
    subprocess.run(command,cwd=root,check=True)
    subprocess.run([str(out/"test")],check=True,timeout=15)
