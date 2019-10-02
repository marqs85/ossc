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

#ifndef OSD_GENERATOR_REGS_H_
#define OSD_GENERATOR_REGS_H_

#include <alt_types.h>

typedef union {
    struct {
        alt_u8 enable:1;
        alt_u8 status_refresh:1;
        alt_u8 menu_active:1;
        alt_u8 status_timeout:2;
        alt_u8 x_offset:3;
        alt_u8 y_offset:3;
        alt_u8 x_size:2;
        alt_u8 y_size:2;
        alt_u32 osd_rsv:17;
    } __attribute__((packed, __may_alias__));
    alt_u32 data;
} osd_config_reg;

// char regs
typedef struct {
    char row1[16];
    char row2[16];
} osd_char_regs;

typedef struct {
    osd_config_reg osd_config;
    osd_char_regs osd_chars;
} __attribute__((packed, __may_alias__)) osd_regs;

#endif //OSD_GENERATOR_REGS_H_
