#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 mindreset
# SPDX-License-Identifier: Apache-2.0
"""中文：真实NVS设置代码的睡眠率模型迁移、原子记录和失败重试回归。
English: Sleep-rate model migration, atomic records and failed-save retry through real NVS settings code.
"""
from pathlib import Path
import os
import re
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
source = (ROOT / "main/settings.c").read_text()
for header in ("esp_log.h", "nvs.h", "nvs_flash.h"):
    source = source.replace(f'#include "{header}"', "")

UNIT = r'''
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
#define ESP_ERR_NVS_INVALID_LENGTH 4
#define NVS_READONLY 0
#define NVS_READWRITE 1
#define ESP_LOGI(...) ((void)0)
#define ESP_LOGW(...) ((void)0)
typedef struct { const char* key; unsigned kind; size_t size; uint8_t data[128]; } cell_t;
static cell_t cells[16];
static size_t cell_count;
static uint8_t staged[128];
static size_t staged_size;
static bool staged_valid;
static unsigned old_reads, old_writes, blob_writes, commits, erases;
static int init_error, open_error, write_error, commit_error;
static cell_t* find(const char* key) {
    for (size_t i=0; i<cell_count; i++) if (!strcmp(cells[i].key,key)) return &cells[i];
    return NULL;
}
static cell_t* put(const char* key, unsigned kind, const void* data, size_t size) {
    cell_t* cell=find(key);
    if (!cell) { assert(cell_count<16); cell=&cells[cell_count++]; cell->key=key; }
    assert(size<=sizeof(cell->data)); cell->kind=kind; cell->size=size;
    memcpy(cell->data,data,size); return cell;
}
static void put_u8(const char* key, uint8_t value) { put(key,1,&value,sizeof(value)); }
static void put_u32(const char* key, uint32_t value) { put(key,2,&value,sizeof(value)); }
static void put_text(const char* key, const char* value) { put(key,3,value,strlen(value)+1); }
static esp_err_t get(const char* key, unsigned kind, void* out, size_t* size) {
    cell_t* cell=find(key);
    if (!cell || cell->kind!=kind) return ESP_ERR_NVS_NOT_FOUND;
    if (!out) { *size=cell->size; return ESP_OK; }
    if (*size<cell->size) return ESP_ERR_NVS_INVALID_LENGTH;
    memcpy(out,cell->data,cell->size); *size=cell->size; return ESP_OK;
}
static size_t host_strlcpy(char* dst,const char* src,size_t cap) {
    size_t n=strlen(src);
    if(cap){size_t copied=n<cap-1?n:cap-1;memcpy(dst,src,copied);dst[copied]=0;}
    return n;
}
#define strlcpy host_strlcpy
static esp_err_t nvs_flash_init(void) { return init_error; }
static esp_err_t nvs_flash_erase(void) { ++erases; return ESP_OK; }
static esp_err_t nvs_open(const char* ns,int mode,nvs_handle_t* out) {
    assert(!strcmp(ns,"read_pico")&&(mode==NVS_READONLY||mode==NVS_READWRITE));
    *out=1; staged_valid=false; return open_error;
}
static void nvs_close(nvs_handle_t h) { assert(h==1); staged_valid=false; }
static esp_err_t nvs_commit(nvs_handle_t h) {
    assert(h==1); ++commits;
    if (commit_error) return commit_error;
    assert(staged_valid); put("sl_clk_v1",4,staged,staged_size); staged_valid=false; return ESP_OK;
}
static esp_err_t nvs_get_u8(nvs_handle_t h,const char* key,uint8_t* out) {
    assert(h==1); size_t size=1; return get(key,1,out,&size);
}
static esp_err_t nvs_get_i8(nvs_handle_t h,const char* key,int8_t* out) { (void)h;(void)key;(void)out;return ESP_ERR_NVS_NOT_FOUND; }
static esp_err_t nvs_get_u32(nvs_handle_t h,const char* key,uint32_t* out) {
    assert(h==1); if (!strcmp(key,"sl_clk_ppm")) ++old_reads;
    size_t size=4; return get(key,2,out,&size);
}
static esp_err_t nvs_get_str(nvs_handle_t h,const char* key,char* out,size_t* cap) {
    assert(h==1); return get(key,3,out,cap);
}
static esp_err_t nvs_get_blob(nvs_handle_t h,const char* key,void* out,size_t* cap) {
    assert(h==1); return get(key,4,out,cap);
}
static esp_err_t nvs_set_u8(nvs_handle_t h,const char* key,uint8_t value) { (void)h;(void)key;(void)value;assert(false);return 1; }
static esp_err_t nvs_set_i8(nvs_handle_t h,const char* key,int8_t value) { (void)h;(void)key;(void)value;assert(false);return 1; }
static esp_err_t nvs_set_u32(nvs_handle_t h,const char* key,uint32_t value) {
    (void)h;(void)value; if (!strcmp(key,"sl_clk_ppm")) ++old_writes; assert(false);return 1;
}
static esp_err_t nvs_set_str(nvs_handle_t h,const char* key,const char* value) { (void)h;(void)key;(void)value;assert(false);return 1; }
static esp_err_t nvs_set_blob(nvs_handle_t h,const char* key,const void* value,size_t size) {
    assert(h==1&&!strcmp(key,"sl_clk_v1")&&size<=sizeof(staged)); ++blob_writes;
    if (write_error) return write_error;
    memcpy(staged,value,size); staged_size=size; staged_valid=true; return ESP_OK;
}
'''

CHECKS = r'''
static void seed_existing(void) {
    memset(cells,0,sizeof(cells)); cell_count=0;
    put_u32("sl_clk_ppm",14410);
    put_u8("bk_night",1); put_u8("bk_direct",1); put_u8("bk_ngprofile",1); put_u8("gc_every",10);
    put_u8("bk_px",60); put_u8("bk_guide",2); put_u8("clk_auto",0);
    put_text("wifi_ssid","saved-home"); put_text("wifi_pass","saved-secret");
    put_text("book_progress","unchanged-progress");
}
static void preserved(void) {
    assert(app_settings_book_night()&&app_settings_book_direct());
    assert(app_settings_book_night_profile()==BOOK_NIGHT_PROFILE_CROSSMUX);
    assert(app_settings_gc_every()==10&&app_settings_book_px()==60&&app_settings_book_guide()==2);
    assert(!app_settings_clock_auto());
    uint32_t old; memcpy(&old,find("sl_clk_ppm")->data,sizeof(old)); assert(old==14410);
    assert(!strcmp((const char*)find("wifi_ssid")->data,"saved-home"));
    assert(!strcmp((const char*)find("wifi_pass")->data,"saved-secret"));
    assert(!strcmp((const char*)find("book_progress")->data,"unchanged-progress"));
    assert(!old_reads&&!old_writes&&!erases);
}
static sleep_clock_record_t valid_record(int32_t ppm) {
    sleep_clock_record_t record={
        .magic=0x31434c53u,.model=OS_CLOCK_RATE_MODEL_VERSION,.source=sleep_clock_source(),
        .cycles=sleep_clock_cycles(),.calibration=OS_CLOCK_RATE_CALIBRATION_VERSION,.ppm=ppm,
    };
    record.checksum=sleep_clock_checksum(&record); return record;
}
static void reject_record(const sleep_clock_record_t* record, size_t size) {
    put("sl_clk_v1",4,record,size); app_settings_init();
    assert(!app_settings_sleep_clock_valid()&&app_settings_sleep_clock_ppm()==0); preserved();
}
static void migration_and_load(void) {
    seed_existing(); app_settings_init();
    assert(!app_settings_sleep_clock_valid()&&app_settings_sleep_clock_ppm()==0); preserved();
    assert(sleep_clock_source()!=0&&sleep_clock_cycles()>10);
    const int32_t rates[]={0,4410,-4149,-10000,10000};
    for(size_t i=0;i<sizeof(rates)/sizeof(rates[0]);i++) {
        sleep_clock_record_t record=valid_record(rates[i]); put("sl_clk_v1",4,&record,sizeof(record));
        app_settings_init(); assert(app_settings_sleep_clock_valid()&&app_settings_sleep_clock_ppm()==rates[i]);
        unsigned before=blob_writes; app_settings_set_sleep_clock_ppm(rates[i]); assert(blob_writes==before);
        app_settings_init(); assert(app_settings_sleep_clock_valid()&&app_settings_sleep_clock_ppm()==rates[i]); preserved();
    }
    sleep_clock_record_t good=valid_record(4410), bad;
    bad=good; bad.magic++; bad.checksum=sleep_clock_checksum(&bad); reject_record(&bad,sizeof(bad));
    bad=good; bad.model++; bad.checksum=sleep_clock_checksum(&bad); reject_record(&bad,sizeof(bad));
    bad=good; bad.source++; bad.checksum=sleep_clock_checksum(&bad); reject_record(&bad,sizeof(bad));
    bad=good; bad.cycles++; bad.checksum=sleep_clock_checksum(&bad); reject_record(&bad,sizeof(bad));
    bad=good; bad.calibration++; bad.checksum=sleep_clock_checksum(&bad); reject_record(&bad,sizeof(bad));
    bad=good; bad.ppm=10001; bad.checksum=sleep_clock_checksum(&bad); reject_record(&bad,sizeof(bad));
    bad=good; bad.ppm=-10001; bad.checksum=sleep_clock_checksum(&bad); reject_record(&bad,sizeof(bad));
    bad=good; bad.ppm=INT32_MIN; bad.checksum=sleep_clock_checksum(&bad); reject_record(&bad,sizeof(bad));
    bad=good; bad.checksum^=1; reject_record(&bad,sizeof(bad));
    for(size_t size=0;size<sizeof(good);size++) reject_record(&good,size);
    uint8_t oversized[sizeof(good)+4]; memcpy(oversized,&good,sizeof(good)); memset(oversized+sizeof(good),0,4);
    put("sl_clk_v1",4,oversized,sizeof(oversized)); app_settings_init();
    assert(!app_settings_sleep_clock_valid()&&app_settings_sleep_clock_ppm()==0);
    put_u32("sl_clk_v1",14410); app_settings_init();
    assert(!app_settings_sleep_clock_valid()&&app_settings_sleep_clock_ppm()==0); preserved();
    assert(!blob_writes&&!commits);
    puts("sleep model: legacy +4410 ignored; matching zero/positive/negative rates reload, invalid identity/checksum/type/length rejected, other settings intact PASS");
}
static void save_and_retry(void) {
    seed_existing(); app_settings_init();
    for(int failure=0;failure<3;failure++) {
        int32_t rate=-4000+failure;
        open_error=failure==0?77:0; write_error=failure==1?77:0; commit_error=failure==2?77:0;
        sleep_clock_record_t previous={0}; cell_t* old=find("sl_clk_v1");
        bool had=old!=NULL; if(had) memcpy(&previous,old->data,sizeof(previous));
        app_settings_set_sleep_clock_ppm(rate);
        assert(app_settings_sleep_clock_ppm()==rate&&!app_settings_sleep_clock_valid());
        old=find("sl_clk_v1"); assert((old!=NULL)==had);
        if(had) assert(!memcmp(old->data,&previous,sizeof(previous)));
        open_error=write_error=commit_error=0;
        unsigned writes=blob_writes, committed=commits;
        app_settings_set_sleep_clock_ppm(rate);
        assert(app_settings_sleep_clock_valid()&&app_settings_sleep_clock_ppm()==rate);
        assert(blob_writes==writes+1&&commits==committed+1);
        sleep_clock_record_t saved; old=find("sl_clk_v1"); assert(old&&old->size==sizeof(saved));
        memcpy(&saved,old->data,sizeof(saved)); assert(sleep_clock_record_valid(&saved)&&saved.ppm==rate);
        app_settings_set_sleep_clock_ppm(rate); assert(blob_writes==writes+1&&commits==committed+1);
        app_settings_init(); assert(app_settings_sleep_clock_valid()&&app_settings_sleep_clock_ppm()==rate); preserved();
    }
    int32_t rate=app_settings_sleep_clock_ppm(); unsigned writes=blob_writes, committed=commits;
    app_settings_set_sleep_clock_ppm(10001); app_settings_set_sleep_clock_ppm(-10001);
    assert(app_settings_sleep_clock_valid()&&app_settings_sleep_clock_ppm()==rate&&blob_writes==writes&&commits==committed);
    init_error=77; app_settings_init(); assert(!app_settings_sleep_clock_valid()&&app_settings_sleep_clock_ppm()==0);
    init_error=0; open_error=77; app_settings_init(); assert(!app_settings_sleep_clock_valid()&&app_settings_sleep_clock_ppm()==0);
    open_error=0; app_settings_init(); assert(app_settings_sleep_clock_valid()&&app_settings_sleep_clock_ppm()==rate); preserved();
    puts("sleep model: one complete blob+commit, open/set/commit failures keep session rate pending, same-value retries and reboot persistence PASS");
}
int main(void) {
    migration_and_load(); save_and_retry();
    printf("actual RTC model: source=%u cycles=%u wrapper=%u algorithm=%u PASS\n",
           (unsigned)sleep_clock_source(),(unsigned)sleep_clock_cycles(),
           (unsigned)OS_CLOCK_RATE_CALIBRATION_VERSION,(unsigned)OS_CLOCK_RATE_MODEL_VERSION);
}
'''


def main():
    # 优先真实构建配置；未构建时读产品配置，测试不得假定板子的慢钟源。
    # Prefer the actual build configuration, or product configuration before a build; never assume the board's slow source.
    config = ROOT / "sdkconfig"
    if not config.exists():
        config = ROOT / "sdkconfig.defaults"
    definitions = []
    for name, value in re.findall(r"^(CONFIG_RTC_CLK_(?:SRC_[A-Z0-9_]+|CAL_CYCLES))=(\w+)$", config.read_text(), re.M):
        definitions.append(f"#define {name} {1 if value == 'y' else value}")
    assert any("CAL_CYCLES" in line for line in definitions)
    flags = ["-std=gnu11", "-Wall", "-Wextra", "-Werror", "-g", "-DESP_PLATFORM",
             "-fsanitize=address,undefined", "-fno-omit-frame-pointer"]
    if os.uname().sysname == "Darwin":
        developer = Path(subprocess.check_output(["xcode-select", "-p"], text=True).strip())
        sdk = developer / "Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk"
        if sdk.exists():
            flags += ["-isysroot", str(sdk)]
    with tempfile.TemporaryDirectory(prefix="pico-sleep-model-", dir="/tmp") as folder:
        work = Path(folder)
        (work / "sdkconfig.h").write_text("\n".join(definitions) + "\n")
        (work / "test.c").write_text(UNIT + source + CHECKS)
        exe = work / "test"
        subprocess.run([os.environ.get("CC", "cc"), *flags, "-I" + str(work), "-Imain", "-Imain/os",
                        str(work / "test.c"), "-o", str(exe)], cwd=ROOT, check=True)
        subprocess.run([str(exe)], check=True)


if __name__ == "__main__":
    main()
