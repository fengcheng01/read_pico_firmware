#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# 真实接收适配器与暂存提交：短暂超时、持续无数据、停止和断线。/ Real receiver and staged commits under transient timeout, idle, stop and disconnect.
from pathlib import Path
import os,subprocess,tempfile
ROOT=Path(__file__).resolve().parents[1]
source=(ROOT/'components/read_pico_transfer/read_pico_transfer.c').read_text()
adapter=source[source.index('static int receive_http('):source.index('\nstatic void upload_progress(')]
unit=r'''
#define READ_PICO_TRANSFER_HOST_TEST
#include "components/read_pico_transfer/read_pico_transfer.c"
#include <assert.h>
#include <unistd.h>
#define HTTPD_SOCK_ERR_TIMEOUT -3
#define portENTER_CRITICAL(x) ((void)(x))
#define portEXIT_CRITICAL(x) ((void)(x))
#define ESP_LOGW(...) ((void)0)
static int s_lock,reads,scenario;
static bool s_stopping;
static int64_t now;
static int64_t esp_timer_get_time(void){return now;}
static int httpd_req_recv(void* ctx,char* buf,size_t n){
 (void)ctx;++reads;now+=5000000;
 if(scenario==1||scenario==2||scenario==3){
  if(scenario==2)s_stopping=true;
  if(scenario==3)return -1;
  return HTTPD_SOCK_ERR_TIMEOUT;
 }
 if(reads%3)return HTTPD_SOCK_ERR_TIMEOUT;
 memset(buf,'x',n);return (int)n;
}
'''+adapter+r'''
int main(void){
 char path[256],part[272],buffer[4096],out[4096];
 snprintf(path,sizeof(path),"/tmp/pagerest-receive-%ld.txt",(long)getpid());snprintf(part,sizeof(part),"%s.part",path);
 assert(receive_file(path,part,1024*1024,buffer,sizeof(buffer),receive_http,NULL,NULL)==0);
 assert(reads==768&&now>30000000);FILE* f=fopen(path,"rb");assert(f);for(int i=0;i<256;++i){assert(fread(out,1,sizeof(out),f)==sizeof(out));assert(!memcmp(out,buffer,sizeof(out)));}assert(fgetc(f)==EOF);fclose(f);
 for(scenario=1;scenario<=3;++scenario){
  reads=0;now=0;s_stopping=false;
  assert(receive_file(path,part,8192,buffer,sizeof(buffer),receive_http,NULL,NULL)==408);
  assert(reads==(scenario==1?6:1));assert(access(part,F_OK)!=0);
  struct stat st;assert(stat(path,&st)==0&&st.st_size==1024*1024);
 }
 s_stopping=true;reads=0;assert(receive_http(NULL,buffer,sizeof(buffer))<0&&!reads);
 remove(path);puts("Production HTTP receive: 1MiB exact commit through intermittent timeouts, per-chunk idle deadline, stop/disconnect, cleanup and old-file preservation PASS");
}
'''
with tempfile.TemporaryDirectory(prefix='pico-upload-',dir='/tmp') as d:
 p=Path(d)/'test.c';p.write_text(unit)
 cmd=[os.environ.get('CC','cc'),'-std=gnu11','-Wall','-Wextra','-Werror','-Wno-unused-function','-fsanitize=address,undefined','-I.','-Icomponents/read_pico_search/include',str(p),'-o',str(Path(d)/'test')]
 subprocess.run(cmd,cwd=ROOT,check=True);subprocess.run([str(Path(d)/'test')],check=True)
