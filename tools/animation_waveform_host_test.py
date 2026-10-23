#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""真实本地49相对照测试，不代表面板观感。/ Real local49-phase comparison, not panel optical acceptance."""
import os,subprocess,tempfile
from pathlib import Path
root=Path(__file__).resolve().parents[1]
flags=['-std=c11','-Wall','-Wextra','-Werror','-g','-fsanitize=address,undefined']
if os.uname().sysname=='Darwin':flags+=['-isysroot',subprocess.check_output(['xcrun','--show-sdk-path'],text=True).strip()]
with tempfile.TemporaryDirectory(prefix='pico-animation-',dir='/tmp') as d:
    binary=str(Path(d)/'test')
    subprocess.run([os.environ.get('CC','cc'),*flags,'-Imain','-Itools/animation_stubs',
        '-Icomponents/e0470_epaper_waveform/include','-Icomponents/e0470_epaper_waveform/waveforms',
        '-Itools/waveform_host_stubs','-Icomponents/epdiy/include','-Icomponents/epdiy/src',
        'tools/animation_waveform_host_test.c','main/display_animation.c',
        'components/e0470_epaper_waveform/e0470_epaper_waveform.c','-o',binary],cwd=root,check=True)
    subprocess.run([binary],check=True)
