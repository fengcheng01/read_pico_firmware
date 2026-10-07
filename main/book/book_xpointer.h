/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 * 中文：有界XHTML祖先、文本节点与Unicode偏移，兼容KOReader的DocFragment。
 * English: Bounded XHTML ancestry, text nodes and Unicode offsets compatible with KOReader DocFragment paths.
 * 冻结：按用户要求保留长段落内部位置；未知DOM、实体或节点拆分返回失败供上层回退，不猜测精度。
 * Frozen: Preserve intra-paragraph positions as requested; unknown DOMs, entities or node splitting fail for caller fallback, never guessing precision.
 */
#pragma once
#include <stdbool.h>
#include <stddef.h>
/// 解析零基spine索引，拒绝不完整或溢出的路径。/ Parse a zero-based spine index, rejecting incomplete or overflowing paths.
bool book_xpointer_chapter(const char* position, size_t* chapter);
/// 从正文UTF-8字节位置生成真实文本节点路径与Unicode偏移。/ Encode a real text-node path and Unicode offset from a rendered UTF-8 byte position.
bool book_xpointer_encode(const char* html, size_t len, size_t chapter, size_t byte,
                          char* out, size_t cap);
/// 恢复text()[N]的Unicode偏移；旧元素路径仍恢复首个实际文字，异常位置返回false。
/// Restore a Unicode offset within text()[N]; legacy element paths restore first visible text, and invalid positions fail.
bool book_xpointer_decode(const char* html, size_t len, const char* position, size_t* byte);
