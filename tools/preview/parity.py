#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 mindreset
# SPDX-License-Identifier: Apache-2.0
"""对等检查：main/ 的源码必须同时进设备 CMake 与预览构建。

Parity check: every main/ source must build on the device AND the preview.

预览分叉只允许出现在显式登记的设备边界清单里；新增产品文件忘记接入预览
（或反之）在这里立即失败，而不是等模拟与实机悄悄漂移。

Preview forks are allowed only for explicitly registered device-boundary files;
forgetting to wire a new product file into the preview (or the device) fails
here immediately instead of drifting silently.
"""
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]

# 只属于设备构建的边界文件：驱动/工厂/主循环装配与 ESP 专属桥。
# Device-boundary files excluded from the preview: drivers, factory wiring, the
# real loop assembly and ESP-only bridges.
DEVICE_ONLY = {
    "app_main.c",          # 板级装配与真机引导 / board assembly and boot
    "display.c",           # 波形/推屏驱动 / waveform and present driver
    "sleep.c",             # PMU/GPIO 睡眠路径 / PMU/GPIO sleep path
    "app/app_loop.c",      # 真机事件循环 / the real event loop
    "os/os_time_pico.c",   # PMU/SNTP 设备桥 / PMU/SNTP bridge
    "os/os_sync_pico.c",
    "os/os_crash_pico.c",  # esp_reset_reason 与内置 FAT 追加 / reset reason + internal-FAT append   # HTTP/MD5 设备桥 / HTTP/MD5 bridge
    "book/book_store.c",   # FATFS 挂载策略（预览用夹具根）/ FATFS policy (preview uses fixture roots)
    "factory/pmu_selftest.c",
    "factory/vcom_setup.c",
}

# 预览以显式替身实现的文件（必须写明理由并保持接口一致）。
# Files with explicit preview stand-ins (reason required, interfaces must match).
ALLOWED_FORKS = {
    "ui/ui_wifi_qr.c",  # 依赖 espressif qrcodegen C++ 组件；transfer_port 画占位框 / espressif qrcodegen is C++; transfer_port draws a placeholder
}


def cmake_sources():
    text = (ROOT / "main/CMakeLists.txt").read_text()
    block = re.search(r"idf_component_register\(\s*SRCS(.*?)REQUIRES", text, re.S).group(1)
    return {f"main/{name}" for name in re.findall(r'"([^"]+\.c)"', block)}


def preview_sources():
    text = (ROOT / "tools/preview/build.py").read_text()
    sources = set(re.findall(r'"(main/[^"]+\.c)"', text))
    # app_registry 驱动的页面按元数据编入；supported 页面都在预览里。
    # Pages compile in via metadata; supported pages all run in the preview.
    registry = (ROOT / "main/app/app_registry.c").read_text()
    unavailable = {name for name in re.findall(r'extern const app_desc_t (app_\w+);', registry)}
    pages = {p.relative_to(ROOT).as_posix() for p in (ROOT / "main/apps").glob("*.c")}
    sources |= {page for page in pages if Path(page).name.replace(".c", "") in unavailable}
    return sources


def main():
    problems = []
    for source in sorted(cmake_sources() - preview_sources()):
        if source.removeprefix("main/") not in DEVICE_ONLY and source.removeprefix("main/") not in ALLOWED_FORKS:
            problems.append(f"未接入预览的产品源码 / not wired into the preview: {source}")
    for source in sorted(preview_sources() - cmake_sources()):
        problems.append(f"预览引用了设备不编译的源码 / preview references a non-device source: {source}")
    if problems:
        print("\n".join(problems))
        return 1
    print(f"parity: {len(cmake_sources())} main/ sources shared or registered ({len(DEVICE_ONLY)} device-boundary)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
