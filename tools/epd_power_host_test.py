#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
# 真实板级函数仅替换硬件边界，检查扫描模式不会破坏高压上下电顺序。
# Test actual board functions with hardware boundaries replaced; scan-mode changes must not disturb high-voltage sequencing.
from pathlib import Path
import os
import subprocess

ROOT = Path(__file__).resolve().parents[1]
source = (ROOT / 'components/read_pico/read_pico_board.c').read_text()
def function(name):
    start = source.index(name + '(')
    start = source.rfind('\n', 0, start) + 1
    body = source.index('{', start)
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
#define ESP_OK 0
#define ESP_LOGE(...) ((void)0)
#define ESP_LOGI(...) ((void)0)
#define EPD_XSTL 42
#define BOARD_LCD_DE_SIG 13
#define pdMS_TO_TICKS(x) (x)
typedef int esp_err_t;
typedef struct { bool ep_mode, ep_output_enable; } epd_ctrl_state_t;
typedef struct { int vcom_mv, temperature_c; } sy7636a_status_t;
static uint8_t ioe_output = 0x80;
static bool rails_on, hv;
static void* s_sy;
static int events[64], count, writes, fail_write, fail_sy;
static void record(int event) { events[count++] = event; }
static int ioe_commit(void) {
    record(0x100 | ioe_output);
    assert(ioe_output & IOE_MODE);
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
'''
test += '\n'.join(function(name) for name in (
    'board_apply_power_ctrl', 'board_set_ctrl', 'board_poweron', 'board_poweroff'))
test += r'''
static void reset(void) { count = writes = fail_write = fail_sy = 0; rails_on = hv = false; ioe_output = 0x80; }
int main(void) {
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
    puts("board power: scan callbacks keep MODE/XOE stable; off order and failed enable rollback passed");
}
'''
(out / 'test.c').write_text(test)
subprocess.run([os.environ.get('CC', 'cc'), '-std=c11', '-Wall', '-Wextra', '-Werror',
                '-fsanitize=address,undefined', '-g', str(out / 'test.c'), '-o', str(out / 'test')], check=True)
subprocess.run([str(out / 'test')], check=True)
