#!/usr/bin/env python3
# SPDX-FileCopyrightText: 2026 mindreset
# SPDX-License-Identifier: Apache-2.0
"""图片解码与书源集成回归。/ Image decoding and book source integration regressions."""
import base64
from pathlib import Path
import os
import struct
import subprocess
import zlib
import zipfile

ROOT = Path(__file__).resolve().parents[1]

def png(width=16, height=8):
    def chunk(tag, data):
        return struct.pack('>I', len(data)) + tag + data + struct.pack('>I', zlib.crc32(tag + data))
    row = b''.join(bytes((0, 0, 0, (0, 128, 255)[x % 3])) for x in range(width))
    return b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', width, height, 8, 6, 0, 0, 0)) + chunk(b'IDAT', zlib.compress((b'\0' + row) * height)) + chunk(b'IEND', b'')

# 自制16×8灰色JPEG，固定夹具避免新增图像工具依赖。
# Locally generated 16x8 gray JPEG; a fixed fixture avoids extra image-tool dependencies.
JPEG = base64.b64decode('/9j/4AAQSkZJRgABAQEAYABgAAD/2wBDAAMCAgMCAgMDAwMEAwMEBQgFBQQEBQoHBwYIDAoMDAsKCwsNDhIQDQ4RDgsLEBYQERMUFRUVDA8XGBYUGBIUFRT/2wBDAQMEBAUEBQkFBQkUDQsNFBQUFBQUFBQUFBQUFBQUFBQUFBQUFBQUFBQUFBQUFBQUFBQUFBQUFBQUFBQUFBQUFBT/wAARCAAIABADASIAAhEBAxEB/8QAHwAAAQUBAQEBAQEAAAAAAAAAAAECAwQFBgcICQoL/8QAtRAAAgEDAwIEAwUFBAQAAAF9AQIDAAQRBRIhMUEGE1FhByJxFDKBkaEII0KxwRVS0fAkM2JyggkKFhcYGRolJicoKSo0NTY3ODk6Q0RFRkdISUpTVFVWV1hZWmNkZWZnaGlqc3R1dnd4eXqDhIWGh4iJipKTlJWWl5iZmqKjpKWmp6ipqrKztLW2t7i5usLDxMXGx8jJytLT1NXW19jZ2uHi4+Tl5ufo6erx8vP09fb3+Pn6/8QAHwEAAwEBAQEBAQEBAQAAAAAAAAECAwQFBgcICQoL/8QAtREAAgECBAQDBAcFBAQAAQJ3AAECAxEEBSExBhJBUQdhcRMiMoEIFEKRobHBCSMzUvAVYnLRChYkNOEl8RcYGRomJygpKjU2Nzg5OkNERUZHSElKU1RVVldYWVpjZGVmZ2hpanN0dXZ3eHl6goOEhYaHiImKkpOUlZaXmJmaoqOkpaanqKmqsrO0tba3uLm6wsPExcbHyMnK0tPU1dbX2Nna4uPk5ebn6Onq8vP09fb3+Pn6/9oADAMBAAIRAxEAPwDKooooA//Z')

def gray_jpeg(width, height, overshoot=False):
    # 单色基线 JPEG：DC 差值零与 AC EOB 各一位，无外部编码器依赖。
    # Constant baseline JPEG: one bit each for zero DC delta and AC EOB, without an external encoder.
    def marker(tag, data):
        return b'\xff' + bytes([tag]) + struct.pack('>H', len(data) + 2) + data
    dqt = marker(0xdb, b'\0' + bytes([1]) * 64)
    sof = marker(0xc0, struct.pack('>BHHBBBB', 8, height, width, 1, 1, 0x11, 0))
    table = bytes([1]) + bytes(15) + b'\0'
    dc_table=bytes([1,1])+bytes(14)+bytes([0,11]) if overshoot else table
    dht = marker(0xc4, b'\0' + dc_table + b'\x10' + table)
    sos = marker(0xda, bytes([1, 1, 0, 0, 63, 0]))
    bits = ((width + 7) // 8) * ((height + 7) // 8) * 2
    bitstream=('10'+format(1080,'011b')+'0'+'0'*(bits-2)) if overshoot else '0'*bits
    bitstream+='1'*((-len(bitstream))%8)
    entropy=bytes(int(bitstream[i:i+8],2) for i in range(0,len(bitstream),8)).replace(b'\xff',b'\xff\0')
    return b'\xff\xd8' + dqt + sof + dht + sos + entropy + b'\xff\xd9'

def filtered_png(color, filter_type):
    def chunk(tag,data):
        return struct.pack('>I',len(data))+tag+data+struct.pack('>I',zlib.crc32(tag+data))
    def paeth(a,b,c):
        p=a+b-c
        return min((a,b,c),key=lambda x: abs(p-x))
    channels={0:1,2:3,3:1,4:2,6:4}[color]
    rows=[];expected=bytearray();previous=bytes(16*channels)
    for y in range(8):
        row=bytearray()
        for x in range(16):
            v=(x*13+y*23)%256;alpha=(x*17+y*31)%256
            rgb=(v,(v+37)%256,(v+91)%256)
            if color==3:
                index=(x+y)%3;row.append(index);gray=(0,128,255)[index];alpha=(0,128,255)[index]
            elif color in (0,4):
                row.append(v);gray=v
                if color==4:row.append(alpha)
                else:alpha=0 if v==0 else 255
            else:
                row.extend(rgb);gray=(77*rgb[0]+150*rgb[1]+29*rgb[2])>>8
                if color==6:row.append(alpha)
                else:alpha=0 if rgb==(0,37,91) else 255
            expected.append((gray*alpha+255*(255-alpha)+127)//255)
        filtered=bytearray([filter_type])
        for x,value in enumerate(row):
            a=row[x-channels] if x>=channels else 0;b=previous[x];c=previous[x-channels] if x>=channels else 0
            add=(0,a,b,(a+b)//2,paeth(a,b,c))[filter_type]
            filtered.append((value-add)%256)
        rows.append(filtered);previous=row
    extra=b''
    if color==3:extra=chunk(b'PLTE',bytes((0,0,0,128,128,128,255,255,255)))+chunk(b'tRNS',bytes((0,128,255)))
    elif color==0:extra=chunk(b'tRNS',b'\0\0')
    elif color==2:extra=chunk(b'tRNS',struct.pack('>HHH',0,37,91))
    packed=zlib.compress(b''.join(rows));mid=len(packed)//2
    data=b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>IIBBBBB',16,8,8,color,0,0,0))+extra+chunk(b'IDAT',packed[:mid])+chunk(b'IDAT',packed[mid:])+chunk(b'IEND',b'')
    return data,bytes(expected)

def big_noisy_png(w, h):
    # 不可压缩 RGBA 噪声图：文件远超 2 MiB，专测 FILE* 流式窗口喂入。
    # An incompressible RGBA noise image far past 2 MiB to exercise the FILE*
    # streaming window feed.
    def chunk(tag, data):
        return struct.pack('>I', len(data)) + tag + data + struct.pack('>I', zlib.crc32(tag + data))
    raw = os.urandom(w * h * 4)
    rows = b''.join(b'\0' + raw[y*w*4:(y+1)*w*4] for y in range(h))
    packed = zlib.compress(rows, 6)
    return (b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 6, 0, 0, 0)) +
            chunk(b'IDAT', packed) + chunk(b'IEND', b'')), raw

def reference_gray_rgba(raw, w, h):
    # 与 C 端相同的输出定尺和面积平均采样参考实现（缩小box平均）。
    # The same output sizing and area-average sampling as the C side (box when shrinking).
    dw, dh = w, h
    if dw > 648: dh = dh * 648 // dw; dw = 648
    if dh > 1000: dw = dw * 1000 // dh; dh = 1000
    out = bytearray()
    # C 端流式路径按 floor 所有权归属源行列，区间端点是 ceil 边界。
    # The C streaming path assigns source rows/columns by floor ownership with ceil span ends.
    ceil_div = lambda a, b: -((-a) // b)
    for dy in range(dh):
        ry0 = ceil_div(dy * h, dh); ry1 = ceil_div((dy + 1) * h, dh)
        for x in range(dw):
            cx0 = ceil_div(x * w, dw); cx1 = ceil_div((x + 1) * w, dw)
            total = 0; count = (ry1 - ry0) * (cx1 - cx0)
            for ry in range(ry0, ry1):
                base = ry * w * 4
                for cx in range(cx0, cx1):
                    r, g, b, a = raw[base+cx*4:base+cx*4+4]
                    gray = (77*r + 150*g + 29*b) >> 8
                    total += (gray*a + 255*(255-a) + 127)//255
            out.append((total + count//2)//count)
    return bytes(out), dw, dh

def rechunk_png(data, size, zero_idat=False):
    # 重切 IDAT 并插入 ancillary gAMA 与零长 IDAT：压窗口/块边界与跳读路径。
    # Re-split IDAT and insert an ancillary gAMA plus zero-length IDATs to stress
    # the window/chunk boundaries and the skip path.
    def chunk(tag, payload):
        return struct.pack('>I', len(payload)) + tag + payload + struct.pack('>I', zlib.crc32(tag + payload))
    at, pre, idat = 8, [], b''
    while at < len(data):
        (n,) = struct.unpack('>I', data[at:at+4]); tag = data[at+4:at+8]
        payload = data[at+8:at+8+n]; at += n + 12
        if tag == b'IDAT': idat += payload
        elif not idat: pre.append(chunk(tag, payload))
    parts = [idat[i:i+size] for i in range(0, len(idat), size)]
    idats = b''.join((chunk(b'IDAT', p) + (chunk(b'IDAT', b'') if zero_idat else b'')) for p in parts)
    return data[:8] + b''.join(pre) + chunk(b'gAMA', struct.pack('>I', 45455)) + idats + chunk(b'IEND', b'')

def progressive_jpeg(width, height, values=None):
    # 手造渐进 JPEG（可变 DC）：DC 首扫 + AC 扫，灰度单分量。values 给出各 8×8 块的
    # 目标灰度（默认全 128）；Q=1 时 Y 的 DC 系数 = 8*(v-128)，差分编码，DC 表 0..11 类
    # 各 4 位码。tjpgd 只认基线会拒收，JPEGDEC 只解 DC 扫输出 1/8 平面——两路都覆盖。
    # Hand-built progressive JPEG with real DC values: a first DC scan, then an AC scan,
    # single grayscale component. values gives each 8x8 block's target gray (all 128 by
    # default); with Q=1 the Y DC coefficient is 8*(v-128), differentially coded, and the
    # DC table carries categories 0..11 as 4-bit codes. tjpgd is baseline-only and rejects
    # it while JPEGDEC decodes the DC scan into the 1/8 plane — covering both paths.
    def marker(tag, data):
        return b'\xff' + bytes([tag]) + struct.pack('>H', len(data) + 2) + data
    dqt = marker(0xdb, b'\0' + bytes([1]) * 64)
    sof = marker(0xc2, struct.pack('>BHHBBBB', 8, height, width, 1, 1, 0x11, 0))
    mcus_x, mcus_y = (width + 7) // 8, (height + 7) // 8
    blocks = mcus_x * mcus_y
    if values is None: values = [128] * blocks
    assert len(values) == blocks and all(0 <= v <= 255 for v in values)
    counts = bytes(3) + bytes([12]) + bytes(12)
    dht = marker(0xc4, b'\x00' + counts + bytes(range(12)) + b'\x10' + (bytes([1]) + bytes(15) + b'\0'))
    dc_bits = ''
    dc = 0
    for v in values:
        coeff = 8 * (v - 128)
        diff = coeff - dc; dc = coeff
        cat = abs(diff).bit_length()
        dc_bits += format(cat, '04b')
        if cat:
            offset = diff if diff > 0 else diff + (1 << cat) - 1
            dc_bits += format(offset, '0%db' % cat)
    def scan(header, bits):
        bits += '1' * ((-len(bits)) % 8)
        entropy = bytes(int(bits[i:i+8], 2) for i in range(0, len(bits), 8)).replace(b'\xff', b'\xff\0')
        return marker(0xda, header) + entropy
    dc_scan = scan(bytes([1, 1, 0, 0, 0, 0]), dc_bits)   # Ss0 Se0 Ah0 Al0
    ac_scan = scan(bytes([1, 1, 0, 1, 63, 0]), '0' * blocks)  # Ss1 Se63 Ah0 Al0
    # JPEGDEC 的 open 拒绝小于 256 字节的文件；极简熵流的小夹具用 COM 段撑过阈值。
    # JPEGDEC's open rejects files under 256 bytes; pad tiny minimal-entropy fixtures
    # past the threshold with a comment segment.
    com = b'\xff\xfe' + struct.pack('>H', 180) + b' ' * 178
    return b'\xff\xd8' + com + dqt + sof + dht + dc_scan + ac_scan + b'\xff\xd9'

def progressive_expected(values, width, height):
    # 与 C 端一致的 1/8 平面双线性/面积平均重采样参考（输出未触 648/1000 上限时等于原图尺寸）。
    # The same bilinear/area-average resampling of the 1/8 plane as the C side (the output
    # equals the source size while under the 648/1000 caps).
    pw, ph = (width + 7) // 8, (height + 7) // 8
    dw, dh = width, height
    if dw > 648: dh = dh * 648 // dw; dw = 648
    if dh > 1000: dw = dw * 1000 // dh; dh = 1000
    up_x, up_y = dw > pw, dh > ph
    lerp = lambda a, b, f: (a * (255 - f) + b * f + 127) // 255
    out = bytearray()
    for dy in range(dh):
        if up_y:
            c = ((dy * 2 + 1) * 256 * ph) // (2 * dh)
            ry0, fy = c >> 8, c & 255
            r1 = ry0 + 1 if ry0 + 1 < ph else ry0
            if r1 == ry0: fy = 255
        else:
            ry0 = dy * ph // dh
            r1 = (dy + 1) * ph // dh
            fy = 0
        for x in range(dw):
            if up_x:
                c = ((x * 2 + 1) * 256 * pw) // (2 * dw)
                x0, fx = c >> 8, c & 255
                x1 = x0 + 1 if x0 + 1 < pw else x0
                if x1 == x0: fx = 255
            else:
                cx0 = x * pw // dw
                cx1 = (x + 1) * pw // dw
                fx = 0
            if up_x and up_y:
                out.append(lerp(lerp(values[ry0 * pw + x0], values[ry0 * pw + x1], fx),
                                lerp(values[r1 * pw + x0], values[r1 * pw + x1], fx), fy))
            elif up_x:
                total = sum(lerp(values[r * pw + x0], values[r * pw + x1], fx) for r in range(ry0, r1))
                out.append((total + (r1 - ry0) // 2) // (r1 - ry0))
            elif up_y:
                s0 = sum(values[ry0 * pw + c] for c in range(cx0, cx1))
                s1 = sum(values[r1 * pw + c] for c in range(cx0, cx1))
                cnt = cx1 - cx0
                out.append(lerp((s0 + cnt // 2) // cnt, (s1 + cnt // 2) // cnt, fy))
            else:
                total, cnt = 0, 0
                for r in range(ry0, r1):
                    for c in range(cx0, cx1):
                        total += values[r * pw + c]; cnt += 1
                out.append((total + cnt // 2) // cnt)
    return bytes(out), dw, dh

def fake_big_progressive():
    # 大于 2 MiB 的 SOF2 骨架：渐进文件超出回退闸门必须被拒绝。
    # An SOF2 skeleton over 2 MiB: progressive files past the fallback gate must reject.
    def marker(tag, data):
        return b'\xff' + bytes([tag]) + struct.pack('>H', len(data) + 2) + data
    dqt = marker(0xdb, b'\0' + bytes([1]) * 64)
    sof2 = marker(0xc2, struct.pack('>BHHBBBB', 8, 48, 64, 1, 1, 0x11, 0))
    table = bytes([1]) + bytes(15) + b'\0'
    dht = marker(0xc4, b'\x00' + table + b'\x10' + table)
    sos = marker(0xda, bytes([1, 1, 0, 0, 63, 0]))
    junk = b'\x55' * (3 * 1024 * 1024)
    return b'\xff\xd8' + dqt + sof2 + dht + sos + junk + b'\xff\xd9'

def main():
    work = ROOT / 'build/book-image-tests'
    work.mkdir(parents=True, exist_ok=True)
    (work / 'alpha.png').write_bytes(png())
    (work / 'gray.jpg').write_bytes(JPEG)
    # 实机验收口径：864×1216 RGBA 壁纸截图必须可解（输出 648×912）。
    # Device acceptance: an 864x1216 RGBA wallpaper shot must decode (output 648x912).
    (work / 'large.png').write_bytes(png(864, 1216))
    for name, width, height in [('cover',1000,1333),('tall',1080,2400),('edge',9,17)]:
        (work / (name + '.jpg')).write_bytes(gray_jpeg(width,height))
    book = work / 'images.epub'
    with zipfile.ZipFile(book, 'w', compression=zipfile.ZIP_DEFLATED) as z:
        z.writestr('META-INF/container.xml', '<container><rootfiles><rootfile full-path="OPS/book.opf"/></rootfiles></container>')
        z.writestr('OPS/book.opf', '<package><manifest><item id="a" href="text/chapter.xhtml" media-type="application/xhtml+xml"/><item id="b" href="other/chapter.xhtml" media-type="application/xhtml+xml"/><item id="c" href="text/titled.xhtml" media-type="application/xhtml+xml"/></manifest><spine><itemref idref="a"/><itemref idref="b"/><itemref idref="c"/></spine></package>')
        z.writestr('OPS/text/chapter.xhtml', '<p>before</p><img src="../images/a%20b&amp;c.png"/><svg><image xlink:href="../images/gray.jpg"/></svg><img src="https://invalid/a.png"/><img src="../../../escape.png"/><img src="../images/broken.png"/><img src="../images/huge.png"/><img src="../images/absent.png"/><p>after</p>')
        z.writestr('OPS/other/chapter.xhtml', '<img src="../images/./a%20b%26c.png"/><img src="../images/gray.jpg#same"/>')
        z.writestr('OPS/text/titled.xhtml', '<img src="../images/a%20b%26c.png"/><h2>Chapter title</h2><img src="../images/gray.jpg"/><p>body</p>')
        z.writestr('OPS/images/a b&c.png', png())
        z.writestr('OPS/images/gray.jpg', JPEG)
        z.writestr('OPS/images/broken.png', b'broken')
        # 流式解码后按峰值堆已拒不掉可压缩大图；改走 4096 像素尺寸上限验证拒绝。
        # Streaming decode no longer rejects compressible large images on heap; the
        # 4096-pixel dimension cap now carries the rejection case.
        z.writestr('OPS/images/huge.png', png(4097, 2))
    exe = work / 'test'
    sources = ['tools/book_image_host_test.c', 'main/book/book_image.c', 'main/book/vendor/tjpgd.c', 'main/book/book_epub.c', 'main/book/book_xpointer.c', 'main/book/html_text.c', 'main/book/zip_reader.c', 'main/book/book_source.c', 'main/book/book_txt.c', 'main/book/book_cache.c', 'main/book/gbk.c']
    # C 源保持严告警；JPEGDEC 是 vendored C++（宽松告警 + 同款消毒器），最终用 c++ 链接。
    # C sources keep strict warnings; JPEGDEC is vendored C++ (relaxed warnings, same
    # sanitizers) and the final link uses c++.
    objects = []
    for src in sources:
        obj = work / (Path(src).name + '.o')
        subprocess.run(['cc', '-std=c11', '-Wall', '-Wextra', '-Werror', '-g', '-fsanitize=address,undefined',
                        '-Itools/book_image_stubs', '-Itools/book_epub_stubs', '-Itools/zip_host_stubs',
                        '-Imain/book', '-Icomponents/jpegdec/src', '-c', src, '-o', str(obj)], cwd=ROOT, check=True)
        objects.append(obj)
    for src in ['components/jpegdec/src/JPEGDEC.cpp', 'components/jpegdec/src/jpegdec_shim.cpp']:
        obj = work / (Path(src).name + '.o')
        # vendored 上游存在奇地址读取与负移位（同款参考固件在同芯片量产运行）；只挂 ASAN
        # 抓越界，不挂 UBSAN，避免把上游已知风格当回归。
        # The vendored upstream has misaligned reads and negative shifts (the same
        # reference firmware ships it on the same chip); keep ASAN for bounds, drop
        # UBSAN so upstream's known style is not treated as a regression.
        subprocess.run(['c++', '-std=c++17', '-w', '-g', '-fsanitize=address',
                        '-Icomponents/jpegdec/src', '-c', src, '-o', str(obj)], cwd=ROOT, check=True)
        objects.append(obj)
    subprocess.run(['c++', '-g', '-fsanitize=address,undefined', *[str(o) for o in objects], '-lz', '-o', str(exe)], cwd=ROOT, check=True)
    subprocess.run([str(exe), str(work / 'alpha.png'), str(work / 'gray.jpg'), str(book), *[str(work / (n + '.jpg')) for n in ['cover','tall','edge']]], check=True, env={**os.environ, 'UBSAN_OPTIONS':'halt_on_error=1'})
    # 大图 RGBA 验收：--inspect 以 2 MiB 输出预算解码并打印尺寸（864×1216→648×912）。
    # Large-RGBA acceptance: --inspect decodes under a 2 MiB output budget and prints
    # dimensions (864x1216 -> 648x912).
    inspect = subprocess.run([str(exe), '--inspect', str(work / 'large.png')], capture_output=True, text=True, check=True)
    assert 'decoded 648x912' in inspect.stdout, inspect.stdout
    for color in [0,2,3,4,6]:
        for filter_type in range(5):
            data,expected=filtered_png(color,filter_type)
            name=work / f'filter-{color}-{filter_type}'
            name.with_suffix('.png').write_bytes(data);name.with_suffix('.gray').write_bytes(expected)
            subprocess.run([str(exe),'--expect',str(name.with_suffix('.png')),str(name.with_suffix('.gray'))],check=True,env={**os.environ,'UBSAN_OPTIONS':'halt_on_error=1'})
    (work/'overshoot.jpg').write_bytes(gray_jpeg(16,8,True))
    (work/'overshoot.gray').write_bytes(bytes([255])*128)
    subprocess.run([str(exe),'--expect',str(work/'overshoot.jpg'),str(work/'overshoot.gray')],check=True,env={**os.environ,'UBSAN_OPTIONS':'halt_on_error=1'})
    # 文件流式入口：大噪声 PNG 字节级对照、双路径一致性、重切 IDAT、渐进 JPEG 与截断拒绝。
    # File-streaming entry: byte-exact big-noise PNG, memory/file parity, re-chunked
    # IDATs, progressive JPEG and truncation rejections.
    env = {**os.environ,'UBSAN_OPTIONS':'halt_on_error=1'}
    big, raw = big_noisy_png(864, 1216)
    assert len(big) > 2*1024*1024
    (work/'huge_rgba.png').write_bytes(big)
    expected, dw, dh = reference_gray_rgba(raw, 864, 1216)
    (work/'huge_rgba.gray').write_bytes(expected)
    subprocess.run([str(exe),'--expect-file',str(work/'huge_rgba.png'),str(work/'huge_rgba.gray')],check=True,env=env)
    inspect_file = subprocess.run([str(exe),'--inspect-file',str(work/'huge_rgba.png')],capture_output=True,text=True,check=True)
    assert f'decoded {dw}x{dh}' in inspect_file.stdout, inspect_file.stdout
    # 重切：小片+零长 IDAT 压块边界；9000 字节大片跨过 8192 窗口。两者都须与参考逐字节一致。
    # Re-chunks: tiny pieces plus zero-length IDATs stress chunk boundaries; 9000-byte
    # pieces span the 8192 window. Both must match the reference byte for byte.
    small, small_expected = filtered_png(6, 1)
    (work/'rechunked.png').write_bytes(rechunk_png(small, 7, zero_idat=True))
    (work/'rechunked.gray').write_bytes(small_expected)
    subprocess.run([str(exe),'--expect-file',str(work/'rechunked.png'),str(work/'rechunked.gray')],check=True,env=env)
    (work/'rechunked_big.png').write_bytes(rechunk_png(big, 9000))
    subprocess.run([str(exe),'--expect-file',str(work/'rechunked_big.png'),str(work/'huge_rgba.gray')],check=True,env=env)
    # 小文件渐进 JPEG：tjpgd 拒收后由 JPEGDEC 只解 DC 扫，输出常量灰。
    # Small progressive JPEG: tjpgd rejects it and JPEGDEC's DC-only scan
    # produces constant gray.
    (work/'progressive.jpg').write_bytes(progressive_jpeg(16,8))
    (work/'progressive.gray').write_bytes(bytes([128])*128)
    subprocess.run([str(exe),'--expect-file',str(work/'progressive.jpg'),str(work/'progressive.gray')],check=True,env=env)
    # DC 渐变渐进 JPEG：1/8 平面 + 最近邻放大，字节级对照（覆盖非零 DC 类别与差分链）。
    # A DC-gradient progressive JPEG compared byte-exact through the 1/8 plane and
    # nearest-neighbour upscale (covers non-zero DC categories and the diff chain).
    mcus_x, mcus_y = (64 + 7) // 8, (48 + 7) // 8
    gradient = [90 + ((bx * 37 + by * 11) % 90) for by in range(mcus_y) for bx in range(mcus_x)]
    (work/'progressive_dc.jpg').write_bytes(progressive_jpeg(64, 48, gradient))
    expected_dc, edw, edh = progressive_expected(gradient, 64, 48)
    (work/'progressive_dc.gray').write_bytes(expected_dc)
    subprocess.run([str(exe),'--expect-file',str(work/'progressive_dc.jpg'),str(work/'progressive_dc.gray')],check=True,env=env)
    # 大于 2 MiB 的真实结构渐进 JPEG 必须仍可解：tjpgd 拒收后 SOF 嗅探须回到 SOI，
    # 再经 JPEGDEC 的 FILE 流式 DC 扫解码（实机壁纸路径，曾因偏移未回绕整类失败）。
    # A structurally valid progressive JPEG past 2 MiB must still decode: after tjpgd
    # rejects it, the SOF sniff must rewind to SOI and JPEGDEC streams the DC scan from
    # FILE* (the device wallpaper path, which failed wholesale without the rewind).
    body = progressive_jpeg(64, 48, gradient)
    pad_at = body.index(b'\xff\xda')
    com = b''.join(b'\xff\xfe' + struct.pack('>H', 65535) + bytes(65533) for _ in range(40))
    (work/'progressive_big.jpg').write_bytes(body[:pad_at] + com + body[pad_at:])
    assert (work/'progressive_big.jpg').stat().st_size > 2 * 1024 * 1024 + 512 * 1024
    subprocess.run([str(exe),'--expect-file',str(work/'progressive_big.jpg'),str(work/'progressive_dc.gray')],check=True,env=env)
    # 双路径一致性：内存整读与文件流式必须同判同字节。
    # Path parity: whole-file memory decoding and streaming file decoding must agree.
    for name in ['alpha.png','gray.jpg','cover.jpg','tall.jpg','edge.jpg','large.png','progressive.jpg','progressive_dc.jpg','rechunked.png']:
        subprocess.run([str(exe),'--file-consistency',str(work/name)],check=True,env=env)
    # 截断与超大渐进骨架一律干净拒绝（PNG 解码器不校验 CRC，只截尾部 CRC 的文件仍可解）。
    # Truncations and the oversized progressive skeleton reject cleanly (PNG CRCs are
    # not validated, so files trimmed only of the trailing CRC can still decode).
    for off in [20, 33, 41, 60, 1000, len(big)//2, len(big)-5000, len(big)-13]:
        (work/f'trunc-{off}.png').write_bytes(big[:off])
        subprocess.run([str(exe),'--file-reject',str(work/f'trunc-{off}.png')],check=True,env=env)
    (work/'big_progressive.jpg').write_bytes(fake_big_progressive())
    subprocess.run([str(exe),'--file-reject',str(work/'big_progressive.jpg')],check=True,env=env)
    print('PNG: all 5 row filters, 5 color types, split IDAT, palette and color-key alpha passed')
    print('file entry: big-noise byte-exact, window/chunk splits, progressive fallback, parity and rejections passed')

if __name__ == '__main__':
    main()
