set(read_pico_requires epdiy e0470_epaper_waveform sc7a20h cst836u read_pico_pmu fatfs sy7636a fca9555
                   esp_driver_gpio esp_driver_i2c esp_driver_ledc esp_driver_rmt esp_driver_sdmmc pwm_audio)

idf_component_register(
    SRCS "read_pico_init.c" "read_pico_board.c" "read_pico_epd_timing.c" "read_pico_buzzer.c" "buzzer_score.c" "buzzer_duty_map.c" "buzzer_1bit.c" "read_pico_sd.c" "read_pico_flash_hpm.c" "read_pico_rtc_cal.c"
    INCLUDE_DIRS "include"
    REQUIRES ${read_pico_requires}
    PRIV_REQUIRES bootloader_support spi_flash freertos esp_hw_support
    # flash_hpm_zb.c 只定义了一个覆盖 IDF weak 符号的表，没有任何
    # 代码引用它，不整包链接的话这个目标文件会被丢掉，覆盖就不生效。
    WHOLE_ARCHIVE
)

# 浅睡短采样沿用本板校准精度，不修改IDF安装。/ Retain board precision for short sleep calibration without modifying the IDF installation.
target_link_options(${COMPONENT_LIB} INTERFACE "-Wl,--wrap=rtc_clk_cal")
