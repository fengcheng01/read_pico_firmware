#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 mindreset
# SPDX-License-Identifier: Apache-2.0
"""夜间Crossmux对照设置及旧实验升级回归。/ Crossmux night comparison settings and legacy-experiment upgrade regressions."""
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
static bool has_profile;
static uint8_t stored_profile;
static bool has_fine;
static uint8_t stored_fine;
static unsigned legacy_reads, writes, commits, erases;
static int init_error, open_error;
static size_t host_strlcpy(char* dst,const char* src,size_t cap) {
    size_t n=strlen(src);
    if(cap){size_t copied=n<cap-1?n:cap-1;memcpy(dst,src,copied);dst[copied]=0;}
    return n;
}
#define strlcpy host_strlcpy
static esp_err_t nvs_flash_init(void){return init_error;}
static esp_err_t nvs_flash_erase(void){++erases;return ESP_OK;}
static esp_err_t nvs_open(const char* ns,int mode,nvs_handle_t* out) {
    assert(!strcmp(ns,"read_pico")&&(mode==NVS_READONLY||mode==NVS_READWRITE));
    *out=1;return open_error;
}
static void nvs_close(nvs_handle_t h){assert(h==1);}
static esp_err_t nvs_commit(nvs_handle_t h){assert(h==1);++commits;return ESP_OK;}
static esp_err_t nvs_get_u8(nvs_handle_t h,const char* key,uint8_t* out) {
    assert(h==1);
    if(!strcmp(key,"bk_ngclean")) {
        ++legacy_reads;
        if(has_cleanup){*out=stored_cleanup;return ESP_OK;}
        return ESP_ERR_NVS_NOT_FOUND;
    }
    if(!strcmp(key,"bk_fine")) { if(has_fine){*out=stored_fine;return ESP_OK;} return ESP_ERR_NVS_NOT_FOUND; }
    if(!strcmp(key,"bk_ngprofile")) {
        if(has_profile){*out=stored_profile;return ESP_OK;}
        return ESP_ERR_NVS_NOT_FOUND;
    }
    // 升级已有的夜间、直刷和周期设置继续使用；旧实验键原样留在NVS里但不读写。
    // Preserve existing night, direct and interval choices; leave the old experiment key in NVS without reading or writing it.
    if(!strcmp(key,"bk_night")||!strcmp(key,"bk_direct")){*out=1;return ESP_OK;}
    if(!strcmp(key,"gc_every")){*out=10;return ESP_OK;}
    return ESP_ERR_NVS_NOT_FOUND;
}
static esp_err_t nvs_get_i8(nvs_handle_t h,const char* key,int8_t* out){(void)h;(void)key;(void)out;return ESP_ERR_NVS_NOT_FOUND;}
static esp_err_t nvs_get_u32(nvs_handle_t h,const char* key,uint32_t* out){(void)h;(void)key;(void)out;return ESP_ERR_NVS_NOT_FOUND;}
static esp_err_t nvs_get_blob(nvs_handle_t h,const char* key,void* out,size_t* cap){(void)h;(void)key;(void)out;(void)cap;return ESP_ERR_NVS_NOT_FOUND;}
static esp_err_t nvs_get_str(nvs_handle_t h,const char* key,char* out,size_t* cap){(void)h;(void)key;(void)out;(void)cap;return ESP_ERR_NVS_NOT_FOUND;}
static esp_err_t nvs_set_u8(nvs_handle_t h,const char* key,uint8_t value) {
    assert(h==1);
    if(!strcmp(key,"bk_fine")){has_fine=true;stored_fine=value;}
    else {assert(!strcmp(key,"bk_ngprofile"));has_profile=true;stored_profile=value;}
    ++writes;return ESP_OK;
}
static esp_err_t nvs_set_i8(nvs_handle_t h,const char* key,int8_t value){(void)h;(void)key;(void)value;assert(false);return 1;}
static esp_err_t nvs_set_u32(nvs_handle_t h,const char* key,uint32_t value){(void)h;(void)key;(void)value;assert(false);return 1;}
static esp_err_t nvs_set_blob(nvs_handle_t h,const char* key,const void* value,size_t size){(void)h;(void)key;(void)value;(void)size;assert(false);return 1;}
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
    if(argc>1) {
        // 启动存储不可用仍使用静态默认，不擦分区。/ An unavailable store at startup retains the static default without erasing the partition.
        init_error=5;app_settings_init();
        assert(!app_settings_book_night()&&!app_settings_book_direct()&&app_settings_gc_every()==5);
        assert(app_settings_book_night_profile()==BOOK_NIGHT_PROFILE_CURRENT);
        init_error=0;open_error=5;app_settings_init();
        assert(!app_settings_book_night()&&!app_settings_book_direct()&&app_settings_gc_every()==5);
        assert(app_settings_book_night_profile()==BOOK_NIGHT_PROFILE_CURRENT);
        assert(!legacy_reads&&!writes&&!commits&&!erases);
        puts("night settings withdrawal: unavailable NVS retains existing defaults without reading legacy keys or erasing PASS");
        return 0;
    }
    has_cleanup=false;app_settings_init();
    check_existing_choices();
    assert(app_settings_book_night_profile()==BOOK_NIGHT_PROFILE_CURRENT);
    // 曾保存的实验1/2以及其它旧值都不得进入生产加载路径，也不删除或迁移键值。
    // Previously saved experiment tiers 1/2 and other old values never enter production loading, and their keys are neither erased nor migrated.
    const uint8_t legacy[] = {1, 2, 0, 255};
    for(size_t i=0;i<sizeof(legacy);++i) {
        has_cleanup=true;stored_cleanup=legacy[i];
        app_settings_init();
        check_existing_choices();
        assert(app_settings_book_night_profile()==BOOK_NIGHT_PROFILE_CURRENT);
        assert(has_cleanup&&stored_cleanup==legacy[i]);
        assert(!legacy_reads&&!writes&&!commits&&!erases);
    }
    has_cleanup=false;app_settings_init();
    check_existing_choices();
    assert(!legacy_reads&&!writes&&!commits&&!erases);
    puts("night settings withdrawal: legacy bk_ngclean tiers 1/2/off/invalid and missing key ignored without reads/writes/erase; night/direct/10-page choices retained PASS");

    // 新档位有独立键；实际写盘后先改缓存再加载，确认旧实验值不会作为该键的默认或迁移来源。
    // The new tier has its own key; change the cache before reloading persisted values so old experiments cannot become defaults or migration sources.
    has_cleanup=true;stored_cleanup=2;
    for(uint8_t mode=BOOK_NIGHT_PROFILE_CURRENT;mode<=BOOK_NIGHT_PROFILE_BLACK_BASELINE;++mode) {
        app_settings_set_book_night_profile((uint8_t)(mode^1));
        unsigned before=writes;
        app_settings_set_book_night_profile(mode);
        assert(app_settings_book_night_profile()==mode&&stored_profile==mode&&writes==before+1&&commits==writes);
        app_settings_set_book_night_profile(mode);
        assert(writes==before+1);
        app_settings_set_book_night_profile((uint8_t)(mode^1));
        stored_profile=mode;app_settings_init();
        assert(app_settings_book_night_profile()==mode&&stored_cleanup==2);
        check_existing_choices();
    }
    unsigned before=writes;
    for(unsigned raw=3;raw<=255;++raw) app_settings_set_book_night_profile((uint8_t)raw);
    assert(app_settings_book_night_profile()==BOOK_NIGHT_PROFILE_BLACK_BASELINE&&writes==before);
    // 非法持久化值回当前方案，不修写旧键，所有其它阅读选择和旧键保持原样。
    // Invalid persisted values fall back to the current profile without repairing keys, preserving every other reader choice and the old key.
    for(unsigned raw=3;raw<=255;++raw) {
        stored_profile=(uint8_t)raw;app_settings_init();
        assert(app_settings_book_night_profile()==BOOK_NIGHT_PROFILE_CURRENT&&stored_profile==raw&&writes==before);
        check_existing_choices();
    }
    has_profile=false;app_settings_init();
    assert(app_settings_book_night_profile()==BOOK_NIGHT_PROFILE_CURRENT&&stored_cleanup==2);
    assert(!app_settings_book_direct_fine());
    before=writes;app_settings_set_book_direct_fine(true);
    assert(stored_fine==1&&writes==before+1&&app_settings_book_direct_fine());
    app_settings_set_book_direct_fine(true);assert(writes==before+1);
    app_settings_set_book_direct_fine(false);assert(stored_fine==0);
    stored_fine=1;app_settings_init();assert(app_settings_book_direct_fine());
    check_existing_choices();
    for(unsigned raw=2;raw<=255;++raw){stored_fine=(uint8_t)raw;app_settings_init();assert(!app_settings_book_direct_fine()&&stored_fine==raw);}
    has_fine=false;app_settings_init();assert(!app_settings_book_direct_fine());
    puts("direct fine: missing/off/on/invalid stored values, persistence and repeated-save skipping preserve night/direct/interval choices PASS");
    assert(!legacy_reads&&!erases&&writes==commits);
    puts("night profile: distinct bk_ngprofile defaults CURRENT, all three profiles persist/reload, repeated saves skip, 253 invalid setter/stored values fall back safely and old experiment tiers never enable Crossmux PASS");
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
