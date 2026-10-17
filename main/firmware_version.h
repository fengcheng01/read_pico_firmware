/* SPDX-License-Identifier: Apache-2.0
 * 中文：产品与诊断共享镜像版本。/ English: Product and diagnostics share image identity.
 */
#pragma once
/// 跨机型的固件名称，独立于设备品牌。/ Firmware name across targets, separate from hardware branding.
static inline const char* firmware_name(void) { return "PageRest"; }
#ifdef ESP_PLATFORM
#include "esp_app_desc.h"
static inline const char* firmware_version(void) { return esp_app_get_description()->version; }
static inline const char* firmware_build_time(void) { return esp_app_get_description()->time; }
#else
static inline const char* firmware_version(void) { return "DESKTOP PREVIEW"; }
static inline const char* firmware_build_time(void) { return "--:--:--"; }
#endif
