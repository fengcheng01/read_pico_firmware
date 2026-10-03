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

// 日常整页先擦白再 GL16；局部更新到期时同样清理整屏。
// 用户要求日常页面、锁屏及清理均避免压黑；诊断波形仍可显式 GC16。
// Daily pages erase white before GL16; accumulated area updates use the same panel cleanup.
// User-requested daily pages, lock and cleanup avoid inversion; diagnostic waveforms may explicitly use GC16.
#define APP_REFRESH_BALANCED 0
#define APP_REFRESH_ALL_DU 1
#define APP_REFRESH_PROFILE APP_REFRESH_BALANCED

#if APP_REFRESH_PROFILE == APP_REFRESH_ALL_DU
#define APP_PAGE_REFRESH_MODE MODE_DU
#define APP_PAGE_FORCE_FULL 0
#define APP_SETTLE_REFRESH_MODE MODE_DU
#else
#define APP_PAGE_REFRESH_MODE MODE_GL16
// 日常强刷也走无压黑清理。/ Daily forced redraws also use white cleanup.
#define APP_PAGE_FORCE_FULL 0
// 抬手定稿使用无压黑清理。/ Settle through white cleanup without inversion.
#define APP_SETTLE_REFRESH_MODE MODE_GL16
#endif

// 通用控件保留灰阶；连续笔迹页面有自己的 DU 出口。/ Generic controls retain grayscale; live ink owns a dedicated DU path.
#define APP_DYNAMIC_REFRESH_MODE MODE_GL16

// 局部更新累计到期后擦白再绘整页，不做自动压黑；普通整页每次均擦白再绘。
// Accumulated partial updates erase white then repaint the panel; full pages always use that path without automatic inversion.
#define APP_GC16_EVERY 5
