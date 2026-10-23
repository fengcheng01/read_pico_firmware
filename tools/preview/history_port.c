/* SPDX-License-Identifier: Apache-2.0
 * 中文：隔离预览历史目录及明确的模拟电量边界。
 * English: Isolated preview history directory and explicit simulated battery boundary.
 */
#include "os_history_service.h"
#include "os_time.h"
#include <stdio.h>
#include <string.h>
static char s_directory[320];
void preview_history_directory(const char* frame) {
    snprintf(s_directory, sizeof(s_directory), "%s", frame);
    char* slash = strrchr(s_directory, '/');
    if (slash) snprintf(slash + 1, sizeof(s_directory) - (size_t)(slash + 1 - s_directory), "history");
    else snprintf(s_directory, sizeof(s_directory), "history");
}
const char* os_history_directory(void) { return s_directory; }
bool os_time_cached_utc(int64_t now, uint32_t* utc) {
    (void)now;
    if (os_time_info()->state != OS_TIME_VALID) return false;
    *utc = os_time_info()->unix_utc; return true;
}
void os_history_snapshot(int64_t now, os_history_snapshot_t* out) {
    memset(out, 0, sizeof(*out));
    out->permille = os_time_battery_permille(); out->millivolts = 3900; out->charge_state = 1;
    out->valid = out->permille >= 0 && os_time_cached_utc(now, &out->utc);
}
