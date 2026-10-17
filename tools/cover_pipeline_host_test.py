#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""真实封面定位、ZIP、PNG/JPEG与缓存集成；只在系统/ROM边界使用宿主替身。
Integrate actual cover lookup, ZIP, PNG/JPEG and cache, substituting only system/ROM boundaries.
"""
from pathlib import Path
import importlib.util
import os
import subprocess
import tempfile
import zipfile

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('image_fixtures', ROOT / 'tools/test_book_images.py')
fixtures = importlib.util.module_from_spec(spec)
spec.loader.exec_module(fixtures)
DRIVER = r'''
#include "book_cover.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int image_fail_after = -1;
int main(int argc,char** argv) {
    assert(argc>1);
    for(int i=1;i<argc;i++) {
        uint8_t *a=NULL,*b=NULL;
        assert(book_cover_read(argv[i],&a)==BOOK_COVER_READY&&a);
        assert(book_cover_read(argv[i],&b)==BOOK_COVER_READY&&b);
        assert(!memcmp(a,b,(size_t)BOOK_COVER_W*BOOK_COVER_H));
        unsigned lo=255,hi=0;
        for(size_t n=0;n<(size_t)BOOK_COVER_W*BOOK_COVER_H;n++){if(a[n]<lo)lo=a[n];if(a[n]>hi)hi=a[n];}
        assert(lo<hi&&lo<200);
        free(a);free(b);
    }
    puts("Real EPUB covers: cold/warm cache, stored/deflated ZIP, PNG/baseline/progressive JPEG, XML prefixes/attribute spacing and encoded dot paths PASS");
}
'''

def main():
    with tempfile.TemporaryDirectory(prefix='rp-cover-pipeline-') as folder:
        work = Path(folder)
        (work / 'driver.c').write_text(DRIVER)
        books = []
        values = bytes((x * 17 + y * 23) % 240 for y in range(6) for x in range(8))
        for index, image in enumerate([fixtures.png(180, 232), fixtures.JPEG,
                                       fixtures.gray_jpeg(1000, 1333), fixtures.progressive_jpeg(64, 48, values)]):
            for encoded in (False, True):
                book = work / f'cover-{index}-{int(encoded)}.epub'
                art = 'OPS/art.png' if index == 0 else 'OPS/art.jpg'
                href = './sub/../art%2E' + art.rsplit('.', 1)[1] + '#front' if encoded else art.split('/', 1)[1]
                container = '<c:container><c:rootfile full-path = "OPS/book.opf"/></c:container>'
                opf = f'<opf:package><opf:manifest><opf:item id="front" properties = "cover-image" href = "{href}"/></opf:manifest></opf:package>'
                with zipfile.ZipFile(book, 'w', compression=zipfile.ZIP_STORED if encoded else zipfile.ZIP_DEFLATED) as z:
                    z.writestr('META-INF/container.xml', container)
                    z.writestr('OPS/book.opf', opf)
                    z.writestr(art, image)
                books.append(book)
        flags = ['-std=gnu11', '-Wall', '-Wextra', '-Werror', '-g', '-fsanitize=address,undefined',
                 '-Itools/book_image_stubs', '-Itools/book_epub_stubs', '-Itools/zip_host_stubs',
                 '-Imain/book', '-Icomponents/jpegdec/src', f'-DBOOK_CACHE_MEDIA_ROOT="{work}"']
        objects = []
        for source in [work / 'driver.c', ROOT / 'main/book/book_cover.c', ROOT / 'main/book/book_cache.c',
                       ROOT / 'main/book/zip_reader.c', ROOT / 'main/book/book_image.c', ROOT / 'main/book/vendor/tjpgd.c']:
            obj = work / (source.name + '.o')
            subprocess.run([os.environ.get('CC', 'cc'), *flags, '-c', str(source), '-o', str(obj)], cwd=ROOT, check=True)
            objects.append(obj)
        for source in ['components/jpegdec/src/JPEGDEC.cpp', 'components/jpegdec/src/jpegdec_shim.cpp']:
            obj = work / (Path(source).name + '.o')
            subprocess.run(['c++', '-std=c++17', '-w', '-g', '-fsanitize=address', '-Icomponents/jpegdec/src',
                            '-c', source, '-o', str(obj)], cwd=ROOT, check=True)
            objects.append(obj)
        binary = work / 'test'
        subprocess.run(['c++', '-fsanitize=address,undefined', *map(str, objects), '-lz', '-o', str(binary)], check=True)
        subprocess.run([str(binary), *map(str, books)], check=True, env={**os.environ, 'UBSAN_OPTIONS': 'halt_on_error=1'})
        assert len(list((work / '.read-pico-cache').glob('*.bin'))) == len(books)

if __name__ == '__main__':
    main()
