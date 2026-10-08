/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 * 中文：封面 XML 无终止符、大小上限与有效封面回归。
 * English: Cover XML regressions for unterminated entries, size bounds and valid covers.
 */
#include "book_cover.h"
#include "book_image.h"
#include "zip_reader.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
struct zip_reader { int unused; };
static struct zip_reader reader;
static size_t container_size, opf_size;
static const char* container;
static const char* opf;
static int extracts;
esp_err_t zip_open(const char* path, zip_reader_t** out) {(void)path;*out=&reader;return ESP_OK;}
void zip_close(zip_reader_t* z) {(void)z;}
bool zip_directory_identity(const zip_reader_t* z,uint32_t* bytes,uint32_t* directory,uint32_t* crc) {
    (void)z;(void)bytes;(void)directory;(void)crc;return false;
}
int zip_find(const zip_reader_t* z,const char* path) {
    (void)z;
    if (!strcmp(path,"META-INF/container.xml")) return 0;
    if (!strcmp(path,"OPS/book.opf")) return 1;
    return !strcmp(path,"OPS/art.png") ? 2 : -1;
}
size_t zip_entry_size(const zip_reader_t* z,int i) {(void)z;return i==0?container_size:i==1?opf_size:1;}
esp_err_t zip_extract(zip_reader_t* z,int i,void* dst,size_t cap) {
    (void)z;++extracts;
    size_t size=zip_entry_size(z,i);assert(cap>=size);
    if(i==2) memset(dst,1,size);
    else if ((i==0?container:opf)) memcpy(dst,i==0?container:opf,size);
    else memset(dst,'x',size);
    return ESP_OK;
}
bool book_image_decode(const uint8_t* d,size_t n,size_t b,uint8_t** out,uint16_t* w,uint16_t* h) {
    (void)d;(void)n;(void)b;
    *w=4;*h=4;*out=malloc(16);memset(*out,0x33,16);return true;
}
static bool load(void) {uint8_t* out=NULL;bool ok=book_cover_load("fixture.epub",&out);if(ok) assert(out);else assert(!out);free(out);return ok;}
int main(void) {
    container_size=0;extracts=0;assert(!load() && extracts==0);
    container_size=256*1024+1;assert(!load() && extracts==0);
    container_size=256*1024;container=NULL;assert(!load());
    container="<container><rootfile full-path='OPS/book.opf'/></container>";container_size=strlen(container);
    opf_size=256*1024;opf=NULL;assert(!load());
    opf="<package><item properties='cover-image' href='art.png'/></package>";opf_size=strlen(opf);
    assert(load());
    container="<rootfile missing='OPS/book.opf'/>";container_size=strlen(container);assert(!load());
    container="<rootfile full-path='OPS/book.opf'/>";container_size=strlen(container);
    opf="<item href='art.png' properties='cover-image'/>";opf_size=strlen(opf);assert(load());
    puts("book_cover: raw XML, bounds, missing attributes and cover scaling passed");
}
