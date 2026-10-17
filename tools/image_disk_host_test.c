/* SPDX-License-Identifier: Apache-2.0
 * 中文：真实图片成品缓存的跨会话复用、身份、损坏与存储失败回退。
 * English: Actual preview-cache reuse across sessions, identity, corruption and storage-failure fallback.
 */
#include "ui_image.h"
#include <assert.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
static int loads;
void* heap_caps_malloc(size_t n, unsigned caps) { (void)caps;return malloc(n); }
void epd_draw_pixel(int x,int y,uint8_t c,uint8_t* fb) {(void)x;(void)y;(void)c;(void)fb;}
bool book_image_decode_file(const char* path,size_t budget,uint8_t** pixels,uint16_t* w,uint16_t* h) {
    (void)path;assert(budget>4);++loads;*w=*h=2;*pixels=malloc(4);assert(*pixels);memcpy(*pixels,"abcd",4);return true;
}
static void load(const char* path,int count) {
    uint8_t* gray=NULL;uint16_t w=0,h=0;assert(ui_image_load_path(path,&gray,&w,&h));
    assert(w==2&&h==2&&!memcmp(gray,"abcd",4)&&loads==count);free(gray);
}
static void clean(void) {
    DIR* d=opendir(UI_IMAGE_MEDIA_ROOT "/.pagerest-images");if(!d)return;
    struct dirent* e;char p[512];while((e=readdir(d)))if(e->d_name[0]!='.'){
        snprintf(p,sizeof(p),UI_IMAGE_MEDIA_ROOT "/.pagerest-images/%s",e->d_name);assert(!unlink(p));
    }
    closedir(d);assert(!rmdir(UI_IMAGE_MEDIA_ROOT "/.pagerest-images"));
}
int main(void) {
    (void)mkdir(UI_IMAGE_MEDIA_ROOT,0755);clean();
    const char* source=UI_IMAGE_MEDIA_ROOT "/sample.png";FILE* f=fopen(source,"wb");assert(f);fputs("source",f);fclose(f);
    load(source,1);load(source,1);
    DIR* d=opendir(UI_IMAGE_MEDIA_ROOT "/.pagerest-images");assert(d);struct dirent* e;char cached[512]={0};
    while((e=readdir(d)))if(e->d_name[0]!='.')snprintf(cached,sizeof(cached),UI_IMAGE_MEDIA_ROOT "/.pagerest-images/%s",e->d_name);
    closedir(d);assert(*cached);
    f=fopen(cached,"r+b");assert(f);assert(!fseek(f,-1,SEEK_END));fputc('x',f);fclose(f);load(source,2);load(source,2);
    f=fopen(cached,"wb");assert(f);fputs("partial",f);fclose(f);load(source,3);load(source,3);
    f=fopen(source,"ab");assert(f);fputc('!',f);fclose(f);load(source,4);load(source,4);
    clean();f=fopen(UI_IMAGE_MEDIA_ROOT "/.pagerest-images","wb");assert(f);fclose(f);
    load(source,5);load(source,6);assert(!unlink(UI_IMAGE_MEDIA_ROOT "/.pagerest-images"));
    assert(!unlink(source));uint8_t* gray=NULL;uint16_t w,h;assert(!ui_image_load_path(source,&gray,&w,&h)&&!gray);
    for (int i=0;i<20;++i) {
        char path[256];snprintf(path,sizeof(path),UI_IMAGE_MEDIA_ROOT "/%d.png",i);
        f=fopen(path,"wb");assert(f);fputs("original",f);fclose(f);load(path,7+i);assert(!unlink(path));
    }
    int entries=0;d=opendir(UI_IMAGE_MEDIA_ROOT "/.pagerest-images");assert(d);
    while((e=readdir(d))) if(e->d_name[0]!='.')++entries;
    closedir(d);assert(entries<=8);clean();
    assert(!rmdir(UI_IMAGE_MEDIA_ROOT));
    puts("Actual preview disk cache: repeat skips decoder, source change, same-size payload corruption, truncation, storage failure and missing source PASS");
}
