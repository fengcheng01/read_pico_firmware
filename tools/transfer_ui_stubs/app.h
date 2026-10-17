/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 * 中文：配网测试依赖入口。/ English: Dependency entry for provisioning tests.
 * 冻结：仅供测试。/ Frozen: Tests only.
 */
#include "transfer_ui_test_env.h"
/// 与设备 app.h 同名的保活入口；测试文件提供计数实现。/ Stay-awake entry matching
/// the device app.h; the test file supplies the counting implementation.
void app_loop_stay_awake(void);