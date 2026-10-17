/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 * 中文：翻页器偏好、连接恢复与输入闸门，UI 任务独占。
 * English: Page-turner preferences, reconnect policy and input gate, exclusively owned by the UI task.
 * 冻结修订：默认关闭；设置/图书页消费输入，导航保留已连链路；网络/锁屏/关闭仍停止，不合成按住连翻。
 * Frozen revision: Default off; consume input in Settings/Books and retain connected links across navigation, stopping for network/lock/off and never synthesising held repeats.
 * 冻结修订：替换保存设备保留学习按键；错误相间隔后自动重连；启动内存不足先回收一次可重建缓存。
 * Frozen revision: Replacement saves keep learned keys; the error phase auto-reconnects after the interval; short start headroom reclaims rebuildable caches once first.
 */
#pragma once
#include "os_ble_radio.h"
enum { OS_PAGE_TURNER_LOCK = 2 };
typedef struct {
    bool enabled, reversed, saved;
    os_ble_snapshot_t radio;
    os_ble_peer_t selected;
    char note[128], test[96];
    int learning; ///< 0 不学习，-1/1 等待上一页/下一页，2锁屏 / Idle or learning previous/next,2lock
    bool bound_prev, bound_next, bound_lock;
} os_page_turner_info_t;
/// 生命周期及暂停必须由 UI 任务串行调用，暂停失败时调用方不应启动网络/睡眠。
/// UI serialises lifecycle and pause; callers must not start networking/sleep after a failed pause.
void os_page_turner_enter(void);
bool os_page_turner_pause(void);
void os_page_turner_leave(void);
/// 开机装配注册一次：启动内存不足时先释放图片/封面/字体等可重建缓存再重试判断。
/// Register once at boot: short start headroom releases rebuildable image/cover/font caches before rechecking.
void os_page_turner_set_reclaimer(void (*reclaim)(void));
/// 消费最多一个翻页或锁屏动作，队列过期/叠层丢弃；enabled=false 不启动栈。/ Consume at most one fresh turn or lock action; discard expired/overlay input, never starting while disabled.
int os_page_turner_poll(int64_t now_ms, bool body, bool wanted);
const os_page_turner_info_t* os_page_turner_info(void);
bool os_page_turner_enable(bool enabled);
bool os_page_turner_reverse(void);
bool os_page_turner_scan(void);
bool os_page_turner_connect(const os_ble_peer_t* peer);
bool os_page_turner_disconnect(void);
bool os_page_turner_forget(void);
void os_page_turner_learn(int direction);
bool os_page_turner_restore_mapping(void);
