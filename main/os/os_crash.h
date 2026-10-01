/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 *
 * 中文：异常重启记录。IDF v6 未公开 panic 钩子，本模块记录的是「上次复位
 * 原因」而非反栈：开机读复位原因，异常复位在内置 FAT 追加一行有界日志。
 * English: Abnormal-reset logging. IDF v6 exposes no panic hook, so this
 * records the last reset reason instead of a backtrace: boot reads the reason
 * and appends one bounded line to the internal FAT.
 *
 * 冻结：不写 TF 卡、不格式化、不改动分区；日志只增不删（有界裁剪尾部除外）。
 * Frozen: Never write the TF card, format storage or touch partitions; the log
 * only appends (bounded tail trimming excepted).
 */
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/// 内置 FAT 上的日志路径。/ Log path on the internal FAT.
#define OS_CRASH_LOG_PATH "/flash/crash.log"
/// 日志字节上限与裁剪后保留量。/ Log byte cap and the retained tail after trimming.
#define OS_CRASH_LOG_MAX 4096
#define OS_CRASH_LOG_KEEP 2048

/// esp_reset_reason_t 数值 → 稳定名（"POWERON"/"PANIC"/…，未知给 "RST?"）。
/// Map esp_reset_reason_t values to stable names; unknown values give RST?.
const char* os_crash_reset_name(int reason);
/// 崩溃类复位（panic/看门狗/掉电）才记日志。/ Only crash-like resets (panic/watchdogs/brownout) log.
bool os_crash_reason_abnormal(int reason);
/// 一行记录："<unix> <NAME>\n"；unix 为 0 表示时间未校准。/ One line "<unix> <NAME>\n"; unix 0 marks uncalibrated time.
size_t os_crash_format_record(char* out, size_t cap, uint32_t unix_utc, int reason);
/// 汇总一行："<n> 条 · 最近 <NAME> <unix>"；无记录返回 false。
/// Summarize as "<n> entries · latest <NAME> <unix>"; false without records.
bool os_crash_format_summary(const char* log_text, size_t len, char* out, size_t cap);
/// 超过 keep 后从行首边界保留最新内容；返回新长度。/ Over keep bytes, retain the newest from a line boundary; return the new length.
size_t os_crash_trim_tail(const char* text, size_t len, size_t keep);

/*
 * 设备桥接口：实现于 os_crash_pico.c；宿主预览/测试用各自夹具替换。
 * Device bridge: implemented in os_crash_pico.c; host previews and tests substitute fixtures.
 */
/// 开机早期读一次复位原因并暂存（幂等）。/ Read and stash the reset reason once early at boot (idempotent).
void os_crash_boot_check(void);
/// 内置存储挂载后把暂存记录写入日志文件（幂等，失败静默保留待重试）。
/// Append the stashed record after the internal FAT mounts (idempotent; failures keep it for retry).
void os_crash_flush(void);
/// 读取日志文件汇总到 out；无日志或不可读返回 false。/ Read the log file into a summary; false when absent or unreadable.
bool os_crash_summary(char* out, size_t cap);

typedef enum {
    OS_CRASH_SUMMARY_EMPTY, ///< 确实无记录 / No records
    OS_CRASH_SUMMARY_READY, ///< 已读摘要 / Summary ready
    OS_CRASH_SUMMARY_ERROR, ///< 无法读取或解析 / Read or parse failure
} os_crash_summary_state_t;
/// 区分无记录和错误的只读摘要。/ Read-only summary distinguishing absence from errors.
os_crash_summary_state_t os_crash_summary_read(char* out, size_t cap);
