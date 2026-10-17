#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""中文：真实构建镜像的配对、地址和失败不发布回归；在 IDF 环境运行。
English: Pair/address/failure-publication regression using real build images; run in the IDF environment.
"""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import struct
import subprocess
import tempfile
import unittest
from unittest.mock import patch

import firmware_images

BUILD = Path('build')


class FirmwareImagesTests(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory(prefix='firmware-pair-')
        self.addCleanup(self.directory.cleanup)
        self.build = Path(self.directory.name) / 'build space'
        self.output = Path(self.directory.name) / 'output space'
        self.build.mkdir()
        for name in ('flasher_args.json', 'project_description.json'):
            shutil.copy2(BUILD / name, self.build / name)
        self.config = json.loads((BUILD / 'flasher_args.json').read_text())
        for name in self.config['flash_files'].values():
            self.assertFalse(Path(name).is_absolute())
            target = self.build / name
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(BUILD / name, target)

    def write_config(self):
        (self.build / 'flasher_args.json').write_text(json.dumps(self.config))

    def test_real_pair_segments_padding_hashes_and_repeated_build(self):
        manifest = firmware_images.generate(self.build, self.output)
        full = (self.output / manifest['images']['full']['file']).read_bytes()
        app = (self.build / self.config['app']['file']).read_bytes()
        offset = int(self.config['app']['offset'], 0)
        self.assertEqual(manifest['images']['full']['offset'], '0x0')
        self.assertEqual(manifest['images']['incremental']['offset'], hex(offset))
        self.assertEqual((self.output / manifest['images']['incremental']['file']).read_bytes(), app)
        self.assertEqual(full[offset:offset + len(app)], app)
        end = 0
        for address, name in sorted(self.config['flash_files'].items(), key=lambda row: int(row[0], 0)):
            start = int(address, 0)
            data = (self.build / name).read_bytes()
            self.assertEqual(full[end:start], b'\xff' * (start - end))
            self.assertEqual(full[start:start + len(data)], data)
            end = start + len(data)
        self.assertEqual(len(full), end)
        for line in (self.output / 'SHA256SUMS').read_text().splitlines():
            digest, name = line.split('  ', 1)
            self.assertEqual(hashlib.sha256((self.output / name).read_bytes()).hexdigest(), digest)
        self.assertEqual(firmware_images.generate(self.build, self.output), manifest)
        self.assertEqual(len(list(self.output.iterdir())), 5)

    def test_reject_stale_version(self):
        path = self.build / 'project_description.json'
        data = json.loads(path.read_text())
        data['project_version'] += '-wrong'
        path.write_text(json.dumps(data))
        with self.assertRaisesRegex(ValueError, 'version'):
            firmware_images.generate(self.build, self.output)
        self.assertFalse(self.output.exists())

    def test_reject_overlapping_segments(self):
        name = self.config['app']['file']
        del self.config['flash_files'][self.config['app']['offset']]
        self.config['flash_files']['0x7000'] = name
        self.config['app']['offset'] = '0x7000'
        self.write_config()
        with self.assertRaisesRegex(ValueError, 'Overlapping'):
            firmware_images.generate(self.build, self.output)

    def test_reject_missing_bootloader(self):
        (self.build / self.config['bootloader']['file']).unlink()
        with self.assertRaises(FileNotFoundError):
            firmware_images.generate(self.build, self.output)

    def test_reject_app_larger_than_partition(self):
        path = self.build / self.config['partition-table']['file']
        data = bytearray(path.read_bytes())
        for at in range(0, len(data) - 31, 32):
            magic, kind, _, offset, _, _, _ = struct.unpack_from('<HBBII16sI', data, at)
            if magic == 0x50AA and kind == 0 and offset == int(self.config['app']['offset'], 0):
                struct.pack_into('<I', data, at + 8, 4096)
        path.write_bytes(data)
        with self.assertRaisesRegex(ValueError, 'fit'):
            firmware_images.generate(self.build, self.output)

    def test_reject_wrong_chip_encryption_and_flash_size(self):
        original = json.dumps(self.config)
        for field, value in [('chip', 'esp32'), ('encrypted', 'true'), ('size', '1MB')]:
            self.config = json.loads(original)
            if field == 'chip':
                self.config['extra_esptool_args']['chip'] = value
            elif field == 'encrypted':
                self.config['app']['encrypted'] = value
            else:
                self.config['flash_settings']['flash_size'] = value
            self.write_config()
            with self.assertRaises(ValueError):
                firmware_images.generate(self.build, self.output)
            self.assertFalse(self.output.exists())

    def test_failed_merge_preserves_previous_pair(self):
        firmware_images.generate(self.build, self.output)
        previous = {p.name: p.read_bytes() for p in self.output.iterdir()}
        with patch.object(firmware_images.subprocess, 'run', side_effect=subprocess.CalledProcessError(1, 'esptool')):
            with self.assertRaises(subprocess.CalledProcessError):
                firmware_images.generate(self.build, self.output)
        self.assertEqual({p.name: p.read_bytes() for p in self.output.iterdir()}, previous)

    def test_bad_padding_is_not_published(self):
        firmware_images.generate(self.build, self.output)
        previous = {p.name: p.read_bytes() for p in self.output.iterdir()}
        full_name = next(n for n in previous if n.endswith('-full.bin'))
        data = bytearray(previous[full_name])
        # 该产品的设置区位于合并段间空洞；损坏一个填充字节即拒绝发布。
        # Product settings occupy a merged gap; refuse publication after corrupting one padding byte.
        data[0x9000] = 0
        def corrupt(command, **kwargs):
            Path(command[command.index('-o') + 1]).write_bytes(data)
        with patch.object(firmware_images.subprocess, 'run', side_effect=corrupt):
            with self.assertRaisesRegex(ValueError, 'padding'):
                firmware_images.generate(self.build, self.output)
        self.assertEqual({p.name: p.read_bytes() for p in self.output.iterdir()}, previous)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build-dir', type=Path, default=BUILD)
    args = parser.parse_args()
    BUILD = args.build_dir.resolve()
    unittest.main(argv=['firmware-images'], verbosity=2)
