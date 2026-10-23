/* SPDX-License-Identifier: Apache-2.0
 * 中文：真实历史核心/服务的持久化、时钟和内存故障回归。
 * English: Persistence, clock and allocation fault regressions for production history core/service.
 */
#include "os_history_service.h"
#include "os_time.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
static char directory[320];
static int fail_alloc;
void book_stats_store_maintain(void) {}
void book_stats_reconcile(void) {}
void book_stats_archive_media(const char* dir) {(void)dir;}
static os_history_snapshot_t snapshot = {.valid=true,.utc=1760000000,.permille=860,.millivolts=3970,.charge_state=1};
void* heap_caps_calloc(size_t n,size_t size,unsigned caps) { assert(caps==3); return fail_alloc ? NULL : calloc(n,size); }
const char* os_history_directory(void) { return directory; }
void os_history_snapshot(int64_t now,os_history_snapshot_t* out) { (void)now; *out=snapshot; }
int main(int argc,char** argv) {
    assert(argc==2); snprintf(directory,sizeof(directory),"%s/core",argv[1]);
    os_battery_history_t* h=calloc(1,sizeof(*h)),*work=calloc(1,sizeof(*work)),*saved=calloc(1,sizeof(*saved));
    os_battery_history_init(h); int slot=-1;
    assert(os_battery_history_load(directory,h,work,&slot)==OS_BATTERY_LOAD_EMPTY);
    os_battery_sample_t s={.at=1760000000,.permille=0,.millivolts=3300,.charge_state=1};
    assert(os_battery_history_add(h,&s)); assert(!os_battery_history_add(h,&s));
    s.at+=899;assert(!os_battery_history_add(h,&s));s.at++;s.permille=1001;assert(!os_battery_history_add(h,&s));
    s.permille=1000;assert(os_battery_history_add(h,&s));
    assert(os_battery_history_save(directory,h,work,&slot)&&slot==0); *saved=*h;
    s.at+=900;assert(os_battery_history_add(h,&s));assert(os_battery_history_save(directory,h,work,&slot)&&slot==1);
    char path[360];snprintf(path,sizeof(path),"%s/battery-1.bin",directory);FILE*f=fopen(path,"wb");assert(f);fputs("torn",f);fclose(f);
    assert(os_battery_history_load(directory,h,work,&slot)==OS_BATTERY_LOAD_OK&&slot==0&&!memcmp(h,saved,sizeof(*h)));
    snprintf(path,sizeof(path),"%s/battery-0.bin",directory);f=fopen(path,"wb");assert(f);fputc(0,f);fclose(f);
    *saved=*h;assert(os_battery_history_load(directory,h,work,&slot)==OS_BATTERY_LOAD_ERROR&&!memcmp(h,saved,sizeof(*h)));
    os_battery_history_init(h);
    for(unsigned i=0;i<1000;i++){s.at=1760000000+i*900;assert(os_battery_history_add(h,&s));}
    assert(h->count==672&&h->samples[0].at==1760000000+328*900);
    s.at=1760000000+500*900;assert(os_battery_history_add(h,&s));assert(h->samples[h->count-1].at==s.at);
    s.at+=8*86400;assert(os_battery_history_add(h,&s)&&h->count==1);
    assert(!os_battery_history_save("/nonexistent-history-parent-86/data",h,work,&slot));
    snprintf(directory,sizeof(directory),"%s/service",argv[1]);
    fail_alloc=1;os_history_service_poll(0,false);assert(!os_history_battery());fail_alloc=0;
    os_history_service_poll(1000,false);assert(os_history_battery()->count==1&&os_history_battery_pending());
    snapshot.utc+=899;os_history_service_poll(900000,false);assert(os_history_battery()->count==1);
    snapshot.utc++;os_history_service_poll(901000,false);assert(os_history_battery()->count==2);
    // 插卡不把无卡会话混入其他卡。/ Insertion must not mix cardless sessions into another card.
    os_history_service_poll(902000,true);assert(os_history_battery()->count==1&&!os_history_battery_pending());
    assert(os_battery_history_load(directory,h,work,&slot)==OS_BATTERY_LOAD_OK&&h->count==1);
    snapshot.utc+=900;fail_alloc=1;os_history_service_poll(1802000,true);assert(os_history_battery_pending());
    fail_alloc=0;os_history_service_poll(1803000,true);assert(os_history_battery_pending());
    os_history_service_poll(1862000,true);assert(!os_history_battery_pending());
    os_history_service_poll(1863000,false);assert(os_history_battery()->count==1);
    // 两槽损坏时拒绝覆盖旧文件。/ Refuse to overwrite unreadable histories when both slots are corrupt.
    for(int i=0;i<2;i++){snprintf(path,sizeof(path),"%s/battery-%d.bin",directory,i);f=fopen(path,"wb");assert(f);fputc(0,f);fclose(f);}
    os_history_service_poll(1864000,true);assert(os_history_battery_read_error()&&os_history_battery()->count==0);
    os_history_service_poll(2000000,true);assert(os_history_battery_read_error());
    free(h);free(work);free(saved);
    puts("Battery history: sample bounds/0%/rollback/seven days/dual slots/corruption/PSRAM retry/card isolation PASS");
}
