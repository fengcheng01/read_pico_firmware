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

# 夜间实验表逐相核对厂家源表，仅00动作、其余保持。/ Check vendor phase order in night experiments, with actions only for 00.
cc -std=c11 -Wall -Wextra -Werror -g -fsanitize=address,undefined \
    -Icomponents/e0470_epaper_waveform/include -Icomponents/e0470_epaper_waveform/waveforms \
    -Itools/waveform_host_stubs -Icomponents/epdiy/include -Icomponents/epdiy/src tools/night_cleanup_waveform_host_test.c \
    components/e0470_epaper_waveform/e0470_epaper_waveform.c \
    components/e0470_epaper_waveform/e0470_waveform_trim.c -o build/book-tests/night-cleanup-waveform
build/book-tests/night-cleanup-waveform
