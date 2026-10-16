#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""检查提交树的组件源文件和本地头文件依赖。/ Check component sources and local headers in a submitted tree."""
import argparse
from pathlib import Path
import re
import subprocess

PREFIXES = ('e0470_', 'book_', 'ui_', 'os_', 'read_pico_')

def audit(files):
    names = {Path(name).name for name in files}
    errors = []
    for name, content in files.items():
        if name.endswith(('.c', '.h', '.cpp', '.hpp')):
            # 保留行号，忽略注释和字符串中的伪指令。/ Preserve line numbers and ignore directives inside comments or strings.
            source = re.sub(r'/\*.*?\*/|//[^\n]*|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'',
                            lambda match: '\n' * match.group().count('\n'), content, flags=re.S)
            conditionals = []
            for number, line in enumerate(source.splitlines(), 1):
                directive = re.match(r'^\s*#\s*(if|ifdef|ifndef|elif|else|endif)\b', line)
                if not directive:
                    continue
                kind = directive.group(1)
                if kind in ('if', 'ifdef', 'ifndef'):
                    conditionals.append(number)
                elif not conditionals:
                    errors.append(f'{name}:{number}: unmatched #{kind}')
                elif kind == 'endif':
                    conditionals.pop()
            for number in conditionals:
                errors.append(f'{name}:{number}: unterminated conditional')
            for header in re.findall(r'^\s*#\s*include\s*"([^"\n]+)"', content, re.M):
                if header.startswith(PREFIXES) and '/' not in header and header not in names:
                    errors.append(f'{name}: missing local header {header}')
        if name.startswith('components/') and name.endswith('/CMakeLists.txt'):
            source = re.sub(r'#[^\n]*', '', content)
            for block in re.findall(r'\bSRCS\b(.*?)(?=\b(?:INCLUDE_DIRS|PRIV_INCLUDE_DIRS|REQUIRES|PRIV_REQUIRES|EMBED_FILES|EMBED_TXTFILES)\b|\))', source, re.S):
                for src in re.findall(r'"([^"\n]+\.(?:c|cpp|S))"', block):
                    if '$' not in src and (Path(name).parent / src).as_posix() not in files:
                        errors.append(f'{name}: missing component source {src}')
    return errors

def self_test():
    files = {'components/example/CMakeLists.txt':'idf_component_register(SRCS "unit.c" REQUIRES epdiy)',
             'components/example/unit.c':'#include "e0470_missing.h"\n#include "esp_timer.h"'}
    assert audit(files) == ['components/example/unit.c: missing local header e0470_missing.h']
    files['components/example/include/e0470_missing.h'] = ''
    assert not audit(files)
    del files['components/example/unit.c']
    assert audit(files) == ['components/example/CMakeLists.txt: missing component source unit.c']
    assert audit({'main/test.c':'#if FLAG\nint value;\n'}) == ['main/test.c:1: unterminated conditional']
    assert audit({'main/test.c':'#endif\n'}) == ['main/test.c:1: unmatched #endif']
    assert not audit({'main/test.c':'/*\n#if COMMENT\n*/\n#if FLAG\n#else\n#endif\n'})
    print('Build source guard: missing header/source and unbalanced conditionals rejected; complete tree accepted')

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--index',action='store_true')
    parser.add_argument('--self-test',action='store_true')
    args=parser.parse_args()
    if args.self_test:
        self_test()
        return
    root=Path(__file__).resolve().parents[1]
    # 容器与检出任务可能使用不同UID，仅信任本次读取的仓库路径，不修改全局配置。
    # Container and checkout UIDs may differ; trust only this repository for each read without changing global config.
    git=['git','-c','safe.directory='+str(root)]
    command=git+['ls-files','-z'] if args.index else git+['ls-tree','-r','-z','--name-only','HEAD']
    names=subprocess.check_output(command,cwd=root).decode().split('\0')
    files={}
    for name in names:
        if not name or not name.startswith(('main/','components/')):continue
        revision=':'+name if args.index else 'HEAD:'+name
        data=subprocess.check_output(git+['show',revision],cwd=root)
        files[name]=data.decode('utf8',errors='replace')
    errors=audit(files)
    if errors:raise SystemExit('\n'.join(errors))
    print(f'Build source guard passed for {len(files)} tracked entries')

if __name__=='__main__':main()
