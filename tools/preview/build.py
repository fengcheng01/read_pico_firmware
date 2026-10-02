#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 mindreset
# SPDX-License-Identifier: Apache-2.0
"""编译真实绘制代码及预览边界。/ Build real drawing code with the preview boundary."""
import argparse
import json
import os
from pathlib import Path
import re
import subprocess
import zlib
import tempfile

ROOT = Path(__file__).resolve().parents[2]
OUT = ROOT / "build-host/desktop-preview"
SUPPORTED = {"app_os_home", "app_os_today", "app_os_settings", "app_os_time", "app_os_reading", "app_os_sleep", "app_os_pin", "app_os_storage", "app_os_tools", "app_transfer", "app_book", "app_reading", "app_refresh", "app_font_pick"}


def metadata():
    """读取当前注册顺序与原始文案。/ Read current registry order and original labels."""
    registry = (ROOT / "main/app/app_registry.c").read_text()
    table = re.search(r"s_apps\[\]\s*=\s*\{(.*?)\n\};", registry, re.S).group(1)
    sources = [(p, p.read_text()) for p in (ROOT / "main/apps").glob("*.c")]
    items = []
    for symbol in re.findall(r"&(app_\w+)", table):
        for path, source in sources:
            match = re.search(r"const app_desc_t\s+" + symbol + r"\s*=\s*\{(.*?)\};", source, re.S)
            if not match:
                continue
            fields = {}
            for field in ("title", "detail"):
                value = re.search(r"\." + field + r'\s*=\s*("(?:[^"\\]|\\.)*"|[A-Z_]\w*)', match.group(1)).group(1)
                if not value.startswith('"'):
                    value = re.search(r"^#define\s+" + re.escape(value) + r"\s+(.*)$", source, re.M).group(1)
                fields[field] = json.loads(value)
            items.append(dict(symbol=symbol, source=str(path.relative_to(ROOT)),
                              supported=symbol in SUPPORTED, **fields))
            break
        else:
            raise ValueError(f"Missing page descriptor: {symbol}")
    return items


def build(sanitize=False):
    OUT.mkdir(parents=True, exist_ok=True)
    # 将 SD 挂载边界指向隔离字体夹具，列表/选择仍走真实代码。
    # Repoint the SD mount boundary to isolated font fixtures; retain real listing and selection code.
    fonts = OUT / "sd/fonts"
    fonts.mkdir(parents=True, exist_ok=True)
    for index in range(8):
        target = fonts / f"中文字体测试{index + 1}.ttf"
        target.unlink(missing_ok=True)
        target.symlink_to(ROOT / "main/assets/builtin.ttf")
    includes = OUT / "include"
    for name in ("epdiy.h", "epd_highlevel.h", "cst836u.h", "sc7a20h_lab.h",
                 "esp_err.h", "esp_timer.h", "esp_heap_caps.h", "esp_log.h", "nvs.h", "nvs_flash.h", "miniz.h",
                 "e0470_epaper_waveform.h", "display.h", "read_pico_sd.h",
                 "read_pico_init.h", "read_pico_search.h",
                 "freertos/FreeRTOS.h", "freertos/semphr.h", "freertos/task.h"):
        path = includes / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text('#pragma once\n#include "preview_host.h"\n')

    # 原样提取 LGPL epdiy 的 CPU 绘制与旋转，排除硬件扫描函数；不修改上游文件。
    # Extract unmodified LGPL CPU drawing/rotation; omit hardware scanning and leave upstream intact.
    source = (ROOT / "components/epdiy/src/epdiy.c").read_text()
    graphics = source[source.index("// Simple x and y coordinate"):source.index("enum EpdDrawError epd_draw_image(")]
    graphics += source[source.index("void epd_set_rotation("):source.index("void epd_poweron(")]
    graphics = graphics.replace("static const EpdDisplay_t* display = NULL;",
                                "static const EpdDisplay_t panel = {1216, 684};\nstatic const EpdDisplay_t* display = &panel;")
    graphics += "\nint epd_width(void) { return display->width; }\nint epd_height(void) { return display->height; }\n"
    (OUT / "graphics.c").write_text('/* SPDX-License-Identifier: LGPL-3.0-or-later */\n#include "preview_host.h"\n#include <assert.h>\n' + graphics)

    # 夹具书目录：预览用真实文件系统扫描与解析。/ Fixture book dirs feed the real scanner and parsers.
    fixtures = OUT / "fx"
    full = fixtures / "f"; empty = fixtures / "e"; flash = fixtures / "h"
    for d in (full, empty, flash):
        d.mkdir(parents=True, exist_ok=True)
        for old in d.iterdir():
            old.unlink()
    sample_src = (ROOT / "main/assets/reading.md").read_text()
    sample = re.sub(r"^#+[ \t]*", "", sample_src, flags=re.M).replace("**", "")
    books = {
        "日常阅读.txt": sample,
        "纸上的时间.txt": "纸上的时间\n\n" + "旧码头潮水涨落，故事在字里行间沉淀。" * 200,
        "阅读记录.txt": "阅读记录\n\n" + "第七行的批注写着：这里的光线适合长读。" * 180,
        "城市漫步.txt": "城市漫步\n\n" + "从广场向西，香樟树影一路铺到旧码头。" * 220,
        # 两三页即读完，供读完面板场景翻到底。/ Finishes in two or three pages for the end-of-book scene.
        "短篇.txt": "短篇\n\n" + "黄昏退潮，码头只剩一盏灯。\n" * 10,
    }
    for name, text in books.items():
        (full / name).write_text(text)

    # 真 EPUB 夹具（含生成的 PNG 封面）：验证封面提取与 EPUB 打开全链路。
    # A real EPUB fixture (with a generated PNG cover) exercising cover extraction end to end.
    import struct as _struct, zipfile as _zipfile

    def _png_gray(width, height, pixel):
        raw = b''.join(b'\x00' + bytes(pixel(x, y) for x in range(width)) for y in range(height))
        def chunk(kind, data):
            return (_struct.pack('>I', len(data)) + kind + data +
                    _struct.pack('>I', zlib.crc32(kind + data) & 0xFFFFFFFF))
        return (b'\x89PNG\r\n\x1a\n' +
                chunk(b'IHDR', _struct.pack('>IIBBBBB', width, height, 8, 0, 0, 0, 0)) +
                chunk(b'IDAT', zlib.compress(raw, 6)) + chunk(b'IEND', b''))

    cover_png = _png_gray(200, 260, lambda x, y: 255 if x < 8 or x >= 192 or y < 8 or y >= 252 else
                          max(0, min(255, 250 - (x * 3 + y) // 3)))
    body = '<p>图文正文，图片应直接显示。</p><img src="cover.png"/>' + ''.join('<p>第%d段。潮水在旧码头往复，纸页间的光随之明暗。</p>' % i for i in range(220))
    with _zipfile.ZipFile(full / '封面之书.epub', 'w') as epub:
        info = _zipfile.ZipInfo('mimetype'); info.compress_type = _zipfile.ZIP_STORED
        epub.writestr(info, 'application/epub+zip')
        epub.writestr('META-INF/container.xml',
            '<?xml version="1.0"?><container version="1.0" xmlns="urn:oasis:names:tc:opendocument:xmlns:container">'
            '<rootfiles><rootfile full-path="OEBPS/content.opf" media-type="application/oebps-package+xml"/></rootfiles></container>')
        epub.writestr('OEBPS/cover.png', cover_png)
        epub.writestr('OEBPS/nav.xhtml',
            '<?xml version="1.0" encoding="utf-8"?><html xmlns="http://www.w3.org/1999/xhtml" '
            'xmlns:epub="http://www.idpf.org/2007/ops"><head><title>目录</title></head>'
            '<body><nav epub:type="toc"><ol><li><a href="c1.xhtml">潮水</a></li></ol></nav></body></html>')
        epub.writestr('OEBPS/c1.xhtml',
            '<?xml version="1.0" encoding="utf-8"?><html xmlns="http://www.w3.org/1999/xhtml">'
            '<head><title>潮水</title></head><body><h1>潮水</h1>' + body + '</body></html>')
        epub.writestr('OEBPS/content.opf',
            '<?xml version="1.0" encoding="utf-8"?><package xmlns="http://www.idpf.org/2007/opf" version="3.0" unique-identifier="id">'
            '<metadata xmlns:dc="http://purl.org/dc/elements/1.1/"><dc:title>封面之书</dc:title><dc:language>zh</dc:language>'
            '<meta name="cover" content="cover-image"/></metadata>'
            '<manifest>'
            '<item id="cover-image" href="cover.png" media-type="image/png" properties="cover-image"/>'
            '<item id="nav" href="nav.xhtml" media-type="application/xhtml+xml" properties="nav"/>'
            '<item id="c1" href="c1.xhtml" media-type="application/xhtml+xml"/>'
            '</manifest><spine><itemref idref="c1"/></spine></package>')

    items = metadata()
    (OUT / "pages.json").write_text(json.dumps(items, ensure_ascii=False, indent=2))
    unavailable = ['#include "app.h"']
    for item in items:
        if not item["supported"]:
            unavailable.append("const app_desc_t %s = {.title=%s, .detail=%s};" %
                               (item["symbol"], json.dumps(item["title"], ensure_ascii=False),
                                json.dumps(item["detail"], ensure_ascii=False)))
    (OUT / "unavailable.c").write_text("\n".join(unavailable))
    assets = ['#ifdef __APPLE__', '.section __TEXT,__const', '#else', '.section .rodata', '#endif']
    # 样文转换成纯文本，避免把 Markdown 标记当作读者正文。/ Convert the sample to plaintext instead of showing Markdown marks as prose.
    sample = (ROOT / "main/assets/reading.md").read_text()
    (OUT / "sample.txt").write_text(re.sub(r"^#+[ \t]*", "", sample, flags=re.M).replace("**", ""))
    for name, path in (("builtin_pack", "main/assets/builtin.pack"), ("reading_md", "main/assets/reading.md"),
                       ("preview_sample_txt", str(OUT / "sample.txt"))):
        assets += [f".globl _binary_{name}_start", f"_binary_{name}_start:",
                   f'.incbin "{path}"', f".globl _binary_{name}_end", f"_binary_{name}_end:", ".byte 0"]
    if os.uname().sysname != "Darwin":
        assets += ['.section .note.GNU-stack,"",@progbits']
    (OUT / "assets.S").write_text("\n".join(assets) + "\n")
    command = [os.environ.get("CC", "cc"), "-std=gnu11", "-O1" if sanitize else "-O2", "-g",
               '-DTTF_SD_ROOT="build-host/desktop-preview/sd"',
               "-Wall", "-Wextra", "-Wno-sign-compare", "-Wno-unused-variable", "-Wno-unused-function", "-Wno-unused-parameter",
               "-Itools/preview", f"-I{includes}", "-Imain/app", "-Imain/ui", "-Imain/ui/product",
               "-Imain/os", "-Imanaged_components/espressif__cjson/cJSON", "-Imain/book", "-Imain/font", "-Imain", "-Icomponents/read_pico_transfer/include"]
    if os.uname().sysname == "Darwin":
        developer = Path(subprocess.check_output(["xcode-select", "-p"], text=True).strip())
        sdk = Path(os.environ.get("PREVIEW_MACOS_SDK", developer / "Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk"))
        if sdk.exists():
            command += ["-isysroot", str(sdk)]
    if sanitize:
        command += ["-fsanitize=address,undefined", "-fno-omit-frame-pointer"]
    command += ["tools/preview/native.c", "tools/preview/esp_host.c",
                "tools/preview/today_port.c", "tools/preview/transfer_port.c", "tools/preview/device_port.c", "tools/preview/miniz_host.c",
                "main/asset_pack.c", "main/settings.c", "main/book/book_progress.c", "main/book/book_stats.c", "main/book/book_stats_store.c",
                "main/book/book_entry.c", "main/book/book_layout.c", "main/book/book_source.c", "main/book/book_home.c", "main/book/book_cover.c", "main/book/book_marks.c",
                "main/book/book_txt.c", "main/book/gbk.c", "main/book/book_epub.c", "main/book/zip_reader.c",
                "main/book/html_text.c", "main/book/book_image.c", "main/book/vendor/tjpgd.c",
                "components/read_pico_search/read_pico_search.c", "main/os/os_time.c", "main/os/os_device_pico.c", "main/os/os_sync.c", "main/os/os_sync_http.c", "managed_components/espressif__cjson/cJSON/cJSON.c", "main/os/os_lunar.c", "main/os/os_crash.c",
                "main/app/app_sleep_hooks.c",
                "main/ui/product/ui_product.c", str(OUT / "graphics.c"), str(OUT / "unavailable.c"),
                str(OUT / "assets.S"), "main/ui/ui_kit.c", "main/ui/ui_gesture.c", "main/ui/ui_menu.c", "main/ui/datamatrix.c",
                "main/font/ttf_font.c", "main/app/app_registry.c",
                *[item["source"] for item in items if item["supported"]], "-lz", "-lm"]
    # 编译成功才替换程序，避免影响仍在运行的预览。/ Replace only after success; preserve running previews.
    with tempfile.TemporaryDirectory(prefix="compile-", dir=OUT) as temporary:
        binary = Path(temporary) / "preview"
        subprocess.run(command + ["-o", str(binary)], cwd=ROOT, check=True)
        os.replace(binary, OUT / "preview")
    return OUT / "preview"


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--sanitize", action="store_true")
    args = parser.parse_args()
    print(build(args.sanitize))
