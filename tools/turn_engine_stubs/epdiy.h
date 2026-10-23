/* SPDX-License-Identifier: Apache-2.0
 * 中文：真实错相引擎测试的硬件边界。/ English: Hardware boundary for actual stagger-engine tests.
 */
#pragma once
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "epd_waveform.h"
typedef struct { int x,y,width,height; } EpdRect;
typedef enum { EPD_ROT_LANDSCAPE, EPD_ROT_PORTRAIT, EPD_ROT_INVERTED_LANDSCAPE, EPD_ROT_INVERTED_PORTRAIT } EpdRotation;
enum EpdDrawMode { MODE_DU=1, MODE_GL16=5, MODE_PACKING_1PPB_DIFFERENCE=0x100 };
enum EpdDrawError { EPD_DRAW_SUCCESS, EPD_DRAW_OTHER_ERROR, EPD_DRAW_NO_PHASES_AVAILABLE, EPD_DRAW_INVALID_CROP };
int epd_width(void);
int epd_height(void);
EpdRotation epd_get_rotation(void);
EpdRect epd_full_screen(void);
void epd_poweron(void);
void epd_build_1ppB_lut_1k(uint8_t*,const EpdWaveformPhases*,int);
void epd_set_line_phase_luts(const uint8_t*const*,const int8_t*);
void epd_set_col_phase_luts(const uint8_t*const*,const int*,const int*,const int8_t*,int);
void epd_clear_phase_luts(void);
void epd_difference_image_cropped(const uint8_t*,const uint8_t*,EpdRect,uint8_t*,bool*,uint8_t*);
enum EpdDrawError epd_draw_base(EpdRect,const uint8_t*,EpdRect,enum EpdDrawMode,int,const bool*,const uint8_t*,const EpdWaveform*);
