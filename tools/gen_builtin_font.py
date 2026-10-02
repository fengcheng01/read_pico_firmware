#!/usr/bin/env python3
"""生成界面与 GB2312 常用字库及压缩资源。/ Generate the UI/GB2312 font and compressed assets."""

from __future__ import annotations

import argparse
import re
import sys
import struct
import zlib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CHARSET = ROOT / "main/font/charset.txt"
OUT = ROOT / "main/assets/builtin.ttf"
SCAN_DIRS = [
    ROOT / "main/os",
    ROOT / "main/usb",
    ROOT / "main/ui",
    ROOT / "main/app",
    ROOT / "main/apps",
    ROOT / "main/book",
    ROOT / "main/factory",
    ROOT / "main/assets",
    ROOT / "main/app_main.c",
    ROOT / "main/display.c",
    ROOT / "main/sleep.c",
    ROOT / "main/settings.c",
    ROOT / "components/sy7636a",
    ROOT / "components/sc7a20h",
    ROOT / "components/read_pico",
    ROOT / "components/read_pico_pmu",
]
TEXT_SUFFIXES = {".md", ".txt"}
# 使用 Regular 实例和压缩资源，在原应用分区内补齐常用字。/ Use a packed Regular instance to fit common characters in the existing app partition.
MAX_BYTES = 1600 * 1024
STRING_RE = re.compile(r'"(?:\\.|[^"\\])*"')


def ascii_printable() -> str:
    return "".join(chr(i) for i in range(0x20, 0x7F))


def common_chinese() -> str:
    # 覆盖 GB2312 全部字符，避免内置字库只认识界面文案。/ Cover all GB2312 characters, beyond UI literals.
    chars = []
    for lead in range(0xa1, 0xf8):
        for trail in range(0xa1, 0xff):
            try:
                chars.append(bytes((lead, trail)).decode("gb2312"))
            except UnicodeDecodeError:
                pass
    return "".join(chars)


def load_preset(path: Path) -> str:
    chars: list[str] = []
    if not path.is_file():
        return ""
    for line in path.read_text(encoding="utf-8").splitlines():
        s = line.strip()
        if not s or s.startswith("#"):
            continue
        chars.append(s)
    return "".join(chars)


def unescape(literal: str) -> str:
    body = literal[1:-1]
    return (
        body.replace("\\\\", "\0")
        .replace("\\n", "\n")
        .replace("\\t", "\t")
        .replace("\\\"", '"')
        .replace("\0", "\\")
    )


def scan_sources() -> str:
    texts: list[str] = []
    files: list[Path] = []
    for item in SCAN_DIRS:
        if item.is_file():
            files.append(item)
        elif item.is_dir():
            files.extend(item.rglob("*.c"))
            files.extend(item.rglob("*.h"))
            files.extend(item.rglob("*.md"))
            files.extend(item.rglob("*.txt"))
    for path in files:
        data = path.read_text(encoding="utf-8", errors="ignore")
        if path.suffix in TEXT_SUFFIXES:
            texts.append(data)
            continue
        for match in STRING_RE.finditer(data):
            texts.append(unescape(match.group(0)))
    return "".join(texts)


def unique_text(*parts: str) -> str:
    seen: set[str] = set()
    out: list[str] = []
    for part in parts:
        for ch in part:
            if ch in seen or ch in "\n\r\t":
                continue
            seen.add(ch)
            out.append(ch)
    return "".join(sorted(out, key=lambda c: ord(c)))


def rename_subset(font) -> None:
    # 子集属于修改版，使用自有字体名并保留原版权和 OFL 元数据。
    # Subsets are modified versions: use our own family name and retain copyright and OFL metadata.
    names = {1: "Read Pico UI", 3: "Read Pico UI 1.0", 4: "Read Pico UI", 6: "ReadPicoUI",
             16: "Read Pico UI", 18: "Read Pico UI", 21: "Read Pico UI", 25: "ReadPicoUI"}
    for record in list(font["name"].names):
        if record.nameID in names:
            font["name"].setName(names[record.nameID], record.nameID, record.platformID, record.platEncID, record.langID)
    if "fvar" in font:
        for instance in font["fvar"].instances:
            name_id = instance.postscriptNameID
            if name_id == 0xffff:
                continue
            for record in list(font["name"].names):
                if record.nameID == name_id:
                    value = re.sub(r"[^A-Za-z0-9-]", "", record.toUnicode().replace("ChillDuanSans", "ReadPicoUI"))
                    font["name"].setName(value, name_id, record.platformID, record.platEncID, record.langID)


def subset_variable(src: Path, text: str, dest: Path) -> int:
    from fontTools.subset import Subsetter, Options
    from fontTools.ttLib import TTFont

    font = TTFont(src)
    options = Options()
    options.layout_features = ["*"]
    options.notdef_outline = True
    options.recommended_glyphs = True
    options.name_IDs = ["*"]
    options.name_legacy = True
    options.glyph_names = False
    options.ignore_missing_glyphs = True
    subsetter = Subsetter(options=options)
    subsetter.populate(text=text)
    subsetter.subset(font)
    rename_subset(font)
    dest.parent.mkdir(parents=True, exist_ok=True)
    font.save(dest)
    font.close()
    return dest.stat().st_size


def subset_regular(src: Path, text: str, dest: Path) -> int:
    from fontTools.subset import Subsetter, Options
    from fontTools.ttLib import TTFont
    from fontTools.varLib.instancer import instantiateVariableFont

    vf = TTFont(src)
    if "fvar" in vf:
        inst = instantiateVariableFont(vf, {"wght": 400}, inplace=False)
        vf.close()
        vf = inst
    # 原字体缺少全角拉丁字母及 ǐ；补拉丁别名与真实组合字形，不映射到缺字方框。
    # Fill absent fullwidth Latin aliases and compose a real i-caron glyph instead of a missing-glyph box.
    cmap = vf.getBestCmap()
    aliases = {cp: cmap[cp - 0xfee0] for cp in range(0xff01, 0xff5f)
               if cp not in cmap and cp - 0xfee0 in cmap}
    if 0x01d0 not in cmap:
        from fontTools.pens.ttGlyphPen import TTGlyphPen
        base, mark = cmap[0x0131], cmap[0x02c7]
        glyf = vf["glyf"]
        glyf[base].recalcBounds(glyf)
        glyf[mark].recalcBounds(glyf)
        b, m = glyf[base], glyf[mark]
        pen = TTGlyphPen(vf.getGlyphSet())
        pen.addComponent(base, (1, 0, 0, 1, 0, 0))
        pen.addComponent(mark, (1, 0, 0, 1, (b.xMin + b.xMax - m.xMin - m.xMax) // 2,
                                b.yMax + 30 - m.yMin))
        order = vf.getGlyphOrder()
        order.append("pico.icaron")
        vf.setGlyphOrder(order)
        glyf["pico.icaron"] = pen.glyph()
        vf["hmtx"]["pico.icaron"] = vf["hmtx"][base]
        aliases[0x01d0] = "pico.icaron"
    for table in vf["cmap"].tables:
        if table.isUnicode():
            table.cmap.update(aliases)
    options = Options()
    # stb_truetype 不执行 TTF hinting；删除指令节省固件空间。/ stb_truetype ignores TTF hinting; omit instructions to save flash.
    options.hinting = False
    options.layout_features = ["*"]
    options.notdef_outline = True
    options.recommended_glyphs = True
    options.ignore_missing_glyphs = True
    subsetter = Subsetter(options=options)
    subsetter.populate(text=text)
    subsetter.subset(vf)
    rename_subset(vf)
    dest.parent.mkdir(parents=True, exist_ok=True)
    vf.save(dest)
    vf.close()
    return dest.stat().st_size


def default_src() -> Path:
    for path in (
        ROOT / "sdcard/fonts/ChillDuanSansVF.ttf",
        ROOT / "ChillDuanSansVF.ttf",
        ROOT / "tools/fonts/ChillDuanSansVF.ttf",
    ):
        if path.is_file():
            return path
    return ROOT / "ChillDuanSansVF.ttf"


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--src", type=Path, default=default_src())
    parser.add_argument("--out", type=Path, default=OUT)
    args = parser.parse_args()
    if not args.src.is_file():
        print(f"missing source font: {args.src}", file=sys.stderr)
        return 1

    text = unique_text(ascii_printable(), common_chinese(), load_preset(CHARSET), scan_sources())
    cjk = sum(1 for ch in text if "\u4e00" <= ch <= "\u9fff")
    print(f"charset {len(text)} chars ({cjk} CJK)")

    size = subset_regular(args.src, text, args.out)
    data = args.out.read_bytes()
    packed = b"RPFT" + struct.pack("<I", size) + zlib.compress(data, 9)
    if len(packed) > MAX_BYTES:
        print("compressed subset too large", file=sys.stderr)
        return 1
    args.out.with_suffix(".pack").write_bytes(packed)
    for name in ("lock_4bpp", "loading_4bpp"):
        raw = (ROOT / f"main/assets/{name}.bin").read_bytes()
        (ROOT / f"main/assets/{name}.pack").write_bytes(b"RPFT" + struct.pack("<I", len(raw)) + zlib.compress(raw, 9))
    print(f"wrote {args.out} ({size} bytes, Regular); packed {len(packed)} bytes")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
