/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 * 中文：ROM miniz 的宿主适配：环形/一次性 tinfl 语义映射 zlib 增量 inflate。
 * English: Host adapter for ROM miniz: ring-mode and one-shot tinfl over zlib's incremental inflate.
 */
#include "miniz.h"
#include <string.h>
#include <zlib.h>

void tinfl_init(tinfl_decompressor* state) {
    if (state->live) inflateEnd(&state->stream);
    memset(state, 0, sizeof(*state));
}

static tinfl_status tinfl_terminal(tinfl_decompressor* state, int rc) {
    // 终态（完成或失败）立即归还 zlib 内部状态，结构可安全释放；可续状态保持活跃。
    // Terminal states (done or failed) release zlib internals at once so the struct
    // frees safely; resumable states stay live.
    if (rc == TINFL_STATUS_DONE || rc == TINFL_STATUS_FAILED) {
        inflateEnd(&state->stream);
        state->live = 0;
    }
    return (tinfl_status)rc;
}

// 输出直写调用方窗口（zlib 自持窗口），与 ROM 环形模式外部契约一致；调用方须先清零结构。
// Output lands directly in the caller's window (zlib keeps its own history), matching the
// ROM ring contract; callers must zero the struct first.
tinfl_status tinfl_decompress(tinfl_decompressor* state, const mz_uint8* input, size_t* in_size,
                              mz_uint8* output, mz_uint8* out_buf_mark, size_t* out_size, unsigned flags) {
    (void)out_buf_mark;
    if (!state->live) {
        state->live = 1;
        if (inflateInit2(&state->stream, flags & TINFL_FLAG_PARSE_ZLIB_HEADER ? MAX_WBITS : -MAX_WBITS) != Z_OK) {
            state->live = 0;
            return TINFL_STATUS_FAILED;
        }
    }
    state->stream.next_in = (Bytef*)input;
    state->stream.avail_in = (uInt)*in_size;
    state->stream.next_out = *out_size ? output : &state->scratch;
    state->stream.avail_out = *out_size ? (uInt)*out_size : 1;
    int rc = inflate(&state->stream, Z_FINISH);
    *in_size -= state->stream.avail_in;
    if (*out_size) *out_size -= state->stream.avail_out;
    if (rc == Z_STREAM_END) return tinfl_terminal(state, TINFL_STATUS_DONE);
    if (rc != Z_OK && rc != Z_BUF_ERROR) return tinfl_terminal(state, TINFL_STATUS_FAILED);
    return tinfl_terminal(state, state->stream.avail_out ? TINFL_STATUS_NEEDS_MORE_INPUT : TINFL_STATUS_HAS_MORE_OUTPUT);
}
