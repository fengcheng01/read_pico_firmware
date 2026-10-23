/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 * 中文：真实统计迭代与摘录持久化的隔离回归。
 * English: Isolated regressions for actual stats iteration and excerpt persistence.
 */
#include "book_quotes.h"
#include "book_stats.h"
#include "book_stats_archive.h"
#include "os_time.h"
#include "nvs.h"
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
typedef struct {char ns[16],key[16];uint8_t bytes[1024];size_t len;uint16_t value;bool used,blob;} cell_t;
static cell_t cells[128];
static char handles[64][16];static int nh, fail_save, iter_live;
struct nvs_iter {int at;char ns[16];};
static cell_t* find(nvs_handle_t h,const char* key){for(int i=0;i<128;++i)if(cells[i].used&&!strcmp(cells[i].ns,handles[h])&&!strcmp(cells[i].key,key))return &cells[i];return NULL;}
static cell_t* put(nvs_handle_t h,const char* key){cell_t* c=find(h,key);if(c)return c;for(int i=0;i<128;++i)if(!cells[i].used){c=&cells[i];c->used=true;snprintf(c->ns,16,"%s",handles[h]);snprintf(c->key,16,"%s",key);return c;}return NULL;}
esp_err_t nvs_open(const char* ns,int mode,nvs_handle_t* h){(void)mode;*h=++nh%63+1;snprintf(handles[*h],16,"%s",ns);return ESP_OK;}
void nvs_close(nvs_handle_t h){(void)h;}
esp_err_t nvs_commit(nvs_handle_t h){(void)h;return ESP_OK;}
esp_err_t nvs_get_u16(nvs_handle_t h,const char* k,uint16_t* v){cell_t*c=find(h,k);if(!c||c->blob)return ESP_ERR_NVS_NOT_FOUND;*v=c->value;return ESP_OK;}
esp_err_t nvs_set_u16(nvs_handle_t h,const char*k,uint16_t v){cell_t*c=put(h,k);assert(c);c->blob=false;c->value=v;return ESP_OK;}
esp_err_t nvs_get_blob(nvs_handle_t h,const char*k,void*b,size_t*n){cell_t*c=find(h,k);if(!c||!c->blob)return ESP_ERR_NVS_NOT_FOUND;if(!b){*n=c->len;return ESP_OK;}if(*n<c->len)return ESP_ERR_NVS_INVALID_LENGTH;memcpy(b,c->bytes,c->len);*n=c->len;return ESP_OK;}
esp_err_t nvs_set_blob(nvs_handle_t h,const char*k,const void*b,size_t n){if(fail_save)return ESP_ERR_NVS_NOT_ENOUGH_SPACE;cell_t*c=put(h,k);assert(c&&n<=1024);c->blob=true;memcpy(c->bytes,b,n);c->len=n;return ESP_OK;}
esp_err_t nvs_erase_key(nvs_handle_t h,const char*k){cell_t*c=find(h,k);if(!c)return ESP_ERR_NVS_NOT_FOUND;c->used=false;return ESP_OK;}
void nvs_release_iterator(nvs_iterator_t it){if(it){--iter_live;free(it);}}
esp_err_t nvs_entry_next(nvs_iterator_t* p){while(++(*p)->at<128){cell_t*c=&cells[(*p)->at];if(c->used&&!c->blob&&!strcmp(c->ns,(*p)->ns))return ESP_OK;}nvs_release_iterator(*p);*p=NULL;return ESP_ERR_NVS_NOT_FOUND;}
esp_err_t nvs_entry_find(const char* part,const char* ns,int type,nvs_iterator_t* p){assert(!strcmp(part,"nvs")&&ns&&type==NVS_TYPE_U16);*p=calloc(1,sizeof(**p));assert(*p);++iter_live;(*p)->at=-1;snprintf((*p)->ns,16,"%s",ns);return nvs_entry_next(p);}
void nvs_entry_info(nvs_iterator_t it,nvs_entry_info_t* out){cell_t*c=&cells[it->at];snprintf(out->namespace_name,16,"%s",c->ns);snprintf(out->key,16,"%s",c->key);}
static unsigned visits;
static bool one(uint32_t d,uint16_t m,void*ctx){(void)d;(void)m;(void)ctx;++visits;return false;}
static void archive_test(const char* root) {
    char path[384],directory[320];snprintf(directory,sizeof(directory),"%s/reading-history",root);
    uint32_t dates[65];
    for(unsigned i=0;i<65;++i){dates[i]=os_time_date_shift(20260601,(int)i);assert(book_stats_store_put(dates[i],(uint16_t)(i+1)));}
    book_stats_store_prune();
    uint16_t m=0;assert(book_stats_store_get(dates[0],&m)&&m==1);
    book_stats_archive_media(directory);book_stats_store_maintain();
    unsigned count=0;for(unsigned i=0;i<128;++i)if(cells[i].used&&!strcmp(cells[i].ns,"rp_stats"))++count;
    assert(count==30&&!iter_live);
    for(unsigned i=0;i<65;++i)assert(book_stats_store_get(dates[i],&m)&&m==i+1);
    assert(book_stats_total_minutes(dates[0],dates[64])==2145);
    book_stats_store_maintain();assert(book_stats_total_minutes(dates[0],dates[64])==2145);
    assert(book_stats_archive_merge(20240229,17));assert(!book_stats_archive_merge(20230229,2));
    assert(book_stats_archive_merge(20241231,22)&&book_stats_archive_merge(20250101,23));
    assert(book_stats_archive_total(20241231,20250101)==45);
    assert(book_stats_archive_merge(20240229,19)&&book_stats_archive_merge(20240229,18));
    assert(book_stats_archive_get(20240229,&m)&&m==19);
    // 第二份损坏，第一份17分钟仍可恢复，未读到的数据不清零覆盖。
    // Corrupt the second copy: recover the first 17 minutes and never overwrite unknown data with zero.
    snprintf(path,sizeof(path),"%s/reading/2024.bin",directory);FILE*f=fopen(path,"r+b");assert(f);
    assert(!fseek(f,59*32+16,SEEK_SET));fputc(0,f);fclose(f);
    book_stats_archive_media(directory);assert(book_stats_archive_get(20240229,&m)&&m==17);
    f=fopen(path,"r+b");assert(f);assert(!fseek(f,59*32,SEEK_SET));fputc(0,f);fclose(f);
    book_stats_archive_media(directory);assert(!book_stats_archive_get(20240229,&m)&&!book_stats_archive_merge(20240229,30));
    // 缺卡期间保留未归档旧记录；再次插卡按max合并，不重复累计。
    // Retain unarchived older records without a card; reinsertion merges by max, never double counts.
    book_stats_archive_media(NULL);
    for(unsigned i=0;i<35;i++)assert(book_stats_store_put(os_time_date_shift(20260701,(int)i),100));
    book_stats_store_prune();assert(book_stats_store_get(20260701,&m)&&m==100);
    book_stats_archive_media("/nonexistent-pico-history-parent-86/archive");book_stats_store_maintain();
    assert(book_stats_store_get(20260701,&m)&&m==100&&book_stats_archive_error());
    book_stats_archive_media(directory);book_stats_store_maintain();assert(book_stats_store_get(20260701,&m)&&m==100);
    // 当前日在换卡后发现更大累计：保留未满分钟部分，下一分钟继续向上。
    // A higher current-day total on insertion retains the sub-minute remainder and continues upward.
    book_stats_observe(0,true,true,20261022);book_stats_observe(30000,true,true,20261022);
    assert(book_stats_archive_merge(20261022,50));book_stats_reconcile();
    book_stats_observe(60000,true,true,20261022);assert(book_stats_minutes(20261022)==51);
    assert(book_stats_total_minutes(20261022,20261022)==51);
    puts("Reading archive: 65-day migration/30-record window/idempotence/max merge/leap/year/dual-copy corruption/no-card/no-prune/live reconciliation PASS");
}
int main(int argc,char**argv){
    assert(argc==2);
    const uint32_t dates[]={20260928,20260929,20260930,20261001,20261002,20261003,20261004};
    for(unsigned i=0;i<7;++i)assert(book_stats_store_put(dates[i],i==6?8:1));
    assert(book_stats_minutes(20261004)==8 && book_stats_total_minutes(20260928,20261004)==14 && !iter_live);
    book_stats_store_visit(one,NULL);assert(visits==1&&!iter_live);
    size_t a,b;const char*t="第一句。第二句！第三句？";
    assert(book_quote_sentence(t,strlen(t),14,&a,&b)&&a==12&&b==24);
    book_quote_t q={.chapter=2,.byte_off=12,.end_off=24,.pct=18};strcpy(q.path,"/flash/books/a.txt");memcpy(q.text,t+a,b-a);
    // v2 章节名快照随记录往返；滚动与锁屏副本都保留。/ v2 chapter-title snapshots
    // round-trip through the rolling records and the lock copy.
    strcpy(q.chapter_title,"第七章 夜航西飞");
    assert(book_quotes_add(&q)==ESP_OK);book_quote_t out[BOOK_QUOTES_MAX];assert(book_quotes_list(out,16)==1 && !strcmp(out[0].text,"第二句！") && out[0].byte_off==12);
    assert(!strcmp(out[0].chapter_title,"第七章 夜航西飞"));
    assert(book_quotes_add(&q)==ESP_OK && book_quotes_list(out,16)==1 && !strcmp(out[0].chapter_title,"第七章 夜航西飞"));
    fail_save=1;q.byte_off=24;q.end_off=36;assert(book_quotes_add(&q)!=ESP_OK);assert(book_quotes_list(out,16)==1&&out[0].byte_off==12);fail_save=0;
    for(unsigned i=1;i<=20;++i){q.byte_off=i*40;q.end_off=q.byte_off+12;assert(book_quotes_add(&q)==ESP_OK);}
    assert(book_quotes_list(out,16)==16 && out[0].byte_off==800 && out[15].byte_off==200);
    // 固定非最新摘录；重读、写失败、滚动替换与坏记录都不偷偷切成最新一句。
    // Pin a non-newest excerpt; reopen, failed writes, rolling eviction and corrupt data never silently choose the newest.
    book_quote_t selected, pinned=out[10];assert(!book_quotes_lock_get(&selected));
    assert(book_quotes_lock_set(&pinned)==ESP_OK&&book_quotes_lock_get(&selected));
    assert(selected.byte_off==pinned.byte_off&&!strcmp(selected.text,pinned.text));
    fail_save=1;assert(book_quotes_lock_set(&out[0])!=ESP_OK);fail_save=0;
    assert(book_quotes_lock_get(&selected)&&selected.byte_off==pinned.byte_off);
    for(unsigned i=21;i<=40;++i){q.byte_off=i*40;q.end_off=q.byte_off+12;assert(book_quotes_add(&q)==ESP_OK);}
    assert(book_quotes_list(out,16)==16&&out[15].byte_off>pinned.byte_off);
    assert(book_quotes_lock_get(&selected)&&selected.byte_off==pinned.byte_off);
    assert(book_quotes_lock_set(NULL)==ESP_ERR_INVALID_ARG);
    nvs_handle_t lh;assert(nvs_open("rp_lockquote",NVS_READWRITE,&lh)==ESP_OK);
    cell_t* locked=find(lh,"chosen");assert(locked);locked->bytes[0]='X';
    assert(!book_quotes_lock_get(&selected));nvs_close(lh);
    assert(book_quotes_list(out,16)==16);
    // v1 旧记录（无章节名字段）照常载入，标题为空回退仅书名。/ Legacy v1 records
    // without the title field load normally with an empty title for book-only sources.
    {
        nvs_handle_t h;assert(nvs_open("rp_quotes",NVS_READWRITE,&h)==ESP_OK);
        static const char* legacy_path="/flash/books/旧书.epub", *legacy_text="旧记录的句子。";
        size_t pn=strlen(legacy_path)+1, tn=strlen(legacy_text)+1;
        uint8_t v1[96]={'R','P','Q',1};
        v1[4]=9; v1[8]=3; v1[9]=0; v1[10]=30; v1[11]=0; v1[12]=0; v1[13]=0; v1[14]=44; v1[15]=0; v1[16]=0; v1[17]=0; v1[18]=50;
        v1[20]=(uint8_t)pn; v1[21]=(uint8_t)(pn>>8); v1[22]=(uint8_t)tn; v1[23]=(uint8_t)(tn>>8);
        memcpy(v1+24,legacy_path,pn); memcpy(v1+24+pn,legacy_text,tn);
        cell_t* slot=find(h,"q00");assert(slot);slot->blob=true;slot->used=true;snprintf(slot->ns,16,"rp_quotes");snprintf(slot->key,16,"q00");
        memcpy(slot->bytes,v1,24+pn+tn);slot->len=24+pn+tn;nvs_close(h);
        assert(book_quotes_list(out,16)==16);
        bool seen=false;for(unsigned i=0;i<16;++i)if(!strcmp(out[i].path,legacy_path)){seen=true;assert(out[i].chapter_title[0]==0&&out[i].pct==50);break;}
        assert(seen);
    }
    puts("quote lock snapshot: explicit non-newest selection, NVS reopen, failed replacement, sixteen-record eviction, invalid/corrupt isolation PASS");
    char longtext[900];memset(longtext,'a',899);longtext[899]=0;assert(book_quote_sentence(longtext,899,780,&a,&b)&&a<=780&&b>780&&b-a<BOOK_QUOTE_TEXT_MAX);
    char chinese[901];for(unsigned i=0;i<300;++i)memcpy(chinese+i*3,"字",3);chinese[900]=0;
    assert(book_quote_sentence(chinese,900,780,&a,&b)&&a<=780&&b>780&&b-a<BOOK_QUOTE_TEXT_MAX&&a%3==0&&b%3==0);
    assert(!book_quote_sentence(t,strlen(t),strlen(t),&a,&b));
    assert(book_quote_sentence(" title\r\nnext",12,2,&a,&b)&&a==1&&b==6);
    puts("book_records: real NVS seven-day totals, iterator lifetime, excerpts, anchors, capacity and save failure PASS");
    archive_test(argv[1]);
}
