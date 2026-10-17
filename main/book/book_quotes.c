/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 * 中文：小端版本化摘录槽，保存原文而非书签文案，坏记录隔离。
 * English: Versioned little-endian excerpt slots store source text and isolate corrupt records.
 * 冻结修订：用户要求自行选择锁屏摘录；独立副本只在明确确认后更新，不随新增摘录或16条轮换改变。
 * Frozen revision: User-selected lock excerpts update an independent snapshot only on confirmation, never on new excerpts or sixteen-record rotation.
 */
#include "book_quotes.h"
#include "nvs.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define HEADER 24
// v2 记录在头部追加章节标题长度；v1 无标题，读取时空串回退。/ v2 records append a
// chapter-title length to the header; v1 records carry none and load with an empty title.
#define HEADER2 26
#define RECORD_MAX (HEADER2 + BOOK_STORE_PATH_MAX + BOOK_QUOTE_TEXT_MAX + BOOK_QUOTE_TITLE_MAX)
static unsigned s_revision;
static uint32_t get32(const uint8_t* p) { return p[0] | (uint32_t)p[1]<<8 | (uint32_t)p[2]<<16 | (uint32_t)p[3]<<24; }
static void put32(uint8_t* p, uint32_t v) { for (unsigned i=0;i<4;++i) p[i]=(uint8_t)(v>>(i*8)); }
static void key_of(unsigned slot, char key[8]) { snprintf(key,8,"q%02u",slot); }
static bool load_key(nvs_handle_t h, const char* key, book_quote_t* q) {
    uint8_t b[RECORD_MAX]; size_t len=sizeof(b);
    if(nvs_get_blob(h,key,b,&len)!=ESP_OK || len<HEADER) return false;
    bool v2 = !memcmp(b,"RPQ\2",4);
    if(!v2 && memcmp(b,"RPQ\1",4)) return false;
    size_t head = v2 ? HEADER2 : HEADER;
    size_t pn=b[20]|(size_t)b[21]<<8, tn=b[22]|(size_t)b[23]<<8;
    // v1 无标题字段：cn 视为 1 仅满足下界，不参与长度合计。/ v1 carries no title
    // field: cn stays 1 for the lower bound alone and never joins the length sum.
    size_t cn = v2 ? b[24]|(size_t)b[25]<<8 : 1;
    if(pn<2 || pn>sizeof(q->path) || tn<2 || tn>sizeof(q->text) || cn<1 || cn>sizeof(q->chapter_title) ||
       len!=head+pn+tn+(v2?cn:0) ||
       strnlen((char*)b+head,pn)!=pn-1 || strnlen((char*)b+head+pn,tn)!=tn-1 ||
       (v2 && strnlen((char*)b+head+pn+tn,cn)!=cn-1) || b[18]>100) return false;
    *q=(book_quote_t){.seq=get32(b+4),.chapter=b[8]|(uint16_t)b[9]<<8,
        .byte_off=get32(b+10),.end_off=get32(b+14),.pct=b[18]};
    if(q->end_off<=q->byte_off) return false;
    memcpy(q->path,b+head,pn);memcpy(q->text,b+head+pn,tn);
    if(v2) memcpy(q->chapter_title,b+head+pn+tn,cn);
    return true;
}
static bool load(nvs_handle_t h, unsigned slot, book_quote_t* q) {
    char key[8]; key_of(slot,key); return load_key(h,key,q);
}
// v2 序列化共享：路径、摘录与章节标题快照连排；标题允许为空（旧调用方）。/ Shared
// v2 serialization: path, excerpt and the chapter-title snapshot in a row; the title may stay empty.
static esp_err_t store(nvs_handle_t h, const char* key, const book_quote_t* q) {
    size_t title = strnlen(q->chapter_title, sizeof(q->chapter_title));
    if (title == sizeof(q->chapter_title)) return ESP_ERR_INVALID_ARG;
    uint8_t b[RECORD_MAX] = {'R','P','Q',2};
    size_t pn = strlen(q->path)+1, tn = strlen(q->text)+1, cn = title+1;
    put32(b+4,q->seq); b[8]=(uint8_t)q->chapter; b[9]=(uint8_t)(q->chapter>>8);
    put32(b+10,q->byte_off); put32(b+14,q->end_off); b[18]=q->pct;
    b[20]=(uint8_t)pn; b[21]=(uint8_t)(pn>>8); b[22]=(uint8_t)tn; b[23]=(uint8_t)(tn>>8);
    b[24]=(uint8_t)cn; b[25]=(uint8_t)(cn>>8);
    memcpy(b+HEADER2,q->path,pn); memcpy(b+HEADER2+pn,q->text,tn); memcpy(b+HEADER2+pn+tn,q->chapter_title,cn);
    esp_err_t err = nvs_set_blob(h,key,b,HEADER2+pn+tn+cn);
    if (err == ESP_OK) err = nvs_commit(h);
    return err;
}
bool book_quotes_lock_get(book_quote_t* out) {
    if (!out) return false;
    nvs_handle_t h;
    if (nvs_open("rp_lockquote", NVS_READONLY, &h) != ESP_OK) return false;
    bool valid = load_key(h, "chosen", out);
    nvs_close(h); return valid;
}
esp_err_t book_quotes_lock_set(const book_quote_t* q) {
    if (!q || !q->path[0] || !q->text[0] || q->end_off <= q->byte_off || q->pct > 100 ||
        strnlen(q->path,sizeof(q->path)) == sizeof(q->path) ||
        strnlen(q->text,sizeof(q->text)) == sizeof(q->text)) return ESP_ERR_INVALID_ARG;
    nvs_handle_t h;
    esp_err_t err = nvs_open("rp_lockquote", NVS_READWRITE, &h);
    if (err != ESP_OK) return err;
    err = store(h, "chosen", q);
    nvs_close(h); return err;
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
    // 序号在写入口统一递增，锁屏副本沿用记录自带序号。/ The rolling store assigns the
    // next sequence here; the lock snapshot keeps the record's own.
    book_quote_t next=*q;next.seq=newest+1;
    char key[8];key_of(slot,key);err=store(h,key,&next);
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
