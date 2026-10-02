/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 * 中文：USB 卡盘的一次性重启请求；只在解锁后的专用开机路径运行。
 * English: One-shot USB card-disk reboot request; runs only in the unlocked dedicated boot path.
 * 冻结：电脑独占 TF 卡，不导出内置存储，不自动格式化；普通启动保留 USB Serial/JTAG。
 * Frozen: The computer owns the TF card, never exports internal storage or auto-formats; normal boot retains USB Serial/JTAG.
 */
#pragma once
#include "sdmmc_cmd.h"
/// 专用 USB 启动时独占裸卡，不挂 FAT；本次启动不得再探卡或格式化。/ Claim raw media on dedicated USB boot without FAT; refuse later probes/formatting.
esp_err_t read_pico_sd_open_raw(sdmmc_card_t** out);
/// USB 请求已收齐后关闭裸卡。/ Close raw media only after USB requests have drained.
void read_pico_sd_close_raw(sdmmc_card_t* raw);
