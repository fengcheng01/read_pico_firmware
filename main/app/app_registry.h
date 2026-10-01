/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 *
 * 产品与诊断的唯一页面目录；菜单索引与稳定产品 ID 分开。
 *
 * Single product/diagnostic catalog; menu indices and stable product IDs are separate.
 */

#pragma once

#include "app.h"
#include "../os/os_catalog.h"

#ifdef __cplusplus
extern "C" {
#endif

int app_count(void);
/// 全局导航只显示目录头部的四个产品根入口，索引仍为真实注册索引。/ Global navigation shows the four leading product roots, retaining registry indices.
int app_product_count(void);
/// 诊断目录排除产品根页和设置快捷入口。/ Diagnostic catalog excludes product roots and settings shortcuts.
int app_diagnostic_count(void);
const app_desc_t* app_diagnostic_at(int index);
const app_desc_t* app_at(int index);
/// 找不到时返回 -1。/ Returns -1 when not found.
int app_index_of(const app_desc_t* app);
/// 按稳定产品 ID 查找，未实现入口返回 NULL。/ Resolve a stable product ID; unimplemented entries return NULL.
const app_desc_t* app_by_id(os_app_id_t id);

/// 开机默认页与设备功能自检页，供 app_main 与首页引用。/ Boot default page and device self-test page, for app_main and the home page.
const app_desc_t* app_home_page(void);
const app_desc_t* app_selftest_page(void);

#ifdef __cplusplus
}
#endif
