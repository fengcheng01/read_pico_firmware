#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""生产历史核心与服务回归。/ Production history core and service regression."""
import os, subprocess, tempfile
from pathlib import Path
root=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix='pico-history-',dir='/tmp') as d:
    binary=str(Path(d)/'history')
    flags=['-std=gnu11','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-g']
    if os.uname().sysname=='Darwin':
        flags+=['-isysroot',subprocess.check_output(['xcrun','--show-sdk-path'],text=True).strip()]
    subprocess.run([os.environ.get('CC','cc'),*flags,'-Itools/history_stubs','-Imain/os','-Imain/book',
        'tools/history_host_test.c','main/os/os_battery_history.c','main/os/os_history_service.c','-o',binary],cwd=root,check=True)
    subprocess.run([binary,d],check=True)
    binary=str(Path(d)/'faults')
    subprocess.run([os.environ.get('CC','cc'),*flags,'-Imain/os','-Imain/book',
        'tools/history_io_fault_test.c','-o',binary],cwd=root,check=True)
    subprocess.run([binary,d],check=True)
    binary=str(Path(d)/'bridge')
    subprocess.run([os.environ.get('CC','cc'),*flags,'-Itools/history_stubs','-Imain/os',
        'tools/history_bridge_host_test.c','main/os/os_history_pico.c','-o',binary],cwd=root,check=True)
    subprocess.run([binary],check=True)
