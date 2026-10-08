#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 mindreset
# SPDX-License-Identifier: Apache-2.0
"""真实 TXT/ZIP 缓存、跨进程及故障验证。/ Real TXT/ZIP cache, cross-process and failure checks."""
import os
from pathlib import Path
import subprocess
import tempfile
import zipfile

ROOT = Path(__file__).resolve().parents[1]


def write_cover(path: Path, shade: int) -> None:
    with zipfile.ZipFile(path, "w", compression=zipfile.ZIP_STORED) as book:
        book.writestr("META-INF/container.xml", "<container><rootfile full-path='OPS/book.opf'/></container>")
        book.writestr("OPS/book.opf", "<package><item properties='cover-image' href='art.png'/></package>")
        book.writestr("OPS/art.png", bytes([shade]) * 16)


def main() -> None:
    with tempfile.TemporaryDirectory(prefix="rp-cache-", dir="/tmp") as folder:
        media = Path(folder) / "sd"
        books = media / "books"
        books.mkdir(parents=True)
        source = books / "sample.txt"
        source.write_text("Chapter 1 First\nhello\nChapter 2 Last\nworld\n")
        cover = books / "cover.epub"
        write_cover(cover, 0x33)
        flags = ["-std=gnu11", "-Wall", "-Wextra", "-Werror", "-g", "-fsanitize=address,undefined", "-fno-omit-frame-pointer",
                 f'-DBOOK_CACHE_MEDIA_ROOT="{media}"', "-Itools/book_epub_stubs", "-Itools/zip_host_stubs", "-Imain/book"]
        if os.uname().sysname == "Darwin":
            developer = Path(subprocess.check_output(["xcode-select", "-p"], text=True).strip())
            sdk = Path(os.environ.get("PREVIEW_MACOS_SDK", developer / "Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk"))
            if sdk.exists():
                flags += ["-isysroot", str(sdk)]
        exe = Path(folder) / "test"
        subprocess.run([os.environ.get("CC", "cc"), *flags, "tools/book_cache_host_test.c", "main/book/book_txt.c",
                        "main/book/gbk.c", "main/book/book_cover.c", "main/book/zip_reader.c", "-lz", "-o", str(exe)], cwd=ROOT, check=True)
        env = dict(os.environ, UBSAN_OPTIONS="halt_on_error=1:print_stacktrace=1")
        for mode in ["cold", "warm"]:
            subprocess.run([str(exe), mode], cwd=ROOT, env=env, check=True)
        old_size, cover_size = source.stat().st_size, cover.stat().st_size
        source.write_text("Chapter 1 Other\nhello\nChapter 2 Last\nworld\n")
        write_cover(cover, 0x77)
        assert source.stat().st_size == old_size and cover.stat().st_size == cover_size
        subprocess.run([str(exe), "replace"], cwd=ROOT, env=env, check=True)
        subprocess.run([str(exe), "boundaries"], cwd=ROOT, env=env, check=True)
    print("PASS: persistent TXT/cover cold/warm processes, same-size replacement, corruption, interrupted commit, full-card fallback, quota and media loss")


if __name__ == "__main__":
    main()
