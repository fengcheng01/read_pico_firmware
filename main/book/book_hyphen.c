/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 *
 * 中文：英文词内断点（Liang/TeX 模式 + 软连字符），供折行在整词放不下时挑选前缀。
 * English: Intra-word English break points (Liang/TeX patterns plus soft hyphens)
 * for the wrapper to pick a fitting prefix when a whole word cannot stay.
 *
 * 冻结：只读小写 ASCII 词；非 ASCII 词只返回软连字符断点；不改词内容。
 * Frozen: Lowercase ASCII words only; non-ASCII words return soft-hyphen points
 * alone; the word content is never modified.
 */
#include "book_hyphen.h"

#include <stdbool.h>
#include <string.h>

#include "hyphen_en.h"

// 词首/词尾保护：断点前至少两字母、后至少三字母（英文惯例 2/3）。
// Edge guards: at least two letters before and three after a break (the usual 2/3 mins).
#define HYPHEN_LEFT_MIN 2
#define HYPHEN_RIGHT_MIN 3
#define HYPHEN_WORD_MAX 60

static uint32_t hyphen_bucket(char a, char b) {
    if (a == '.') a = '{';
    if (b == '.') b = '{';
    for (unsigned i = 0; i < HYPHEN_EN_BUCKET_COUNT; ++i)
        if (k_hyphen_keys[2 * i] == a && k_hyphen_keys[2 * i + 1] == b) return i;
    return UINT32_MAX;
}

// 词内候选断点：软连字符优先并入；模式奇数值位置即断点；输出按降序。
// Candidate in-word breaks: soft hyphens merge in first; odd pattern values mark
// breaks; output runs longest-first.
int book_hyphen_candidates(const char* word, size_t len, size_t* offsets, int max) {
    if (!word || !len || !offsets || max <= 0) return 0;
    char dotted[HYPHEN_WORD_MAX + 3];
    size_t letter_at[HYPHEN_WORD_MAX];
    size_t sh[16];
    int sh_count = 0;
    unsigned letters = 0;
    bool ascii = true;
    for (size_t i = 0; i < len;) {
        unsigned char c = (unsigned char)word[i];
        if (c == 0xC2 && i + 1 < len && (unsigned char)word[i + 1] == 0xAD) {
            if (sh_count < (int)(sizeof(sh) / sizeof(sh[0]))) sh[sh_count++] = i;
            i += 2;
            continue;
        }
        if (c < 0x80 && letters < HYPHEN_WORD_MAX) {
            letter_at[letters] = i;
            dotted[1 + letters] = (char)c;
            ++letters;
            if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '\'' || c == '-')) ascii = false;
        } else if (c >= 0x80) ascii = false;
        i += c < 0x80 ? 1 : (c < 0xE0 ? 2 : c < 0xF0 ? 3 : 4);
    }
    if (letters < 2) return 0;
    // 点框架：'.' + 词 + '.'，字母 j 位于下标 1+j。/ Dotted frame: '.' + word + '.',
    // with letter j at index 1+j.
    dotted[0] = '.';
    for (unsigned i = 0; i < letters; ++i) {
        char c = dotted[1 + i];
        if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
        dotted[1 + i] = c;
    }
    dotted[1 + letters] = '.';
    unsigned span = letters + 2;

    // 值表：字母间隙的奇偶决定可断；桶化匹配同首两字符的模式。
    // Values decide breakability at letter gaps; bucketed matching scans patterns
    // sharing the first two characters.
    unsigned char values[HYPHEN_WORD_MAX + 3];
    memset(values, 0, sizeof(values));
    if (ascii) {
        for (unsigned start = 0; start + 1 < span; ++start) {
            uint32_t bucket = hyphen_bucket(dotted[start], dotted[start + 1]);
            if (bucket == UINT32_MAX) continue;
            unsigned begin = k_hyphen_bucket_range[2 * bucket], count = k_hyphen_bucket_range[2 * bucket + 1];
            for (unsigned e = 0; e < count; ++e) {
                unsigned index = k_hyphen_flat[begin + e];
                const char* pattern = k_hyphen_pool + k_hyphen_letters[index];
                const uint8_t* digits = k_hyphen_digit_data + k_hyphen_digits[index];
                unsigned match = 0;
                while (pattern[match] && start + match < span && pattern[match] == dotted[start + match]) ++match;
                if (pattern[match]) continue;
                for (unsigned k = 0; k <= match; ++k)
                    if (digits[k] > values[start + k]) values[start + k] = digits[k];
            }
        }
    }

    int count = 0;
    bool taken[HYPHEN_WORD_MAX + 1] = {false};
    // 软连字符优先：断点记“断前字母数”。/ Soft hyphens first, keyed by letters-before-break.
    for (int s = 0; s < sh_count; ++s) {
        unsigned before = 0;
        for (unsigned i = 0; i < letters; ++i) if (letter_at[i] < sh[s]) before = i + 1;
        if (before < HYPHEN_LEFT_MIN || letters - before < HYPHEN_RIGHT_MIN || taken[before]) continue;
        taken[before] = true;
        if (count < max) offsets[count++] = sh[s];
    }
    // 显式连字符后可断（后随字母，两侧各至少一个字符），不受 2/3 最小保护限制。
    // Explicit hyphens break when a letter follows, with at least one character on
    // each side, outside the 2/3 minimum guards.
    for (size_t i = 0; i + 1 < len; ++i) {
        if (word[i] != '-') continue;
        unsigned char next = (unsigned char)word[i + 1];
        if (!((next >= 'a' && next <= 'z') || (next >= 'A' && next <= 'Z'))) continue;
        if (!i || !((unsigned char)word[i - 1] >= '0')) {
            // 前面至少一个非 '-' 字符。/ At least one non-hyphen character before.
            bool any = false;
            for (size_t j = 0; j < i; ++j) if (word[j] != '-') { any = true; break; }
            if (!any) continue;
        }
        size_t offset = i + 1;
        unsigned before = 0;
        for (unsigned li2 = 0; li2 < letters; ++li2) if (letter_at[li2] < offset) before = li2 + 1;
        if (taken[before]) continue;
        taken[before] = true;
        if (count < max) offsets[count++] = offset;
    }
    // 模式断点：字母 i 后可断当 values[i+2] 为奇数（词字母 i 在点框架下标 1+i）。
    // Pattern breaks: letter i may end a segment when values[i+2] is odd (word letter
    // i sits at dotted index 1+i).
    for (int i = (int)letters - HYPHEN_RIGHT_MIN; i >= HYPHEN_LEFT_MIN - 1; --i) {
        if (!(values[i + 2] & 1) || taken[i + 1]) continue;
        taken[i + 1] = true;
        if (count < max) offsets[count++] = letter_at[i] + 1;
    }
    // 降序排列。/ Sort descending.
    for (int i = 1; i < count; ++i) {
        size_t key = offsets[i];
        int j = i - 1;
        while (j >= 0 && offsets[j] < key) { offsets[j + 1] = offsets[j]; --j; }
        offsets[j + 1] = key;
    }
    return count;
}
