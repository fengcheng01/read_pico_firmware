/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 * 中文：产品入口的稳定标识；不持久化菜单序号或页面指针。
 * English: Stable product entry identifiers; never persist menu indices or page pointers.
 */
#pragma once

typedef enum {
    OS_APP_NONE = 0, ///< 非产品入口 / Not a product entry
    OS_APP_HOME = 1, ///< 正在读 / Now reading
    OS_APP_LIBRARY = 2, ///< 书架 / Library
    OS_APP_TRANSFER = 3, ///< 传书 / Transfer
    OS_APP_FONTS = 4, ///< 字库 / Fonts
    OS_APP_DIAGNOSTICS = 5, ///< 硬件概览 / Hardware overview
    OS_APP_TODAY = 6, ///< 今日 / Today
    OS_APP_SETTINGS = 7, ///< 设置 / Settings
    OS_APP_TOOLS = 8, ///< 诊断目录 / Diagnostic catalog
    OS_APP_SLEEP = 9, ///< 睡眠设置 / Sleep settings
    OS_APP_STORAGE = 10, ///< 存储状态 / Storage status
    OS_APP_TIME = 11, ///< 时间与时区 / Time and timezone
    OS_APP_READING = 12, ///< 阅读设置 / Reading settings
    OS_APP_PIN = 13, ///< 锁屏密码 / Lock PIN
} os_app_id_t;
