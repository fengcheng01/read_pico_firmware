#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# 真实板级函数仅替换硬件边界，检查扫描模式不会破坏高压上下电顺序。
# Test actual board functions with hardware boundaries replaced; scan-mode changes must not disturb high-voltage sequencing.
from pathlib import Path
import os
import re
import subprocess

ROOT = Path(__file__).resolve().parents[1]
source = (ROOT / 'components/read_pico/read_pico_board.c').read_text()
def function(name):
    match = re.search(r'^(?:static )?[^\n]+\b' + name + r'\([^;]*?\) \{', source, re.M)
    assert match, name
    start, body = match.start(), match.end() - 1
    depth = 1
    end = body + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]

out = ROOT / 'build/book-tests/power'
out.mkdir(parents=True, exist_ok=True)
test = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#define IOE_MODE 1
#define IOE_XOE 2
#define IOE_CW_INT 4
#define IOE_CONFIG_PORT0 0x64
#define IOE_CONFIG_PORT0_HOST_OFF 0x60
#define IOE_CONFIG_PORT1 0xFF
#define IOE_CONFIG_PORT1_SLEEP 0
#define ESP_OK 0
#define ESP_ERR_INVALID_STATE 2
#define ESP_LOGE(...) ((void)0)
#define ESP_LOGI(...) ((void)0)
#define EPD_XSTL 42
#define EPD_XCL 21
#define EPD_CKV 48
#define EPD_XLE 3
#define EPD_SPV 47
#define SD_CLK_GPIO 38
#define D0 4
#define D1 5
#define D2 6
#define D3 7
#define D4 8
#define D5 9
#define D6 10
#define D7 11
#define D8 12
#define D9 13
#define D10 14
#define D11 15
#define D12 16
#define D13 17
#define D14 18
#define D15 45
#define GPIO_MODE_INPUT 1
#define GPIO_PULLDOWN_ONLY 2
typedef int gpio_num_t;
#define BOARD_LCD_DE_SIG 13
#define pdMS_TO_TICKS(x) (x)
typedef int esp_err_t;
typedef struct { bool ep_mode, ep_output_enable; } epd_ctrl_state_t;
typedef struct { int vcom_mv, temperature_c; } sy7636a_status_t;
static uint8_t ioe_output = 0x80;
static bool rails_on, hv;
static void* s_sy;
static int ioe_device;
static void* s_ioe=&ioe_device;
static bool s_ioe_sleeping;
static uint8_t s_ioe_resume_output;
static int cfg0=0x64,cfg1=0xFF,out1;
static int ioe_calls,fail_ioe_at;
static uint64_t sleep_pins;
static unsigned sleep_directions,sleep_pulls;
static int events[128], count, writes, fail_write, fail_sy;
static void record(int event) { events[count++] = event; }
static int ioe_commit(void) {
    record(0x100 | ioe_output);
    assert(s_ioe_sleeping || (ioe_output & IOE_MODE));
    ++writes;
    return writes == fail_write ? -1 : ESP_OK;
}
static void esp_rom_gpio_connect_out_signal(int gpio, int sig, bool a, bool b) {
    assert(gpio == EPD_XSTL && sig == BOARD_LCD_DE_SIG && !a && !b);
}
static int sy7636a_power_on(void* p) {
    (void)p; assert((ioe_output & 3) == 1); record(1);
    hv = !fail_sy; return fail_sy ? -1 : 0;
}
static int sy7636a_power_off(void* p) {
    (void)p; assert(!(ioe_output & IOE_XOE)); record(3); hv = false; return 0;
}
static int sy7636a_read(void* p, sy7636a_status_t* status) { (void)p; (void)status; return -1; }
static void vTaskDelay(int ticks) { assert(ticks == 1); record(2); }
static int ioe_result(void){return ++ioe_calls==fail_ioe_at?-1:0;}
static int fca9555_set_config(void* dev,int port,uint8_t value){
 assert(dev==s_ioe);int err=ioe_result();if(!err){if(port==0)cfg0=value;else cfg1=value;}return err;
}
static int fca9555_set_output(void* dev,int port,uint8_t value){assert(dev==s_ioe&&port==1);int err=ioe_result();if(!err)out1=value;return err;}
static int fca9555_clear_int(void* dev){assert(dev==s_ioe);return ioe_result();}
static void gpio_sleep_set_direction(int pin,int mode){assert(pin!=39&&pin!=40&&pin!=41&&mode==GPIO_MODE_INPUT);sleep_directions++;}
static void gpio_sleep_set_pull_mode(int pin,int mode){assert(pin!=39&&pin!=40&&pin!=41&&mode==GPIO_PULLDOWN_ONLY);sleep_pulls++;}
static void gpio_sleep_sel_en(int pin){assert(!(sleep_pins&(1ULL<<pin)));sleep_pins|=1ULL<<pin;}
'''
test += '\n'.join(function(name) for name in (
    'read_pico_ioe_wake', 'read_pico_ioe_sleep', 'sleep_pulldown_pins',
    'board_apply_power_ctrl', 'board_set_ctrl', 'board_poweron', 'board_poweroff'))
test += r'''
static void reset(void) { count = writes = fail_write = fail_sy = ioe_calls = fail_ioe_at = 0; rails_on = hv = s_ioe_sleeping = false; ioe_output = 0x80;cfg0=0x64;cfg1=0xFF;out1=0; }
int main(void) {
    sleep_pulldown_pins();assert(sleep_directions==22&&sleep_pulls==22&&(sleep_pins&(1ULL<<38)));
    epd_ctrl_state_t state = {0}, mask = {.ep_mode = true, .ep_output_enable = true};
    reset(); board_poweron(&state);
    assert(rails_on && hv && count == 3);
    assert(events[0] == 0x181 && events[1] == 1 && events[2] == 0x183);
    for (int i = 0; i < 20; ++i) {
        state.ep_mode = state.ep_output_enable = (i & 1);
        board_set_ctrl(&state, &mask);
        assert(count == 3 && ioe_output == 0x83 && rails_on && hv);
    }
    board_poweron(&state); assert(count == 3);
    board_poweroff(&state);
    assert(count == 6 && events[3] == 0x181 && events[4] == 2 && events[5] == 3);
    assert(!rails_on && !hv && ioe_output == 0x81);
    board_poweroff(&state); assert(count == 6);
    reset(); fail_write = 1; board_poweron(&state);
    assert(!rails_on && !hv && count == 1);
    reset(); fail_sy = 1; board_poweron(&state);
    assert(!rails_on && !hv && count == 2 && !(ioe_output & IOE_XOE));
    reset(); fail_write = 2; board_poweron(&state);
    assert(!rails_on && !hv && count == 5 && !(ioe_output & IOE_XOE));
    reset();rails_on=true;assert(read_pico_ioe_sleep(false)==ESP_ERR_INVALID_STATE&&!ioe_calls);
    reset();ioe_output=0x81;assert(read_pico_ioe_sleep(false)==ESP_OK);
    assert(ioe_output==0x80&&cfg0==0x64&&cfg1==0&&out1==0xFF&&s_ioe_sleeping);
    assert(read_pico_ioe_wake()==ESP_OK&&ioe_output==0x81&&cfg0==0x64&&cfg1==0xFF&&!s_ioe_sleeping);
    reset();ioe_output=0x81;assert(read_pico_ioe_sleep(true)==ESP_OK&&cfg0==0x60);
    assert(read_pico_ioe_wake()==ESP_OK&&cfg0==0x64&&ioe_output==0x81);
    for(int failure=1;failure<=4;failure++){
      reset();ioe_output=0x81;fail_ioe_at=failure;
      assert(read_pico_ioe_sleep(true)!=ESP_OK);
      assert(ioe_output==0x81&&cfg0==0x64&&cfg1==0xFF&&!s_ioe_sleeping);
    }
    reset();ioe_output=0x81;fail_write=1;assert(read_pico_ioe_sleep(true)!=ESP_OK);
    assert(ioe_output==0x81&&cfg0==0x64&&cfg1==0xFF&&!s_ioe_sleeping);
    reset();ioe_output=0x81;assert(!read_pico_ioe_sleep(true));fail_ioe_at=ioe_calls+1;
    board_poweron(&state);assert(!rails_on&&!hv&&s_ioe_sleeping);
    board_poweron(&state);assert(rails_on&&hv&&!s_ioe_sleeping&&cfg0==0x64);
    puts("board power: scan callbacks keep MODE/XOE stable; off order and failed enable rollback passed");
    puts("board sleep: rails-off guard, light/key vs host-off directions, every I2C rollback and no scan before wake recovery PASS");
}
'''
(out / 'test.c').write_text(test)
subprocess.run([os.environ.get('CC', 'cc'), '-std=c11', '-Wall', '-Wextra', '-Werror',
                '-fsanitize=address,undefined', '-g', str(out / 'test.c'), '-o', str(out / 'test')], check=True)
subprocess.run([str(out / 'test')], check=True)
