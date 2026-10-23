/* SPDX-License-Identifier: Apache-2.0
 * 中文：保留日志参数类型检查。/ English: Retain log argument type checking.
 */
#pragma once
#include <stdio.h>
#define ESP_LOGI(tag,...) do { (void)(tag); if(0) fprintf(stderr,__VA_ARGS__); } while(0)
