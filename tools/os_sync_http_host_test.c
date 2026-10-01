/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 * 中文：HTTP 短写、分段、截断、溢出、取消与失败回归。
 * English: HTTP short-write, fragmented-body, truncation, overflow, cancellation and failure regressions.
 */
#include "os_sync_http.h"
#include <assert.h>
#include <string.h>
#include <stdio.h>
typedef struct {const char* text;size_t at;int calls;bool failed,complete,active;char sent[80];size_t written;} fake_t;
static int write_part(void* arg,const char* text,size_t size) {fake_t* f=arg;++f->calls;if(f->failed)return -1;size_t n=size>2?2:size;memcpy(f->sent+f->written,text,n);f->written+=n;return (int)n;}
static int read_part(void* arg,char* out,size_t cap) {fake_t* f=arg;++f->calls;if(f->failed)return -1;size_t size=strlen(f->text)-f->at,n=size>3?3:size;if(n>cap)n=cap;memcpy(out,f->text+f->at,n);f->at+=n;return (int)n;}
static bool complete(void* arg) {return ((fake_t*)arg)->complete;}
static bool active(void* arg) {return ((fake_t*)arg)->active;}
int main(void) {
    fake_t f={.text="{\"progress\":\"rp1\"}",.complete=true,.active=true};
    os_sync_stream_t io={write_part,read_part,complete,active,&f};char out[40];
    assert(os_sync_http_write(&io,"abcdef") && f.written==6 && f.calls==3 && !memcmp(f.sent,"abcdef",6));
    assert(os_sync_http_read(&io,out,sizeof(out)) && !strcmp(out,f.text));
    f.at=0;assert(!os_sync_http_read(&io,out,4) && !out[0]);
    f.at=0;assert(os_sync_http_read(&io,out,strlen(f.text)+1));
    f.at=0;f.complete=false;assert(!os_sync_http_read(&io,out,sizeof(out)) && !out[0]);
    f.at=0;f.failed=true;assert(!os_sync_http_read(&io,out,sizeof(out)) && !out[0]);
    assert(!os_sync_http_write(&io,"a"));
    f.failed=false;f.active=false;assert(!os_sync_http_read(&io,out,sizeof(out)) && !out[0]);
    assert(!os_sync_http_write(&io,"a"));
    f.active=true;f.complete=true;f.at=0;assert(os_sync_http_read(&io,NULL,0));
    puts("os_sync_http: short writes, fragments, exact bounds, overflow, EOF, errors and cancellation passed");
}
