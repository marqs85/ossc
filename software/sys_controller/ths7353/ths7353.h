//
// Copyright (C) 2015-2016  Markus Hiienkari <mhiienka@niksula.hut.fi>
//
// This file is part of Open Source Scan Converter project.
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//

#ifndef THS7353_H_
#define THS7353_H_

#include "sysconfig.h"

#define THS_BASE (0x58>>1)

#define THS_CH1 0x01
#define THS_CH2 0x02
#define THS_CH3 0x03

typedef enum {
    THS_INPUT_A = 0,
    THS_INPUT_B = 1,
    THS_STANDBY = 2
} ths_input_t;

#define THS_LPF_9MHZ 0x00
#define THS_LPF_16MHZ 0x01
#define THS_LPF_35MHZ 0x02
#define THS_LPF_BYPASS 0x03
#define THS_LPF_DEFAULT 0x3
#define THS_LPF_MASK 0x18
#define THS_LPF_OFFS 3

#define THS_SRC_MASK 0x20
#define THS_SRC_OFFS 5

#define THS_MODE_MASK   0x7
#define THS_MODE_OFFS   0

#define THS_MODE_DISABLE    0
#define THS_MODE_AVMUTE     1
#define THS_MODE_AC_BIAS    4
#define THS_MODE_STC        6   //mid bias

int ths_init();

void ths_set_lpf(alt_u8 val);

void ths_source_sel(ths_input_t input, alt_u8 lpf);

#endif /* THS7353_H_ */
