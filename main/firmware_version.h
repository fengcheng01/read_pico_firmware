/* SPDX-License-Identifier: Apache-2.0
 * 中文：产品与诊断共享镜像版本。/ English: Product and diagnostics share image identity.
 */
#pragma once
#ifdef ESP_PLATFORM
#include "esp_app_desc.h"
static inline const char* firmware_version(void) { return esp_app_get_description()->version; }
static inline const char* firmware_build_time(void) { return esp_app_get_description()->time; }
#else
static inline const char* firmware_version(void) { return "DESKTOP PREVIEW"; }
static inline const char* firmware_build_time(void) { return "--:--:--"; }
#endif
