/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 *
 * 应用界面刷新档位。日常用均衡配置；ALL_DU 只做整机 DU 实验。
 *
 * App refresh profile. Daily use is balanced; ALL_DU is a whole-device
 * DU experiment only.
 */

#pragma once

#include "epdiy.h"

// 普通页与标准正文使用GL16；夜间按保存周期清理，日间正文不周期，控件反馈不触发整屏清理。
// Ordinary pages and standard body use GL16; night follows the saved cleanup interval, day body omits it, and control feedback never cleans the whole screen.
#define APP_REFRESH_BALANCED 0
#define APP_REFRESH_ALL_DU 1
#define APP_REFRESH_PROFILE APP_REFRESH_BALANCED

#if APP_REFRESH_PROFILE == APP_REFRESH_ALL_DU
#define APP_PAGE_REFRESH_MODE MODE_DU
#define APP_PAGE_FORCE_FULL 0
#define APP_SETTLE_REFRESH_MODE MODE_DU
#else
#define APP_PAGE_REFRESH_MODE MODE_GL16
// 日常强刷走完整全像素 GC16 深度清理。/ Full redraws use true GC16 cleanup.
#define APP_PAGE_FORCE_FULL 1
// 抬手定稿使用 GL16。/ Settle through GL16.
#define APP_SETTLE_REFRESH_MODE MODE_GL16
#endif

// 通用控件保留灰阶；连续笔迹页面有自己的 DU 出口。/ Generic controls retain grayscale; live ink owns a dedicated DU path.
#define APP_DYNAMIC_REFRESH_MODE MODE_GL16

// 默认周期档位；运行时沿用保存设置。通用页与成功夜间正文翻页可周期GC16，日间正文/控件排除。
// Default interval tier; runtime retains saved settings. Generic pages and successful night body turns may schedule GC16, excluding day body/controls.
#define APP_GC16_EVERY 5
