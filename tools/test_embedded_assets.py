#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 mindreset
# SPDX-License-Identifier: Apache-2.0
"""内置中文覆盖、资源往返和实际 C 解压边界回归。/ Embedded Chinese coverage, asset round trips and real C inflate bounds."""
from pathlib import Path
import os
import struct
import subprocess
import tempfile
import zlib
from fontTools.ttLib import TTFont
from gen_builtin_font import common_chinese

ROOT = Path(__file__).resolve().parents[1]
font = TTFont(ROOT / "main/assets/builtin.ttf")
cmap = font.getBestCmap()
missing = set(common_chinese() + "中庸一剑青莲十步千里寒蝉清风明月") - {chr(cp) for cp in cmap}
assert not missing, sorted(missing)
font.close()
for name, suffix in (("builtin", "ttf"), ("lock_4bpp", "bin"), ("loading_4bpp", "bin")):
    raw = (ROOT / f"main/assets/{name}.{suffix}").read_bytes()
    packed = (ROOT / f"main/assets/{name}.pack").read_bytes()
    assert packed[:4] == b"RPFT" and struct.unpack("<I", packed[4:8])[0] == len(raw)
    assert zlib.decompress(packed[8:]) == raw
unit = r'''
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "asset_pack.h"
#include "ui_kit.h"
static int live, allocation_calls, fail_allocation, paintings;
void* asset_test_malloc(size_t bytes, int caps) {
    assert(caps == 3); allocation_calls++;
    if (allocation_calls == fail_allocation) return NULL;
    void* p=malloc(bytes); if(p) live++; return p;
}
void asset_test_free(void* p) { if(p) { assert(live>0); live--; free(p); } }
int epd_rotated_display_width(void) { return 684; }
int epd_rotated_display_height(void) { return 1216; }
void ui_draw_full_image(uint8_t* fb,const uint8_t* image) {
    paintings++; assert(fb && fb!=image);
    if(image) memcpy(fb,image,684*1216/2); else memset(fb,255,684*1216/2);
}
static void assert_white(const unsigned char* output,size_t raw) {
    for(size_t i=0;i<raw;i++) assert(output[i]==255);
}
static unsigned char* load(const char* path, size_t* length) {
    FILE* f=fopen(path,"rb"); assert(f); fseek(f,0,SEEK_END); *length=(size_t)ftell(f); rewind(f);
    unsigned char* data=malloc(*length); assert(data); assert(fread(data,1,*length,f)==*length); fclose(f); return data;
}
int main(int argc,char** argv) {
    assert(argc==7);
    for(int i=1;i<argc;i+=2) {
        size_t packed,raw; unsigned char *input=load(argv[i],&packed), *expected=load(argv[i+1],&raw), *output=malloc(raw);
        assert(asset_pack_size(input,packed)==raw);
        assert(asset_pack_unpack(input,packed,output,raw) && !memcmp(output,expected,raw));
        assert(!asset_pack_unpack(input,packed,output,raw-1));
        assert(!asset_pack_unpack(input,packed-1,output,raw));
        input[packed-1]^=1; assert(!asset_pack_unpack(input,packed,output,raw)); input[packed-1]^=1;
        input[0]^=1; assert(!asset_pack_size(input,packed) && !asset_pack_unpack(input,packed,output,raw));
        assert(!asset_pack_unpack(NULL,0,output,raw));
        input[0]^=1;
        assert(!live);
        if (raw==684*1216/2) {
            allocation_calls=0; fail_allocation=0;
            assert(ui_draw_packed_full_image(output,input,packed));
            assert(!memcmp(output,expected,raw) && !live && allocation_calls==2);
            for(int fail=1;fail<=2;fail++) {
                memset(output,0,raw); allocation_calls=0; fail_allocation=fail;
                assert(!ui_draw_packed_full_image(output,input,packed));
                assert_white(output,raw); assert(!live && allocation_calls==fail);
            }
            fail_allocation=0;
            input[packed-1]^=1;
            assert(!ui_draw_packed_full_image(output,input,packed));
            assert_white(output,raw); assert(!live); input[packed-1]^=1;
            assert(!ui_draw_packed_full_image(output,input,packed-1));
            assert_white(output,raw); assert(!live);
            int before=allocation_calls;
            assert(!ui_draw_packed_full_image(output,NULL,0));
            assert_white(output,raw); assert(!live && allocation_calls==before);
            assert(!ui_draw_packed_full_image(NULL,input,packed));
            assert(!live && allocation_calls==before);
        }
        free(input);free(expected);free(output);
    }
    puts("embedded assets: GB2312 coverage, exact C round trips, truncation, checksum/capacity bounds, separate bitmap buffer and allocation/decode failure cleanup passed");
}
'''
with tempfile.TemporaryDirectory(prefix="pico-assets-") as temp:
    path = Path(temp)
    (path / "test.c").write_text(unit)
    (path / "esp_heap_caps.h").write_text("#pragma once\n#include <stddef.h>\n#define MALLOC_CAP_SPIRAM 1\n#define MALLOC_CAP_8BIT 2\nvoid* asset_test_malloc(size_t,int);\nvoid asset_test_free(void*);\n#define heap_caps_malloc asset_test_malloc\n#define heap_caps_free asset_test_free\n")
    exe = path / "test"
    subprocess.run([os.environ.get("CC", "cc"), "-std=c11", "-Wall", "-Wextra", "-Werror", "-fsanitize=address,undefined",
                    f"-I{path}", "-Itools/ui_gesture_stubs", "-Itools/zip_host_stubs", "-Imain", "-Imain/ui", "-Imain/font",
                    str(path / "test.c"), "main/asset_pack.c", "main/ui/ui_assets.c", "-lz", "-o", str(exe)], cwd=ROOT, check=True)
    args=[]
    for name,suffix in (("builtin","ttf"),("lock_4bpp","bin"),("loading_4bpp","bin")):
        args += [str(ROOT / f"main/assets/{name}.pack"), str(ROOT / f"main/assets/{name}.{suffix}")]
    subprocess.run([str(exe), *args],check=True)
