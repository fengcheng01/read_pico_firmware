/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 *
 * 中文：阅读进度同步的协议核心，兼容 KOReader (kosync) 服务器。HTTP 传输与
 * 文档哈希由注入的 io 提供：设备用 esp_http_client + 部分 MD5，测试用替身；
 * 本文件不读 NVS、不碰网络。
 * English: Protocol core for reading-progress sync, compatible with KOReader
 * (kosync) servers. HTTP transport and document hashing are injected: the
 * device uses esp_http_client plus the partial MD5, tests use fakes; this file
 * reads no NVS and touches no network.
 *
 * 冻结：密码只以 MD5 十六进制参与协议，不存/不传明文；进度串是自有 rp1 格式，
 * 解析不了的远端进度只按百分比回退，不伪造精确位置。
 * Frozen: Passwords exist only as MD5 hex on the wire, never plain; the
 * progress string is our rp1 format, and unparsable remote progress falls back
 * to percentage only, never faking a precise position.
 */
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define OS_SYNC_URL_MAX 128
#define OS_SYNC_USER_MAX 32
#define OS_SYNC_PROGRESS_MAX 64
#define OS_SYNC_DEVICE "Read Pico"
#define OS_SYNC_DEFAULT_URL "https://sync.koreader.rocks"

typedef enum {
    OS_SYNC_OK = 0,
    OS_SYNC_NO_CONFIG, ///< 未配置用户名或密钥 / Missing user or key
    OS_SYNC_OFFLINE, ///< 传输失败 / Transport failed
    OS_SYNC_AUTH, ///< 401/403 / Auth rejected
    OS_SYNC_NOT_FOUND, ///< 404，尚无该书的远端进度 / 404, no remote progress
    OS_SYNC_EXISTS, ///< 402，注册时用户名已存在 / 402, username taken
    OS_SYNC_SERVER, ///< 其它非 2xx / Other non-2xx
    OS_SYNC_PARSE, ///< 响应或进度串解析失败 / Response or progress parse failure
    OS_SYNC_IO, ///< 参数/文档哈希失败 / Bad arguments or hashing failure
} os_sync_result_t;
const char* os_sync_result_name(os_sync_result_t result);

typedef struct {
    /// 返回 HTTP 状态码（>=0），负值表示传输失败；resp 可为 NULL。
    /// / Return the HTTP status (>=0) or negative on transport failure; resp may be NULL.
    int (*request)(const char* method, const char* url,
                   const char* user, const char* key,
                   const char* content_type, const char* body,
                   char* resp, size_t resp_cap);
    /// KOReader 部分 MD5：12 个偏移（0,1K,4K,…,1G）各取 1024 字节拼接哈希。
    /// / KOReader partial MD5 over 1024-byte chunks at 12 offsets (0,1K,4K,…,1G).
    bool (*doc_id)(const char* path, char out[33]);
} os_sync_io_t;

/// 绑定传输与哈希实现；未绑定时所有请求返回 IO。/ Bind transport/hash; unbound requests fail with IO.
void os_sync_bind(const os_sync_io_t* io);

typedef struct {
    /// 服务器基址，结尾不带斜杠。/ Server base URL without a trailing slash.
    char url[OS_SYNC_URL_MAX];
    char user[OS_SYNC_USER_MAX];
    /// 密码的 MD5 小写十六进制，不存明文。/ Lowercase MD5 hex of the password, never the plain text.
    char key[33];
} os_sync_config_t;

/// 校验配置完整；补默认服务器。/ Validate completeness and apply the default server.
bool os_sync_config_ready(os_sync_config_t* config);

os_sync_result_t os_sync_register(const os_sync_config_t* config);
os_sync_result_t os_sync_auth(const os_sync_config_t* config);
/// 上传：progress 为 rp1 串，percent 为 0..1。/ Push an rp1 string with a 0..1 percentage.
os_sync_result_t os_sync_push(const os_sync_config_t* config, const char* doc_id,
                              const char* progress, float percent);
/// 拉取：输出远端 progress（可为 KOReader 任意串）与百分比。/ Pull the remote progress string and percentage.
os_sync_result_t os_sync_pull(const os_sync_config_t* config, const char* doc_id,
                              char* progress, size_t cap, float* percent);

/// rp1 进度串："rp1|<文件字节数>|<章索引>|<章内字节偏移>|<字号>”。/ rp1 string: "rp1|<size>|<chapter>|<offset>|<px>".
bool os_sync_progress_encode(char* out, size_t cap, uint32_t file_size, uint16_t chapter,
                             uint32_t byte_off, uint8_t px);
/// 只接受 rp1；其它（KOReader XPath 等）返回 false 由调用方按百分比回退。
/// / Accept rp1 only; anything else (KOReader XPaths, …) returns false so callers fall back to percentage.
bool os_sync_progress_decode(const char* in, uint32_t* file_size, uint16_t* chapter,
                             uint32_t* byte_off, uint8_t* px);

/* 设备侧粘合（os_sync_pico.c），宿主测试不链接。/ Device glue (os_sync_pico.c); host tests never link it. */
/// 明文密码即时转 MD5 保存。/ Convert a plain password to MD5 and store it at once.
void os_sync_set_password(const char* plain);
typedef enum {
    OS_SYNC_JOB_AUTH, ///< 测试账号 / Test credentials
    OS_SYNC_JOB_REGISTER, ///< 注册账号 / Register credentials
    OS_SYNC_JOB_PUSH, ///< 上传快照 / Push a snapshot
    OS_SYNC_JOB_PULL, ///< 下载待确认位置 / Pull a position for confirmation
} os_sync_job_t;
/// UI 任务准备快照，最多一个后台请求；note 给出启动结果。/ UI prepares a snapshot for at most one worker; note reports admission.
bool os_sync_job_start(os_sync_job_t job, char* note, size_t cap);
/// UI 非阻塞接收结果；下载成功只准备待确认记录，不写 NVS。/ UI polls without blocking; successful pulls stage a record without NVS writes.
bool os_sync_job_poll(char* note, size_t cap);
/// 是否有正在执行的请求。/ Whether a request is running.
bool os_sync_job_busy(void);
/// 是否有待用户确认的下载进度。/ Whether a downloaded position awaits confirmation.
bool os_sync_pull_pending(void);
/// UI 确认时校验文件与原进度未变，再保存；取消保留本地。/ UI confirms only unchanged file/progress snapshots; cancellation preserves local progress.
bool os_sync_pull_confirm(bool apply, char* note, size_t cap);
/// 离页取消并收齐后台请求，再释放快照。/ Cancel and join the worker before releasing snapshots on exit.
void os_sync_job_cancel(void);

/// 睡眠钩子仅置取消位，不等待网络。/ Sleep hooks only set the cancellation bit and never wait for the network.
void os_sync_job_request_cancel(void);
