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
#define ESP_ERR_NVS_NOT_FOUND 3
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
static unsigned legacy_reads, fine_reads, writes, commits, erases;
static uint8_t stored_fine = 1, stored_edge, stored_prewipe;
static bool has_edge, has_prewipe, has_grid;
static uint8_t stored_grid;
static bool has_lock_style;
static uint8_t stored_lock_style;
static bool fail_style_save, has_stroke, fail_stroke_save, fail_stroke_commit;
static uint8_t stored_stroke;
static bool has_goal, fail_goal_save, fail_goal_commit;
static uint8_t stored_goal, pending_goal;
static bool has_animation, fail_animation_save, fail_animation_commit;
static uint8_t stored_animation, pending_animation;
static bool has_edge_style, fail_edge_save, fail_edge_commit;
static unsigned edge_reads;
static uint8_t stored_edge_style, pending_edge_style;
static bool has_balanced, fail_balanced_save, fail_balanced_commit;
static uint8_t stored_balanced, pending_balanced;
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
static esp_err_t nvs_commit(nvs_handle_t h){assert(h==1);++commits;if(fail_stroke_commit || fail_goal_commit || fail_animation_commit || fail_edge_commit || fail_balanced_commit)return 1; if(has_goal)stored_goal=pending_goal; if(has_animation)stored_animation=pending_animation; if(has_edge_style)stored_edge_style=pending_edge_style; if(has_balanced)stored_balanced=pending_balanced; return ESP_OK;}
static esp_err_t nvs_erase_key(nvs_handle_t h,const char* key){assert(h==1);(void)key;++erases;return ESP_OK;}
static esp_err_t nvs_get_u8(nvs_handle_t h,const char* key,uint8_t* out) {
    if(!strcmp(key,"bk_gbalance")&&has_balanced){*out=stored_balanced;return ESP_OK;}
    if(!strcmp(key,"bk_edgeopt")){++edge_reads;if(has_edge_style){*out=stored_edge_style;return ESP_OK;}}
    if(!strcmp(key,"bk_tanim")&&has_animation){*out=stored_animation;return ESP_OK;}
    assert(h==1);
    if(!strcmp(key,"bk_ngclean")) {
        ++legacy_reads;
        if(has_cleanup){*out=stored_cleanup;return ESP_OK;}
        return ESP_ERR_NVS_NOT_FOUND;
    }
    if(!strcmp(key,"daily_goal") && has_goal){*out=stored_goal;return ESP_OK;}
    if(!strcmp(key,"bk_stroke") && has_stroke){*out=stored_stroke;return ESP_OK;}
    if(!strcmp(key,"bk_grid") && has_grid){*out=stored_grid;return ESP_OK;}
    if(!strcmp(key,"lk_style") && has_lock_style){*out=stored_lock_style;return ESP_OK;}
    if(!strcmp(key,"bk_fine")) { ++fine_reads; *out=stored_fine; return ESP_OK; }
    if(!strcmp(key,"bk_edgegray") && has_edge){*out=stored_edge;return ESP_OK;}
    if(!strcmp(key,"bk_prewipe") && has_prewipe){*out=stored_prewipe;return ESP_OK;}
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
    if(!strcmp(key,"bk_gbalance")){if(fail_balanced_save)return 1;has_balanced=true;pending_balanced=value;++writes;return ESP_OK;}
    if(!strcmp(key,"bk_edgeopt")){if(fail_edge_save)return 1;has_edge_style=true;pending_edge_style=value;++writes;return ESP_OK;}
    if(!strcmp(key,"bk_tanim")){if(fail_animation_save)return 1;has_animation=true;pending_animation=value;++writes;return ESP_OK;}
    if(!strcmp(key,"daily_goal")){if(fail_goal_save)return 1; has_goal=true; pending_goal=value; ++writes;return ESP_OK;}
    if(!strcmp(key,"lk_style")&&fail_style_save)return 1;
    if(!strcmp(key,"bk_stroke")){if(fail_stroke_save)return 1;has_stroke=true;stored_stroke=value;}
    else if(!strcmp(key,"bk_ngprofile")){has_profile=true;stored_profile=value;}
    else if(!strcmp(key,"bk_edgegray")){has_edge=true;stored_edge=value;}
    else if(!strcmp(key,"bk_prewipe")){has_prewipe=true;stored_prewipe=value;}
    else if(!strcmp(key,"bk_grid")){has_grid=true;stored_grid=value;}
    else if(!strcmp(key,"lk_style")){has_lock_style=true;stored_lock_style=value;}
    else if(!strcmp(key,"lk_org")||!strcmp(key,"lk_rd")||!strcmp(key,"sleep")){}
    else assert(false);
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
    // 夜间模式暂时下线：存量夜间值仍在存储里但读取恒为关。
    // Night mode is temporarily withdrawn: the stored night value remains but reads as off.
    assert(!app_settings_book_night()&&app_settings_book_direct());
    assert(app_settings_gc_every()==10);
}
// 新键与旧字体及刷新设置独立，所有原始值和写失败均走真实设置代码。
// The new key is independent of legacy fonts/refresh; exercise every raw value and write failure through real settings.
static void check_animation_settings(void) {
    unsigned before=writes, erased=erases;
    for(unsigned raw=0;raw<256;++raw){
        has_balanced=true;stored_balanced=pending_balanced=(uint8_t)raw;app_settings_init();
        assert(app_settings_book_gray_balanced()==(raw>=1&&raw<=5));
        assert(app_settings_book_gray_profile()==(raw>=1&&raw<=5?1:0));
    }
    assert(writes==before&&erases==erased);
    has_balanced=false;app_settings_init();assert(!app_settings_book_gray_balanced());
    fail_balanced_save=true;assert(!app_settings_set_book_gray_balanced(true)&&!app_settings_book_gray_balanced());fail_balanced_save=false;
    fail_balanced_commit=true;assert(!app_settings_set_book_gray_balanced(true)&&!app_settings_book_gray_balanced());fail_balanced_commit=false;
    app_settings_init();assert(!app_settings_book_gray_balanced());
    open_error=1;assert(!app_settings_set_book_gray_balanced(true));open_error=0;
    assert(app_settings_set_book_gray_balanced(true));app_settings_init();assert(app_settings_book_gray_balanced());
    fail_balanced_save=true;assert(!app_settings_set_book_gray_profile(0)&&app_settings_book_gray_profile()==1);fail_balanced_save=false;
    fail_balanced_commit=true;assert(!app_settings_set_book_gray_profile(0)&&app_settings_book_gray_profile()==1);fail_balanced_commit=false;
    open_error=1;assert(!app_settings_set_book_gray_profile(0)&&app_settings_book_gray_profile()==1);open_error=0;
    app_settings_init();assert(app_settings_book_gray_profile()==1);
    assert(app_settings_set_book_gray_profile(0));app_settings_init();assert(app_settings_book_gray_profile()==0);
    before=writes;
    for(unsigned raw=2;raw<256;++raw)assert(!app_settings_set_book_gray_profile((uint8_t)raw)&&writes==before);
    assert(app_settings_set_book_gray_balanced(true));assert(app_settings_book_gray_profile()==1);
    before=writes;assert(app_settings_set_book_gray_balanced(true)&&writes==before);
    assert(app_settings_set_book_gray_balanced(false));app_settings_init();assert(!app_settings_book_gray_balanced());
    before=writes;
    puts("Balanced gray: independent default-off key,256 stored values, write/commit/open failures, no-op/reload and no migration PASS");
    for(unsigned raw=0;raw<256;++raw){
        has_edge_style=true;stored_edge_style=pending_edge_style=(uint8_t)raw;app_settings_init();
        assert(app_settings_book_edge_style()==0&&!app_settings_book_edge_opt());
        assert(app_settings_set_book_edge_style((uint8_t)raw)==(raw==0));
    }
    assert(writes==before&&erases==erased);
    has_edge_style=false;app_settings_init();assert(app_settings_book_edge_style()==0);
    unsigned edge_commits=commits;
    fail_edge_save=fail_edge_commit=true;open_error=1;
    assert(app_settings_set_book_edge_style(0));
    assert(!app_settings_set_book_edge_style(1)&&!app_settings_set_book_edge_style(2));
    app_settings_set_book_edge_opt(true);assert(app_settings_book_edge_style()==0);
    app_settings_set_book_edge_opt(false);assert(app_settings_book_edge_style()==0);
    fail_edge_save=fail_edge_commit=false;open_error=0;
    app_settings_init();assert(!app_settings_book_edge_opt());
    assert(edge_reads==0&&writes==before&&commits==edge_commits&&erases==erased);
    before=writes;
    puts("Edge withdrawal: all256 legacy values ignored without reads/writes/commits/erase; original0 only, other setters rejected and unavailable NVS irrelevant PASS");
    for(unsigned raw=0;raw<256;++raw){
        has_animation=true;stored_animation=pending_animation=(uint8_t)raw;app_settings_init();
        assert(app_settings_book_turn_style()==(raw<=2?raw:0));
        assert(app_settings_book_turn_anim()==(raw==1||raw==2));
    }
    assert(writes==before&&erases==erased);
    has_animation=false;app_settings_init();assert(app_settings_book_turn_style()==0);
    assert(app_settings_set_book_turn_style(1));
    fail_animation_save=true;assert(!app_settings_set_book_turn_style(2)&&app_settings_book_turn_style()==1);fail_animation_save=false;
    fail_animation_commit=true;assert(!app_settings_set_book_turn_style(2)&&app_settings_book_turn_style()==1);fail_animation_commit=false;
    app_settings_init();assert(app_settings_book_turn_style()==1);
    assert(app_settings_set_book_turn_style(2));app_settings_init();assert(app_settings_book_turn_style()==2);
    before=writes;assert(!app_settings_set_book_turn_style(3)&&writes==before);
    app_settings_set_book_turn_anim(true);assert(app_settings_book_turn_style()==1);
    app_settings_set_book_turn_anim(false);assert(app_settings_book_turn_style()==0&&erases==erased);
    puts("Animation settings: all256 legacy values, default off,0/1 compatibility,2 opt-in and failed write/commit preserve mode PASS");
}
static void check_stroke_settings(void) {
    unsigned before=writes;
    has_stroke=false;app_settings_init();assert(app_settings_book_stroke()==0);
    for(unsigned raw=0;raw<256;++raw){
        has_stroke=true;stored_stroke=(uint8_t)raw;app_settings_init();
        assert(app_settings_book_stroke()==(raw<=32?(int)raw-16:0));
    }
    assert(writes==before);
    for(int step=-16;step<=16;++step){
        assert(app_settings_set_book_stroke(step));assert(app_settings_book_stroke()==step);
        assert(stored_stroke==step+16);app_settings_init();assert(app_settings_book_stroke()==step);
    }
    before=writes;
    assert(!app_settings_set_book_stroke(-17)&&!app_settings_set_book_stroke(17)&&writes==before);
    fail_stroke_save=true;assert(!app_settings_set_book_stroke(0)&&app_settings_book_stroke()==16);fail_stroke_save=false;
    fail_stroke_commit=true;assert(!app_settings_set_book_stroke(0)&&app_settings_book_stroke()==16);fail_stroke_commit=false;
    open_error=5;assert(!app_settings_set_book_stroke(0)&&app_settings_book_stroke()==16);open_error=0;
    assert(app_settings_set_book_stroke(0)&&app_settings_book_stroke()==0);check_existing_choices();
}
int main(int argc,char** argv) {
    (void)argv;
    if(argc>1) {
        // 启动存储不可用仍使用静态默认，不擦分区。/ An unavailable store at startup retains the static default without erasing the partition.
        init_error=5;app_settings_init();
        assert(!app_settings_book_night()&&!app_settings_book_direct()&&app_settings_gc_every()==10);
        assert(app_settings_sleep_mode()==APP_SLEEP_LIGHT);
        assert(app_settings_book_night_profile()==BOOK_NIGHT_PROFILE_CURRENT);
        init_error=0;open_error=5;app_settings_init();
        assert(!app_settings_book_night()&&!app_settings_book_direct()&&app_settings_gc_every()==10);
        assert(app_settings_sleep_mode()==APP_SLEEP_LIGHT);
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
    const uint8_t modes[] = {BOOK_NIGHT_PROFILE_CURRENT, BOOK_NIGHT_PROFILE_WHITE_REPAINT};
    for(unsigned m=0;m<sizeof(modes);++m) {
        uint8_t mode=modes[m], other=(uint8_t)(mode==BOOK_NIGHT_PROFILE_CURRENT?BOOK_NIGHT_PROFILE_WHITE_REPAINT:BOOK_NIGHT_PROFILE_CURRENT);
        app_settings_set_book_night_profile(other);
        unsigned before=writes;
        app_settings_set_book_night_profile(mode);
        assert(app_settings_book_night_profile()==mode&&stored_profile==mode&&writes==before+1&&commits==writes);
        app_settings_set_book_night_profile(mode);
        assert(writes==before+1);
        app_settings_set_book_night_profile(other);
        stored_profile=mode;app_settings_init();
        assert(app_settings_book_night_profile()==mode&&stored_cleanup==2);
        check_existing_choices();
    }
    unsigned before=writes;
    for(unsigned raw=1;raw<=255;++raw) if(raw!=3) app_settings_set_book_night_profile((uint8_t)raw);
    assert(app_settings_book_night_profile()==BOOK_NIGHT_PROFILE_WHITE_REPAINT&&writes==before);
    // 非法持久化值（含已移除的Crossmux值1）回当前方案，不修写旧键，所有其它阅读选择和旧键保持原样。
    // Invalid persisted values, including the removed Crossmux value 1, fall back to the current profile without repairing keys, preserving every other reader choice and the old key.
    for(unsigned raw=1;raw<=255;++raw) {
        if(raw==3)continue;
        stored_profile=(uint8_t)raw;app_settings_init();
        assert(app_settings_book_night_profile()==BOOK_NIGHT_PROFILE_CURRENT&&stored_profile==raw&&writes==before);
        check_existing_choices();
    }
    has_profile=false;app_settings_init();
    assert(app_settings_book_night_profile()==BOOK_NIGHT_PROFILE_CURRENT&&stored_cleanup==2);
    // 0.5.29黑基准值2回当前方案，旧细边值原样保留且不访问；其它阅读选择不变。
    // The 0.5.29 black-baseline value 2 falls back to Current; fine-edge values remain untouched and unread, preserving other reader choices.
    for(unsigned fine=0;fine<=255;++fine) {
        stored_fine=(uint8_t)fine;has_profile=true;stored_profile=2;app_settings_init();
        assert(app_settings_book_night_profile()==BOOK_NIGHT_PROFILE_CURRENT&&stored_profile==2);
        assert(stored_fine==fine&&!fine_reads&&writes==before);
        check_existing_choices();
    }
    app_settings_set_book_night_profile(BOOK_NIGHT_PROFILE_WHITE_REPAINT);
    assert(app_settings_book_night_profile()==3 && stored_profile==3 && writes==before+1);
    app_settings_init();assert(app_settings_book_night_profile()==3);
    app_settings_set_book_night_profile(3);assert(writes==before+1);
    assert(!app_settings_book_edge_gray());
    has_edge=true;stored_edge=0;app_settings_init();assert(!app_settings_book_edge_gray());
    app_settings_set_book_edge_gray(true);assert(stored_edge==1&&writes==before+2);
    app_settings_init();assert(app_settings_book_edge_gray());
    app_settings_set_book_edge_gray(true);assert(writes==before+2);
    app_settings_set_book_edge_gray(false);assert(writes==before+3);
    for(unsigned raw=0;raw<256;++raw){stored_edge=raw;app_settings_init();assert(app_settings_book_edge_gray()==(raw==1));check_existing_choices();}
    // 白预驱实验默认关、独立键、只认值1；与灰缘键互不影响。
    // The white pre-drive experiment defaults off on its own key, accepting only value 1, independent of the edge-gray key.
    assert(!app_settings_book_night_prewipe());
    app_settings_set_book_night_prewipe(true);assert(stored_prewipe==1&&writes==before+4);
    app_settings_init();assert(app_settings_book_night_prewipe());
    app_settings_set_book_night_prewipe(true);assert(writes==before+4);
    app_settings_set_book_night_prewipe(false);assert(writes==before+5&&stored_prewipe==0);
    for(unsigned raw=0;raw<256;++raw){stored_prewipe=raw;app_settings_init();assert(app_settings_book_night_prewipe()==(raw==1));check_existing_choices();}
    assert(!legacy_reads&&!fine_reads&&!erases&&writes==commits);
    // 新布局独立保存，升级缺键/脏值回三行，不改旧选择也不自动修写。
    // Persist layout separately; missing/dirty upgrade keys use three rows without changing older choices or repair writes.
    has_grid=false;app_settings_init();assert(!app_settings_shelf_grid());check_existing_choices();
    unsigned grid_writes=writes;app_settings_set_shelf_grid(true);assert(stored_grid==1&&writes==grid_writes+1);
    app_settings_init();assert(app_settings_shelf_grid());check_existing_choices();
    app_settings_set_shelf_grid(true);assert(writes==grid_writes+1);
    for(unsigned raw=0;raw<256;++raw){stored_grid=raw;app_settings_init();assert(app_settings_shelf_grid()==(raw==1));check_existing_choices();}
    assert(writes==grid_writes+1);
    for(int mode=0;mode<3;++mode)for(int style=0;style<4;++style){
        app_settings_set_sleep_mode((app_sleep_mode_t)mode);app_settings_set_lock_style((uint8_t)style);
        static const int expected[3][4]={{0,0,0,0},{1,0,0,0},{2,2,2,2}};
        assert((int)app_settings_effective_lock_mode()==expected[mode][style]);
        assert((strstr(app_settings_lock_wake_hint(),"长按")!=NULL)==(mode==2));
    }
    assert(!erases);
    // 摘录值5独立保存，旧壁纸4仍归图片0；升级不修写未知值或改旧偏好。
    // Persist excerpt value 5 independently, retaining legacy wallpaper 4 to image 0 migration without repairs or other preference changes.
    app_settings_set_lock_style(APP_LOCK_STYLE_QUOTE);
    unsigned style_writes=writes;
    app_settings_init();assert(app_settings_lock_style()==APP_LOCK_STYLE_QUOTE);check_existing_choices();
    app_settings_set_lock_style(APP_LOCK_STYLE_QUOTE);assert(writes==style_writes);
    for(unsigned raw=0;raw<256;++raw){
        s_lock_style=0;stored_lock_style=raw;app_settings_init();
        assert(app_settings_lock_style()==(raw<=3||raw==APP_LOCK_STYLE_QUOTE?raw:0));
        check_existing_choices();
    }
    assert(writes==style_writes);
    for(int mode=0;mode<3;++mode){
        app_settings_set_sleep_mode((app_sleep_mode_t)mode);app_settings_set_lock_style(APP_LOCK_STYLE_QUOTE);
        assert((int)app_settings_effective_lock_mode()==(mode==APP_SLEEP_DEEP?APP_SLEEP_LIGHT:mode));
        assert((strstr(app_settings_lock_wake_hint(),"长按")!=NULL)==(mode==2));
    }
    assert(app_settings_set_lock_style(0));
    unsigned before_failure=writes;fail_style_save=true;
    assert(!app_settings_set_lock_style(APP_LOCK_STYLE_QUOTE)&&app_settings_lock_style()==0&&writes==before_failure);
    fail_style_save=false;open_error=1;
    assert(!app_settings_set_lock_style(APP_LOCK_STYLE_QUOTE)&&app_settings_lock_style()==0);
    open_error=0;assert(app_settings_set_lock_style(APP_LOCK_STYLE_QUOTE));
    puts("Personal excerpt NVS: persist/reload/no-op, all 256 upgrade values, legacy 4 migration, failed activation retaining old style, static light/deep/off behavior PASS");
    puts("Shelf layout NVS: missing/256 stored values, persist/reload, repeated-save skip, old preferences retained; effective lock/wake policy for all modes/styles PASS");
    puts("0.5.29 withdrawal: saved black profile returns CURRENT without repair writes, all legacy fine values ignored, night/direct/10-page choices retained PASS");
    puts("night profile: distinct bk_ngprofile defaults CURRENT, the two tiers persist/reload, repeated saves skip, invalid setter/stored values including removed Crossmux 1 fall back safely PASS");
    check_stroke_settings();
    check_animation_settings();
    assert(app_settings_daily_goal()==0);
    for(unsigned raw=0;raw<256;++raw){has_goal=true;stored_goal=pending_goal=raw;app_settings_init();assert(app_settings_daily_goal()==(raw<=72 ? raw*5u : 0));}
    stored_goal=pending_goal=6;app_settings_init();assert(app_settings_daily_goal()==30);
    assert(!app_settings_set_daily_goal(361)&&!app_settings_set_daily_goal(31));
    unsigned goal_writes=writes;assert(app_settings_set_daily_goal(30)&&writes==goal_writes);
    fail_goal_save=true;assert(!app_settings_set_daily_goal(60)&&app_settings_daily_goal()==30);fail_goal_save=false;
    fail_goal_commit=true;assert(!app_settings_set_daily_goal(60)&&app_settings_daily_goal()==30);fail_goal_commit=false;
    app_settings_init();assert(app_settings_daily_goal()==30);
    assert(app_settings_set_daily_goal(360));app_settings_init();assert(app_settings_daily_goal()==360);
    assert(app_settings_set_daily_goal(0));app_settings_init();assert(app_settings_daily_goal()==0);
    open_error=1;assert(!app_settings_set_daily_goal(60)&&app_settings_daily_goal()==0);open_error=0;
    puts("Daily goal NVS: 256 stored values, 5-minute bounds, unset, no-op, save/commit/open failures, reload and retry PASS");
    puts("night pre-drive: distinct bk_prewipe defaults off, persists/reloads once, repeated saves skip, all 256 stored values collapse to off except 1 PASS");
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
    subprocess.run([os.environ.get("CC", "cc"), *flags, "-Imain", "-Imain/os", "-Imain/book", "-Itools/book_storage_stubs", str(source_path), "-o", str(binary)], cwd=ROOT, check=True)
    environment = dict(os.environ, UBSAN_OPTIONS="halt_on_error=1:print_stacktrace=1")
    subprocess.run([str(binary)], cwd=ROOT, env=environment, check=True)
    subprocess.run([str(binary), "unavailable"], cwd=ROOT, env=environment, check=True)
