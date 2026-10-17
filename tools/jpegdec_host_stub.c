/* SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 *
 * 中文：宿主/预览桩。真机构建由 components/jpegdec 提供渐进 JPEG 的 DC-only 解码；
 * 这些挂具的夹具不含渐进图（渐进路径由 test_book_images.py 用真组件覆盖），桩保持
 * 解码失败的原判定（占位/回退）。
 * English: Host/preview stub. The device build provides progressive-JPEG DC-only
 * decoding through components/jpegdec; these harnesses carry no progressive
 * fixtures (test_book_images.py exercises the real component), so the stub keeps
 * the previous failure verdict (placeholder/fallback).
 */
#include "JPEGDEC.h"
#include <stdbool.h>
#include <stdio.h>

int jpegdec_gray_progressive(const uint8_t *data, int size, JPEG_DRAW_CALLBACK *draw,
                             void *user, int *drawn_width, int *drawn_height) {
    (void)data; (void)size; (void)draw; (void)user; (void)drawn_width; (void)drawn_height;
    return 0;
}

int jpegdec_gray_file(FILE *file, int size, unsigned source_w, unsigned source_h,
                      unsigned out_w, unsigned out_h, bool progressive,
                      JPEG_DRAW_CALLBACK *draw, void *user, unsigned *scaled_w, unsigned *scaled_h) {
    (void)file; (void)size; (void)source_w; (void)source_h; (void)out_w; (void)out_h;
    (void)progressive; (void)draw; (void)user; (void)scaled_w; (void)scaled_h;
    return 0;
}
