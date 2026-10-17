/* SPDX-FileCopyrightText: 2026 mindreset
 * 中文：确定性生成封面的主机回归：同路径逐字节复现、异路径不同、三版式可达、空标识拒绝。
 * English: Host regression for deterministic generated covers: byte-identical repeats per path,
 * distinct across paths, all three layouts reachable, empty identities rejected.
 * 冻结：仅供主机验证。/ Frozen: Host validation only.
 */
#include "book_cover_auto.h"
#include "book_cover.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static unsigned band_mean(const uint8_t* gray, int y0, int y1) {
    unsigned long sum = 0;
    for (int y = y0; y < y1; ++y)
        for (int x = 0; x < BOOK_COVER_W; ++x) sum += gray[(size_t)y * BOOK_COVER_W + x];
    return (unsigned)(sum / (size_t)((y1 - y0) * BOOK_COVER_W));
}

int main(void) {
    assert(!book_cover_auto_alloc(NULL));
    assert(!book_cover_auto_alloc(""));
    uint8_t* a = book_cover_auto_alloc("/sdcard/books/河边记事.txt");
    uint8_t* b = book_cover_auto_alloc("/sdcard/books/河边记事.txt");
    assert(a && b && !memcmp(a, b, (size_t)BOOK_COVER_W * BOOK_COVER_H));
    free(b);
    uint8_t* c = book_cover_auto_alloc("/sdcard/books/城市漫步.epub");
    assert(c && memcmp(a, c, (size_t)BOOK_COVER_W * BOOK_COVER_H));
    free(a); free(c);
    // 三版式签名：深底、近白底、上深下浅分割。/ Three layout signatures: dark ground,
    // near-white ground, and a dark-over-light split.
    bool dark = false, light = false, split = false;
    for (int i = 0; i < 96 && !(dark && light && split); ++i) {
        char id[64];
        snprintf(id, sizeof(id), "/sdcard/books/书%03d.txt", i);
        uint8_t* gray = book_cover_auto_alloc(id);
        assert(gray);
        unsigned top = band_mean(gray, 0, 60), bottom = band_mean(gray, 120, BOOK_COVER_H);
        unsigned all = (top + bottom) / 2;
        if (all < 0x70) dark = true;
        else if (top > 0xc0 && bottom > 0xc0) light = true;
        else if (top + 0x30 < bottom) split = true;
        free(gray);
    }
    assert(dark && light && split);
    puts("book cover auto host tests passed");
    return 0;
}
