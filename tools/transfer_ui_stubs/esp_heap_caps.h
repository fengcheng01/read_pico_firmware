/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 * 中文：配网测试依赖入口。/ English: Dependency entry for provisioning tests.
 * 冻结：仅供测试。/ Frozen: Tests only.
 */
#include "transfer_ui_test_env.h"
// 查询函数仅出现在日志实参里；补内联桩避免宿主隐式声明。/ Query calls appear only in log arguments; inline stubs avoid implicit declarations.
static inline size_t heap_caps_get_free_size(int caps) { (void)caps; return 8u * 1024u * 1024u; }
static inline size_t heap_caps_get_largest_free_block(int caps) { (void)caps; return 4u * 1024u * 1024u; }
