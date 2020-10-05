//
// Copyright (C) 2019-2020  Markus Hiienkari <mhiienka@niksula.hut.fi>
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

#define OSD_CHAR_ROWS 30
#define OSD_CHAR_COLS 16
#define OSD_CHAR_SECTIONS 2

#include <stdint.h>

typedef union {
    struct {
        uint8_t enable:1;
        uint8_t status_refresh:1;
        uint8_t menu_active:1;
        uint8_t status_timeout:2;
        uint8_t x_offset:3;
        uint8_t y_offset:3;
        uint8_t x_size:2;
        uint8_t y_size:2;
        uint8_t border_color:2;
        uint32_t osd_rsv:15;
    } __attribute__((packed, __may_alias__));
    uint32_t data;
} osd_config_reg;

// char regs
typedef struct {
    char data[OSD_CHAR_ROWS][OSD_CHAR_SECTIONS][OSD_CHAR_COLS];
} osd_char_array;

typedef struct {
    uint32_t mask;
} osd_enable_color_reg;

typedef struct {
    osd_char_array osd_array;
    osd_config_reg osd_config;
    osd_enable_color_reg osd_sec_enable[OSD_CHAR_SECTIONS];
    osd_enable_color_reg osd_row_color;
} __attribute__((packed, __may_alias__)) osd_regs;

#endif //OSD_GENERATOR_REGS_H_
