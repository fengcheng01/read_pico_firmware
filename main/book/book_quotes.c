/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 * 中文：小端版本化摘录槽，保存原文而非书签文案，坏记录隔离。
 * English: Versioned little-endian excerpt slots store source text and isolate corrupt records.
 */
#include "book_quotes.h"
#include "nvs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define HEADER 24
#define RECORD_MAX (HEADER + BOOK_STORE_PATH_MAX + BOOK_QUOTE_TEXT_MAX)
static unsigned s_revision;
static uint32_t get32(const uint8_t* p) { return p[0] | (uint32_t)p[1]<<8 | (uint32_t)p[2]<<16 | (uint32_t)p[3]<<24; }
static void put32(uint8_t* p, uint32_t v) { for (unsigned i=0;i<4;++i) p[i]=(uint8_t)(v>>(i*8)); }
static void key_of(unsigned slot, char key[8]) { snprintf(key,8,"q%02u",slot); }
static bool load(nvs_handle_t h, unsigned slot, book_quote_t* q) {
    char key[8]; key_of(slot,key);
    uint8_t b[RECORD_MAX]; size_t len=sizeof(b);
    if(nvs_get_blob(h,key,b,&len)!=ESP_OK || len<HEADER || memcmp(b,"RPQ\1",4)) return false;
    size_t pn=b[20]|(size_t)b[21]<<8, tn=b[22]|(size_t)b[23]<<8;
    if(pn<2 || pn>sizeof(q->path) || tn<2 || tn>sizeof(q->text) || len!=HEADER+pn+tn ||
       strnlen((char*)b+HEADER,pn)!=pn-1 || strnlen((char*)b+HEADER+pn,tn)!=tn-1 || b[18]>100) return false;
    *q=(book_quote_t){.seq=get32(b+4),.chapter=b[8]|(uint16_t)b[9]<<8,
        .byte_off=get32(b+10),.end_off=get32(b+14),.pct=b[18]};
    if(q->end_off<=q->byte_off) return false;
    memcpy(q->path,b+HEADER,pn);memcpy(q->text,b+HEADER+pn,tn);return true;
}
size_t book_quotes_list(book_quote_t* out,size_t cap) {
    if(!out || !cap) return 0;
    nvs_handle_t h;if(nvs_open("rp_quotes",NVS_READONLY,&h)!=ESP_OK)return 0;
    size_t count=0;
    for(unsigned i=0;i<BOOK_QUOTES_MAX;++i){
        book_quote_t q;if(!load(h,i,&q))continue;
        size_t at=0;while(at<count && out[at].seq>=q.seq)++at;
        if(at>=cap)continue;
        if(count<cap)++count;
        memmove(out+at+1,out+at,(count-at-1)*sizeof(*out));out[at]=q;
    }
    nvs_close(h);return count;
}
esp_err_t book_quotes_add(const book_quote_t* q) {
    if(!q || !q->path[0] || !q->text[0] || q->end_off<=q->byte_off || q->pct>100 ||
       strnlen(q->path,sizeof(q->path))==sizeof(q->path) || strnlen(q->text,sizeof(q->text))==sizeof(q->text))return ESP_ERR_INVALID_ARG;
    nvs_handle_t h;esp_err_t err=nvs_open("rp_quotes",NVS_READWRITE,&h);if(err!=ESP_OK)return err;
    unsigned slot=BOOK_QUOTES_MAX;uint32_t newest=0,oldest=UINT32_MAX;unsigned oldest_slot=0;
    for(unsigned i=0;i<BOOK_QUOTES_MAX;++i){
        book_quote_t prior;if(!load(h,i,&prior)){
            char key[8];key_of(i,key);size_t len=0;
            esp_err_t check=nvs_get_blob(h,key,NULL,&len);
            if(check==ESP_ERR_NVS_NOT_FOUND && slot==BOOK_QUOTES_MAX)slot=i;
            else if(check!=ESP_OK && check!=ESP_ERR_NVS_NOT_FOUND){nvs_close(h);return check;}
            continue;
        }
        if(prior.seq>newest)newest=prior.seq;
        if(prior.seq<oldest){oldest=prior.seq;oldest_slot=i;}
        if(!strcmp(prior.path,q->path) && prior.chapter==q->chapter && prior.byte_off==q->byte_off)slot=i;
    }
    if(newest==UINT32_MAX){nvs_close(h);return ESP_ERR_INVALID_STATE;}
    if(slot==BOOK_QUOTES_MAX && oldest==UINT32_MAX){nvs_close(h);return ESP_ERR_INVALID_STATE;}
    if(slot==BOOK_QUOTES_MAX)slot=oldest_slot;
    uint8_t b[RECORD_MAX]={ 'R','P','Q',1 };size_t pn=strlen(q->path)+1,tn=strlen(q->text)+1;
    put32(b+4,newest+1);b[8]=(uint8_t)q->chapter;b[9]=(uint8_t)(q->chapter>>8);
    put32(b+10,q->byte_off);put32(b+14,q->end_off);b[18]=q->pct;
    b[20]=(uint8_t)pn;b[21]=(uint8_t)(pn>>8);b[22]=(uint8_t)tn;b[23]=(uint8_t)(tn>>8);
    memcpy(b+HEADER,q->path,pn);memcpy(b+HEADER+pn,q->text,tn);
    char key[8];key_of(slot,key);err=nvs_set_blob(h,key,b,HEADER+pn+tn);
    if(err==ESP_OK)err=nvs_commit(h);
    nvs_close(h);
    if(err==ESP_OK)++s_revision;
    return err;
}
unsigned book_quotes_revision(void){return s_revision;}
static size_t char_size(const char* text,size_t len,size_t at){
    unsigned char c=(unsigned char)text[at];size_t n=c<128?1:(c&0xe0)==0xc0?2:(c&0xf0)==0xe0?3:(c&0xf8)==0xf0?4:1;
    if(n>len-at)return 1;
    for(size_t i=1;i<n;++i)if(((unsigned char)text[at+i]&0xc0)!=0x80)return 1;
    return n;
}
static bool boundary(const char* p,size_t n){
    return (n==1 && strchr(".!?;\n\r",*p)) || (n==3 && (!memcmp(p,"。",3)||!memcmp(p,"！",3)||!memcmp(p,"？",3)||!memcmp(p,"；",3)));
}
bool book_quote_sentence(const char* text,size_t len,size_t hit,size_t* start,size_t* end){
    if(!text || hit>=len || !start || !end)return false;
    size_t a=0,b=len;
    for(size_t i=0;i<len;){size_t n=char_size(text,len,i);if(boundary(text+i,n)){
        if(i+n<=hit)a=i+n;else{b=i+n;break;}}
        i+=n;
    }
    while(a<b && (text[a]==' '||text[a]=='\n'||text[a]=='\r'||text[a]=='\t'))++a;
    while(b>a && (text[b-1]==' '||text[b-1]=='\n'||text[b-1]=='\r'||text[b-1]=='\t'))--b;
    // 超长句截取命中处附近，始终落在字符边界。/ Clip long sentences around the hit at character boundaries.
    if(b-a>=BOOK_QUOTE_TEXT_MAX){
        size_t desired=hit>BOOK_QUOTE_TEXT_MAX/2?hit-BOOK_QUOTE_TEXT_MAX/2:0;
        while(a<desired)a+=char_size(text,len,a);
        size_t i=a;while(i<b){size_t n=char_size(text,len,i);if(i+n-a>=BOOK_QUOTE_TEXT_MAX)break;i+=n;}b=i;
    }
    if(b<=a)return false;
    *start=a;*end=b;return true;
}
