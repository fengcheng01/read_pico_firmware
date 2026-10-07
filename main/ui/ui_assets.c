/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 * 中文：内置竖屏位图的有界解包与旋转绘制，临时 PSRAM 在绘制后释放。
 * English: Bounded inflation and rotated drawing of embedded portrait bitmaps; release temporary PSRAM after painting.
 * 冻结：资源不得直接覆盖物理帧缓冲；失败时绘制白页，不显示部分解码内容。
 * Frozen: Never inflate assets into the physical framebuffer; paint white on failure instead of partial decoded data.
 */
#include "ui_kit.h"
#include "asset_pack.h"
#include "esp_heap_caps.h"

bool ui_draw_packed_full_image(uint8_t* framebuffer, const uint8_t* packed, size_t size) {
    if (!framebuffer) return false;
    const size_t bytes = (size_t)epd_rotated_display_width() * epd_rotated_display_height() / 2;
    uint8_t* image = NULL;
    bool decoded = false;
    if (bytes && asset_pack_size(packed, size) == bytes) {
        image = heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
        if (image) decoded = asset_pack_unpack(packed, size, image, bytes);
    }
    // 逻辑竖屏和物理扫描排列不同；绘图源不能与目标别名。/ Logical portrait and physical scan layouts differ; source and destination must not alias.
    ui_draw_full_image(framebuffer, decoded ? image : NULL);
    heap_caps_free(image);
    return decoded;
}
