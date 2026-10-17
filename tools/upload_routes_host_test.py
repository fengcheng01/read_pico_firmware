#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# 中文：实际上传检查/接收路由与POSIX文件、cJSON集成；故障只在文件和HTTP边界注入。
# English: Integrate actual preflight/upload routes with POSIX files and cJSON; inject faults only at file/HTTP boundaries.
from pathlib import Path
import os,subprocess,tempfile
ROOT=Path(__file__).resolve().parents[1]
s=(ROOT/'components/read_pico_transfer/read_pico_transfer.c').read_text()
def cut(a,b):return s[s.index(a):s.index(b,s.index(a))]
unit=r'''
#define READ_PICO_TRANSFER_HOST_TEST
#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include "cJSON.h"
static int fail_open,fail_write,fail_close;
static size_t largest_write;
static FILE* io_open(const char* p,const char* m){if(fail_open&&strstr(p,".part")){errno=fail_open;return NULL;}return fopen(p,m);}
static size_t io_write(const void* p,size_t size,size_t n,FILE* f){if(n>largest_write)largest_write=n;if(fail_write){errno=fail_write;return 0;}return fwrite(p,size,n,f);}
static int io_close(FILE* f){int rc=fclose(f);if(fail_close){errno=fail_close;return -1;}return rc;}
#define fopen io_open
#define fwrite io_write
#define fclose io_close
#include "components/read_pico_transfer/read_pico_transfer.c"
typedef int esp_err_t;
enum{ESP_OK=0,ESP_FAIL=-1,ESP_ERR_NO_MEM=0x101,ESP_ERR_NOT_FOUND=0x102,READ_PICO_TRANSFER_UPLOADING=3};
typedef struct{char query[512];size_t content_len,pos;void* user_ctx;const unsigned char* body;}httpd_req_t;
#define portENTER_CRITICAL(x) ((void)(x))
#define portEXIT_CRITICAL(x) ((void)(x))
#define ESP_LOGI(...) log_noop(__VA_ARGS__)
#define ESP_LOGW(...) log_noop(__VA_ARGS__)
#define HTTPD_SOCK_ERR_TIMEOUT -3
#define TRANSFER_IMAGE_MAX (20u*1024u*1024u)
static void log_noop(const char* tag,const char* fmt,...){(void)tag;(void)fmt;}
static char s_root[160],s_font_root[160],s_image_root[160],storage_buffer[16384],response[2048],response_status[64];
static char *s_buffer=storage_buffer;
static bool s_stopping,s_sync_active,s_upload_active;
static int s_lock,changed_calls;
static uint64_t available=1024ull*1024*1024;
static uint64_t free_bytes(void* c){(void)c;return available;}
static int changed(const char* p){(void)p;++changed_calls;return 0;}
static struct{size_t file_limit;uint64_t(*free_bytes_cb)(void*);void* free_bytes_ctx;file_changed_cb_t file_changed_cb;}s_cfg={.free_bytes_cb=free_bytes,.file_changed_cb=changed};
static struct{int state,last_error;char cur_name[121];size_t cur_bytes,cur_total;unsigned done_count,changed_count;}s_status;
static struct{uint32_t seq;char name[121];size_t total;file_result_t result;}s_last_upload;
static bool admission_begin(bool stop,bool* active){if(stop||*active)return false;*active=true;return true;}
static int64_t esp_timer_get_time(void){return 1000000;}
static int httpd_req_recv(void* ctx,char* buffer,size_t n){httpd_req_t* r=ctx;if(n>r->content_len-r->pos)n=r->content_len-r->pos;memcpy(buffer,r->body+r->pos,n);r->pos+=n;return(int)n;}
static bool management_request(httpd_req_t* r){(void)r;return true;}
static int httpd_req_get_url_query_str(httpd_req_t* r,char* out,size_t cap){if(strlen(r->query)>=cap)return-1;strcpy(out,r->query);return 0;}
static int httpd_query_key_value(const char* query,const char* key,char* out,size_t cap){char field[64];snprintf(field,sizeof(field),"%s=",key);const char* p=strstr(query,field);if(!p)return ESP_ERR_NOT_FOUND;p+=strlen(field);const char* end=strchr(p,'&');size_t n=end?(size_t)(end-p):strlen(p);if(n>=cap)return -1;memcpy(out,p,n);out[n]=0;return 0;}
static int httpd_resp_set_status(httpd_req_t* r,const char* st){(void)r;snprintf(response_status,sizeof(response_status),"%s",st);return 0;}
static int httpd_resp_set_type(httpd_req_t* r,const char* st){(void)r;(void)st;return 0;}
static int httpd_resp_set_hdr(httpd_req_t* r,const char* a,const char* b){(void)r;(void)a;(void)b;return 0;}
static int httpd_resp_sendstr(httpd_req_t* r,const char* body){(void)r;snprintf(response,sizeof(response),"%s",body);return 0;}
static void set_error(int e){(void)e;}
static int wifi_response(httpd_req_t* r,const char* st,const char* body){httpd_resp_set_status(r,st);httpd_resp_sendstr(r,body);return -1;}
static int respond_error(httpd_req_t* r,int code){char st[32];snprintf(st,sizeof(st),"%d",code);return wifi_response(r,st,"{\"error\":\"request\"}");}
static int file_response(httpd_req_t* r,file_result_t result,bool deletion,const char* p){(void)p;(void)deletion;assert(result.status==200);return httpd_resp_sendstr(r,"{\"ok\":true}");}
'''+cut('static const char *upload_stage_name(', 'static esp_err_t info_handler(')+cut('static int receive_http(', 'static bool decode_query(')+cut('static esp_err_t send_json(', 'static esp_err_t file_response(')+cut('static esp_err_t upload_error_response(', 'static esp_err_t font_info_handler(')+r'''
#undef fopen
#undef fwrite
#undef fclose
static httpd_req_t req(const char* name,int kind,size_t total,const unsigned char* data){httpd_req_t r={.content_len=total,.body=data,.user_ctx=(void*)(intptr_t)kind};snprintf(r.query,sizeof(r.query),"name=%s",name);return r;}
static cJSON* check(const char* name,const char* kind,size_t size){httpd_req_t r={0};snprintf(r.query,sizeof(r.query),"kind=%s&name=%s&size=%zu",kind,name,size);response[0]=0;upload_check_handler(&r);assert(!s_upload_active);cJSON* j=cJSON_Parse(response);assert(j);return j;}
static void value(cJSON* j,const char* key,const char* expected){assert(!strcmp(cJSON_GetStringValue(cJSON_GetObjectItem(j,key)),expected));}
int main(void){
 char root[]="/tmp/pagerest-routes-XXXXXX";assert(mkdtemp(root));snprintf(s_root,sizeof(s_root),"%s",root);snprintf(s_font_root,sizeof(s_font_root),"%s",root);snprintf(s_image_root,sizeof(s_image_root),"%s",root);
 size_t n=2590000;unsigned char* data=malloc(n);assert(data);for(size_t k=0;k<n;k++)data[k]=(unsigned char)(k*13);memcpy(data,"\x89PNG\r\n\x1a\n",8);
 cJSON* j=check("fresh.png","image",n);assert(cJSON_IsFalse(cJSON_GetObjectItem(j,"exists")));assert(!cJSON_HasObjectItem(j,"error"));cJSON_Delete(j);
 httpd_req_t r=req("fresh.png",2,n,data);assert(upload_handler(&r)==0&&r.pos==n);assert(s_last_upload.result.status==200&&s_last_upload.result.io.received==n&&s_last_upload.result.io.written==n&&s_status.done_count==1&&changed_calls==0&&largest_write<=4096);
 char path[288],part[296];snprintf(path,sizeof(path),"%s/fresh.png",root);snprintf(part,sizeof(part),"%s.part",path);assert(access(part,F_OK)!=0);
 FILE* f=fopen(path,"rb");assert(f);unsigned char* actual=malloc(n);assert(fread(actual,1,n,f)==n&&!memcmp(data,actual,n));fclose(f);free(actual);
 j=check("fresh.png","image",n);assert(cJSON_IsTrue(cJSON_GetObjectItem(j,"exists")));cJSON_Delete(j);
 r=req("fresh.png",2,n,data);assert(upload_handler(&r)==-1&&r.pos==0&&s_last_upload.result.status==409);
 fail_open=EROFS;j=check("new.png","image",n);value(j,"stage","open");assert(cJSON_GetObjectItem(j,"errno")->valueint==EROFS);assert(strstr(cJSON_GetStringValue(cJSON_GetObjectItem(j,"error")),"目录不可写"));cJSON_Delete(j);fail_open=0;
 fail_write=EIO;j=check("new.ttf","font",n);value(j,"stage","write");assert(cJSON_GetObjectItem(j,"errno")->valueint==EIO);cJSON_Delete(j);fail_write=0;
 fail_close=EIO;j=check("new.png","image",n);value(j,"stage","close");cJSON_Delete(j);fail_close=0;
 available=1;j=check("new.png","image",n);value(j,"stage","capacity");cJSON_Delete(j);available=1024ull*1024*1024;
 fail_open=EMFILE;j=check("new.ttf","font",n);assert(strstr(cJSON_GetStringValue(cJSON_GetObjectItem(j,"error")),"打开的文件过多"));cJSON_Delete(j);fail_open=0;
 fail_write=EIO;r=req("fresh.png",2,n,data);strcat(r.query,"&overwrite=1");assert(upload_handler(&r)==-1&&r.pos==4096);assert(s_last_upload.result.status==507&&s_last_upload.result.io.received==4096&&s_last_upload.result.io.error==EIO);j=cJSON_Parse(response);assert(j);value(j,"stage","write");cJSON_Delete(j);fail_write=0;assert(access(part,F_OK)!=0);
 struct stat st;assert(!stat(path,&st)&&(size_t)st.st_size==n);
 FILE* font=fopen("main/assets/builtin.ttf","rb");assert(font);assert(!fseek(font,0,SEEK_END));size_t fn=(size_t)ftell(font);rewind(font);unsigned char* fb=malloc(fn);assert(fread(fb,1,fn,font)==fn);fclose(font);j=check("new.ttf","font",fn);assert(!cJSON_HasObjectItem(j,"error"));cJSON_Delete(j);r=req("new.ttf",1,fn,fb);assert(!upload_handler(&r)&&r.pos==fn&&changed_calls==0);free(fb);
 r=req("bad.jpg",2,8,(const unsigned char*)"not-jpeg");assert(upload_handler(&r)==-1&&s_last_upload.result.status==422);j=cJSON_Parse(response);value(j,"stage","image");cJSON_Delete(j);
 char interrupted[288];snprintf(interrupted,sizeof(interrupted),"%s/broken.png.part",root);f=fopen(interrupted,"wb");assert(f);fclose(f);snprintf(interrupted,sizeof(interrupted),"%s/recover.jpg.rename-backup",root);f=fopen(interrupted,"wb");assert(f);fputs("old image",f);fclose(f);unsigned removed=0,restored=0;assert(!cleanup_interrupted(root,&removed,&restored)&&removed==1&&restored==1);assert(access(interrupted,F_OK)!=0);snprintf(interrupted,sizeof(interrupted),"%s/recover.jpg",root);assert(!access(interrupted,F_OK));unlink(interrupted);
 char fontpath[288];snprintf(fontpath,sizeof(fontpath),"%s/new.ttf",root);unlink(fontpath);unlink(path);rmdir(root);free(data);
 puts("Actual preflight/upload routes:2.47MB fresh PNG exact bytes, image conflict, real TTF,4KiB bound, spacious-card EIO/EROFS/EMFILE/close faults and old-file preservation PASS");
}
'''
with tempfile.TemporaryDirectory(prefix='pico-upload-routes-') as d:
 p=Path(d)/'test.c';p.write_text(unit)
 cjson=ROOT/'managed_components/espressif__cjson/cJSON'
 cmd=[os.environ.get('CC','cc'),'-std=gnu11','-Wall','-Wextra','-Werror','-Wno-unused-function','-Wno-deprecated-declarations','-fsanitize=address,undefined','-I.','-Icomponents/read_pico_search/include','-I'+str(cjson),str(p),str(cjson/'cJSON.c'),'-o',str(Path(d)/'test')]
 subprocess.run(cmd,cwd=ROOT,check=True);subprocess.run([str(Path(d)/'test')],cwd=ROOT,check=True)
