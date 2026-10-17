/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 * 中文：翻页器产品状态机。无线只负责 HID，偏好落盘后才发布，失败保留旧值。
 * English: Page-turner product state machine. Radio owns HID; publish preferences only after persistence succeeds.
 * 冻结修订：导航保留已连接链路，用户要求设置到阅读衔接；网络/锁屏/关闭时仍停止，退出清队列。
 * Frozen revision: Preserve established links across navigation for settings-to-reading continuity; network/lock/off still stop radio and exits clear input.
 * 冻结修订：用户增加独立锁屏学习动作，仅正文返回请求；方向反转不改变锁屏，旧双键偏好原样迁移且不默认分配锁屏键。
 * Frozen revision: Add a learned independent lock action returned only in body text; reversal never affects it, and migrate old two-key preferences without assigning a default lock key.
 * 冻结修订：真机反馈学习按键"突然未映射"与失联后无响应；替换保存设备不再清空学习按键（地址轮换保映射），绑定按帧内容而非通道号匹配，链路错误相在间隔后自动重连，启动内存不足先请求一次可重建缓存回收。
 * Frozen revision: Hardware lost learned keys and went unresponsive after link errors; replacing the saved peer never clears learned keys (address rotation keeps mappings), bindings match frame content rather than channel index, the error phase auto-reconnects after the interval, and short start headroom requests one rebuildable-cache reclaim first.
 */
#include "os_page_turner.h"
#include "nvs.h"
#include <stdio.h>
#include <string.h>

typedef struct { uint8_t channel, len, bytes[BLE_HID_REPORT_BYTES]; } binding_t;
typedef struct {
    uint32_t magic;
    uint8_t enabled, reversed, saved, reserved;
    os_ble_peer_t peer;
    binding_t binding[2];
} legacy_preferences_t;
typedef struct {
    uint32_t magic;
    uint8_t enabled, reversed, saved, reserved;
    os_ble_peer_t peer;
    binding_t binding[3];
} preferences_t;
static preferences_t s_prefs;
static os_page_turner_info_t s_info;
static void (*s_reclaim)(void);
static bool s_loaded, s_active, s_running, s_attempted, s_manual_disconnect, s_pending_forget;
static int64_t s_retry_at, s_last_turn, s_save_retry_at, s_learn_until, s_health_at;
static os_ble_phase_t s_previous_phase;
static uint32_t s_forget_revision;
static uint8_t s_tries;
static struct { os_ble_input_t frame; bool down; } s_last[8];

static void note(const char* text) { snprintf(s_info.note, sizeof(s_info.note), "%s", text); }
static void publish(void) {
    s_info.enabled = s_prefs.enabled; s_info.reversed = s_prefs.reversed;
    s_info.saved = s_prefs.saved; s_info.selected = s_prefs.peer;
    s_info.bound_prev = s_prefs.binding[0].len != 0; s_info.bound_next = s_prefs.binding[1].len != 0;
    s_info.bound_lock = s_prefs.binding[2].len != 0;
}
static void load(void) {
    if (s_loaded) return;
    s_loaded = true; s_prefs.magic = 0x32545052;
    nvs_handle_t h;
    if (nvs_open("rp_turner", NVS_READONLY, &h) == ESP_OK) {
        preferences_t p = {0}; size_t n = sizeof(p);
        if (nvs_get_blob(h, "prefs", &p, &n) == ESP_OK) {
            bool legacy = n == sizeof(legacy_preferences_t) && p.magic == 0x31545052;
            if (legacy) { memset(&p.binding[2], 0, sizeof(p.binding[2])); p.magic = s_prefs.magic; }
            bool valid = (legacy || n == sizeof(p)) && p.magic == s_prefs.magic &&
                p.enabled <= 1 && p.reversed <= 1 && p.saved <= 1 && p.peer.type <= 3 &&
                memchr(p.peer.name, 0, sizeof(p.peer.name));
            for (unsigned i = 0; i < 3; ++i)
                valid = valid && p.binding[i].len <= BLE_HID_REPORT_BYTES && p.binding[i].channel < 8;
            if (valid) s_prefs = p;
        }
        nvs_get_u8(h, "tries", &s_tries); nvs_close(h);
    }
    publish(); os_ble_radio_snapshot(&s_info.radio);
}
static bool save(const preferences_t* prefs) {
    nvs_handle_t h;
    esp_err_t rc = nvs_open("rp_turner", NVS_READWRITE, &h);
    if (rc == ESP_OK) {
        rc = nvs_set_blob(h, "prefs", prefs, sizeof(*prefs));
        if (rc == ESP_OK) rc = nvs_commit(h);
        nvs_close(h);
    }
    if (rc != ESP_OK) { note("保存失败，原设置保留，请重试"); return false; }
    s_prefs = *prefs; publish(); return true;
}
static bool tries_store(uint8_t tries) {
    nvs_handle_t h; esp_err_t rc = nvs_open("rp_turner", NVS_READWRITE, &h);
    if (rc == ESP_OK) {
        rc = nvs_set_u8(h, "tries", tries); if (rc == ESP_OK) rc = nvs_commit(h); nvs_close(h);
    }
    if (rc == ESP_OK) s_tries = tries;
    return rc == ESP_OK;
}
const os_page_turner_info_t* os_page_turner_info(void) { load(); return &s_info; }
void os_page_turner_set_reclaimer(void (*reclaim)(void)) { s_reclaim = reclaim; }
void os_page_turner_enter(void) {
    load(); s_active = true; s_attempted = s_manual_disconnect = false;
    s_retry_at = 0; s_info.learning = 0; s_info.test[0] = 0;
    s_save_retry_at = 0;
    s_previous_phase = OS_BLE_OFF;
    memset(s_last, 0, sizeof(s_last)); os_ble_radio_clear();
}
bool os_page_turner_pause(void) {
    load();
    if (os_ble_radio_stop() != ESP_OK) {
        os_ble_radio_snapshot(&s_info.radio);
        note(s_info.radio.error[0] ? s_info.radio.error : "蓝牙尚未停止，请关闭蓝牙后重试");
        return false;
    }
    // 主动停机是健康退出，不累计为启动崩溃。/ An intentional clean stop is not a failed boot.
    if (s_running && s_tries) tries_store(0);
    s_running = false; s_attempted = false; s_info.learning = 0;
    if (s_pending_forget) { s_pending_forget = false; note("遗忘尚未确认完成，请重试"); }
    s_info.test[0] = 0;
    s_retry_at = 0; s_last_turn = 0;
    memset(s_last, 0, sizeof(s_last)); os_ble_radio_clear(); os_ble_radio_snapshot(&s_info.radio);
    return true;
}
void os_page_turner_leave(void) {
    os_ble_radio_snapshot(&s_info.radio);
    // 页面导航只停按键消费，已连接链路交给阅读继续使用；未完成的搜索仍收尾。
    // Navigation stops input consumption but preserves an established link for reading; unfinished searches still stop.
    if (!s_running || s_info.radio.phase != OS_BLE_READY) os_page_turner_pause();
    s_info.learning = 0; memset(s_last, 0, sizeof(s_last)); os_ble_radio_clear(); s_active = false;
}
bool os_page_turner_enable(bool enabled) {
    load();
    if (!enabled && !os_page_turner_pause()) return false;
    preferences_t p = s_prefs; p.enabled = enabled;
    if (!save(&p)) return false;
    tries_store(0); s_attempted = false; s_retry_at = 0; s_manual_disconnect = false;
    note(enabled ? "已开启，请搜索并选择你的翻页器" : "蓝牙已关闭"); return true;
}
bool os_page_turner_reverse(void) {
    load(); preferences_t p = s_prefs; p.reversed = !p.reversed;
    if (!save(&p)) return false;
    os_ble_radio_clear(); memset(s_last, 0, sizeof(s_last));
    note(p.reversed ? "翻页方向已反转" : "已恢复正常方向"); return true;
}
static bool result(esp_err_t rc, const char* success) {
    if (rc != ESP_OK) { note("蓝牙忙或未就绪，请稍后重试"); return false; }
    note(success); return true;
}
bool os_page_turner_scan(void) {
    s_manual_disconnect = true; s_info.learning = 0;
    return result(os_ble_radio_scan(), "搜索约 12 秒，请让翻页器进入配对模式");
}
bool os_page_turner_connect(const os_ble_peer_t* peer) {
    if (!peer) return false;
    s_info.learning = 0; s_manual_disconnect = false; s_save_retry_at = 0;
    return result(os_ble_radio_connect(peer), "正在连接，按键就绪后会自动保存设备");
}
bool os_page_turner_disconnect(void) {
    s_info.learning = 0; s_manual_disconnect = true;
    return result(os_ble_radio_disconnect(), "已请求断开，点选已保存设备可重新连接");
}
bool os_page_turner_forget(void) {
    if (!s_prefs.saved) return false;
    os_ble_radio_snapshot(&s_info.radio);
    if (!result(os_ble_radio_forget(&s_prefs.peer), "正在遗忘设备…")) return false;
    s_pending_forget = true; s_forget_revision = s_info.radio.revision;
    s_info.learning = 0; s_manual_disconnect = true; return true;
}
void os_page_turner_learn(int direction) {
    load();
    if (s_info.radio.phase != OS_BLE_READY || !s_prefs.saved ||
        s_prefs.peer.type != s_info.radio.peer.type || memcmp(s_prefs.peer.addr, s_info.radio.peer.addr, 6)) {
        note("请先连接翻页器，等到设备保存完成"); return;
    }
    s_info.learning = direction == -1 ? -1 : direction == 1 ? 1 : direction == OS_PAGE_TURNER_LOCK ? OS_PAGE_TURNER_LOCK : 0;
    s_learn_until = 0;
    os_ble_radio_clear();
    note(direction ? "松开所有按键，再按一次要使用的按钮" : "已取消按键学习");
}
bool os_page_turner_restore_mapping(void) {
    preferences_t p = s_prefs; memset(p.binding, 0, sizeof(p.binding));
    if (!save(&p)) return false;
    s_info.learning = 0; note("已恢复标准 HID 按键映射"); return true;
}
static bool same(const binding_t* b, const os_ble_input_t* input) {
    // 只按帧内容匹配：重连后 GATT 枚举顺序可能变化，通道号不可作为身份。
    // Match frame content only: GATT enumeration order may shift across connections, so the channel index is not identity.
    return b->len && b->len == input->len && !memcmp(b->bytes, input->bytes, b->len);
}
static int accept_input(const os_ble_input_t* input, int64_t now, bool body) {
    // 章节边界排版加刷屏可超过一秒，输入以 1.5 秒为限，期间排队的按键仍有效。
    // Chapter-boundary layout plus a refresh can exceed a second; queued presses stay valid for 1.5 seconds.
    if (input->channel >= 8 || !input->len || input->len > BLE_HID_REPORT_BYTES ||
        (uint32_t)now - input->at_ms > 1500) return 0;
    bool nonzero = false;
    for (unsigned i = 0; i < input->len; ++i) nonzero |= input->bytes[i] != 0;
    bool down = input->key.known ? input->key.down : nonzero;
    unsigned ch = input->channel;
    os_ble_input_t previous = s_last[ch].frame;
    bool duplicate = s_last[ch].down && previous.len == input->len &&
        !memcmp(previous.bytes, input->bytes, input->len);
    s_last[ch].frame = *input; s_last[ch].down = down;
    if (!down) return 0;
    // 标准 HID 等释放；无布局遥控器以报告间隔作保守兜底，不自行生成重复。
    // Standard HID waits for release; unknown remotes use a conservative report-gap fallback, never generated repeats.
    if (duplicate && (input->key.known || input->at_ms - previous.at_ms < 350)) return 0;
    bool selected = s_prefs.saved && s_prefs.peer.type == s_info.radio.peer.type && !memcmp(s_prefs.peer.addr, s_info.radio.peer.addr, 6);
    int dir = selected && same(&s_prefs.binding[2], input) ? OS_PAGE_TURNER_LOCK : selected && same(&s_prefs.binding[0], input) ? -1 : selected && same(&s_prefs.binding[1], input) ? 1 : input->key.direction;
    int actual = dir == OS_PAGE_TURNER_LOCK ? dir : s_prefs.reversed ? -dir : dir;
    snprintf(s_info.test, sizeof(s_info.test), "%s · %s", input->key.known ? "收到 HID 按键" : "收到非标准报告",
        actual == OS_PAGE_TURNER_LOCK ? "锁屏" : actual < 0 ? "上一页" : actual > 0 ? "下一页" : "未映射，可学习");
    if (s_info.learning) {
        unsigned slot = s_info.learning < 0 ? 0 : s_info.learning == 1 ? 1 : 2;
        for (unsigned i = 0; i < 3; ++i)
            if (i != slot && same(&s_prefs.binding[i], input)) { note("此键已用于其他操作，请换一个按钮"); return 0; }
        if (slot == 2 && input->key.direction) { note("此键用于标准翻页，请选择独立按钮"); return 0; }
        preferences_t p = s_prefs; binding_t* b = &p.binding[slot]; memset(b, 0, sizeof(*b));
        b->channel = input->channel; b->len = input->len; memcpy(b->bytes, input->bytes, input->len);
        if (save(&p)) { s_info.learning = 0; note(slot == 2 ? "锁屏按键已保存，仅在阅读正文生效" : slot ? "下一页按键已保存" : "上一页按键已保存"); }
        return 0;
    }
    if (!body || !dir || now - s_last_turn < 300) return 0;
    s_last_turn = now;
    return actual;
}
int os_page_turner_poll(int64_t now, bool body, bool wanted) {
    load();
    if (!s_active) return 0;
    if (!s_prefs.enabled) { if (s_running) os_page_turner_pause(); return 0; }
    if (!wanted) {
        if (s_running) {
            os_ble_radio_snapshot(&s_info.radio);
            if (s_info.radio.phase != OS_BLE_READY) os_page_turner_pause();
        }
        os_ble_radio_clear(); memset(s_last, 0, sizeof(s_last)); return 0;
    }
    if (!s_running) {
        if (s_tries >= 3) { note("蓝牙启动多次中断，请关闭再开启后重试"); return 0; }
        if (s_attempted && now < s_retry_at) return 0;
        s_attempted = true; s_retry_at = now + 30000;
        if (!tries_store(s_tries + 1)) { note("无法保存蓝牙启动状态，请重试"); return 0; }
        // 内存不足先请求一次可重建缓存回收（图片/封面/字体），仍不足才拒绝启动。
        // Short headroom requests one rebuildable-cache reclaim (images/covers/fonts) before refusing to start.
        if (!os_ble_radio_headroom() && s_reclaim) s_reclaim();
        if (os_ble_radio_start() != ESP_OK) { os_ble_radio_snapshot(&s_info.radio); note(s_info.radio.error); return 0; }
        s_running = true; s_retry_at = now + 500; s_health_at = now + 10000;
    }
    os_ble_radio_snapshot(&s_info.radio);
    if (s_info.radio.phase != OS_BLE_READY && s_previous_phase == OS_BLE_READY)
        memset(s_last, 0, sizeof(s_last));
    if (s_info.radio.phase == OS_BLE_READY && s_previous_phase != OS_BLE_READY) {
        note("按键已就绪，可测试或返回阅读");
        s_info.test[0] = 0;
    }
    s_previous_phase = s_info.radio.phase;
    if (s_info.learning) {
        if (!s_learn_until) s_learn_until = now + 30000;
        if (now >= s_learn_until || s_info.radio.phase != OS_BLE_READY) {
            s_info.learning = 0; note("按键学习已结束，可重新开始");
        }
    }
    if (s_tries && now >= s_health_at) { tries_store(0); s_health_at = now + 30000; }
    if (s_pending_forget && s_info.radio.revision != s_forget_revision) {
        if (s_info.radio.phase == OS_BLE_IDLE) {
            preferences_t p = s_prefs; p.saved = 0; memset(&p.peer, 0, sizeof(p.peer)); memset(p.binding, 0, sizeof(p.binding));
            s_pending_forget = false;
            if (save(&p)) note("已遗忘设备与自定义按键");
        } else if (s_info.radio.phase == OS_BLE_ERROR) { s_pending_forget = false; note(s_info.radio.error); }
    }
    if (s_info.radio.phase == OS_BLE_READY && (!s_prefs.saved ||
        s_prefs.peer.type != s_info.radio.peer.type || memcmp(s_prefs.peer.addr, s_info.radio.peer.addr, 6))) {
        // 首次保存清空旧映射；替换保存（地址轮换/换机重连）保留学习按键，遗忘或恢复标准才清除。
        // First saves start clean; replacement saves (address rotation or reconnects) keep learned keys, cleared only by Forget or Restore.
        preferences_t p = s_prefs; p.saved = 1; p.peer = s_info.radio.peer;
        if (!s_prefs.saved) memset(p.binding, 0, sizeof(p.binding));
        if (now >= s_save_retry_at) {
            if (save(&p)) note("已保存设备，可测试按键");
            s_save_retry_at = now + 30000;
        }
    }
    // 错误相不再死等用户干预：间隔到点后与空闲相一样自动重连保存的设备。
    // The error phase no longer waits for user action: reconnect the saved peer after the interval, same as idle.
    if (!s_manual_disconnect && s_prefs.saved &&
        (s_info.radio.phase == OS_BLE_IDLE || s_info.radio.phase == OS_BLE_ERROR) && now >= s_retry_at) {
        os_ble_radio_connect(&s_prefs.peer); s_retry_at = now + 10000;
    }
    if (s_info.radio.phase == OS_BLE_ERROR && s_info.radio.error[0]) note(s_info.radio.error);
    int direction = 0; os_ble_input_t input;
    while (os_ble_radio_pop(&input)) {
        int dir = accept_input(&input, now, body && !direction && s_info.radio.phase == OS_BLE_READY);
        if (dir) direction = dir;
    }
    return direction;
}
