#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 mindreset
# SPDX-License-Identifier: Apache-2.0
# 中文：链接真实显示、高层和厂家表，抽取原物理清白循环并用负对照拒绝提前白参考。
# English: Link actual display, high-level and vendor tables, extracting original physical-clear loops and rejecting premature white references with a negative control.
from pathlib import Path
import os
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]


def function(source, name):
    start = source.index("void " + name + "(")
    brace = source.index("{", start)
    depth = 1
    end = brace + 1
    while depth:
        if source[end] == "{":
            depth += 1
        elif source[end] == "}":
            depth -= 1
        end += 1
    return source[start:end]


def build_and_run(out, source, suffix, expected_failure=False):
    command = shlex.split(os.environ.get("CC", "cc")) + [
        "-std=gnu11", "-g", "-Wall", "-Wextra", "-Werror", "-fsanitize=address,undefined",
        "-ffunction-sections", "-fdata-sections",
        "-Wl,-dead_strip" if os.uname().sysname == "Darwin" else "-Wl,--gc-sections",
        "-I" + str(out), "-I" + str(ROOT / "main"), "-I" + str(ROOT / "main/app"),
        "-I" + str(ROOT / "components/epdiy/src"), "-I" + str(ROOT / "components/epdiy/include"),
        "-I" + str(ROOT / "components/e0470_epaper_waveform/include"),
        "-I" + str(ROOT / "components/e0470_epaper_waveform/waveforms"),
        str(ROOT / "tools/night_white_repaint_host_test.c"), str(source), str(out / "actual_clear.c"),
        str(ROOT / "main/display_pixels.c"), str(ROOT / "components/epdiy/src/highlevel.c"),
        str(ROOT / "components/e0470_epaper_waveform/e0470_epaper_waveform.c"),
        str(ROOT / "components/e0470_epaper_waveform/e0470_waveform_trim.c"),
        "-o", str(out / suffix),
    ]
    subprocess.run(command, check=True)
    result = subprocess.run([str(out / suffix)], text=True, capture_output=True)
    if expected_failure:
        assert result.returncode != 0 and "Assertion" in result.stderr, (result.returncode, result.stdout, result.stderr)
        print("negative control: fictional pre-clear white reference rejected PASS")
    else:
        print(result.stdout, end="")
        if result.returncode:
            print(result.stderr, end="")
            raise subprocess.CalledProcessError(result.returncode, command)


with tempfile.TemporaryDirectory(prefix="pico-night-white-") as directory:
    out = Path(directory)
    headers = {
        "esp_attr.h": "#pragma once\n#define IRAM_ATTR\n",
        "esp_types.h": "#pragma once\n#include <stdint.h>\n",
        "esp_err.h": "#pragma once\ntypedef int esp_err_t;\n",
        "esp_log.h": "#pragma once\n#define ESP_LOGI(tag, ...) ((void)(tag))\n#define ESP_LOGW(tag, ...) ((void)(tag))\n",
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
    for name in ("read_pico_board.h", "read_pico_epd_timing.h"):
        headers[name] = (ROOT / "tools/display_host_stubs" / name).read_text()
    for name, content in headers.items():
        path = out / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(content)
    render = (ROOT / "components/epdiy/src/render.c").read_text()
    epdiy = (ROOT / "components/epdiy/src/epdiy.c").read_text()
    constant = next(line for line in render.splitlines() if line.startswith("const int clear_cycle_time = "))
    (out / "actual_clear.c").write_text(
        '#include "epdiy.h"\n' + constant + "\n" +
        function(render, "epd_clear_area_cycles") + "\n" +
        function(render, "epd_clear_area") + "\n" + function(epdiy, "epd_clear") + "\n"
    )
    build_and_run(out, ROOT / "main/display.c", "actual")
    # 中文：仅篡改编译的临时副本，生产文件和厂家表保持。/ English: Mutate only a compiled temporary copy, leaving production files and vendor tables intact.
    source = (ROOT / "main/display.c").read_text()
    start = source.index("enum EpdDrawError update_display_night_white_repaint(")
    location = source.index("        epd_clear();\n        memset(hl->back_fb, 255,", start)
    source = source[:location] + "        memset(hl->back_fb, 255, (size_t)epd_width() * epd_height() / 2);\n" + source[location:]
    negative = out / "fictional_white.c"
    negative.write_text(source)
    build_and_run(out, negative, "negative", expected_failure=True)
