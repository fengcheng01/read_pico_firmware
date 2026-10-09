#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 mindreset
# SPDX-License-Identifier: Apache-2.0
"""真实字体、排版及调用侧的细边回归；只验证软件像素边界。/ Fine-edge regression through real font/layout/caller policy; software pixel boundaries only."""
from pathlib import Path
import argparse
import os
import re
import subprocess
import tempfile

from test_font_punctuation import STUBS

ROOT = Path(__file__).resolve().parents[1]


def function(source, name):
    match = re.search(r"^(?:static )?[^\n]+\b" + name + r"\([^;]*?\) \{", source, re.M)
    assert match, name
    at, depth = match.end(), 1
    while depth:
        depth += (source[at] == "{") - (source[at] == "}")
        at += 1
    return source[match.start():at]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--baseline", help="Optional existing Git revision for a complete old-layout byte comparison")
    parser.add_argument("--verify-regression", action="store_true")
    args = parser.parse_args()
    layout = (ROOT / "main/book/book_layout.c").read_text(encoding="utf-8")
    renderer = function(layout, "draw_body_text")
    prior_route = renderer.replace("if (s_direct_fine)", "if (false)")
    assert prior_route != renderer and "ttf_draw_text_px_bw" in renderer
    legacy = None
    if args.baseline:
        legacy = subprocess.check_output(["git", "show", args.baseline + ":main/book/book_layout.c"], cwd=ROOT, text=True)
        print("Complete layout reference:", args.baseline, flush=True)
    else:
        print("Self-contained raw-coverage reference; no Git history required", flush=True)
    header = (ROOT / "main/book/book_layout.h").read_text(encoding="utf-8")
    prototypes = re.findall(r"^(?:void|bool|size_t) book_layout_\w+\([^;]+\);", header, re.M)
    assert len(prototypes) >= 20
    names = [re.search(r"\b(book_layout_\w+)\(", line).group(1) for line in prototypes]
    renames = "\n".join("#define " + name + " legacy_" + name for name in names)
    legacy_api = "\n".join(re.sub(r"\b(book_layout_\w+)\(", r"legacy_\1(", line) for line in prototypes)
    reader = (ROOT / "main/apps/app_book.c").read_text(encoding="utf-8")
    policy = "\n".join(function(reader, name) for name in (
        "reader_night_profile_enabled", "reader_crossmux_enabled", "reader_direct_enabled", "apply_typography"))
    with tempfile.TemporaryDirectory(prefix="pico-direct-fine-") as directory:
        work = Path(directory)
        for name, content in STUBS.items():
            if name == "epdiy.h":
                content = content.replace("enum EpdFontFlags", "typedef struct { int x,y,width,height; } EpdRect;\nenum EpdFontFlags")
                content += "\nvoid epd_fill_rect(EpdRect rect, uint8_t color, uint8_t* framebuffer);\n"
            elif name == "esp_heap_caps.h":
                content += "\nstatic inline void* heap_caps_realloc(void* p, size_t n, unsigned caps) { (void)caps; return realloc(p,n); }\n"
            (work / name).write_text(content, encoding="utf-8")
        (work / "legacy_api.h").write_text(legacy_api, encoding="utf-8")
        (work / "reader_policy.h").write_text(policy, encoding="utf-8")
        if legacy is not None:
            (work / "legacy.c").write_text(renames + "\n" + legacy, encoding="utf-8")
        binary = work / "fine"
        flags = ["-std=gnu11", "-D_DEFAULT_SOURCE", "-Wall", "-Wextra", "-Werror", "-O1", "-g", "-fsanitize=address,undefined", "-fno-omit-frame-pointer"]
        includes = ["-I" + str(work), "-I" + str(ROOT), "-Imain", "-Imain/book", "-Imain/font"]
        sources = ["tools/book_direct_fine_host_test.c", "main/book/book_layout.c", "main/display_pixels.c"]
        if legacy is not None:
            flags += ["-DHOST_LEGACY_REFERENCE=1"]
            sources += [str(work / "legacy.c")]
        subprocess.run([os.environ.get("CC", "cc"), *flags, *includes, *sources, "-lm", "-o", str(binary)], cwd=ROOT, check=True)
        fonts = [str(ROOT / "main/assets/builtin.ttf"), str(ROOT / "sdcard/fonts/ChillDuanSansVF.ttf")]
        subprocess.run([str(binary), *fonts], cwd=ROOT, check=True)
        if args.verify_regression:
            # 恢复灰阶路线会通过默认帧对照，但必须在候选黑白路径断言处失败。
            # Restoring the gray route passes the default comparison but must fail the candidate binary-path assertion.
            broken = work / "broken.c"
            broken.write_text(layout.replace(renderer, prior_route), encoding="utf-8")
            negative = work / "negative"
            sources[1] = str(broken)
            subprocess.run([os.environ.get("CC", "cc"), *flags, *includes, *sources, "-lm", "-o", str(negative)], cwd=ROOT, check=True)
            result = subprocess.run([str(negative), *fonts], cwd=ROOT, capture_output=True, text=True)
            assert result.returncode != 0 and "bw_calls&&!gray_calls" in result.stderr, result.stderr
            print("Negative control: the old gray-only candidate route fails binary draw selection PASS")


if __name__ == "__main__":
    main()
