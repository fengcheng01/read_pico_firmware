/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 * 中文：同步刷新期间的触摸采样；只缓存输入，不执行页面动作。
 * English: Sample touch through blocking display updates; buffer input without page actions.
 * 冻结：溢出/读错取消旧手势；切页丢弃旧输入并等原手指松开；睡眠/诊断暂停采样。
 * Frozen: Overflow/errors cancel stale gestures; page switches drop old input until release; sleep/diagnostics pause sampling.
 */
#pragma once
#include "cst836u.h"
#include <stdint.h>
#include <stdbool.h>

/// 初始化一次；分配失败保留前台读取。/ Initialize once, falling back to foreground reads on allocation failure.
void app_touch_input_init(cst836u_handle_t tp);
/// 按采样时刻领取输入；无排队时领取当前快照。/ Receive timestamped input or the latest snapshot when the queue is empty.
esp_err_t app_touch_input_read(cst836u_handle_t tp, cst836u_touch_t* out, int64_t* sampled_ms);
/// 暂停会等在途读取结束，再让睡眠或诊断独占触摸。/ Pause joins an in-flight read before sleep or diagnostics own touch.
void app_touch_input_enable(bool enabled);
/// 丢弃旧页输入，原手指须松开才能操作新页。/ Drop old-page input and require the original finger to lift before operating the new page.
void app_touch_input_reset(void);
