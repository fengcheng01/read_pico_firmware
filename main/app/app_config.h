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

// 普通页与正文 GL16，周期/手动 GC16，按钮反馈不触发整屏清理。
// Ordinary pages/body use GL16; GC16 is scheduled/manual and control feedback never cleans the whole screen.
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

// 非跟手更新累计到期时使用整屏 GC16。/ Accumulated non-tracking updates trigger full-screen GC16.
#define APP_GC16_EVERY 5
