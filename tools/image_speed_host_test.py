#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""验证真实解码让步策略不累积固定休眠。/ Verify actual decode yields do not accumulate fixed sleeps."""
from pathlib import Path
import subprocess
import tempfile
ROOT=Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory() as temp:
    folder=Path(temp)
    for name in ['esp_timer.h','freertos/FreeRTOS.h','freertos/task.h']:
        p=folder/name;p.parent.mkdir(parents=True,exist_ok=True)
        p.write_text('#pragma once\n#include <stdint.h>\nint64_t esp_timer_get_time(void);\nvoid vTaskDelay(unsigned ticks);\n')
    source=folder/'test.c'
    source.write_text('''#include <assert.h>
#include <stdio.h>
#define ESP_PLATFORM
#include "book_image_yield.h"
static int64_t clock_us=1;static unsigned sleeps;
int64_t esp_timer_get_time(void){return clock_us;}
void vTaskDelay(unsigned ticks){assert(ticks==1);++sleeps;clock_us+=10000;}
int main(void){
 int64_t last=0;
 for(int i=0;i<10000;++i){++clock_us;book_image_yield(&last);}
 assert(sleeps==0);
 for(int i=0;i<10;++i){clock_us+=40000;book_image_yield(&last);assert(sleeps==(unsigned)i+1);}
 for(int i=0;i<10000;++i)book_image_yield(&last);
 assert(sleeps==10);
 puts("Actual decode yield: 10000 fast callbacks add no sleep; elapsed budget still yields; no immediate repeat sleeps PASS (simulated clock)");
}''')
    subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',f'-I{folder}','-Imain/book',str(source),'-o',str(folder/'test')],cwd=ROOT,check=True)
    subprocess.run([str(folder/'test')],check=True)
