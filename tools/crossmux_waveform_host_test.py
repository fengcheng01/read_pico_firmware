#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 mindreset
# SPDX-License-Identifier: Apache-2.0
# 真实Crossmux对照波形的离线指纹回归；期望来自独立编译SDK初始化产物，不由本地算法重算。
# Offline fingerprints for the actual Crossmux reference waveforms; expectations come from independently compiled SDK initialization products, not the local algorithm.
# 冻结：不联网、不读取忽略目录；完整packed表指纹覆盖全部256迁移与相位，软件通过不代表光学去残影。
# Frozen: No network or ignored-directory inputs; complete packed-table fingerprints cover all 256 transitions and phases, without proving optical ghost removal.

import hashlib
import json
import os
from pathlib import Path
import shlex
import subprocess


ROOT = Path(__file__).resolve().parents[1]
BUILD = ROOT / "build/book-tests/crossmux-waveform"

# 指纹取得过程：独立编译下列SDK的真实waveform.c与trim.c，初始化后按mode导出phases×16×4字节。
# Fingerprint capture: independently compile the SDK's actual waveform.c and trim.c, initialize and export phases×16×4 bytes for each mode.
# 来源与许可 / Source and license: Apache-2.0, copyright 2026 mindreset.
SDK_COMMIT = "96de1be6ce08eb732909e6e8149af8f892b9a2c5"
SDK_SOURCE = (
    "https://github.com/0x1abin/freeink-sdk/blob/"
    + SDK_COMMIT
    + "/libs/display/EpdiyLcd/src/e0470/e0470_epaper_waveform.c"
)
SDK_SOURCE_SHA256 = "1c91443268357bac29bb1e4d16bdc90b98f48dbf9a4cc72c53a407d6c2619307"
SDK_TRIM_SHA256 = "57fd7f5ffa7aeca9f57240dde6eb620e4ac54118668a5b0556b253fd376f8eda"
SDK_EXPECTED = {
    1: (20, "55832d3d441faf15a4974aeec9d4f8b6f2f2a4faae5717484fa71a583d18be41"),
    2: (36, "ffe0635907fa75844a2f3f174565b0d056d01ce2840f7ae753be9c65a64fb05c"),
    5: (37, "05c5240425700e2e63973a7940162ca09a14044d08cee1423c4702039cae4e89"),
}
VENDOR_SOURCE_SHA256 = {
    "du.h": "8bd94b91e05dc5219d978c1358ae83b9bdcd00991eab91869bdb291c78bff38d",
    "gc16.h": "12b275d21abc1a578af2fcee87c1f64152e7dbe8eec7613f7d0d4ebead0ad9a1",
    "gl16.h": "98ff6c06d99f13ae4daf69096852814ae5a846b64605b54eb27fb56b0166279d",
}

# 原档位指纹独立取自0.5.26@0b7611c60079337334b7e433bfc04f4731ede4f5的真实初始化产物。
# Original-profile fingerprints were independently captured from actual 0.5.26@0b7611c60079337334b7e433bfc04f4731ede4f5 initialization products.
LOCAL_EXPECTED = {
    "local-default:1": (20, "55832d3d441faf15a4974aeec9d4f8b6f2f2a4faae5717484fa71a583d18be41"),
    "local-default:2": (48, "986fa1847aeb06e3c02a2732680e5a3c9159088235b4af8742d7a1dd97b77a06"),
    "local-default:5": (49, "0b80363e5fcf76e5b51d4a01461f497f9342648d62d6556b46cd7acbdd363ccb"),
    "local-full:1": (20, "4fa98fb3e9d558ad8f5078bc35ba7c91465e6f714cf82e574ae367894f0096ba"),
    "local-full:2": (48, "986fa1847aeb06e3c02a2732680e5a3c9159088235b4af8742d7a1dd97b77a06"),
    "local-full:5": (48, "6c8e310b86178234f6473fa99ca9817b4c25c8792a2e06c29e3931708c4bdc4e"),
    "local-day:5": (49, "0b80363e5fcf76e5b51d4a01461f497f9342648d62d6556b46cd7acbdd363ccb"),
    "local-night:5": (49, "4fe24fdb7a69aa621b197988b42c590d7de90ff22c65331ddd24c13146d7f629"),
    "local-direct:5": (21, "c28bcda924735a38b12c97a87423e9d5ead7b9332d0c0e2ddefb860757bc5b8e"),
    "local-navigation:5": (49, "0b80363e5fcf76e5b51d4a01461f497f9342648d62d6556b46cd7acbdd363ccb"),
    "local-entry-diagnostic:5": (49, "3c940bce6a8d4ec04f64adcb41c203d5097d813c78e49b6b27095c7e2e0c0081"),
    "local-white-diagnostic:1": (6, "f6beec27dac93b7dc63aa9698fd60952e6c35912bde7d82b36e2994adaa91e06"),
    "local-gray8:1": (20, "55832d3d441faf15a4974aeec9d4f8b6f2f2a4faae5717484fa71a583d18be41"),
    "local-gray8:2": (30, "66c9cf156bf11b4d9f98ac78756da048aaf662d052e7002390f5d7a2f79a9626"),
    "local-gray8:5": (30, "3571789c4fd7b49355a959fef8cd21be6620d7771c155b4757e0c22c89e377df"),
    "local-follow:1": (8, "1608607c2e227f0505abedc962321220e486c828ea7fe12fd7ab208dc5f25910"),
}


def actions(data, frames, to, prior):
    return [
        (data[phase * 64 + to * 4 + prior // 4] >> (6 - 2 * (prior % 4))) & 3
        for phase in range(frames)
    ]


def main():
    for name, expected in VENDOR_SOURCE_SHA256.items():
        actual = hashlib.sha256(
            (ROOT / "components/e0470_epaper_waveform/waveforms" / name).read_bytes()
        ).hexdigest()
        assert actual == expected, ("immutable vendor source differs", name, actual)

    BUILD.mkdir(parents=True, exist_ok=True)
    binary = BUILD / "waveforms"
    command = shlex.split(os.environ.get("CC", "cc")) + [
        "-std=c11", "-Wall", "-Wextra", "-Werror", "-g", "-fsanitize=address,undefined",
        "-I" + str(ROOT / "components/e0470_epaper_waveform/include"),
        "-I" + str(ROOT / "components/e0470_epaper_waveform/waveforms"),
        "-I" + str(ROOT / "tools/waveform_host_stubs"),
        "-I" + str(ROOT / "components/epdiy/include"),
        "-I" + str(ROOT / "components/epdiy/src"),
        str(ROOT / "tools/crossmux_waveform_host_test.c"),
        str(ROOT / "components/e0470_epaper_waveform/e0470_epaper_waveform.c"),
        str(ROOT / "components/e0470_epaper_waveform/e0470_waveform_trim.c"),
        "-o", str(binary),
    ]
    subprocess.run(command, check=True)
    result = subprocess.run([str(binary)], check=True, text=True, capture_output=True)
    expected_cases = set(LOCAL_EXPECTED) | {"crossmux:" + str(mode) for mode in SDK_EXPECTED}
    observed = [set(), set()]
    crossmux = {}
    for line in result.stdout.splitlines():
        item = json.loads(line)
        run, name, mode = item["run"], item["name"], item["mode"]
        key = name + ":" + str(mode)
        assert run in (0, 1) and key not in observed[run], ("duplicate/invalid case", run, key)
        observed[run].add(key)
        frames, digest = SDK_EXPECTED[mode] if name == "crossmux" else LOCAL_EXPECTED[key]
        data = bytes.fromhex(item["data"])
        assert item["frames"] == frames and len(data) == frames * 64, (key, "phase/byte count")
        assert item["min"] == 0 and item["max"] == 50 and item["phase_times_null"], (key, "timing/temperature metadata")
        actual = hashlib.sha256(data).hexdigest()
        assert actual == digest, (key, "complete packed-table fingerprint", actual, digest)
        if name == "crossmux":
            if run:
                assert crossmux[mode] == data, (mode, "reinitialization changed bytes")
            else:
                crossmux[mode] = data
    assert observed == [expected_cases, expected_cases], ("missing/extra modes", observed)

    # 固定SDK端点相序明确区分DU、裁剪GC和含白白动作的GL，不能把完整或TextTurn表误标为对照。
    # Fixed SDK endpoint sequences distinguish DU, trimmed GC and white-tick GL from incorrectly labeled Full/TextTurn tables.
    assert actions(crossmux[1], 20, 0, 15) == [1] * 18 + [0] * 2
    assert actions(crossmux[1], 20, 0, 0) == [0] * 20
    assert actions(crossmux[2], 36, 0, 15) == [0] * 21 + [1] * 12 + [0] * 3
    assert actions(crossmux[2], 36, 0, 0) == [0] * 10 + [2] * 11 + [1] * 12 + [0] * 3
    assert actions(crossmux[5], 37, 0, 15) == [0] * 21 + [1] * 13 + [0] * 3
    assert actions(crossmux[5], 37, 0, 0) == [0] * 10 + [2] * 11 + [1] * 13 + [0] * 3
    assert actions(crossmux[5], 37, 15, 15) == [0] * 33 + [2] + [0] * 3
    print("Crossmux SDK 96de1be DU20/GC36/GL37 full-byte fingerprints, metadata, endpoint phase order and reinit PASS")
    print("All 16 original local mode tables and immutable vendor headers unchanged under ASAN/UBSAN PASS (no optical claim)")


if __name__ == "__main__":
    main()
