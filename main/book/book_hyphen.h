/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 *
 * 中文：英文词内断点候选，供折行挑选可放下的最长前缀。
 * English: English in-word break candidates for the wrapper to pick the longest fitting prefix.
 *
 * 冻结：纯函数、无状态；软连字符优先于模式断点。
 * Frozen: Pure and stateless; soft hyphens outrank pattern breaks.
 */
#pragma once
#include <stddef.h>

/// 词内候选断点的词首字节偏移（在该偏移处断开，前段保留）；输出降序，至多 max 个。
/// 词首两字母与词尾三字母内不返回断点；非 ASCII 词只给软连字符。
/// In-word byte offsets where a break may fall (the prefix keeps everything before
/// it), longest-first, capped at max. No breaks inside the first two or last three
/// letters; non-ASCII words only carry soft-hyphen points.
int book_hyphen_candidates(const char* word, size_t len, size_t* offsets, int max);
