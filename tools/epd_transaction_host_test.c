/* SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 * 中文：真实 epdiy 参考帧提交和行队列分配回归，仅替换硬件边界。
 * English: Test production epdiy baseline commits and queue allocations with hardware seams only.
 */
#include "epd_highlevel.h"
#include "line_queue.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static bool fail_draw;
static bool require_diagonals, require_selectors;
static const uint8_t* expected_selective;
static unsigned draw_calls;
static size_t table_bytes;
static const EpdWaveform waveform = {0};
static const EpdDisplay_t display = {.width = 64, .height = 4, .default_waveform = &waveform};
void* record_calloc(size_t n, size_t size) { table_bytes = n * size; return calloc(n, size); }
int epd_width(void) { return 64; }
int epd_height(void) { return 4; }
const EpdDisplay_t* epd_get_display(void) { return &display; }
enum EpdRotation epd_get_rotation(void) { return EPD_ROT_LANDSCAPE; }
EpdRect epd_full_screen(void) { return (EpdRect){0, 0, 64, 4}; }
void epd_clear(void) {}
void epd_leading_skip_discard(void) {}
void epd_leading_skip_set_present(const uint8_t* data, const uint8_t present[256]) {
    uint8_t actual[256] = {0};
    for (int i = 0; i < 256; ++i) actual[data[i]] = 1;
    assert(!memcmp(actual, present, sizeof(actual)));
}
void epd_difference_column_range(EpdRect area, int* first, int* end) {
    *first = area.x & ~31; *end = (area.x + area.width + 31) & ~31;
}
EpdRect epd_difference_image_cropped(const uint8_t* to, const uint8_t* from, EpdRect area,
                                    uint8_t* diff, bool* lines, uint8_t* columns) {
    int first, end; epd_difference_column_range(area, &first, &end);
    memset(lines, 0, 4 * sizeof(*lines)); memset(columns, 0, 32);
    bool changed = false;
    for (int y = area.y; y < area.y + area.height; ++y) {
        for (int x = first; x < end; ++x) {
            size_t pos = (size_t)y * 64 + x; int shift = (x & 1) * 4;
            unsigned a = (to[pos / 2] >> shift) & 15, b = (from[pos / 2] >> shift) & 15;
            diff[pos] = (uint8_t)((a << 4) | b);
            if (a != b) { changed = lines[y] = true; columns[x / 2] = 255; }
        }
    }
    return changed ? area : (EpdRect){0};
}
enum EpdDrawError epd_draw_base(EpdRect area, const uint8_t* data, EpdRect crop,
                               enum EpdDrawMode mode, int temp, const bool* lines,
                               const uint8_t* columns, const EpdWaveform* wave) {
    (void)area; (void)crop; (void)mode; (void)temp; (void)wave;
    draw_calls++;
    if (require_diagonals) {
        for (int y = 0; y < 4; ++y) assert(lines[y]);
        for (int x = 0; x < 32; ++x) assert(columns[x] == 255);
        for (int p = 0; p < 256; ++p) {
            assert((data[p] >> 4) == (data[p] & 15));
            assert((data[p] & 15) == (p / 2 % 16));
        }
    }
    if (expected_selective) assert(!memcmp(data, expected_selective, 256));
    if (require_selectors) {
        for (int p = 0; p < 256; ++p) {
            unsigned original = (p % 16) | (p / 16 << 4);
            unsigned expected = p % 16 == p / 16 ? (p == 255 ? 255 : 0xee) : original;
            assert(data[p] == expected);
        }
    }
    return fail_draw ? EPD_DRAW_EMPTY_LINE_QUEUE : EPD_DRAW_SUCCESS;
}
int main(void) {
    uint8_t front[128], back[128], diff[256], cols[32]; bool lines[4];
    EpdiyHighlevelState hl = {.front_fb = front, .back_fb = back, .difference_fb = diff,
                             .dirty_lines = lines, .dirty_columns = cols, .waveform = &waveform};
    for (int full = 0; full < 2; ++full) {
        memset(front, 0x33, sizeof(front)); memset(back, 0xff, sizeof(back));
        fail_draw = true;
        enum EpdDrawError err = full ? epd_hl_update_screen_full(&hl, MODE_GC16, 25)
                                    : epd_hl_update_screen(&hl, MODE_GL16, 25);
        assert(err == EPD_DRAW_EMPTY_LINE_QUEUE);
        for (size_t i = 0; i < sizeof(back); ++i) assert(back[i] == 255);
        fail_draw = false;
        err = full ? epd_hl_update_screen_full(&hl, MODE_GC16, 25)
                   : epd_hl_update_screen(&hl, MODE_GL16, 25);
        assert(err == EPD_DRAW_SUCCESS && !memcmp(front, back, sizeof(front)));
    }
    // 完全相同的灰阶帧也必须送出真实对角线和全像素掩码，白底/灰字定稿不能被差分裁掉。
    // Identical gray frames must still carry real diagonals and full-pixel masks so diff cropping cannot skip white/gray settling.
    for (int i = 0; i < 128; ++i) front[i] = back[i] = (uint8_t)((i % 16) * 17);
    unsigned before = draw_calls;
    require_diagonals = true;
    assert(epd_hl_update_screen_full(&hl, MODE_GL16, 25) == EPD_DRAW_SUCCESS);
    assert(draw_calls == before + 1 && !memcmp(front, back, sizeof(front)));
    require_diagonals = false;
    // 局推失败不回写；成功只回写实际驱动的列段与行。/ Failed partials do not commit; successful partials commit only driven columns/rows.
    memset(front, 0x55, sizeof(front)); memset(back, 0xff, sizeof(back)); fail_draw = true;
    assert(epd_hl_update_area_full(&hl, MODE_GL16, 25, (EpdRect){2,1,10,1}) != EPD_DRAW_SUCCESS);
    for (size_t i = 0; i < sizeof(back); ++i) assert(back[i] == 255);
    fail_draw = false;
    assert(epd_hl_update_area(&hl, MODE_GL16, 25, (EpdRect){2,1,10,1}) == EPD_DRAW_SUCCESS);
    for (size_t i = 0; i < sizeof(back); ++i) assert(back[i] == (i >= 32 && i < 48 ? 0x55 : 0xff));

    // 所有256迁移仍真实，只有对角线改为动作码；失败不提交，成功提交实际灰阶。
    // All 256 transitions stay real except diagonal action selectors; failure retains baseline and success commits actual grayscale.
    uint8_t selected[32] = {0}; selected[31] = 128;
    for (int p = 0; p < 256; ++p) {
        front[p / 2] = (p & 1) ? front[p / 2] | (p / 16 << 4) : p / 16;
        back[p / 2] = (p & 1) ? back[p / 2] | (p % 16 << 4) : p % 16;
    }
    uint8_t prior[128]; memcpy(prior, back, sizeof(prior));
    require_selectors = fail_draw = true;
    assert(epd_hl_update_screen_selective(&hl, MODE_GL16, 25, selected) != EPD_DRAW_SUCCESS);
    assert(!memcmp(back, prior, sizeof(back)));
    fail_draw = false;
    assert(epd_hl_update_screen_selective(&hl, MODE_GL16, 25, selected) == EPD_DRAW_SUCCESS);
    assert(!memcmp(front, back, sizeof(front)));
    require_selectors = false;
    // 无选择图时，白白也保持EE；恢复接口仍保留原始对角线。
    // Without selectors held-white stays EE; recovery APIs still retain original diagonals.
    memset(front, 255, sizeof(front)); memset(back, 255, sizeof(back));
    assert(epd_hl_update_screen_selective(&hl, MODE_GL16, 25, NULL) == EPD_DRAW_SUCCESS);
    for (int p = 0; p < 256; ++p) assert(diff[p] == 0xee);

    // 相同灰阶组也保持；改变一个半字节必须仍输出真实迁移，失败不能提交。
    // Identical gray groups also hold; one changed nibble must retain its real transition and failed draws cannot commit.
    for (int byte = 0; byte < 256; ++byte) {
        memset(front, byte, sizeof(front)); memset(back, byte, sizeof(back));
        assert(epd_hl_update_screen_selective(&hl, MODE_GL16, 25, NULL) == EPD_DRAW_SUCCESS);
        for (int p = 0; p < 256; ++p) assert(diff[p] == 0xee);
        front[47] ^= 1; fail_draw = true;
        assert(epd_hl_update_screen_selective(&hl, MODE_GL16, 25, NULL) != EPD_DRAW_SUCCESS);
        for (int p = 0; p < 256; ++p) assert(diff[p] == (p == 94 ? ((byte ^ 1) & 15) << 4 | (byte & 15) : 0xee));
        for (int b = 0; b < 128; ++b) assert(back[b] == byte);
        fail_draw = false;
    }

    uint8_t expected[256]; expected_selective = expected;
    for (int mask_byte = 0; mask_byte < 256; ++mask_byte) {
        memset(front, 255, sizeof(front)); memset(back, 255, sizeof(back));
        memset(selected, mask_byte, sizeof(selected));
        for (int p=0;p<256;++p) expected[p] = mask_byte & (1u << (p % 8)) ? 255 : 0xee;
        assert(epd_hl_update_screen_selective(&hl, MODE_GL16, 25, selected) == EPD_DRAW_SUCCESS);
    }
    uint32_t rng = 77;
    for (int trial=0;trial<32;++trial) {
        for (int b=0;b<128;++b) {
            rng=rng*1664525u+1013904223u; front[b]=rng>>24;
            rng=rng*1664525u+1013904223u; back[b]=rng>>24;
        }
        for (int b=0;b<32;++b) {rng=rng*1664525u+1013904223u;selected[b]=rng>>24;}
        for (int p=0;p<256;++p) {
            unsigned to=front[p/2]>>((p%2)*4)&15,from=back[p/2]>>((p%2)*4)&15;
            expected[p]=to==from?(to==15&&(selected[p/8]&(1u<<(p%8)))?255:0xee):to<<4|from;
        }
        assert(epd_hl_update_screen_selective(&hl, MODE_GL16, 25, selected) == EPD_DRAW_SUCCESS);
        assert(!memcmp(front,back,sizeof(front)));
    }
    expected_selective = NULL;

    LineQueue_t queue = lq_init(64, 304);
    assert(table_bytes == 64 * sizeof(uint8_t*));
    uint8_t row[304];
    // 满队列、环回和逐行内容必须保持。/ Preserve full-queue behavior, wrapping and each row's contents.
    for (int cycle = 0; cycle < 3; ++cycle) {
        for (int i = 0; i < 63; ++i) {
            uint8_t* p = lq_current(&queue); assert(p && !((uintptr_t)p & 15));
            memset(p, i, 304); lq_commit(&queue);
        }
        assert(!lq_current(&queue));
        for (int i = 0; i < 63; ++i) {
            assert(lq_read(&queue, row) == 0);
            for (int j = 0; j < 304; ++j) assert(row[j] == i);
        }
        assert(lq_read(&queue, row) == -1);
    }
    lq_free(&queue);
    puts("epdiy: failed full/partial baseline retention, successful crop commits and pointer-sized queue wrap passed");
}
