/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 * 中文：复用 ROM miniz 解包字库和位图，解码状态放 PSRAM。
 * English: Inflate fonts and bitmaps through ROM miniz with decoder state in PSRAM.
 * 冻结：长度不匹配或校验失败拒绝输出。/ Frozen: Reject mismatched lengths and failed checksums.
 */
#include "asset_pack.h"
#include "esp_heap_caps.h"
#include "miniz.h"
#include <string.h>
size_t asset_pack_size(const uint8_t* data, size_t size) {
    if (!data || size < 8 || memcmp(data, "RPFT", 4)) return 0;
    return (uint32_t)data[4] | ((uint32_t)data[5] << 8) | ((uint32_t)data[6] << 16) | ((uint32_t)data[7] << 24);
}
bool asset_pack_unpack(const uint8_t* data, size_t size, uint8_t* out, size_t capacity) {
    if (!out || !capacity || asset_pack_size(data, size) != capacity || capacity > 3u * 1024u * 1024u) return false;
    tinfl_decompressor* decoder = heap_caps_malloc(sizeof(*decoder), MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (!decoder) return false;
    tinfl_init(decoder);
    size_t in = size - 8, written = capacity;
    tinfl_status status = tinfl_decompress(decoder, data + 8, &in, out, out, &written,
        TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF | TINFL_FLAG_PARSE_ZLIB_HEADER);
    heap_caps_free(decoder);
    return status == TINFL_STATUS_DONE && in == size - 8 && written == capacity;
}
