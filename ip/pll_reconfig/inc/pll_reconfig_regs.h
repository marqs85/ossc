//
// Copyright (C) 2019  Markus Hiienkari <mhiienka@niksula.hut.fi>
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

#ifndef PLL_RECONFIG_REGS_H_
#define PLL_RECONFIG_REGS_H_

#include <alt_types.h>

typedef union {
    struct {
        alt_u8 reset:1;
        alt_u8 update:1;
        alt_u8 t_config_id:4;
        alt_u32 pll_reconfig_rsv:21;
        alt_u8 c_config_id:4;
        alt_u8 busy:1;
    } __attribute__((packed, __may_alias__));
    alt_u32 data;
} pll_config_status_reg;

// char regs
typedef struct {
    char data[160];
} pll_config_data_regs;

typedef struct {
    pll_config_status_reg pll_config_status;
    pll_config_data_regs pll_config_data;
} __attribute__((packed, __may_alias__)) pll_reconfig_regs;

#endif //OSD_GENERATOR_REGS_H_
