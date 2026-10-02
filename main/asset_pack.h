/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 * 中文：内置资源的定长头与有界 zlib 解压，不管理输出所有权。
 * English: Fixed-header embedded assets and bounded zlib inflation; callers own outputs.
 * 冻结：仅解包构建生成资源，不读取外部文件。/ Frozen: Unpack build-generated assets only; no external files.
 */
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
/// 返回 RPFT 资源的原始长度，头错误时为零。/ Return the raw RPFT length, zero for invalid headers.
size_t asset_pack_size(const uint8_t* data, size_t size);
/// 输出容量必须等于原始长度；验证长度和 zlib 校验，失败时输出不可用。
/// Require the exact raw output capacity; validate length and the zlib checksum; failed outputs are unusable.
bool asset_pack_unpack(const uint8_t* data, size_t size, uint8_t* out, size_t capacity);
