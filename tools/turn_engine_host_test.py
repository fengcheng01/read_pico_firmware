#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""真实错相引擎和28相波形回归，仅替换硬件边界。/ Real stagger engine and28-phase regression with hardware boundaries only."""
from pathlib import Path
import subprocess
import tempfile

ROOT=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix='pico-turn-engine-') as temp:
    binary=Path(temp)/'test'
    includes=['tools/turn_engine_stubs','components/e0470_page_turn/include',
              'components/e0470_epaper_waveform/include','components/e0470_epaper_waveform/waveforms',
              'components/epdiy/include','components/epdiy/src']
    subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-g',
                    '-fsanitize=address,undefined','-fno-omit-frame-pointer',
                    *['-I'+str(ROOT/p) for p in includes],str(ROOT/'tools/turn_engine_host_test.c'),
                    str(ROOT/'components/e0470_page_turn/e0470_page_turn.c'),
                    str(ROOT/'components/e0470_epaper_waveform/e0470_epaper_waveform.c'),
                    '-o',str(binary)],check=True,cwd=ROOT)
    subprocess.run([str(binary)],check=True,cwd=ROOT)
