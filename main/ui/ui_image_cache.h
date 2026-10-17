/* SPDX-License-Identifier: Apache-2.0
 * 中文：查看器的三槽灰度缓存与唯一后台解码任务，不写文件、不绘图。
 * English: Three grayscale viewer slots and a sole background decoder, without writes or drawing.
 * 冻结：所有权留在缓存；退出/媒体丢失汇合后释放，睡眠仅保留当前展示成品供唤醒重绘。
 * Frozen: The cache owns outputs; join and release on exit/media loss, retaining only the displayed output for wake redraw.
 */
#pragma once
#include <stdbool.h>
#include <stdint.h>

/// true表示完成，空输出表示失败；成功输出借用至下一次poll或clear。
/// True means complete, NULL means failed; successful output is borrowed until the next poll or clear.
bool ui_image_cache_poll(const char* path, uint8_t** gray, uint16_t* width, uint16_t* height);
/// 仅空闲且有内存时预解码一张；不替换当前显示的槽。
/// Predecode one image only while idle with memory available, preserving the displayed slot.
void ui_image_cache_prefetch(const char* path);
/// 用户重新选择时允许失败项重试，成功成品继续复用。
/// Allow failed entries to retry on explicit selection, retaining successful outputs.
void ui_image_cache_retry(const char* path);
/// 等待唯一任务并释放所有输出、注销睡眠钩子。
/// Join the sole task, release every output and unregister the sleep hook.
void ui_image_cache_clear(void);
