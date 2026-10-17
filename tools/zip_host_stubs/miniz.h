/* SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 * 仅为主机测试把 ROM miniz 的环形/一次性 tinfl 语义映射到 zlib 增量 inflate。
 * Map ROM miniz's ring-mode and one-shot tinfl semantics onto zlib's incremental inflate for host tests only.
 * 冻结：不替换 ZIP/PNG 解析，不用于验证 ROM 实现。/ Frozen: Do not replace ZIP/PNG parsing or claim ROM validation.
 */
#pragma once
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include <zlib.h>
typedef unsigned char mz_uint8;
typedef unsigned int mz_uint32;
typedef enum { TINFL_STATUS_FAILED = -1, TINFL_STATUS_DONE = 0, TINFL_STATUS_NEEDS_MORE_INPUT = 1, TINFL_STATUS_HAS_MORE_OUTPUT = 2 } tinfl_status;
#define TINFL_FLAG_PARSE_ZLIB_HEADER 1
#define TINFL_FLAG_HAS_MORE_INPUT 2
#define TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF 4
#define TINFL_LZ_DICT_SIZE 32768
// 有状态增量替身：输出直写调用方窗口（zlib 自持窗口，缓冲可滚动复用），终态即释放，
// 与 ROM 环形模式外部契约一致；调用方须先清零结构（设备容忍任意初值，替身不行）。
// Stateful incremental stand-in: output lands directly in the caller's window (zlib
// keeps its own history, so buffers roll freely), terminal states release at once —
// the ROM ring contract; callers must zero the struct first (ROM tolerates garbage).
typedef struct { z_stream stream; int live; unsigned char scratch; } tinfl_decompressor;
static inline void tinfl_init(tinfl_decompressor* r) {
    if (r->live) inflateEnd(&r->stream);
    memset(r, 0, sizeof(*r));
}
static inline tinfl_status tinfl_terminal(tinfl_decompressor* r, int rc) {
    // 终态（完成或失败）立即归还 zlib 内部状态，结构可安全释放。
    // Terminal states (done or failed) release zlib internals at once so the struct frees safely.
    if (rc == TINFL_STATUS_DONE || rc == TINFL_STATUS_FAILED) {
        inflateEnd(&r->stream);
        r->live = 0;
    }
    return (tinfl_status)rc;
}
static inline tinfl_status tinfl_decompress(tinfl_decompressor* r, const mz_uint8* input,
    size_t* in_size, mz_uint8* start, mz_uint8* output, size_t* out_size, mz_uint32 flags) {
    (void)start;
    if (!r->live) {
        r->live = 1;
        if (inflateInit2(&r->stream, flags & TINFL_FLAG_PARSE_ZLIB_HEADER ? MAX_WBITS : -MAX_WBITS) != Z_OK) {
            r->live = 0;
            return TINFL_STATUS_FAILED;
        }
    }
    r->stream.next_in = (Bytef*)input;
    r->stream.avail_in = (uInt)*in_size;
    // 零长度输出借一个字节落点，保持“无进展即 NEEDS_MORE_INPUT”的判定不被零缓冲干扰。
    // Zero-length output borrows one landing byte so the no-progress mapping stays intact.
    r->stream.next_out = *out_size ? output : &r->scratch;
    r->stream.avail_out = *out_size ? (uInt)*out_size : 1;
    int rc = inflate(&r->stream, Z_FINISH);
    *in_size -= r->stream.avail_in;
    // 零长度输出借的字节不计入产出，避免 0-1 下溢。
    // The borrowed scratch byte for zero-length output never counts as produced, avoiding a 0-1 underflow.
    if (*out_size) *out_size -= r->stream.avail_out;
    if (rc == Z_STREAM_END) return tinfl_terminal(r, TINFL_STATUS_DONE);
    if (rc != Z_OK && rc != Z_BUF_ERROR) return tinfl_terminal(r, TINFL_STATUS_FAILED);
    return tinfl_terminal(r, r->stream.avail_out ? TINFL_STATUS_NEEDS_MORE_INPUT : TINFL_STATUS_HAS_MORE_OUTPUT);
}
