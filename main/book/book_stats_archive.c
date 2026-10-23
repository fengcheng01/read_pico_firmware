/* SPDX-License-Identifier: Apache-2.0
 * 中文：固定366日槽的年度文件，每日两份校验记录，不复制第三方实现。
 * English: Year files with 366 fixed day slots and two checked records per day, independently implemented.
 * 冻结：缺卡不创建挂载根；失败不回退为零后覆盖未知数据，不原地覆盖最后有效副本。
 * Frozen: Never create mount roots without media, overwrite unknown data after a failed read, or overwrite the last valid copy.
 */
#include "book_stats_archive.h"
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
typedef struct { uint32_t magic, date; uint16_t minutes, reserved; uint32_t checksum; } record_t;
_Static_assert(sizeof(record_t)==16,"Stable archive record");
static const char* s_root;
static bool s_error;
static unsigned s_generation;
static unsigned s_cache_year;
static uint16_t s_cache[366];
static uint8_t s_known[366];
static bool leap(unsigned y) { return y%4==0 && (y%100!=0 || y%400==0); }
static int ordinal(uint32_t date) {
    unsigned y=date/10000,m=date/100%100,d=date%100;
    const unsigned days[]={31,28,31,30,31,30,31,31,30,31,30,31};
    if(y<2020||y>2099||m<1||m>12||!d||d>days[m-1]+(m==2&&leap(y)))return -1;
    unsigned n=d-1;for(unsigned i=1;i<m;++i)n+=days[i-1]+(i==2&&leap(y));return (int)n;
}
static uint32_t sum(const record_t* r) {
    const unsigned char* p=(const unsigned char*)r;uint32_t v=2166136261u;
    for(unsigned i=0;i<12;i++)v=(v^p[i])*16777619u;
    return v;
}
static bool good(const record_t* r,uint32_t date) { return r->magic==0x50525331 && r->date==date && !r->reserved && r->checksum==sum(r); }
static bool empty(const record_t* r) { const record_t zero={0};return !memcmp(r,&zero,sizeof(zero)); }
static bool path_of(char* out,size_t cap,unsigned year) {
    if(!s_root)return false;
    int n=snprintf(out,cap,"%s/reading/%04u.bin",s_root,year);return n>0&&(size_t)n<cap;
}
static int read_pair(FILE* f,int day,uint32_t date,record_t pair[2]) {
    memset(pair,0,32);
    if(fseek(f,(long)day*32,SEEK_SET))return -1;
    size_t n=fread(pair,1,32,f);
    if(ferror(f))return -1;
    if(n<32)clearerr(f);
    bool a=good(&pair[0],date),b=good(&pair[1],date);
    if(a||b)return !a?1:!b?0:pair[1].minutes>pair[0].minutes?1:0;
    return empty(&pair[0])&&empty(&pair[1]) ? -2 : -1;
}
void book_stats_archive_media(const char* directory) {
    s_root=directory;s_cache_year=0;s_error=false;memset(s_known,0,sizeof(s_known));++s_generation;
}
unsigned book_stats_archive_generation(void) { return s_generation; }
bool book_stats_archive_ready(void) { return s_root!=NULL; }
bool book_stats_archive_error(void) { return s_error; }
void book_stats_archive_clear_error(void) { s_error=false; }
bool book_stats_archive_get(uint32_t date,uint16_t* minutes) {
    int day=ordinal(date);if(day<0||!minutes||!s_root)return false;
    unsigned year=date/10000;
    if(s_cache_year!=year){s_cache_year=year;memset(s_known,0,sizeof(s_known));}
    if(s_known[day]){*minutes=s_cache[day];return true;}
    char path[320];if(!path_of(path,sizeof(path),year))return false;
    FILE* f=fopen(path,"rb");int which=-2;record_t pair[2];
    if(!f){if(errno!=ENOENT){s_error=true;return false;}}
    else {which=read_pair(f,day,date,pair);if(fclose(f))which=-1;}
    if(which==-1){s_error=true;return false;}
    *minutes=which<0?0:pair[which].minutes;s_cache[day]=*minutes;s_known[day]=1;return true;
}
static bool merge_copy(uint32_t date,uint16_t minutes,bool initialize) {
    int day=ordinal(date);char path[320],directory[320];
    if(day<0||!path_of(path,sizeof(path),date/10000))return false;
    if(mkdir(s_root,0755)&&errno!=EEXIST){s_error=true;return false;}
    int n=snprintf(directory,sizeof(directory),"%s/reading",s_root);
    if(n<0||(size_t)n>=sizeof(directory)||(mkdir(directory,0755)&&errno!=EEXIST)){s_error=true;return false;}
    FILE* f=fopen(path,"r+b");
    if(!f&&errno==ENOENT)f=fopen(path,"w+b");
    if(!f){s_error=true;return false;}
    record_t pair[2];int which=read_pair(f,day,date,pair);
    if(which==-1){fclose(f);s_error=true;return false;}
    if(which>=0&&pair[which].minutes>=minutes){
        minutes=pair[which].minutes;
        if(good(&pair[1-which],date)){bool ok=fclose(f)==0;if(!ok)s_error=true;return ok;}
    }
    // 显式补零而非依赖FAT空洞内容。/ Fill gaps explicitly rather than relying on FAT hole contents.
    bool ok=fseek(f,0,SEEK_END)==0;long end=ok?ftell(f):-1,target=(long)(day+1)*32;
    if(end<0)ok=false;
    const unsigned char zero[32]={0};
    while(ok&&end<target){size_t len=(size_t)(target-end);if(len>32)len=32;ok=fwrite(zero,1,len,f)==len;end+=(long)len;}
    int next=which==0?1:0;long offset=(long)day*32+next*16;
    record_t r={.magic=0x50525331,.date=date,.minutes=minutes};r.checksum=sum(&r);
    if(ok)ok=fseek(f,offset,SEEK_SET)==0&&fwrite(&r,1,16,f)==16;
    if(fflush(f)||fsync(fileno(f)))ok=false;
    if(fclose(f))ok=false;
    if(ok){f=fopen(path,"rb");record_t verify={0};ok=f&&fseek(f,offset,SEEK_SET)==0&&fread(&verify,1,16,f)==16&&!memcmp(&verify,&r,16);if(f&&fclose(f))ok=false;}
    if(s_cache_year==date/10000)s_known[day]=0;
    if(!ok)s_error=true;
    // 首次归档也建立第二副本；任何一步失败都不能清理NVS。
    // Establish a second copy on first archival too; failure at either step must retain NVS.
    if(ok&&which==-2&&initialize)return merge_copy(date,minutes,false);
    return ok;
}
bool book_stats_archive_merge(uint32_t date,uint16_t minutes) { return merge_copy(date,minutes,true); }
uint32_t book_stats_archive_total(uint32_t from,uint32_t to) {
    if(from>to||!s_root)return 0;
    uint32_t total=0;
    unsigned first=from/10000,last=to/10000;if(first<2020)first=2020;if(last>2099)last=2099;
    for(unsigned y=first;y<=last;++y)for(unsigned m=1;m<=12;++m)for(unsigned d=1;d<=31;++d){
        uint32_t date=y*10000+m*100+d;uint16_t minutes=0;
        if(date>=from&&date<=to&&ordinal(date)>=0&&book_stats_archive_get(date,&minutes))total+=minutes;
    }
    return total;
}
