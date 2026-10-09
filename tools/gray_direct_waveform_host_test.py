#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 mindreset
# SPDX-License-Identifier: Apache-2.0
# 编译真实灰边装配及厂家源的逐相回归，并固定两份不可修改源的指纹。
# Compile actual gray-edge assembly and per-phase vendor-source regression, fixing fingerprints of both immutable sources.
# 冻结：不联网、不读取Git历史或忽略目录，不把数字LUT一致性当作光学验收。
# Frozen: No network, Git-history or ignored-directory inputs; digital LUT equality is not optical acceptance.

import hashlib
import os
from pathlib import Path
import shlex
import subprocess


ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build/book-tests/gray-direct-waveform"
SOURCE_HASHES = {
    "du.h": "8bd94b91e05dc5219d978c1358ae83b9bdcd00991eab91869bdb291c78bff38d",
    "gray8_gl16.h": "bf4ffbac31b95af6855bd51bccc38f88cce3470f3b39fd6a71ed1a6b1fe21569",
}


def main():
    for name, digest in SOURCE_HASHES.items():
        source = ROOT / "components/e0470_epaper_waveform/waveforms" / name
        assert hashlib.sha256(source.read_bytes()).hexdigest() == digest, name
    BUILD.mkdir(parents=True, exist_ok=True)
    binary = BUILD / "gray-direct-waveform"
    command = shlex.split(os.environ.get("CC", "cc")) + [
        "-std=c11", "-Wall", "-Wextra", "-Werror", "-g", "-fsanitize=address,undefined",
        "-I" + str(ROOT / "components/e0470_epaper_waveform/include"),
        "-I" + str(ROOT / "components/e0470_epaper_waveform/waveforms"),
        "-I" + str(ROOT / "tools/waveform_host_stubs"),
        "-I" + str(ROOT / "components/epdiy/include"),
        "-I" + str(ROOT / "components/epdiy/src"),
        str(ROOT / "tools/gray_direct_waveform_host_test.c"),
        str(ROOT / "components/e0470_epaper_waveform/e0470_epaper_waveform.c"),
        str(ROOT / "components/e0470_epaper_waveform/e0470_waveform_trim.c"),
        "-o", str(binary),
    ]
    subprocess.run(command, check=True)
    subprocess.run([str(binary)], check=True)
    print("Immutable DU20/8-gray GL30 source headers and production assembly under ASAN/UBSAN PASS (device optical validation pending)")


if __name__ == "__main__":
    main()
