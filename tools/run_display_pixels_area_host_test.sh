#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 mindreset
# SPDX-License-Identifier: Apache-2.0
# 编译真实区域量化器，启用内存与未定义行为检查。/ Compile the real area quantizer with memory and undefined-behavior checks.
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p build/book-tests
cc -std=c11 -Wall -Wextra -Werror -g -fsanitize=address,undefined \
    -Imain tools/display_pixels_area_host_test.c main/display_pixels.c \
    -o build/book-tests/display-pixels-area
build/book-tests/display-pixels-area
