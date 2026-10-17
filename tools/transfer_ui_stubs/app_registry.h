/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 * 中文：配网测试依赖入口。/ English: Dependency entry for provisioning tests.
 * 冻结：仅供测试。/ Frozen: Tests only.
 */
#pragma once
#include "transfer_ui_test_env.h"
#include "os_catalog.h"
static const app_desc_t test_library_app = {0};
static inline const app_desc_t* app_by_id(os_app_id_t id) { return id == OS_APP_LIBRARY ? &test_library_app : NULL; }
