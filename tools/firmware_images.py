#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""中文：按实际 IDF 布局同步生成完整和仅应用升级镜像，不连接设备。
English: Produce full and app-only upgrade images from IDF's actual layout, without a device.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import struct
import subprocess
import sys
import tempfile


def sha256(data):
    return hashlib.sha256(data).hexdigest()


def inputs(build):
    description = json.loads((build / 'project_description.json').read_text())
    config = json.loads((build / 'flasher_args.json').read_text())
    version = description['project_version']
    if not re.fullmatch(r'[A-Za-z0-9][A-Za-z0-9._-]*', version):
        raise ValueError('Invalid firmware version')
    chip = config['extra_esptool_args']['chip']
    if chip != 'esp32s3' or description['target'] != chip:
        raise ValueError('Expected the Read Pico ESP32-S3 target')
    app_offset = int(config['app']['offset'], 0)
    app_path = (build / config['app']['file']).resolve()
    segments = []
    for address, name in config['flash_files'].items():
        offset = int(address, 0)
        path = (build / name).resolve()
        data = path.read_bytes()
        if offset < 0 or offset % 4096 or not data:
            raise ValueError('Invalid flash segment: ' + name)
        segments.append((offset, path, data))
    segments.sort()
    for i, (offset, path, data) in enumerate(segments):
        if i and offset < segments[i - 1][0] + len(segments[i - 1][2]):
            raise ValueError('Overlapping flash segments')
    if not any(offset == app_offset and path == app_path for offset, path, _ in segments):
        raise ValueError('App descriptor does not match the flash map')
    if config['bootloader']['offset'] != '0x0':
        raise ValueError('Expected ESP32-S3 bootloader at 0x0')
    for key in ('bootloader', 'partition-table', 'app'):
        entry = config[key]
        if str(entry.get('encrypted', 'false')).lower() != 'false':
            raise ValueError('Encrypted builds require a separate provisioning flow')
        if not any(o == int(entry['offset'], 0) and p == (build / entry['file']).resolve()
                   for o, p, _ in segments):
            raise ValueError('Missing flash descriptor: ' + key)
    app = app_path.read_bytes()
    # 标准 IDF 应用描述紧跟首段头；核对版本，防止旧镜像被误命名。
    # The standard IDF app descriptor follows the first segment header; reject stale image versions.
    if (len(app) < 80 or app[0] != 0xE9 or app[32:36] != struct.pack('<I', 0xABCD5432)
            or app[48:80].split(b'\0', 1)[0].decode() != version):
        raise ValueError('App image/version does not match this build')
    table = (build / config['partition-table']['file']).read_bytes()
    matching = []
    for at in range(0, len(table) - 31, 32):
        magic, kind, _, offset, size, _, _ = struct.unpack_from('<HBBII16sI', table, at)
        if magic == 0x50AA and kind == 0 and offset == app_offset:
            matching.append(size)
    if len(matching) != 1 or len(app) > matching[0]:
        raise ValueError('App does not fit its partition')
    settings = config['flash_settings']
    size = re.fullmatch(r'(\d+)MB', settings['flash_size'])
    if not size or segments[-1][0] + len(segments[-1][2]) > int(size[1]) * 1024 * 1024:
        raise ValueError('Flash map exceeds configured flash size')
    return description, settings, segments, app_offset, app


def generate(build, output):
    build, output = Path(build).resolve(), Path(output).resolve()
    description, settings, segments, app_offset, app = inputs(build)
    stem = 'PageRest-Read_Pico-' + description['project_version']
    full_name, incremental_name = stem + '-full.bin', stem + '-incremental.bin'
    output.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix='.firmware-', dir=output) as directory:
        stage = Path(directory)
        command = [sys.executable, '-m', 'esptool', '--chip', description['target'],
                   'merge-bin', '-o', str(stage / full_name), '--target-offset', '0x0',
                   '--flash-mode', settings['flash_mode'], '--flash-freq', settings['flash_freq'],
                   '--flash-size', settings['flash_size']]
        for offset, path, _ in segments:
            command.extend([hex(offset), str(path)])
        subprocess.run(command, check=True)
        full = (stage / full_name).read_bytes()
        # 完整镜像只填充段间空洞，止于最后一段；不扩到整片 flash 或伪造用户存储。
        # Pad only inter-segment gaps and stop at the final segment; never fabricate a whole-flash user-storage dump.
        end = 0
        for offset, _, data in segments:
            if full[end:offset] != b'\xff' * (offset - end) or full[offset:offset + len(data)] != data:
                raise ValueError('Merged image segment/padding verification failed')
            end = offset + len(data)
        if len(full) != end:
            raise ValueError('Unexpected merged image length')
        (stage / incremental_name).write_bytes(app)
        manifest = {
            'firmware': 'PageRest', 'version': description['project_version'],
            'chip': description['target'], 'project': description['project_name'],
            'config_defaults': description.get('config_defaults', ''), 'flash_settings': settings,
            'incremental_is_app_only': True,
            'images': {
                'full': {'file': full_name, 'offset': '0x0', 'bytes': len(full), 'sha256': sha256(full)},
                'incremental': {'file': incremental_name, 'offset': hex(app_offset), 'bytes': len(app), 'sha256': sha256(app)},
            },
            'segments': [{'offset': hex(o), 'file': p.name, 'bytes': len(d), 'sha256': sha256(d)}
                         for o, p, d in segments],
        }
        (stage / 'manifest.json').write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + '\n')
        (stage / 'SHA256SUMS').write_text(''.join(sha256((stage / n).read_bytes()) + '  ' + n + '\n'
                                               for n in [full_name, incremental_name, 'manifest.json']))
        (stage / 'README.txt').write_text(
            'PageRest ' + description['project_version'] + ' · Read Pico\n\n'
            'full: ' + full_name + ' -> 0x0\n'
            'incremental: ' + incremental_name + ' -> ' + hex(app_offset) + '\n\n'
            '完整固件包含启动程序、分区表与应用；段间空洞含NVS，写入会重置设置。\n'
            'Full contains bootloader, partition table and app; padded gaps include NVS, so writing it resets settings.\n'
            '增量为仅应用升级镜像，不是二进制差分补丁；匹配已有分区时关闭全片擦除。\n'
            'Incremental is app-only, not a binary delta patch; disable full-chip erase when upgrading a matching partition layout.\n'
            '完整镜像不包含个人书库或出厂PMU标定值。\n'
            'Full does not contain personal books or factory PMU calibration.\n')
        # 所有校验完成后发布一对镜像，最后发布清单作为本轮构建标识。
        # Publish the pair only after validation, with the manifest last as the build marker.
        for name in [full_name, incremental_name, 'README.txt', 'SHA256SUMS', 'manifest.json']:
            os.replace(stage / name, output / name)
    return manifest


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build-dir', type=Path, default=Path('build'))
    parser.add_argument('--output-dir', type=Path)
    args = parser.parse_args()
    try:
        result = generate(args.build_dir, args.output_dir or args.build_dir / 'firmware')
    except (ValueError, KeyError, OSError, subprocess.CalledProcessError) as error:
        parser.exit(1, 'Firmware images failed: ' + str(error) + '\n')
    for image in result['images'].values():
        print(image['file'] + ' @ ' + image['offset'] + ' SHA256 ' + image['sha256'])


if __name__ == '__main__':
    main()
