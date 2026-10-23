/* SPDX-License-Identifier: Apache-2.0
 * 中文：按年、逐日双副本的阅读分钟归档，不管理卡挂载。
 * English: Yearly reading-minute archives with two copies per day; card mounts belong elsewhere.
 * 冻结：只合并较大累计值，回读成功才允许清理对应NVS记录。
 * Frozen: Merge only higher cumulative totals; NVS pruning requires successful readback.
 */
#pragma once
#include <stdbool.h>
#include <stdint.h>
/// 挂载沿更新目录并失效缓存；NULL表示缺卡。/ Update directory and invalidate cache at mount edges; NULL means absent.
void book_stats_archive_media(const char* directory);
/// 挂载缓存，不探卡。/ Cached mount state without probing.
bool book_stats_archive_ready(void);
/// 读单日；无记录为零，损坏/IO失败返回false。/ Read a day; absent records are zero, corruption/IO returns false.
bool book_stats_archive_get(uint32_t date, uint16_t* minutes);
/// 单调合并并同步回读，失败保留另一个副本。/ Monotonic merge with sync/readback, preserving the other copy on failure.
bool book_stats_archive_merge(uint32_t date, uint16_t minutes);
/// 闭区间归档总量。/ Archived inclusive-range total.
uint32_t book_stats_archive_total(uint32_t from, uint32_t to);
/// 最近的存储故障状态。/ Most recent storage failure state.
bool book_stats_archive_error(void);
/// 仅新一轮完整迁移开始时清故障，后续任一失败重新置位。/ Clear only at the start of a full migration round; any later failure sets it again.
void book_stats_archive_clear_error(void);
/// 挂载沿代数，用于跳过无变化迁移。/ Mount-edge generation for skipping unchanged migrations.
unsigned book_stats_archive_generation(void);
