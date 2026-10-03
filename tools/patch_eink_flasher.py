#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# 修正用户指定网页的复位序列，不下载或部署网站。/ Patch the supplied page's reset sequence without downloading/deploying.
import argparse
from pathlib import Path


def patch(source: str) -> str:
    needle = '          await esploader.after("hard_reset");'
    if source.count(needle) != 1:
        raise ValueError("Unrecognized flasher page: expected exactly one reset call")
    reset = Path(__file__).with_name("eink_flasher_reset.js").read_text()
    source = source.replace(needle, '          resetSent = await picoRestartTransport(esploader, transport);')
    source = source.replace('// 重启设备并释放串口\n        try {', '// 重启设备并释放串口 / Reset and release the port.\n        let resetSent = false;\n        try {')
    source = source.replace('          console.log(err);\n        }\n        await transport.disconnect();', '          terminal.writeLine(`复位未确认: ${err.message || err}`);\n        }\n        try { await transport.disconnect(); } catch (err) { console.log(err); }')
    source = source.replace('alert("🎉 固件烧录完成！设备已重启。");', 'alert(resetSent ? "固件写入成功，已发送重启指令。请在设备开机图底部核对固件版本。" : "固件写入成功，软复位未确认但已发送复位脉冲；屏幕无变化时请长按电源键重启，无需重新烧录。");')
    source = source.replace('正在软重启墨水屏设备', '正在退出下载模式并重启设备')
    marker = '  <script>'
    if marker not in source:
        raise ValueError("Missing script boundary")
    return source.replace(marker, marker + '\n' + reset, 1)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('input', type=Path)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(patch(args.input.read_text()))
