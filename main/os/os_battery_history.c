/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 * 中文：电量历史的确定性采样和双槽持久化；内存由调用方提供。
 * English: Deterministic battery sampling and dual-slot persistence with caller-owned memory.
 * 冻结：不创建卡根目录，不改用户文件，不把回读成功冒充断电验证。
 * Frozen: Never create the card root, modify user files or treat readback as power-loss acceptance.
 */
#include "os_battery_history.h"
#include "os_time.h"
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define BATTERY_MAGIC UINT32_C(0x50424831)
_Static_assert(sizeof(os_battery_sample_t) == 12, "Stable battery sample layout");
static uint32_t checksum(const os_battery_history_t* h) {
    const uint8_t* p = (const uint8_t*)h;
    uint32_t sum = 2166136261u;
    for (size_t i = 0; i < sizeof(*h); ++i)
        if (i < offsetof(os_battery_history_t, checksum) || i >= offsetof(os_battery_history_t, checksum) + 4)
            sum = (sum ^ p[i]) * 16777619u;
    return sum;
}
void os_battery_history_init(os_battery_history_t* h) {
    memset(h, 0, sizeof(*h)); h->magic = BATTERY_MAGIC; h->version = 1;
}
static bool sample_valid(const os_battery_sample_t* s) {
    return s->at >= OS_TIME_UNIX_MIN && s->at < OS_TIME_UNIX_MAX && s->permille <= 1000 &&
        s->millivolts >= 2000 && s->millivolts <= 6000 && s->charge_state <= 4 &&
        !s->reserved[0] && !s->reserved[1] && !s->reserved[2];
}
static bool valid(const os_battery_history_t* h) {
    if (h->magic != BATTERY_MAGIC || h->version != 1 || h->count > OS_BATTERY_SAMPLES ||
        h->reserved[0] || h->reserved[1] || h->reserved[2] || h->checksum != checksum(h)) return false;
    for (size_t i = 0; i < h->count; ++i)
        if (!sample_valid(&h->samples[i]) || (i && h->samples[i].at <= h->samples[i - 1].at)) return false;
    const os_battery_sample_t empty = {0};
    for (size_t i = h->count; i < OS_BATTERY_SAMPLES; ++i)
        if (memcmp(&h->samples[i], &empty, sizeof(empty))) return false;
    return true;
}
bool os_battery_history_add(os_battery_history_t* h, const os_battery_sample_t* s) {
    if (!h || !s || !sample_valid(s)) return false;
    size_t n = h->count;
    if (n > OS_BATTERY_SAMPLES) return false;
    while (n && h->samples[n - 1].at > s->at) --n;
    if (n && s->at - h->samples[n - 1].at < OS_BATTERY_INTERVAL) {
        if (n == h->count) return false;
        h->count = (uint32_t)n;
        memset(h->samples + n, 0, (OS_BATTERY_SAMPLES - n) * sizeof(*s));
        return true;
    }
    size_t drop = 0;
    while (drop < n && s->at - h->samples[drop].at > OS_BATTERY_WINDOW) ++drop;
    if (n - drop == OS_BATTERY_SAMPLES) ++drop;
    if (drop) { memmove(h->samples, h->samples + drop, (n - drop) * sizeof(*s)); n -= drop; }
    h->samples[n++] = *s; h->count = (uint32_t)n;
    memset(h->samples + n, 0, (OS_BATTERY_SAMPLES - n) * sizeof(*s));
    return true;
}
static bool file_path(char path[320], const char* directory, int slot) {
    int n = snprintf(path, 320, "%s/battery-%d.bin", directory, slot);
    return n > 0 && n < 320;
}
static int read_slot(const char* directory, int slot, os_battery_history_t* out) {
    char path[320]; if (!file_path(path, directory, slot)) return -1;
    FILE* f = fopen(path, "rb");
    if (!f) return errno == ENOENT ? 0 : -1;
    bool ok = fread(out, 1, sizeof(*out), f) == sizeof(*out) && fgetc(f) == EOF && !ferror(f);
    if (fclose(f)) ok = false;
    return ok && valid(out) ? 1 : -1;
}
os_battery_load_t os_battery_history_load(const char* dir, os_battery_history_t* h,
                                        os_battery_history_t* work, int* slot) {
    int a = read_slot(dir, 0, work); uint32_t sequence = a == 1 ? work->sequence : 0;
    // 候选暂借调用方work，第二槽比较后才决定是否替换当前序列。
    // Use caller work for candidates and replace the current series only after slot selection.
    if (a == 1) {
        int b = read_slot(dir, 1, work);
        if (b == 1 && (int32_t)(work->sequence - sequence) > 0) { *h = *work; *slot = 1; return OS_BATTERY_LOAD_OK; }
        if (read_slot(dir, 0, work) != 1) return OS_BATTERY_LOAD_ERROR;
        *h = *work; *slot = 0; return OS_BATTERY_LOAD_OK;
    }
    int b = read_slot(dir, 1, work);
    if (b == 1) { *h = *work; *slot = 1; return OS_BATTERY_LOAD_OK; }
    if (!a && !b) { os_battery_history_init(h); *slot = -1; return OS_BATTERY_LOAD_EMPTY; }
    return OS_BATTERY_LOAD_ERROR;
}
bool os_battery_history_save(const char* dir, os_battery_history_t* h,
                             os_battery_history_t* work, int* slot) {
    if (mkdir(dir, 0755) && errno != EEXIST) return false;
    int next = *slot == 0 ? 1 : 0; char path[320];
    if (!file_path(path, dir, next)) return false;
    uint32_t previous = h->sequence, previous_sum = h->checksum;
    ++h->sequence; h->checksum = checksum(h);
    if (!valid(h)) { h->sequence = previous; h->checksum = previous_sum; return false; }
    FILE* f = fopen(path, "wb"); bool ok = false;
    if (f) {
        ok = fwrite(h, 1, sizeof(*h), f) == sizeof(*h);
        if (fflush(f) || fsync(fileno(f))) ok = false;
        if (fclose(f)) ok = false;
    }
    if (ok) ok = read_slot(dir, next, work) == 1 && !memcmp(h, work, sizeof(*h));
    if (ok) *slot = next;
    else { h->sequence = previous; h->checksum = previous_sum; }
    return ok;
}
