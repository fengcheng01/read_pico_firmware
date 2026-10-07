#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 mindreset
# SPDX-License-Identifier: Apache-2.0
"""中文：编译真实 epdiy 提交与队列，注入失败并记录分配。
English: Compile production epdiy commits/queues with failure injection and allocation recording.
"""
from pathlib import Path
import os
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix="pico-epd-transactions-") as directory:
    out = Path(directory)
    headers = {
        "esp_attr.h": "#pragma once\n#define IRAM_ATTR\n",
        "esp_types.h": "#pragma once\n#include <stdint.h>\n",
        "esp_err.h": "#pragma once\ntypedef int esp_err_t;\n",
        "esp_log.h": "#pragma once\n#define ESP_LOGI(...) ((void)0)\n#define ESP_LOGW(...) ((void)0)\n",
        "esp_timer.h": "#pragma once\n#include <stdint.h>\nstatic inline int64_t esp_timer_get_time(void) { return 0; }\n",
        "xtensa/core-macros.h": "#pragma once\n",
        "sdkconfig.h": "#pragma once\n#define CONFIG_IDF_TARGET_ESP32S3 1\n#define CONFIG_SPIRAM 1\n",
        "esp_heap_caps.h": """#pragma once
#include <stdlib.h>
#define MALLOC_CAP_SPIRAM 1
#define MALLOC_CAP_INTERNAL 2
#define MALLOC_CAP_8BIT 4
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
    flags = [os.environ.get("CC", "cc"), "-std=gnu11", "-g", "-Wall", "-Wextra", "-Werror",
             "-fsanitize=address,undefined", "-I" + str(out),
             "-I" + str(ROOT / "components/epdiy/src"),
             "-I" + str(ROOT / "components/epdiy/include"),
             "-I" + str(ROOT / "components/epdiy/src/output_common")]
    subprocess.run(flags + ["-Dcalloc=record_calloc", "-c",
                           str(ROOT / "components/epdiy/src/output_common/line_queue.c"),
                           "-o", str(out / "queue.o")], check=True)
    subprocess.run(flags + [str(ROOT / "tools/epd_transaction_host_test.c"),
                           str(ROOT / "components/epdiy/src/highlevel.c"), str(out / "queue.o"),
                           "-o", str(out / "test")], check=True)
    subprocess.run([str(out / "test")], check=True)
