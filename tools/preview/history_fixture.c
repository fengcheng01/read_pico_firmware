/* SPDX-License-Identifier: Apache-2.0
 * 中文：仅供预览的明确历史夹具，不进入设备构建。
 * English: Explicit preview history fixtures, excluded from device builds.
 */
#include "os_history_service.h"
#include "os_time.h"
#include "book_stats.h"
#include <stdlib.h>
void preview_history_fixture(int64_t now) {
    os_battery_history_t* h=calloc(1,sizeof(*h)),*work=calloc(1,sizeof(*h));
    if(!h||!work){free(h);free(work);return;}
    os_battery_history_init(h);int slot=-1;
    uint32_t end=os_time_info()->unix_utc;
    for(unsigned i=0;i<96;++i){
        if(i>=30&&i<40)continue;
        os_battery_sample_t sample={.at=end-(95-i)*900,.permille=(uint16_t)(900-i*2),.millivolts=3900,.charge_state=1};
        os_battery_history_add(h,&sample);
    }
    os_battery_history_save(os_history_directory(),h,work,&slot);free(h);free(work);
    uint32_t today=0;os_time_date_key(&today);
    for(int i=0;today&&i<90;++i)book_stats_store_put(os_time_date_shift(today,-i),(uint16_t)(10+i*17%120));
    os_history_service_poll(now,true);
}
