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
    (void)d;(void)n;assert(b==128u*1024u);
    *w=4;*h=4;*out=malloc(16);memset(*out,0x33,16);return true;
}

// 封面缩放宿主替身：最近邻即可；管线测试不校验像素质量（像素规则由 test_book_images 覆盖）。
// Host stand-in for cover scaling: nearest sampling; the pipeline test does not check
// pixel quality (sampling rules are covered by test_book_images).
void book_image_resample(const uint8_t* src, unsigned stride, unsigned sw, unsigned sh,
                         uint8_t* dst, unsigned dw, unsigned dh) {
    for (unsigned y = 0; y < dh; ++y)
        for (unsigned x = 0; x < dw; ++x)
            dst[(size_t)y * dw + x] = src[(size_t)(y * sh / dh) * stride + x * sw / dw];
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
    container="<c:container><c:rootfile full-path = 'OPS/book.opf'/></c:container>";container_size=strlen(container);
    opf="<opf:item properties = 'cover-image' href = './../OPS/art%2Epng#cover'/>";opf_size=strlen(opf);assert(load());
    opf="<item properties='cover-image' href='missing.png'/>";opf_size=strlen(opf);uint8_t* out=NULL;assert(book_cover_read("fixture.epub",&out)==BOOK_COVER_RETRY&&!out);
    opf="<package><manifest/></package>";opf_size=strlen(opf);assert(book_cover_read("fixture.epub",&out)==BOOK_COVER_ABSENT&&!out);
    assert(book_cover_read("fixture.txt",&out)==BOOK_COVER_ABSENT&&!out);
    puts("book_cover: raw XML, bounds, missing attributes and cover scaling passed");
}
