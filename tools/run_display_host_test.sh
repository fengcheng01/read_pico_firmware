#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 mindreset
# SPDX-License-Identifier: Apache-2.0
# 编译真实显示出口，只替换硬件边界。/ Compile the production display path, replacing hardware boundaries only.
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build/book-tests
link_gc=-Wl,--gc-sections
if [[ "$(uname -s)" == Darwin ]]; then link_gc=-Wl,-dead_strip; fi
cc -std=c11 -Wall -Wextra -Werror -g -fsanitize=address,undefined \
    -ffunction-sections -fdata-sections "$link_gc" \
    -Itools/display_host_stubs -Imain -Imain/app \
    tools/display_host_test.c main/display.c main/display_pixels.c -o build/book-tests/display
build/book-tests/display

# 导航表测试链接真实厂家源表和裁剪器，不用波形标识替身。/ Navigation tests link real vendor tables and trimming, without waveform shims.
cc -std=c11 -Wall -Wextra -Werror -g -fsanitize=address,undefined \
    -Icomponents/e0470_epaper_waveform/include -Icomponents/e0470_epaper_waveform/waveforms \
    -Itools/waveform_host_stubs -Icomponents/epdiy/include -Icomponents/epdiy/src tools/waveform_navigation_host_test.c \
    components/e0470_epaper_waveform/e0470_epaper_waveform.c \
    components/e0470_epaper_waveform/e0470_waveform_trim.c -o build/book-tests/navigation-waveform
build/book-tests/navigation-waveform

# 独立固定SDK指纹检查对照表全部字节，并保证原模式不变。/ Check all comparison bytes against pinned SDK fingerprints and retain original modes.
python3 tools/crossmux_waveform_host_test.py

# 灰边档逐相复用厂家DU/8灰GL，保持EE及中性尾，不把数字一致性当作光学通过。
# Gray edges reuse each vendor DU/8-gray GL phase with held EE and neutral tails, without treating digital equality as optical success.
python3 tools/gray_direct_waveform_host_test.py

# 三轮真实清白后直绘与失败恢复。/ Actual three-round white-clear repaint and failure recovery.
python3 tools/night_white_repaint_host_test.py
