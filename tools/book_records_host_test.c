/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 * 中文：真实统计迭代与摘录持久化的隔离回归。
 * English: Isolated regressions for actual stats iteration and excerpt persistence.
 */
#include "book_quotes.h"
#include "book_stats.h"
#include "nvs.h"
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
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
int main(void){
    const uint32_t dates[]={20260928,20260929,20260930,20261001,20261002,20261003,20261004};
    for(unsigned i=0;i<7;++i)assert(book_stats_store_put(dates[i],i==6?8:1));
    assert(book_stats_minutes(20261004)==8 && book_stats_total_minutes(20260928,20261004)==14 && !iter_live);
    book_stats_store_visit(one,NULL);assert(visits==1&&!iter_live);
    size_t a,b;const char*t="第一句。第二句！第三句？";
    assert(book_quote_sentence(t,strlen(t),14,&a,&b)&&a==12&&b==24);
    book_quote_t q={.chapter=2,.byte_off=12,.end_off=24,.pct=18};strcpy(q.path,"/flash/books/a.txt");memcpy(q.text,t+a,b-a);
    assert(book_quotes_add(&q)==ESP_OK);book_quote_t out[BOOK_QUOTES_MAX];assert(book_quotes_list(out,16)==1 && !strcmp(out[0].text,"第二句！") && out[0].byte_off==12);
    assert(book_quotes_add(&q)==ESP_OK && book_quotes_list(out,16)==1);
    fail_save=1;q.byte_off=24;q.end_off=36;assert(book_quotes_add(&q)!=ESP_OK);assert(book_quotes_list(out,16)==1&&out[0].byte_off==12);fail_save=0;
    for(unsigned i=1;i<=20;++i){q.byte_off=i*40;q.end_off=q.byte_off+12;assert(book_quotes_add(&q)==ESP_OK);}
    assert(book_quotes_list(out,16)==16 && out[0].byte_off==800 && out[15].byte_off==200);
    char longtext[900];memset(longtext,'a',899);longtext[899]=0;assert(book_quote_sentence(longtext,899,780,&a,&b)&&a<=780&&b>780&&b-a<BOOK_QUOTE_TEXT_MAX);
    char chinese[901];for(unsigned i=0;i<300;++i)memcpy(chinese+i*3,"字",3);chinese[900]=0;
    assert(book_quote_sentence(chinese,900,780,&a,&b)&&a<=780&&b>780&&b-a<BOOK_QUOTE_TEXT_MAX&&a%3==0&&b%3==0);
    assert(!book_quote_sentence(t,strlen(t),strlen(t),&a,&b));
    assert(book_quote_sentence(" title\r\nnext",12,2,&a,&b)&&a==1&&b==6);
    puts("book_records: real NVS seven-day totals, iterator lifetime, excerpts, anchors, capacity and save failure PASS");
}
