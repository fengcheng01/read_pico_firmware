#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 mindreset
# SPDX-License-Identifier: Apache-2.0
"""中文：编译真实队列和相位准备，模拟 DMA 结束和任务汇合边界。
English: Compile production queues and phase preparation with mocked DMA and task joins.
"""
from pathlib import Path
import os
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
source_path = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / "components/epdiy/src/output_lcd/render_lcd.c"
source = source_path.read_text()
start = source.index("static void IRAM_ATTR handle_lcd_frame_done(")
end = source.index("__attribute__", start)
production = source[start:end]
with tempfile.TemporaryDirectory(prefix="pico-lcd-lifecycle-") as directory:
    out = Path(directory)
    headers = {
        "esp_attr.h": "#pragma once\n#define IRAM_ATTR\n",
        "esp_err.h": "#pragma once\ntypedef int esp_err_t;\n",
        "esp_types.h": "#pragma once\n#include <stdint.h>\n",
        "xtensa/core-macros.h": "#pragma once\n",
        "esp_log.h": "#pragma once\nvoid phase_test_log(const char*, const char*, ...);\n#define ESP_LOGW(...) phase_test_log(__VA_ARGS__)\n",
        "sdkconfig.h": "#pragma once\n#define CONFIG_IDF_TARGET_ESP32S3 1\n",
        "freertos/FreeRTOS.h": "#pragma once\n#include <assert.h>\n#include <stddef.h>\ntypedef int BaseType_t;\n#define pdFALSE 0\n#define portMAX_DELAY -1\n#define portYIELD_FROM_ISR() ((void)0)\n",
        "freertos/semphr.h": "#pragma once\ntypedef int SemaphoreHandle_t;\n",
        "freertos/task.h": "#pragma once\ntypedef int TaskHandle_t;\n",
        "esp_heap_caps.h": """#pragma once
#include <stdlib.h>
#define MALLOC_CAP_INTERNAL 2
static inline void* heap_caps_aligned_alloc(size_t align, size_t size, int caps) {
    (void)caps; void* p = NULL; return posix_memalign(&p, align, size) ? NULL : p;
}
static inline void heap_caps_free(void* p) { free(p); }
""",
    }
    for name, content in headers.items():
        path = out / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(content)
    (out / "lcd_scheduler.inc").write_text(production)
    flags = [os.environ.get("CC", "cc"), "-std=gnu11", "-g", "-Wall", "-Wextra", "-Werror",
             "-fsanitize=address,undefined", "-I" + str(out),
             "-I" + str(ROOT / "components/epdiy/src"),
             "-I" + str(ROOT / "components/epdiy/include"),
             "-I" + str(ROOT / "components/epdiy/src/output_common")]
    subprocess.run(flags + [str(ROOT / "tools/lcd_frame_lifecycle_host_test.c"),
                           str(ROOT / "components/epdiy/src/output_common/line_queue.c"),
                           str(ROOT / "components/epdiy/src/output_common/render_context.c"),
                           "-o", str(out / "test")], check=True)
    subprocess.run([str(out / "test")], check=True)
