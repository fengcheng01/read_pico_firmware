/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 *
 * 双模式传书生命周期和跨任务状态；存储策略由调用方注入。
 * AP/STA transfer lifecycle and cross-task status; caller supplies storage policy.
 * 冻结：图书 TXT/EPUB；完整字体部署使用独立 TF 目录的 TTF；不依赖页面或图书实现。
 * Frozen: TXT/EPUB books and complete TTF fonts in a separate TF directory; no page or book implementation dependency.
 * 冻结：热点网页或停服后的设备触屏可配网；已有WiFi模式不接受远程修改凭据。
 * Frozen: AP webpage or stopped-service device UI may provision; STA rejects remote credential changes.
 */
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "esp_err.h"
#include "read_pico_transfer_network.h"

#define READ_PICO_TRANSFER_PASSWORD "readpico"
#define READ_PICO_TRANSFER_URL "http://192.168.4.1"

typedef enum {
    READ_PICO_TRANSFER_MODE_AP = 0, ///< 设备热点 / Device hotspot
    READ_PICO_TRANSFER_MODE_STA, ///< 已有 WiFi / Existing WiFi
} read_pico_transfer_mode_t;

typedef enum {
    READ_PICO_TRANSFER_STOPPED, ///< 已停止 / Stopped
    READ_PICO_TRANSFER_STARTING, ///< 启动中 / Starting
    READ_PICO_TRANSFER_READY, ///< 等待上传 / Ready
    READ_PICO_TRANSFER_UPLOADING, ///< 接收中 / Receiving
    READ_PICO_TRANSFER_ERROR, ///< 网络或请求失败 / Network or request failed
} read_pico_transfer_state_t;

typedef struct {
    bool network_only; ///< 仅 STA 联网，不启动文件服务或访问存储 / STA network only, without file service or storage access
    bool use_selected_wifi; ///< 本次严格连接已显式选择的首选网络 / Connect only the explicitly selected preferred network this session
    read_pico_transfer_mode_t mode; ///< 网络模式，默认热点 / Network mode, default AP
    const char *root_dir; ///< 已挂载根目录，start 内复制 / Mounted root, copied by start
    const char *font_dir; ///< 可选 TF 字体目录；调用方须暂停 SD 字体读取至 stop 返回 / Optional TF font directory; caller must suspend SD font reads until stop returns
    const char *image_dir; ///< 可选 TF 图片目录；上传 JPG/PNG 供锁屏图片页使用 / Optional TF image directory; JPG/PNG uploads feed the lock-image page
    bool is_flash; ///< 内置存储标志 / Internal storage flag
    size_t file_limit; ///< 单文件上限，零表示不限 / Per-file limit, zero means unlimited
    uint64_t (*free_bytes_cb)(void *ctx); ///< 查询可用字节 / Query available bytes
    void *free_bytes_ctx; ///< 回调上下文，stop 前有效 / Callback context valid until stop
    esp_err_t (*file_changed_cb)(const char *path); ///< 文件提交或删除后清除进度；失败不回滚文件 / Clear progress after commit or deletion; failure never rolls back the file
} read_pico_transfer_cfg_t;

typedef struct {
    read_pico_transfer_state_t state; ///< 当前状态 / Current state
    read_pico_transfer_mode_t mode; ///< 网络模式 / Network mode
    char url[64]; ///< 可访问地址，未联网为空 / Reachable address, empty when offline
    char wifi_ssid[33]; ///< 当前首选或正在连接的网络，不含密码 / Preferred or connecting network, never a password
    bool wifi_configured; ///< 至少存在一个有效凭据 / At least one valid credential is saved
    bool wifi_associated; ///< 已关联路由器，可能仍在等待 DHCP / Associated with the router, possibly still awaiting DHCP
    bool network_ready; ///< 已取得可用网络地址 / Usable network address acquired
    unsigned sta_count; ///< 连接数 / Station count
    char ssid[33]; ///< 热点名称 / AP name
    char cur_name[121]; ///< 完整 UTF-8 文件名 / Complete UTF-8 filename
    size_t cur_bytes; ///< 已接收字节 / Received bytes
    size_t cur_total; ///< 本次总字节 / Request total bytes
    unsigned done_count; ///< 成功提交数 / Successfully committed files
    unsigned changed_count; ///< 提交、删除或进度清理重试成功的次数 / Commits, deletions, or successful progress-cleanup retries
    esp_err_t last_error; ///< 最近错误 / Last error
} read_pico_transfer_status_t;

/// 同一控制任务串行启动/停止；重复启动返回状态错误。/ Serialize start/stop on one owner task; repeated start fails.
esp_err_t read_pico_transfer_start(const read_pico_transfer_cfg_t *cfg);
/// 等待请求退出；释放失败保留资源并在状态中报告，可再次调用。/ Join requests; failed release retains resources and reports status for a later retry.
void read_pico_transfer_stop(void);
/// 文件操作或释放失败则返回false；成功则已停服，由同一控制任务调用。/ Return false during file operations or failed release; true means stopped. Owner task only.
bool read_pico_transfer_try_stop_if_idle(void);
/// 跨任务复制一致状态快照。/ Copy a consistent snapshot across tasks.
void read_pico_transfer_get_status(read_pico_transfer_status_t *out);
/// 判断连接错误是否由 WiFi 密码/认证失败引起；不暴露驱动原因码。/ Report whether a connection error came from WiFi password/authentication failure without exposing driver reason codes.
bool read_pico_transfer_error_is_password(esp_err_t error);
/// 控制任务每500ms驱动有界连接重试，关联与 DHCP 各自限时。/ Owner task polls every 500 ms for bounded retries with separate association and DHCP deadlines.
void read_pico_transfer_service_poll(void);
/// 只读当前首选SSID；未配置仍返回ESP_OK。/ Read only the preferred SSID; unconfigured still returns ESP_OK.
esp_err_t read_pico_transfer_get_saved_wifi(char ssid[33], bool *configured);
/// 仅热点或停止状态可遗忘当前首选网络；上传中拒绝。/ Forget the preferred network only while AP or stopped; rejected during upload.
esp_err_t read_pico_transfer_forget_wifi(void);
/// 停服后小缓冲同步扫描2.4GHz网络；最多16个去重SSID，按信号降序；释放失败交给stop重试。
/// Scan synchronously with small buffers while stopped; up to 16 unique SSIDs by RSSI; failed resource release is retried by stop.
esp_err_t read_pico_transfer_scan_wifi(read_pico_transfer_network_t out[READ_PICO_TRANSFER_SCAN_MAX], size_t *count);
/// 停服后记住或更新设备输入的凭据并设为首选，不自动连接。/ Remember or update device-entered credentials as preferred while stopped, without connecting.
esp_err_t read_pico_transfer_save_wifi(const char *ssid, const char *password);
/// 停服后把已记住的SSID设为首选；密码不离开组件。/ Make a remembered SSID preferred while stopped; its password never leaves the component.
esp_err_t read_pico_transfer_select_saved_wifi(const char *ssid);

/// UI 获取文件/进度互斥会话；上传或删除中立即返回 false，不等待。
/// UI claims an exclusive file/progress session; return false immediately during upload/deletion.
bool read_pico_transfer_claim_sync(void);
/// 后台请求已收齐且 UI 完成进度操作后释放。/ Release after joining network work and finishing UI progress operations.
void read_pico_transfer_release_sync(void);

/// 控制任务用已保存凭据启动仅联网 STA。/ Owner task starts a network-only STA with saved credentials.
esp_err_t read_pico_transfer_start_saved_network(void);
