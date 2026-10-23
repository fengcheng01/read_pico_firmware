/* SPDX-License-Identifier: Apache-2.0
 * 中文：历史设备桥仅读取快照，验证有效位、0%和显式充电状态。
 * English: The history device bridge reads snapshots only; verify flags, 0% and explicit charging states.
 */
#include "os_history_service.h"
#include "os_time.h"
#include "read_pico_pmu.h"
#include <assert.h>
#include <stdio.h>
static bool ready,clock_ready;
static pmu_snapshot_t snapshot;
bool read_pico_pmu_ready(void){return ready;}
const pmu_snapshot_t* read_pico_pmu_get(void){return &snapshot;}
bool os_time_cached_utc(int64_t now,uint32_t* utc){assert(now==1000);*utc=1760000000;return clock_ready;}
int main(void){
    os_history_snapshot_t out;
    os_history_snapshot(1000,&out);assert(!out.valid&&out.permille==-1);
    ready=true;snapshot.status_ok=true;snapshot.soc_permille=860;
    os_history_snapshot(1000,&out);assert(!out.valid&&out.permille==-1);
    snapshot.flags=PMU_STATUS_BATTERY_VALID;snapshot.battery_mv=3900;snapshot.charge_state=2;
    os_history_snapshot(1000,&out);assert(!out.valid&&out.permille==860&&out.charge_state==2);
    clock_ready=true;os_history_snapshot(1000,&out);assert(out.valid&&out.utc==1760000000);
    snapshot.soc_permille=0;snapshot.charge_state=3;snapshot.flags|=16;
    os_history_snapshot(1000,&out);assert(out.valid&&out.permille==0&&out.charge_state==3);
    snapshot.soc_permille=1001;os_history_snapshot(1000,&out);assert(!out.valid&&out.permille==-1);
    puts("History PMU bridge: valid flag, unknown/0%, uncalibrated time, cached UTC and explicit charge states PASS");
}
