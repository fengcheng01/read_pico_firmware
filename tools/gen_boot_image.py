#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""把已完成的开机插画编码为面板资源，不修改原图。/ Encode finished boot artwork for the panel without changing its source."""
import argparse
from pathlib import Path
import struct
import zlib
from PIL import Image, ImageOps

ROOT = Path(__file__).resolve().parents[1]


def encode(source, output):
    # 保持原构图比例；4bpp 的左像素在低半字节，与 epdiy 资源契约一致。
    # Preserve composition proportions; the left pixel uses the low nibble under epdiy's asset contract.
    with Image.open(source) as image:
        image = ImageOps.exif_transpose(image).convert('RGB')
        fitted = ImageOps.contain(image, (684, 1216), Image.Resampling.LANCZOS)
        canvas = Image.new('RGB', (684, 1216), 'white')
        canvas.paste(fitted, ((684 - fitted.width) // 2, (1216 - fitted.height) // 2))
        pixels = canvas.convert('L').tobytes()
    levels = bytes(min(15, (value + 8) // 17) for value in pixels)
    raw = bytes(a | b << 4 for a, b in zip(levels[::2], levels[1::2]))
    output.parent.mkdir(parents=True, exist_ok=True)
    output.with_suffix('.bin').write_bytes(raw)
    output.with_suffix('.pack').write_bytes(b'RPFT' + struct.pack('<I', len(raw)) + zlib.compress(raw, 9))
    print(f'{output}: {len(raw)} raw bytes, {output.with_suffix(".pack").stat().st_size} packed bytes')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', type=Path, default=ROOT / 'assets/image/pagerest-boot.png')
    parser.add_argument('--output', type=Path, default=ROOT / 'main/assets/loading_4bpp')
    args = parser.parse_args()
    encode(args.source, args.output)
