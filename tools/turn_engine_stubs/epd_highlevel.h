/* SPDX-License-Identifier: Apache-2.0
 * 中文：完整差分与脏行边界。/ English: Full difference and dirty-line boundary.
 */
#pragma once
#include "epdiy.h"
typedef struct { uint8_t *front_fb,*back_fb,*difference_fb;bool*dirty_lines;uint8_t*dirty_columns; } EpdiyHighlevelState;
