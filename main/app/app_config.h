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

// 日常整页用 GL16，白底残影靠 APP_GC16_EVERY 周期 GC16 压掉。
// KEY2、铺白、睡眠画面这类「清屏重来」仍直接走 GC16。
// Daily pages use GL16; APP_GC16_EVERY periodically GC16-cleans the gray floor.
// KEY2, wipe-to-white and sleep faces still go GC16.
#define APP_REFRESH_BALANCED 0
#define APP_REFRESH_ALL_DU 1
#define APP_REFRESH_PROFILE APP_REFRESH_BALANCED

#if APP_REFRESH_PROFILE == APP_REFRESH_ALL_DU
#define APP_PAGE_REFRESH_MODE MODE_DU
#define APP_PAGE_FORCE_FULL 0
#define APP_SETTLE_REFRESH_MODE MODE_DU
#else
#define APP_PAGE_REFRESH_MODE MODE_GL16
// APP_REDRAW_FULL（KEY2 清残影、首帧、诊断 enter_full）仍是整屏 GC16。
// APP_REDRAW_FULL (KEY2, first frame, diagnostic enter_full) stays GC16.
#define APP_PAGE_FORCE_FULL 1
// 触摸抬手定稿：笔迹连续 DU 之后白底已脏，顺手用 GC16 清一次。
// Touch settle: continuous DU dirties the white; GC16 once on lift.
#define APP_SETTLE_REFRESH_MODE MODE_GC16
#endif

// 手指、传感器等连续变化过程始终优先响应速度。/ Motion always prefers DU.
#define APP_DYNAMIC_REFRESH_MODE MODE_DU

// 差分刷（DU/GL16）连续多少次后，下一次自动升为全像素 GC16 压掉累积的灰底。
// 跟随 DU（E0470_FOLLOW_WAVEFORM）不计数也不升级。0 = 关闭。
// 运行期档位在设置 gc_every（默认取本值）；本宏仅作出厂默认与文档。
// After this many DU/GL16 updates, the next one is a full-pixel GC16.
// FOLLOW DU does not count. 0 disables the upgrade. The runtime tier lives in
// the gc_every setting (defaulting to this value); this macro is only the
// factory default and documentation.
#define APP_GC16_EVERY 5
