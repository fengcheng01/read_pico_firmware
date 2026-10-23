/* SPDX-FileCopyrightText: 2026 mindreset
 *
 * 中文：宿主单测的日志空实现；吞掉 tag 与格式串避免未用告警。/ English: No-op logging for host
 * unit tests; consume tag and format so unused-argument warnings stay silent.
 */
#pragma once
#include <stdarg.h>
static inline void host_log_noop(const char* tag, const char* fmt, ...) {
    (void)tag; (void)fmt;
}
#define ESP_LOGI(tag, fmt, ...) host_log_noop(tag, fmt __VA_OPT__(,) __VA_ARGS__)
#define ESP_LOGW(tag, fmt, ...) host_log_noop(tag, fmt __VA_OPT__(,) __VA_ARGS__)
