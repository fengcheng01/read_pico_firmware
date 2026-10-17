/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 * 中文：书架跳转请求替身。/ English: Library-navigation request stand-in.
 * 冻结：仅供测试。/ Frozen: Tests only.
 */
#pragma once
#include "transfer_ui_test_env.h"
typedef enum { BOOK_ENTRY_SHELF, BOOK_ENTRY_OPEN } book_entry_kind_t;
static bool test_shelf_requested;
static inline bool book_entry_request(book_entry_kind_t kind, const char* path) {
    (void)path;
    test_shelf_requested = kind == BOOK_ENTRY_SHELF;
    return test_shelf_requested;
}
