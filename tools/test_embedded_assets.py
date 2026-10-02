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
        free(input);free(expected);free(output);
    }
    puts("embedded assets: GB2312 coverage, exact C round trips, truncation, checksum and capacity bounds passed");
}
'''
with tempfile.TemporaryDirectory(prefix="pico-assets-") as temp:
    path = Path(temp)
    (path / "test.c").write_text(unit)
    exe = path / "test"
    subprocess.run([os.environ.get("CC", "cc"), "-std=c11", "-Wall", "-Wextra", "-Werror", "-fsanitize=address,undefined",
                    "-Dheap_caps_free=free", "-Itools/book_epub_stubs", "-Itools/zip_host_stubs", "-Imain",
                    str(path / "test.c"), "main/asset_pack.c", "-lz", "-o", str(exe)], cwd=ROOT, check=True)
    args=[]
    for name,suffix in (("builtin","ttf"),("lock_4bpp","bin"),("loading_4bpp","bin")):
        args += [str(ROOT / f"main/assets/{name}.pack"), str(ROOT / f"main/assets/{name}.{suffix}")]
    subprocess.run([str(exe), *args],check=True)
