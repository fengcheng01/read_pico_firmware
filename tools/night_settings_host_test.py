#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 mindreset
# SPDX-License-Identifier: Apache-2.0
"""真实设置的夜间实验迁移与持久化回归。/ Real settings night-experiment migration and persistence regressions."""
from pathlib import Path
import os
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
source = (ROOT / "main/settings.c").read_text()
for header in ("esp_log.h", "nvs.h", "nvs_flash.h"):
    source = source.replace(f'#include "{header}"', "")

unit = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
typedef int esp_err_t;
typedef int nvs_handle_t;
#define ESP_OK 0
#define ESP_ERR_NVS_NOT_FOUND 1
#define ESP_ERR_NVS_NO_FREE_PAGES 2
#define ESP_ERR_NVS_NEW_VERSION_FOUND 3
#define NVS_READONLY 0
#define NVS_READWRITE 1
#define ESP_LOGI(...) ((void)0)
#define ESP_LOGW(...) ((void)0)
static bool has_cleanup;
static uint8_t stored_cleanup;
static unsigned writes, commits;
static int init_error, open_error;
static size_t host_strlcpy(char* dst,const char* src,size_t cap) {
    size_t n=strlen(src);
    if(cap){size_t copied=n<cap-1?n:cap-1;memcpy(dst,src,copied);dst[copied]=0;}
    return n;
}
#define strlcpy host_strlcpy
static esp_err_t nvs_flash_init(void){return init_error;}
static esp_err_t nvs_flash_erase(void){return ESP_OK;}
static esp_err_t nvs_open(const char* ns,int mode,nvs_handle_t* out) {
    assert(!strcmp(ns,"read_pico")&&(mode==NVS_READONLY||mode==NVS_READWRITE));
    *out=1;return open_error;
}
static void nvs_close(nvs_handle_t h){assert(h==1);}
static esp_err_t nvs_commit(nvs_handle_t h){assert(h==1);++commits;return ESP_OK;}
static esp_err_t nvs_get_u8(nvs_handle_t h,const char* key,uint8_t* out) {
    assert(h==1);
    if(!strcmp(key,"bk_ngclean")&&has_cleanup){*out=stored_cleanup;return ESP_OK;}
    // 升级夹具已有的阅读选择不可被新增选项覆盖。/ Existing reader choices in the upgrade fixture must not be overwritten.
    if(!strcmp(key,"bk_night")||!strcmp(key,"bk_direct")){*out=1;return ESP_OK;}
    if(!strcmp(key,"gc_every")){*out=10;return ESP_OK;}
    return ESP_ERR_NVS_NOT_FOUND;
}
static esp_err_t nvs_get_i8(nvs_handle_t h,const char* key,int8_t* out){(void)h;(void)key;(void)out;return ESP_ERR_NVS_NOT_FOUND;}
static esp_err_t nvs_get_u32(nvs_handle_t h,const char* key,uint32_t* out){(void)h;(void)key;(void)out;return ESP_ERR_NVS_NOT_FOUND;}
static esp_err_t nvs_get_str(nvs_handle_t h,const char* key,char* out,size_t* cap){(void)h;(void)key;(void)out;(void)cap;return ESP_ERR_NVS_NOT_FOUND;}
static esp_err_t nvs_set_u8(nvs_handle_t h,const char* key,uint8_t value) {
    assert(h==1&&!strcmp(key,"bk_ngclean"));
    has_cleanup=true;stored_cleanup=value;++writes;return ESP_OK;
}
static esp_err_t nvs_set_i8(nvs_handle_t h,const char* key,int8_t value){(void)h;(void)key;(void)value;assert(false);return 1;}
static esp_err_t nvs_set_u32(nvs_handle_t h,const char* key,uint32_t value){(void)h;(void)key;(void)value;assert(false);return 1;}
static esp_err_t nvs_set_str(nvs_handle_t h,const char* key,const char* value){(void)h;(void)key;(void)value;assert(false);return 1;}
'''
unit += source
unit += r'''
static void check_existing_choices(void) {
    assert(app_settings_book_night()&&app_settings_book_direct());
    assert(app_settings_gc_every()==10);
}
int main(int argc,char** argv) {
    (void)argv;
    assert(app_settings_book_night_cleanup()==BOOK_NIGHT_CLEAN_BLACK);
    if(argc>1) {
        // 启动存储不可用仍使用静态默认，不擦分区。/ An unavailable store at startup retains the static default without erasing the partition.
        init_error=5;app_settings_init();
        assert(app_settings_book_night_cleanup()==BOOK_NIGHT_CLEAN_BLACK&&writes==0);
        init_error=0;open_error=5;app_settings_init();
        assert(app_settings_book_night_cleanup()==BOOK_NIGHT_CLEAN_BLACK&&writes==0);
        puts("night settings: unavailable NVS boot retains BLACK default PASS");
        return 0;
    }
    app_settings_init();
    assert(app_settings_book_night_cleanup()==BOOK_NIGHT_CLEAN_BLACK&&writes==0&&commits==0);
    check_existing_choices();
    // 所有有效档位均保存并从实际加载路径读回，重复保存不写NVS。/ Every valid tier persists and reloads through production init, while identical saves skip NVS.
    for(uint8_t mode=BOOK_NIGHT_CLEAN_OFF;mode<=BOOK_NIGHT_CLEAN_LOCAL;++mode) {
        unsigned before=writes;
        app_settings_set_book_night_cleanup(mode);
        assert(app_settings_book_night_cleanup()==mode&&writes==before+1&&commits==writes);
        assert(stored_cleanup==mode);
        app_settings_set_book_night_cleanup(mode);
        assert(writes==before+1);
        // 先改缓存，再恢复持久化夹具，防止仅检查setter缓存而漏掉加载缺陷。
        // Change the cache before restoring the stored fixture so a getter-only check cannot hide load defects.
        app_settings_set_book_night_cleanup((mode+1)%3);
        stored_cleanup=mode;
        app_settings_init();
        assert(app_settings_book_night_cleanup()==mode);
        check_existing_choices();
    }
    for(unsigned raw=3;raw<=255;++raw) {
        unsigned before=writes;
        app_settings_set_book_night_cleanup((uint8_t)raw);
        assert(app_settings_book_night_cleanup()==BOOK_NIGHT_CLEAN_LOCAL&&writes==before);
    }
    // 非法磁盘值和缺键回到测试版默认，不改写旧数据或其它阅读选项。/ Invalid stored values and missing keys use the test-build default without rewriting legacy data or other reader choices.
    for(unsigned raw=3;raw<=255;++raw) {
        has_cleanup=true;stored_cleanup=(uint8_t)raw;
        unsigned before=writes;
        app_settings_init();
        assert(app_settings_book_night_cleanup()==BOOK_NIGHT_CLEAN_BLACK&&writes==before&&stored_cleanup==raw);
        check_existing_choices();
    }
    has_cleanup=false;app_settings_init();
    assert(app_settings_book_night_cleanup()==BOOK_NIGHT_CLEAN_BLACK);
    check_existing_choices();
    puts("night settings: default/missing migration, all 3 tiers reload, unchanged saves, 253 invalid setters/stored values and retained night/direct/10-page choices PASS");
}
'''

flags = ["-std=gnu11", "-Wall", "-Wextra", "-Werror", "-g", "-fsanitize=address,undefined", "-fno-omit-frame-pointer"]
if os.uname().sysname == "Darwin":
    developer = Path(subprocess.check_output(["xcode-select", "-p"], text=True).strip())
    sdk = Path(os.environ.get("PREVIEW_MACOS_SDK", developer / "Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk"))
    if sdk.exists():
        flags += ["-isysroot", str(sdk)]
with tempfile.TemporaryDirectory(prefix="pico-night-settings-", dir="/tmp") as folder:
    source_path = Path(folder) / "settings.c"
    source_path.write_text(unit)
    binary = Path(folder) / "settings"
    subprocess.run([os.environ.get("CC", "cc"), *flags, "-Imain", "-Imain/os", str(source_path), "-o", str(binary)], cwd=ROOT, check=True)
    environment = dict(os.environ, UBSAN_OPTIONS="halt_on_error=1:print_stacktrace=1")
    subprocess.run([str(binary)], cwd=ROOT, env=environment, check=True)
    subprocess.run([str(binary), "unavailable"], cwd=ROOT, env=environment, check=True)
