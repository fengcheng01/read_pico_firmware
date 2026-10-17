/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 * 中文：ESP-IDF NimBLE central。命令与发现均在 host 队列上串行执行；UI 只读副本。
 * English: ESP-IDF NimBLE central. Commands and discovery run serially on the host queue; UI reads copies.
 * 冻结：只订阅 HID 输入；描述符限定到对应特征；断链清输入，关闭完整释放协议栈。
 * Frozen: Subscribe to HID inputs only, bound descriptor walks to each characteristic, clear input on disconnect, and release the stack on stop.
 * 修订：NimBLE主动取消扫描不发完成事件，命令同步取消后直接推进，所有连接等待均有期限。
 * Revision: NimBLE active scan cancellation emits no completion event; advance immediately after cancellation and bound every connection wait.
 * 参考：KiikoRead 4bd2b9e 的扫描/订阅边界与 RickyOS ecfc977 的按键映射流程；本文件独立实现。
 * Reference: KiikoRead 4bd2b9e scan/subscription boundaries and RickyOS ecfc977 mapping flow; independently implemented here.
 */
#include "os_ble_radio.h"
#include <stdatomic.h>
#include <stdio.h>
#include <string.h>
#include "esp_heap_caps.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "host/ble_gap.h"
#include "host/ble_gatt.h"
#include "host/ble_hs.h"
#include "host/ble_hs_adv.h"
#include "host/ble_sm.h"
#include "host/ble_store.h"
#include "host/util/util.h"
#include "nimble/nimble_port.h"
#include "os/os_mbuf.h"

#define INPUTS 8
#define CHARS 32
#define CHANNELS 8
static const char* TAG = "page_turner";
static portMUX_TYPE s_lock = portMUX_INITIALIZER_UNLOCKED;
static os_ble_snapshot_t s_status = {.battery = -1};
static os_ble_input_t s_inputs[INPUTS];
static unsigned s_head, s_tail;
static atomic_bool s_running, s_exited;
static bool s_stopped;
static bool s_deinit_failed;
static struct ble_npl_event s_command;
static struct ble_npl_callout s_timeout;
typedef enum { CMD_NONE, CMD_SCAN, CMD_CONNECT, CMD_DISCONNECT, CMD_FORGET } command_t;
static command_t s_cmd;
static os_ble_peer_t s_target;
// 以下连接状态仅由 host 任务访问。/ The host task exclusively owns the connection state below.
static uint16_t s_conn = BLE_HS_CONN_HANDLE_NONE, s_service_start, s_service_end, s_map_handle;
static uint8_t s_own_type, s_char_count, s_channel_count, s_walk, s_subscribed;
static bool s_scanning, s_pending, s_ready, s_security_retry, s_scan_requested;
static bool s_accept_connection, s_input_enabled;
static os_ble_peer_t s_peer;
static struct { uint16_t def, value, uuid; uint8_t properties; } s_chars[CHARS];
static struct {
    uint16_t value, end, cccd, reference;
    uint8_t id, type;
    bool boot;
} s_channels[CHANNELS];
static ble_hid_map_t s_map;

static uint32_t now_ms(void) { return (uint32_t)(esp_timer_get_time() / 1000); }
static void phase(os_ble_phase_t value, const char* error) {
    portENTER_CRITICAL(&s_lock);
    s_status.phase = value;
    snprintf(s_status.error, sizeof(s_status.error), "%s", error ? error : "");
    ++s_status.revision;
    portEXIT_CRITICAL(&s_lock);
}
void os_ble_radio_snapshot(os_ble_snapshot_t* out) {
    if (!out) return;
    portENTER_CRITICAL(&s_lock); *out = s_status; portEXIT_CRITICAL(&s_lock);
}
void os_ble_radio_clear(void) {
    portENTER_CRITICAL(&s_lock); s_head = s_tail = 0; portEXIT_CRITICAL(&s_lock);
}
bool os_ble_radio_pop(os_ble_input_t* out) {
    if (!out) return false;
    portENTER_CRITICAL(&s_lock);
    bool have = s_head != s_tail;
    if (have) { *out = s_inputs[s_tail]; s_tail = (s_tail + 1) % INPUTS; }
    portEXIT_CRITICAL(&s_lock);
    return have;
}
static void fail(const char* message, int code) {
    char note[88];
    snprintf(note, sizeof(note), "%s (%d)", message, code);
    ESP_LOGW(TAG, "%s", note);
    s_pending = s_accept_connection = s_input_enabled = s_scan_requested = false;
    ble_npl_callout_stop(&s_timeout);
    phase(OS_BLE_ERROR, note);
    if (s_conn != BLE_HS_CONN_HANDLE_NONE) ble_gap_terminate(s_conn, BLE_ERR_REM_USER_CONN_TERM);
    else ble_gap_conn_cancel();
}
static void timeout(struct ble_npl_event* event) { (void)event; fail("连接或发现超时，请重试", BLE_HS_ETIMEOUT); }
static void arm_timeout(void) { ble_npl_callout_reset(&s_timeout, ble_npl_time_ms_to_ticks32(18000)); }
static int gap(struct ble_gap_event* event, void* arg);
static void start_scan(void) {
    s_scan_requested = false;
    portENTER_CRITICAL(&s_lock); s_status.count = 0; memset(s_status.peers, 0, sizeof(s_status.peers)); portEXIT_CRITICAL(&s_lock);
    const struct ble_gap_disc_params params = {.passive = 0, .itvl = 160, .window = 80, .filter_duplicates = 0};
    int rc = ble_gap_disc(s_own_type, 12000, &params, gap, NULL);
    if (rc) fail("扫描失败", rc); else { s_scanning = true; phase(OS_BLE_SCANNING, NULL); }
}
// 主动取消不等待DISC_COMPLETE；EALREADY表示扫描已自然结束。
// Active cancellation never waits for DISC_COMPLETE; EALREADY means scanning already ended.
static bool cancel_scan(void) {
    if (!s_scanning) return true;
    int rc = ble_gap_disc_cancel();
    if (rc && rc != BLE_HS_EALREADY) { fail("无法停止扫描", rc); return false; }
    s_scanning = false;
    return true;
}
static void connect_peer(void) {
    if (!s_ready) { fail("蓝牙未就绪，请关闭后重开", BLE_HS_EALREADY); return; }
    if (s_conn != BLE_HS_CONN_HANDLE_NONE) return;
    ble_addr_t addr = {.type = s_peer.type}; memcpy(addr.val, s_peer.addr, 6);
    s_pending = false;
    s_accept_connection = true; s_input_enabled = false;
    phase(OS_BLE_CONNECTING, NULL);
    int rc = ble_gap_connect(s_own_type, &addr, 10000, NULL, gap, NULL);
    if (rc) fail("无法发起连接", rc); else arm_timeout();
}
static void publish_peer(void) {
    portENTER_CRITICAL(&s_lock);
    s_status.peer = s_peer; ++s_status.revision;
    portEXIT_CRITICAL(&s_lock);
}

/* ---- 串行发现与订阅 / Serial discovery and subscription ---- */

static void next_descriptor(void);
static void next_reference(void);
static void next_subscription(void);
static void discover(void);
static bool secure_if_needed(int status) {
    if (!s_security_retry && (status == BLE_HS_ATT_ERR(BLE_ATT_ERR_INSUFFICIENT_AUTHEN) ||
        status == BLE_HS_ATT_ERR(BLE_ATT_ERR_INSUFFICIENT_ENC))) {
        s_security_retry = true;
        int rc = ble_gap_security_initiate(s_conn);
        if (rc) fail("无法启动配对", rc);
        return true;
    }
    return false;
}
static int subscribed(uint16_t conn, const struct ble_gatt_error* error, struct ble_gatt_attr* attr, void* arg) {
    (void)attr; (void)arg;
    if (conn != s_conn) return 0;
    if (secure_if_needed(error->status)) return 0;
    if (!error->status) ++s_subscribed;
    else ESP_LOGW(TAG, "subscribe: %d", error->status);
    next_subscription();
    return 0;
}
static void next_subscription(void) {
    while (s_walk < s_channel_count) {
        unsigned i = s_walk++;
        // Report Reference 类型 1 为输入；Boot 输入不带该描述符。
        // Report Reference type 1 is input; boot inputs have no such descriptor.
        if (!s_channels[i].cccd || (!s_channels[i].boot && s_channels[i].type != 1)) continue;
        const uint8_t enable[2] = {1, 0};
        int rc = ble_gattc_write_flat(s_conn, s_channels[i].cccd, enable, 2, subscribed, NULL);
        if (!rc) return;
        ESP_LOGW(TAG, "subscribe start: %d", rc);
    }
    if (!s_subscribed) { fail("没有可用的 HID 输入", BLE_HS_ENOENT); return; }
    ble_npl_callout_stop(&s_timeout);
    s_input_enabled = true;
    phase(OS_BLE_READY, NULL);
}
static int reference(uint16_t conn, const struct ble_gatt_error* error, struct ble_gatt_attr* attr, void* arg) {
    unsigned i = (unsigned)(uintptr_t)arg;
    if (conn != s_conn || i >= s_channel_count) return 0;
    uint8_t ref[2];
    if (secure_if_needed(error->status)) return 0;
    if (!error->status && attr && OS_MBUF_PKTLEN(attr->om) == 2 && !os_mbuf_copydata(attr->om, 0, 2, ref)) {
        s_channels[i].id = ref[0]; s_channels[i].type = ref[1];
    }
    next_reference(); return 0;
}
static void next_reference(void) {
    while (s_walk < s_channel_count) {
        unsigned i = s_walk++;
        if (s_channels[i].reference && !ble_gattc_read(s_conn, s_channels[i].reference, reference, (void*)(uintptr_t)i)) return;
    }
    s_walk = s_subscribed = 0;
    next_subscription();
}
static int report_map(uint16_t conn, const struct ble_gatt_error* error, struct ble_gatt_attr* attr, void* arg) {
    (void)arg;
    if (conn != s_conn) return 0;
    if (secure_if_needed(error->status)) return 0;
    // read_long 分片按偏移收齐，不能把 mbuf 链首段当作整份映射。
    // Assemble read_long fragments by offset; an mbuf's first segment is not the whole map.
    static uint8_t bytes[2048];
    static size_t used;
    if (!error->status && attr) {
        if (!attr->offset) used = 0;
        size_t n = OS_MBUF_PKTLEN(attr->om);
        if (attr->offset != used || n > sizeof(bytes) - used || os_mbuf_copydata(attr->om, 0, n, bytes + used)) {
            used = 0; memset(&s_map, 0, sizeof(s_map));
            return BLE_HS_EINVAL;
        }
        used += n;
        return 0;
    }
    if (error->status == BLE_HS_EDONE) ble_hid_parse(&s_map, bytes, used);
    else memset(&s_map, 0, sizeof(s_map));
    used = 0; s_walk = 0; next_reference();
    return 0;
}
static int descriptor(uint16_t conn, const struct ble_gatt_error* error, uint16_t value,
                      const struct ble_gatt_dsc* dsc, void* arg) {
    (void)value;
    unsigned i = (unsigned)(uintptr_t)arg;
    if (conn != s_conn || i >= s_channel_count) return 0;
    if (!error->status && dsc) {
        uint16_t uuid = ble_uuid_u16(&dsc->uuid.u);
        if (uuid == 0x2902) s_channels[i].cccd = dsc->handle;
        if (uuid == 0x2908) s_channels[i].reference = dsc->handle;
        return 0;
    }
    next_descriptor(); return 0;
}
static void next_descriptor(void) {
    while (s_walk < s_channel_count) {
        unsigned i = s_walk++;
        if (s_channels[i].end > s_channels[i].value &&
            !ble_gattc_disc_all_dscs(s_conn, s_channels[i].value, s_channels[i].end, descriptor, (void*)(uintptr_t)i)) return;
    }
    if (s_map_handle && !ble_gattc_read_long(s_conn, s_map_handle, 0, report_map, NULL)) return;
    s_walk = 0; next_reference();
}
static int characteristic(uint16_t conn, const struct ble_gatt_error* error, const struct ble_gatt_chr* chr, void* arg) {
    (void)arg;
    if (conn != s_conn) return 0;
    if (!error->status && chr) {
        if (s_char_count == CHARS) { fail("HID 特征过多", BLE_HS_ENOMEM); return BLE_HS_ENOMEM; }
        s_chars[s_char_count].def = chr->def_handle;
        s_chars[s_char_count].value = chr->val_handle;
        s_chars[s_char_count].uuid = ble_uuid_u16(&chr->uuid.u);
        s_chars[s_char_count++].properties = chr->properties;
        return 0;
    }
    if (error->status != BLE_HS_EDONE) { fail("HID 特征读取失败", error->status); return 0; }
    for (unsigned i = 0; i < s_char_count; ++i) {
        if (s_chars[i].uuid == 0x2a4b) s_map_handle = s_chars[i].value;
        if ((s_chars[i].uuid == 0x2a4d || s_chars[i].uuid == 0x2a22) &&
            (s_chars[i].properties & BLE_GATT_CHR_PROP_NOTIFY) && s_channel_count < CHANNELS) {
            unsigned k = s_channel_count++;
            s_channels[k].value = s_chars[i].value;
            s_channels[k].end = i + 1 < s_char_count ? s_chars[i + 1].def - 1 : s_service_end;
            s_channels[k].boot = s_chars[i].uuid == 0x2a22;
        }
    }
    if (!s_channel_count) { fail("此设备没有 HID 输入报告", BLE_HS_ENOENT); return 0; }
    s_walk = 0; next_descriptor(); return 0;
}
static int service(uint16_t conn, const struct ble_gatt_error* error, const struct ble_gatt_svc* svc, void* arg) {
    (void)arg;
    if (conn != s_conn) return 0;
    if (!error->status && svc && ble_uuid_u16(&svc->uuid.u) == 0x1812 && !s_service_start) {
        s_service_start = svc->start_handle; s_service_end = svc->end_handle; return 0;
    }
    if (!error->status) return 0;
    if (error->status != BLE_HS_EDONE || !s_service_start) { fail("不是可用的 BLE HID 设备", error->status); return 0; }
    int rc = ble_gattc_disc_all_chrs(s_conn, s_service_start, s_service_end, characteristic, NULL);
    if (rc) fail("无法读取 HID 特征", rc);
    return 0;
}
static void discover(void) {
    s_char_count = s_channel_count = 0;
    s_service_start = s_service_end = s_map_handle = 0;
    memset(&s_map, 0, sizeof(s_map)); memset(s_channels, 0, sizeof(s_channels));
    phase(OS_BLE_DISCOVERING, NULL); arm_timeout();
    int rc = ble_gattc_disc_all_svcs(s_conn, service, NULL);
    if (rc) fail("无法发现 HID 服务", rc);
}
static void received(uint16_t handle, struct os_mbuf* om) {
    if (!s_input_enabled) return;
    unsigned ch = 0;
    while (ch < s_channel_count && s_channels[ch].value != handle) ++ch;
    size_t n = OS_MBUF_PKTLEN(om);
    if (ch == s_channel_count || !n || n > BLE_HID_REPORT_BYTES ||
        (!s_channels[ch].boot && s_channels[ch].type != 1)) return;
    os_ble_input_t input = {.at_ms = now_ms(), .channel = ch, .len = n};
    if (os_mbuf_copydata(om, 0, n, input.bytes)) return;
    input.key = s_channels[ch].boot ? ble_hid_boot(input.bytes, n) :
        ble_hid_decode(&s_map, s_channels[ch].id, input.bytes, n);
    // 队满丢弃全部旧事件，仅保留当前帧，避免翻页积压；释放也能到达。
    // On overflow discard old events and keep the current frame, allowing releases through without a turn backlog.
    portENTER_CRITICAL(&s_lock);
    unsigned next = (s_head + 1) % INPUTS;
    if (next == s_tail) s_tail = s_head;
    s_inputs[s_head] = input; s_head = next;
    portEXIT_CRITICAL(&s_lock);
}
static int gap(struct ble_gap_event* event, void* arg) {
    (void)arg;
    switch (event->type) {
        case BLE_GAP_EVENT_DISC: {
            struct ble_hs_adv_fields fields;
            if (ble_hs_adv_parse_fields(&fields, event->disc.data, event->disc.length_data)) return 0;
            bool hid = false;
            for (unsigned i = 0; i < fields.num_uuids16; ++i) if (fields.uuids16[i].value == 0x1812) hid = true;
            portENTER_CRITICAL(&s_lock);
            unsigned i = 0;
            while (i < s_status.count && (s_status.peers[i].type != event->disc.addr.type ||
                memcmp(s_status.peers[i].addr, event->disc.addr.val, 6))) ++i;
            if (i == s_status.count && i < OS_BLE_PEERS && (hid || fields.name_len)) ++s_status.count;
            if (i < s_status.count) {
                os_ble_peer_t* p = &s_status.peers[i];
                memcpy(p->addr, event->disc.addr.val, 6); p->type = event->disc.addr.type;
                p->rssi = event->disc.rssi; p->hid |= hid;
                if (fields.name && fields.name_len) {
                    size_t len = fields.name_len < sizeof(p->name) - 1 ? fields.name_len : sizeof(p->name) - 1;
                    memcpy(p->name, fields.name, len); p->name[len] = 0;
                }
                ++s_status.revision;
            }
            portEXIT_CRITICAL(&s_lock); return 0;
        }
        case BLE_GAP_EVENT_DISC_COMPLETE:
            if (!s_scanning) return 0;
            s_scanning = false;
            if (s_pending) connect_peer();
            else if (s_scan_requested) start_scan();
            else phase(OS_BLE_IDLE, NULL);
            return 0;
        case BLE_GAP_EVENT_CONNECT:
            if (!s_accept_connection) {
                if (!event->connect.status) ble_gap_terminate(event->connect.conn_handle, BLE_ERR_REM_USER_CONN_TERM);
                return 0;
            }
            if (event->connect.status) { fail("连接失败，检查配对模式", event->connect.status); return 0; }
            s_conn = event->connect.conn_handle;
            s_security_retry = false;
            // 持久化身份地址而非会轮换的广播地址。/ Persist the identity address rather than a rotating advertising address.
            struct ble_gap_conn_desc desc;
            if (!ble_gap_conn_find(s_conn, &desc)) {
                memcpy(s_peer.addr, desc.peer_id_addr.val, 6); s_peer.type = desc.peer_id_addr.type;
            }
            publish_peer();
            phase(OS_BLE_DISCOVERING, NULL); arm_timeout();
            // 先发现；受保护的读取/订阅再触发配对，兼容无需加密的 HID 翻页器。
            // Discover first; protected reads/subscriptions trigger pairing, retaining plaintext HID compatibility.
            discover();
            return 0;
        case BLE_GAP_EVENT_ENC_CHANGE:
            if (event->enc_change.conn_handle != s_conn) return 0;
            if (event->enc_change.status) fail("配对失败，请遗忘后重试", event->enc_change.status);
            else discover();
            return 0;
        case BLE_GAP_EVENT_REPEAT_PAIRING: {
            // 旧密钥失效时要求用户明确遗忘，不自动删除其他设备的密钥。
            // An obsolete bond requires explicit Forget; never erase another peer's keys automatically.
            fail("已有配对失效，请遗忘后重试", BLE_HS_EALREADY);
            return BLE_GAP_REPEAT_PAIRING_IGNORE;
        }
        case BLE_GAP_EVENT_PASSKEY_ACTION:
            // 本版是无需口令的翻页器；有口令键盘明确失败，不默许数字比对。
            // This version supports no-passkey remotes; fail passkey keyboards explicitly without accepting numeric comparison silently.
            fail("此设备需要口令，暂不支持", BLE_HS_ENOTSUP); return 0;
        case BLE_GAP_EVENT_NOTIFY_RX:
            if (event->notify_rx.conn_handle == s_conn) received(event->notify_rx.attr_handle, event->notify_rx.om);
            return 0;
        case BLE_GAP_EVENT_DISCONNECT: {
            s_input_enabled = false;
            s_conn = BLE_HS_CONN_HANDLE_NONE;
            ble_npl_callout_stop(&s_timeout); os_ble_radio_clear();
            os_ble_snapshot_t status; os_ble_radio_snapshot(&status);
            if (s_scan_requested) start_scan();
            else if (s_pending) connect_peer();
            else if (status.phase != OS_BLE_ERROR) phase(OS_BLE_IDLE, NULL);
            return 0;
        }
        default: return 0;
    }
}
static void command(struct ble_npl_event* event) {
    (void)event;
    portENTER_CRITICAL(&s_lock);
    command_t cmd = s_cmd; os_ble_peer_t target = s_target; s_cmd = CMD_NONE;
    portEXIT_CRITICAL(&s_lock);
    if (cmd == CMD_CONNECT) {
        s_input_enabled = false; phase(OS_BLE_CONNECTING, NULL);
        s_scan_requested = false;
        s_peer = target; publish_peer(); s_pending = true;
        arm_timeout();
        if (!cancel_scan()) return;
        if (s_conn != BLE_HS_CONN_HANDLE_NONE) {
            int rc = ble_gap_terminate(s_conn, BLE_ERR_REM_USER_CONN_TERM);
            if (rc) fail("无法断开旧连接", rc);
        } else connect_peer();
    } else if (cmd == CMD_SCAN) {
        if (!cancel_scan()) return;
        s_pending = s_input_enabled = false;
        if (s_conn != BLE_HS_CONN_HANDLE_NONE) {
            s_scan_requested = true; arm_timeout();
            int rc = ble_gap_terminate(s_conn, BLE_ERR_REM_USER_CONN_TERM);
            if (rc) fail("无法断开旧连接", rc);
        } else { ble_gap_conn_cancel(); start_scan(); }
    } else if (cmd == CMD_DISCONNECT || cmd == CMD_FORGET) {
        s_accept_connection = s_input_enabled = false; ble_npl_callout_stop(&s_timeout);
        s_pending = s_scan_requested = false;
        if (!cancel_scan()) return;
        if (s_conn != BLE_HS_CONN_HANDLE_NONE) ble_gap_terminate(s_conn, BLE_ERR_REM_USER_CONN_TERM);
        else ble_gap_conn_cancel();
        if (cmd == CMD_FORGET) {
            ble_addr_t addr = {.type = target.type}; memcpy(addr.val, target.addr, 6);
            int rc = ble_store_util_delete_peer(&addr);
            if (rc && rc != BLE_HS_ENOENT) { fail("无法删除配对", rc); return; }
        }
        os_ble_radio_clear(); phase(OS_BLE_IDLE, NULL);
    }
}
static esp_err_t post(command_t cmd, const os_ble_peer_t* target) {
    if (!atomic_load(&s_running)) return ESP_ERR_INVALID_STATE;
    portENTER_CRITICAL(&s_lock);
    bool available = s_cmd == CMD_NONE && s_status.phase != OS_BLE_STARTING;
    if ((cmd == CMD_CONNECT || cmd == CMD_SCAN) &&
        (s_status.phase == OS_BLE_CONNECTING || s_status.phase == OS_BLE_DISCOVERING)) available = false;
    if (available) { s_cmd = cmd; if (target) s_target = *target; }
    portEXIT_CRITICAL(&s_lock);
    if (!available) return ESP_ERR_INVALID_STATE;
    ble_npl_eventq_put(nimble_port_get_dflt_eventq(), &s_command);
    return ESP_OK;
}
esp_err_t os_ble_radio_scan(void) { return post(CMD_SCAN, NULL); }
esp_err_t os_ble_radio_connect(const os_ble_peer_t* peer) { return peer ? post(CMD_CONNECT, peer) : ESP_ERR_INVALID_ARG; }
esp_err_t os_ble_radio_disconnect(void) { return post(CMD_DISCONNECT, NULL); }
esp_err_t os_ble_radio_forget(const os_ble_peer_t* peer) { return peer ? post(CMD_FORGET, peer) : ESP_ERR_INVALID_ARG; }
static void synced(void) {
    int rc = ble_hs_util_ensure_addr(0);
    if (!rc) rc = ble_hs_id_infer_auto(0, &s_own_type);
    if (rc) { phase(OS_BLE_ERROR, "蓝牙地址初始化失败"); return; }
    s_ready = true; phase(OS_BLE_IDLE, NULL);
}
static void reset(int reason) { ESP_LOGW(TAG, "host reset: %d", reason); s_conn = BLE_HS_CONN_HANDLE_NONE; s_ready = false; os_ble_radio_clear(); phase(OS_BLE_ERROR, "蓝牙已重置，请关闭后重开"); }
static void host_task(void* arg) { (void)arg; nimble_port_run(); atomic_store(&s_exited, true); vTaskDelete(NULL); }
// EXTALLOC 下控制器仍驻内部 RAM；阈值只拒绝明显不足，实际峰值仍需真机验证。
// The controller stays internal under EXTALLOC; floors reject obvious shortages while peak use still needs device validation.
#define RADIO_FLOOR_INTERNAL (48 * 1024)
#define RADIO_FLOOR_BLOCK (16 * 1024)
#define RADIO_FLOOR_SPIRAM (256 * 1024)
static bool headroom(size_t* internal, size_t* block, size_t* psram) {
    *internal = heap_caps_get_free_size(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    *block = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    *psram = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    return *internal >= RADIO_FLOOR_INTERNAL && *block >= RADIO_FLOOR_BLOCK && *psram >= RADIO_FLOOR_SPIRAM;
}
bool os_ble_radio_headroom(void) {
    size_t internal, block, psram;
    return headroom(&internal, &block, &psram);
}
esp_err_t os_ble_radio_start(void) {
    if (atomic_load(&s_running)) return ESP_OK;
    size_t internal, block, psram;
    if (!headroom(&internal, &block, &psram)) {
        ESP_LOGW(TAG, "headroom short: int %u largest %u psram %u",
            (unsigned)internal, (unsigned)block, (unsigned)psram);
        phase(OS_BLE_ERROR, "蓝牙内存不足，请退出图书后重试"); return ESP_ERR_NO_MEM;
    }
    phase(OS_BLE_STARTING, NULL);
    esp_err_t rc = nimble_port_init();
    if (rc != ESP_OK) { phase(OS_BLE_ERROR, "蓝牙初始化失败，请重试"); return rc; }
    s_conn = BLE_HS_CONN_HANDLE_NONE; s_ready = s_pending = s_scanning = s_stopped = s_scan_requested = false;
    s_accept_connection = s_input_enabled = s_deinit_failed = false;
    s_cmd = CMD_NONE; atomic_store(&s_exited, false); os_ble_radio_clear();
    ble_npl_event_init(&s_command, command, NULL);
    ble_npl_callout_init(&s_timeout, nimble_port_get_dflt_eventq(), timeout, NULL);
    ble_hs_cfg.sync_cb = synced; ble_hs_cfg.reset_cb = reset;
    ble_hs_cfg.store_status_cb = ble_store_util_status_rr;
    ble_hs_cfg.sm_io_cap = BLE_SM_IO_CAP_NO_IO;
    ble_hs_cfg.sm_bonding = 1; ble_hs_cfg.sm_mitm = 0; ble_hs_cfg.sm_sc = 1;
    ble_hs_cfg.sm_our_key_dist = BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID;
    ble_hs_cfg.sm_their_key_dist = BLE_SM_PAIR_KEY_DIST_ENC | BLE_SM_PAIR_KEY_DIST_ID;
    if (xTaskCreatePinnedToCore(host_task, "pageturn_ble", CONFIG_BT_NIMBLE_HOST_TASK_STACK_SIZE,
        NULL, configMAX_PRIORITIES - 4, NULL, CONFIG_BT_NIMBLE_PINNED_TO_CORE) != pdPASS) {
        ble_npl_callout_deinit(&s_timeout); ble_npl_event_deinit(&s_command); nimble_port_deinit();
        phase(OS_BLE_ERROR, "无法创建蓝牙任务"); return ESP_ERR_NO_MEM;
    }
    atomic_store(&s_running, true); return ESP_OK;
}
esp_err_t os_ble_radio_stop(void) {
    if (!atomic_load(&s_running)) return ESP_OK;
    // deinit 可能已部分释放，失败后不能再次释放同一组对象。
    // A failed deinit may have partially freed objects; never deinitialise them twice.
    if (s_deinit_failed) return ESP_FAIL;
    if (!s_stopped) {
        int rc = nimble_port_stop();
        if (rc) { phase(OS_BLE_ERROR, "蓝牙停止失败，请重试"); return ESP_FAIL; }
        s_stopped = true;
    }
    int64_t deadline = esp_timer_get_time() + 1000000;
    while (!atomic_load(&s_exited) && esp_timer_get_time() < deadline) vTaskDelay(pdMS_TO_TICKS(10));
    if (!atomic_load(&s_exited)) return ESP_ERR_TIMEOUT;
    ble_npl_callout_deinit(&s_timeout); ble_npl_event_deinit(&s_command);
    esp_err_t rc = nimble_port_deinit();
    if (rc != ESP_OK) { s_deinit_failed = true; phase(OS_BLE_ERROR, "蓝牙释放失败，请重启后重试"); return rc; }
    atomic_store(&s_running, false); os_ble_radio_clear();
    phase(OS_BLE_OFF, NULL); return ESP_OK;
}
