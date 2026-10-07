/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 *
 * 阅读进度换算和晃动判定，不访问硬件。/ Reading position and shake policy without hardware access.
 * 冻结：两次上升沿600ms内触发，冷却800ms（按实机响应反馈缩短）。
 * Frozen: two rising edges within 600ms, then 800ms cooldown, shortened after hardware response feedback.
 */
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/// UTF8章内偏移映射回文件进度，避免GBK字节单位混用。/ Map UTF8 chapter offsets to source bytes, including GBK.
static inline uint32_t book_position_bytes(uint32_t start, uint32_t end, size_t off, size_t len) {
    if (end < start || !len) return start;
    if (off > len) off = len;
    return start + (uint64_t)(end - start) * off / len;
}

typedef struct {
    bool high; ///< 上次AOI状态 / Previous AOI state
    bool pending; ///< 等待第二次沿 / Waiting for second edge
    int64_t first_ms; ///< 首次沿时间 / First edge time
    int64_t cooldown_ms; ///< 冷却截止 / Cooldown deadline
} book_shake_gate_t;

/// 屏蔽时清除累计动作，持续高电平不重复计数。/ Suppression resets partial gestures; a held level counts once.
static inline bool book_shake_feed(book_shake_gate_t* g, bool high, bool suppressed, int64_t now) {
    bool rising = high && !g->high;
    g->high = high;
    if (suppressed || now < g->cooldown_ms) {
        g->pending = false;
        return false;
    }
    if (!rising) return false;
    if (g->pending && now - g->first_ms <= 600) {
        g->pending = false;
        g->cooldown_ms = now + 800;
        return true;
    }
    g->pending = true;
    g->first_ms = now;
    return false;
}

/// -1 上页、0 工具条、1 下页；比例布局与预览示意共用。/ -1 previous, 0 tools, 1 next; shared by taps and diagrams.
static inline int book_tap_action(unsigned layout, int x, int y, int width, int height) {
    if (layout == 3) return y < height * 3 / 10 ? -1 : y >= height * 7 / 10 ? 1 : 0;
    if (layout == 0) return x < width * 3 / 10 ? -1 : x >= width * 7 / 10 ? 1 : 0;
    if (x >= width * 3 / 10 && x < width * 7 / 10 && y >= height * 3 / 10 && y < height * 7 / 10) return 0;
    if (layout == 1) return x < width / 4 ? -1 : 1;
    return x >= width * 3 / 4 ? -1 : 1;
}

/// 九宫格分区索引：将 (x,y) 映射到 0..8 分区号，非法坐标返回 -1。
/// Grid zone index: map (x,y) into 0..8 zone, returning -1 for invalid coords.
static inline int book_tap_grid_zone(int x, int y, int width, int height) {
    if (width <= 0 || height <= 0 || x < 0 || y < 0 || x >= width || y >= height) return -1;
    int col = x * 3 / width;
    if (col > 2) col = 2;
    int row = y * 3 / height;
    if (row > 2) row = 2;
    return row * 3 + col;
}

/// 九宫格动作码转操作：1=上一页(-1), 2=下一页(1), 3=菜单(0), 其它=无操作(-2)。
/// Map tap zone action: 1=prev(-1), 2=next(1), 3=menu(0), else=none(-2).
static inline int book_tap_zone_action(uint8_t act) {
    if (act == 1) return -1; // 上一页 / Previous page
    if (act == 2) return 1;  // 下一页 / Next page
    if (act == 3) return 0;  // 菜单 / Menu toolbar
    return -2;               // 无操作 / None
}
