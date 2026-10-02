/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 * 中文：ROM miniz 的宿主适配：tinfl 解压映射 zlib inflate。
 * English: Host adapter for ROM miniz: tinfl decompression over zlib inflate.
 */
#include "miniz.h"
#include <zlib.h>
#include <string.h>

tinfl_status tinfl_decompress(tinfl_decompressor* state, const mz_uint8* input, size_t* in_size,
                              mz_uint8* output, mz_uint8* out_buf_mark, size_t* out_size, unsigned flags) {
    (void)state; (void)out_buf_mark;
    // 设备侧 zip_reader 只用“整段进、尽量出”的 one-shot 语义。
    // The device zip_reader uses whole-input, best-effort-output one-shot semantics only.
    z_stream stream = {0};
    stream.next_in = (Bytef*)input;
    stream.avail_in = (uInt)*in_size;
    stream.next_out = output;
    stream.avail_out = (uInt)*out_size;
    if (inflateInit2(&stream, flags & TINFL_FLAG_PARSE_ZLIB_HEADER ? MAX_WBITS : -MAX_WBITS) != Z_OK) return TINFL_STATUS_FAILED;
    int result = inflate(&stream, Z_FINISH);
    *in_size -= stream.avail_in;
    *out_size -= stream.avail_out;
    inflateEnd(&stream);
    return result == Z_STREAM_END || (result == Z_OK && stream.avail_in == 0) ? TINFL_STATUS_DONE : TINFL_STATUS_FAILED;
}
