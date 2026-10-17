/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 *
 * 中文：启动保护与开书事务的固定 NVS 记录；中断开书只屏蔽自动重试，不动阅读记录。
 * English: Fixed NVS record for startup protection and book-open transactions; an interrupted open blocks automatic retries only, never reading records.
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/// 在 NVS 初始化后、硬件初始化前调用；异常或未完成启动启用本次启动保护。/ Call after NVS init and before hardware; abnormal or incomplete startup enables recovery for this boot.
void pico_boot_init(bool abnormal_reset);
/// 首个完整界面输出成功后标记启动完成。/ Mark startup complete after a successful first full presentation.
void pico_boot_ready(void);
bool pico_boot_recovery(void);
/// 自动续读不得重提上次中断的图书；显式开书仍允许。/ Automatic resume prompts must skip a book interrupted mid-open; explicit opening remains allowed.
bool pico_boot_asset_allowed(const char* path);
/// 开书解析前先持久化，失败不得进入解析；结束清除事务。/ Persist before parsing; failure prevents admission. The end call clears the transaction.
esp_err_t pico_boot_book_begin(const char* path);
void pico_boot_book_end(bool success);
/// 仅导出本次启动发现的中断图书；不把一般异常归因给最后阅读的书。/ Export only a book interrupted on this boot, never blame the last read for a generic reset.
bool pico_boot_interrupted_book(char* path, size_t capacity);
void pico_boot_forget_interrupted_book(void);

#ifdef __cplusplus
}
#endif
