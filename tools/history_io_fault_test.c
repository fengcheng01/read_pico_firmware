/* SPDX-License-Identifier: Apache-2.0
 * 中文：对生产双槽/双副本存储注入短写、同步、关闭与回读失败。
 * English: Inject short writes, sync, close and readback failures into production dual-copy storage.
 */
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
static int fault;
static size_t injected_write(const void* p,size_t n,size_t count,FILE*f) {
    if(fault==1){fault=0;return fwrite(p,n,count/2,f);}return fwrite(p,n,count,f);
}
static int injected_sync(int fd) {if(fault==2){fault=0;errno=EIO;return -1;}return fsync(fd);}
static int injected_close(FILE*f) {int result=fclose(f);if(fault==3){fault=0;return EOF;}return result;}
static FILE* injected_open(const char* p,const char* mode) {
    if(fault==4&&!strcmp(mode,"rb")){fault=0;errno=EIO;return NULL;}return fopen(p,mode);
}
#define fwrite injected_write
#define fsync injected_sync
#define fclose injected_close
#define fopen injected_open
#include "../main/os/os_battery_history.c"
#include "../main/book/book_stats_archive.c"
#undef fwrite
#undef fsync
#undef fclose
#undef fopen
int main(int argc,char**argv) {
    assert(argc==2);
    os_battery_history_t* h=calloc(1,sizeof(*h)),*work=calloc(1,sizeof(*h)),*loaded=calloc(1,sizeof(*h));
    for(int kind=1;kind<=4;++kind){
        char directory[320],path[360];snprintf(directory,sizeof(directory),"%s/fault-%d",argv[1],kind);
        os_battery_history_init(h);int slot=-1;
        os_battery_sample_t sample={.at=1760000000,.permille=750,.millivolts=3900,.charge_state=1};
        assert(os_battery_history_add(h,&sample)&&os_battery_history_save(directory,h,work,&slot));
        os_battery_history_t original=*h;
        sample.at+=900;assert(os_battery_history_add(h,&sample));fault=kind;
        assert(!os_battery_history_save(directory,h,work,&slot)&&slot==0&&fault==0);
        snprintf(path,sizeof(path),"%s/battery-0.bin",directory);FILE*f=fopen(path,"rb");assert(f);
        assert(fread(loaded,1,sizeof(*loaded),f)==sizeof(*loaded)&&!memcmp(loaded,&original,sizeof(original)));fclose(f);
        assert(os_battery_history_load(directory,loaded,work,&slot)==OS_BATTERY_LOAD_OK&&loaded->count>=1);
        book_stats_archive_media(directory);uint32_t date=20260600+(unsigned)kind;
        assert(book_stats_archive_merge(date,10));
        snprintf(path,sizeof(path),"%s/reading/2026.bin",directory);f=fopen(path,"rb");assert(f);
        record_t baseline[2];assert(!fseek(f,(long)(151+kind-1)*32,SEEK_SET));assert(fread(baseline,1,32,f)==32);fclose(f);
        assert(good(&baseline[0],date)&&good(&baseline[1],date));
        // close故障在读旧文件之后、写事务关闭时触发；只有一次fclose。
        // The close fault triggers at the write transaction close; the original is read using the same FILE.
        fault=kind;assert(!book_stats_archive_merge(date,11)&&fault==0);
        book_stats_archive_media(directory);uint16_t minutes=0;
        assert(book_stats_archive_get(date,&minutes)&&minutes>=10&&minutes<=11);
        snprintf(path,sizeof(path),"%s/reading/2026.bin",directory);f=fopen(path,"rb");assert(f);
        record_t first;assert(!fseek(f,(long)(151+kind-1)*32,SEEK_SET));assert(fread(&first,1,16,f)==16);fclose(f);
        assert(good(&first,date)&&first.minutes==10);
    }
    free(h);free(work);free(loaded);
    puts("History IO faults: short-write/fsync/close/readback failure preserve last valid battery slot and reading copy PASS");
}
