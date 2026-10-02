/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 * 中文：USB 卡盘的一次性重启请求；只在解锁后的专用开机路径运行。
 * English: One-shot USB card-disk reboot request; runs only in the unlocked dedicated boot path.
 * 冻结：电脑独占 TF 卡，不导出内置存储，不自动格式化；普通启动保留 USB Serial/JTAG。
 * Frozen: The computer owns the TF card, never exports internal storage or auto-formats; normal boot retains USB Serial/JTAG.
 */
#pragma once
#include <stdbool.h>
#include "esp_err.h"
/// 保存一次性请求并重启；失败不重启。/ Persist a one-shot request and reboot; do not reboot on failure.
esp_err_t os_usb_disk_request(void);
/// 开机在探卡前读取并清除请求；清除失败不进入 USB 模式。/ Consume before SD probing; clearing failure refuses USB mode.
bool os_usb_disk_take_request(void);
