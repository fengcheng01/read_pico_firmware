/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 * 中文：历史采样的Pico只读快照桥，不主动访问I2C或重挂载卡。
 * English: Read-only Pico history snapshots without active I2C or card remounts.
 * 冻结：沿用已有PMU有效位与充电状态，不从原始充电引脚猜状态。
 * Frozen: Use existing PMU validity and charging state, never infer status from the raw charge pin.
 */
#include "os_history_service.h"
#include "os_time.h"
#include "read_pico_pmu.h"
#include <string.h>
const char* os_history_directory(void) { return "/sdcard/.pagerest"; }
void os_history_snapshot(int64_t now, os_history_snapshot_t* out) {
    memset(out, 0, sizeof(*out)); out->permille = -1;
    const pmu_snapshot_t* pmu = read_pico_pmu_ready() ? read_pico_pmu_get() : NULL;
    if (!pmu || !pmu->status_ok || !(pmu->flags & PMU_STATUS_BATTERY_VALID) || pmu->soc_permille > 1000) return;
    out->permille = pmu->soc_permille; out->millivolts = pmu->battery_mv;
    out->charge_state = pmu->charge_state;
    out->valid = os_time_cached_utc(now, &out->utc);
}
