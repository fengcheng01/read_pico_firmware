/*
 * SPDX-FileCopyrightText: 2026 mindreset
 * SPDX-License-Identifier: Apache-2.0
 *
 * 中文：IDF6.1每次睡前的10周期慢钟采样使用本板配置精度，避免覆盖开机长采样。
 * English: Give IDF6.1's ten-cycle pre-sleep slow-clock sample the board's configured precision.
 *
 * 冻结：仅提升当前慢钟的10周期采样；不改时钟源，不补偿猜测的走时偏差。
 * Frozen: Only extend ten-cycle samples of the current slow clock; never change sources or invent drift offsets.
 */
#include "esp_attr.h"
#include "sdkconfig.h"
#include "soc/rtc.h"

extern uint32_t __real_rtc_clk_cal(soc_clk_freq_calculation_src_t source, uint32_t cycles);

uint32_t IRAM_ATTR __wrap_rtc_clk_cal(soc_clk_freq_calculation_src_t source, uint32_t cycles) {
    if (source == CLK_CAL_RTC_SLOW && cycles == 10 && CONFIG_RTC_CLK_CAL_CYCLES > 10)
        cycles = CONFIG_RTC_CLK_CAL_CYCLES;
    return __real_rtc_clk_cal(source, cycles);
}
