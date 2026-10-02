#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 mindreset
# SPDX-License-Identifier: Apache-2.0
"""编译真实摘要、入口与存储策略并隔离测试数据。/ Compile real summaries, entries and storage policy with isolated data."""
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
flags = ["-std=gnu11", "-Wall", "-Wextra", "-Werror", "-g", "-fsanitize=address,undefined", "-fno-omit-frame-pointer"]
if os.uname().sysname == "Darwin":
    developer = Path(subprocess.check_output(["xcode-select", "-p"], text=True).strip())
    sdk = Path(os.environ.get("PREVIEW_MACOS_SDK", developer / "Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk"))
    if sdk.exists():
        flags += ["-isysroot", str(sdk)]
with tempfile.TemporaryDirectory(prefix="rp-os.", dir="/tmp") as folder:
    includes = ["-Imain/book", "-Imain/os", "-Imanaged_components/espressif__cjson/cJSON", "-Imain/app", "-Itools/book_storage_stubs", "-Icomponents/read_pico/include"]
    for name, sources in (("home", ["tools/os_home_host_test.c", "main/book/book_home.c", "main/book/book_entry.c"]),
                          ("store", ["tools/book_store_host_test.c"]),
                          ("device", ["tools/os_device_host_test.c", "main/os/os_device_pico.c"]),
                          ("product", ["tools/ui_product_host_test.c", "main/ui/product/ui_product.c", "main/book/book_entry.c"]),
                          ("time", ["tools/os_time_host_test.c", "main/os/os_time.c"]),
                          ("stats", ["tools/book_stats_host_test.c", "main/book/book_stats.c"]),
                          ("sleep", ["tools/app_sleep_hooks_host_test.c", "main/app/app_sleep_hooks.c"]),
                          ("sync", ["tools/os_sync_host_test.c", "main/os/os_sync.c", "managed_components/espressif__cjson/cJSON/cJSON.c"]),
                          ("cover", ["tools/book_cover_host_test.c", "main/book/book_cover.c"]),
                          ("sync_device", ["tools/os_sync_device_host_test.c", "main/os/os_sync_pico.c", "main/os/os_sync.c", "main/os/os_sync_http.c", "managed_components/espressif__cjson/cJSON/cJSON.c"]),
                          ("usb_disk", ["tools/usb_disk_host_test.c"]),
                          ("sync_http", ["tools/os_sync_http_host_test.c", "main/os/os_sync_http.c"]),
                          ("lunar", ["tools/os_lunar_host_test.c", "main/os/os_lunar.c"]),
                          ("marks", ["tools/book_marks_host_test.c", "main/book/book_marks.c"]),
                          ("crash", ["tools/os_crash_host_test.c", "main/os/os_crash.c"])):
        binary = str(Path(folder) / name)
        target_includes = includes
        target_flags = flags
        if name in ("sync", "sync_device") and os.uname().sysname == "Darwin":
            target_flags = flags + ["-Wno-deprecated-declarations"]
        if name == "usb_disk":
            target_includes = ["-Itools/usb_disk_stubs", "-Imain/os"]
            target_flags = flags + ["-Wno-unused-function"]
        elif name == "sync_device":
            target_includes = ["-Itools/os_sync_device_stubs", *includes]
            target_flags = target_flags + ["-Wno-unused-variable", "-pthread"]
        elif name == "device":
            target_includes = includes + ["-DESP_ERR_NOT_FINISHED=6"]
        elif name == "product":
            target_includes = ["-Imain/book", "-Imain/font", "-Imain/ui", "-Imain/ui/product", "-Imain/app", "-Imain/os", "-Itools/os_home_stubs", "-Itools/app_loop_stubs", "-Itools/book_layout_stubs"]
        subprocess.run([os.environ.get("CC", "cc"), *target_flags, *target_includes, *sources, "-o", binary], cwd=ROOT, check=True)
        subprocess.run([binary, folder] if name == "home" else [binary], cwd=ROOT, check=True,
                       env=dict(os.environ, UBSAN_OPTIONS="halt_on_error=1:print_stacktrace=1"))
