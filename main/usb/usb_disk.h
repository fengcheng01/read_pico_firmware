/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 * 中文：USB 卡盘的一次性重启请求；只在解锁后的专用开机路径运行。
 * English: One-shot USB card-disk reboot request; runs only in the unlocked dedicated boot path.
 * 冻结：电脑独占 TF 卡，不导出内置存储，不自动格式化；普通启动保留 USB Serial/JTAG。
 * Frozen: The computer owns the TF card, never exports internal storage or auto-formats; normal boot retains USB Serial/JTAG.
 */
#pragma once
#include "epd_highlevel.h"
#include "cst836u.h"
/// 仅全新、未探卡的专用开机调用；退出重启回阅读。/ Call only on a fresh non-probing boot; exit reboots to reading.
void usb_disk_run(EpdiyHighlevelState* hl, uint8_t* fb, cst836u_handle_t tp);

/// 正常启动/退出时把内部 PHY 交还刷机串口。/ Return the internal PHY to the flash serial port on normal boot/exit.
void usb_disk_restore_serial(void);
