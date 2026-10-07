#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""编译真实慢钟包装器，验证仅提升睡前短采样。/ Compile the actual slow-clock wrapper and verify only short sleep samples are extended."""
from pathlib import Path
import os
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix='pico-rtc-cal-', dir='/tmp') as folder:
    d = Path(folder)
    (d / 'soc').mkdir()
    (d / 'esp_attr.h').write_text('#define IRAM_ATTR\n')
    (d / 'sdkconfig.h').write_text('#define CONFIG_RTC_CLK_CAL_CYCLES 3000\n')
    (d / 'soc/rtc.h').write_text('#include <stdint.h>\ntypedef enum {CLK_CAL_RTC_SLOW, CLK_CAL_32K_XTAL, CLK_CAL_RC_FAST} soc_clk_freq_calculation_src_t;\n')
    (d / 'test.c').write_text(r'''
#include "soc/rtc.h"
#include <assert.h>
#include <stdio.h>
static uint32_t seen_cycles;
static soc_clk_freq_calculation_src_t seen_source;
uint32_t __real_rtc_clk_cal(soc_clk_freq_calculation_src_t source, uint32_t cycles) {
    seen_cycles=cycles; seen_source=source; return cycles+123;
}
extern uint32_t __wrap_rtc_clk_cal(soc_clk_freq_calculation_src_t, uint32_t);
int main(void) {
    for (int minute=0; minute<13*60; ++minute) {
        assert(__wrap_rtc_clk_cal(CLK_CAL_RTC_SLOW,10)==3123);
        assert(seen_cycles==3000 && seen_source==CLK_CAL_RTC_SLOW);
    }
    const uint32_t sizes[]={1,9,11,100,3000,8192};
    for (unsigned i=0;i<sizeof(sizes)/sizeof(sizes[0]);++i) {
        assert(__wrap_rtc_clk_cal(CLK_CAL_RTC_SLOW,sizes[i])==sizes[i]+123);
        assert(seen_cycles==sizes[i]);
    }
    assert(__wrap_rtc_clk_cal(CLK_CAL_32K_XTAL,10)==133 && seen_cycles==10);
    assert(__wrap_rtc_clk_cal(CLK_CAL_RC_FAST,10)==133 && seen_cycles==10);
    puts("RTC calibration: 780 sleep samples use 3000 cycles; boot, other sample sizes and clock sources unchanged PASS");
}
''')
    flags = ['-std=c11', '-Wall', '-Wextra', '-Werror', '-fsanitize=address,undefined']
    if os.uname().sysname == 'Darwin':
        flags += ['-isysroot', '/Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs/MacOSX.sdk']
    subprocess.run([os.environ.get('CC','cc'), *flags, '-I'+str(d), str(d/'test.c'),
                    str(ROOT/'components/read_pico/read_pico_rtc_cal.c'), '-o', str(d/'test')], check=True)
    subprocess.run([str(d/'test')], check=True)
