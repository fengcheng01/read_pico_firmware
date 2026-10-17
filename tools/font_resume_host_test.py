#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# 真实字体暂停/恢复策略：首次返回、缺文件、重复恢复与内置选择。/ Actual suspend/resume policy for first returns, missing files, repeated calls and built-in preference.
from pathlib import Path
import os,subprocess,tempfile
r=Path(__file__).resolve().parents[1];s=(r/'main/font/ttf_font.c').read_text();a=s.index('esp_err_t ttf_font_suspend_sd(');b=s.index('\nesp_err_t ttf_font_open(',a)
unit=r'''
#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
typedef int esp_err_t;
#define ESP_OK 0
#define ESP_ERR_NOT_FOUND 1
static bool sd_suspended,font_ready=true,builtin,missing;
static char font_path[160]="/sdcard/fonts/saved.ttf";
static const char* saved="/sdcard/fonts/saved.ttf";
static unsigned opens,builtins;
static bool ttf_font_is_builtin(void){return builtin;}
static bool ttf_font_path_is_builtin(const char*p){return !p||!*p||!strcmp(p,"builtin");}
static const char* app_settings_font_path(void){return saved;}
static int ttf_font_open_builtin(void){++builtins;builtin=true;strcpy(font_path,"builtin");return 0;}
static int ttf_font_open(const char*p){assert(!sd_suspended);++opens;if(missing){ttf_font_open_builtin();return ESP_ERR_NOT_FOUND;}builtin=false;font_ready=true;strcpy(font_path,p);return ESP_OK;}
'''+s[a:b]+r'''
int main(void){
 for(int n=0;n<100;++n){assert(!ttf_font_suspend_sd(true)&&builtin&&sd_suspended);assert(!ttf_font_resume_sd()&&!builtin&&!sd_suspended);assert(!strcmp(font_path,saved));}
 assert(opens==100&&builtins==100);assert(!ttf_font_resume_sd()&&opens==100);
 assert(!ttf_font_suspend_sd(true));missing=true;assert(ttf_font_resume_sd()==ESP_ERR_NOT_FOUND&&builtin&&!sd_suspended);assert(!strcmp(saved,"/sdcard/fonts/saved.ttf"));
 missing=false;assert(!ttf_font_suspend_sd(true));assert(!ttf_font_resume_sd()&&!builtin);
 saved="builtin";assert(!ttf_font_suspend_sd(true));unsigned before=opens;assert(!ttf_font_resume_sd()&&builtin&&opens==before);
 puts("Production font suspend/resume: 100 synchronous saved-font returns, no duplicate opens, missing-font fallback, unchanged preferences and built-in choice PASS");
}
'''
with tempfile.TemporaryDirectory(prefix='pico-font-resume-',dir='/tmp') as d:
 p=Path(d)/'test.c';p.write_text(unit);subprocess.run([os.environ.get('CC','cc'),'-std=c11','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',str(p),'-o',str(Path(d)/'test')],check=True);subprocess.run([str(Path(d)/'test')],check=True)
