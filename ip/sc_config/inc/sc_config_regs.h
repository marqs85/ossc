//
// Copyright (C) 2015-2019  Markus Hiienkari <mhiienka@niksula.hut.fi>
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

#ifndef SC_CONFIG_REGS_H_
#define SC_CONFIG_REGS_H_

#include <alt_types.h>

// bit-fields coded as little-endian
typedef union {
    struct {
        alt_u16 vmax:11;
        alt_u8 interlace_flag:1;
        alt_u8 sc_rsv2:4;
        alt_u8 fpga_vsyncgen:2;
        alt_u16 vmax_tvp:11;
        alt_u8 sc_rsv:2;
        alt_u8 vsync_flag:1;
    } __attribute__((packed, __may_alias__));
    alt_u32 data;
} sc_status_reg;

typedef union {
    struct {
        alt_u32 pcnt_frame:20;
        alt_u16 sc_rsv:12;
    } __attribute__((packed, __may_alias__));
    alt_u32 data;
} sc_status2_reg;

typedef union {
    struct {
        alt_u16 lt_lat_result:16;
        alt_u16 lt_stb_result:12;
        alt_u8 lt_rsv:3;
        alt_u8 lt_finished:1;
    } __attribute__((packed, __may_alias__));
    alt_u32 data;
} lt_status_reg;

typedef union {
    struct {
        alt_u16 h_active:11;
        alt_u16 h_backporch:9;
        alt_u8 h_synclen:8;
        alt_u8 h_l3_240x360:1;
        alt_u8 h_l5fmt:1;
        alt_u8 h_multmode:2;
    } __attribute__((packed, __may_alias__));
    alt_u32 data;
} h_config_reg;

typedef union {
    struct {
        alt_u16 h_opt_startoff:10;
        alt_u8 h_opt_sample_mult:3;
        alt_u8 h_opt_sample_sel:3;
        alt_u8 h_opt_scale:3;
        alt_u16 h_mask:11;
        alt_u8 h_rsv:2;
    } __attribute__((packed, __may_alias__));
    alt_u32 data;
} h_config2_reg;

typedef union {
    struct {
        alt_u16 v_active:11;
        alt_u8 v_backporch:6;
        alt_u8 v_synclen:3;
        alt_u8 v_mask:6;
        alt_u8 v_rsv:3;
        alt_u8 v_multmode:3;
    } __attribute__((packed, __may_alias__));
    alt_u32 data;
} v_config_reg;

typedef union {
    struct {
        alt_u8 mask_br:4;
        alt_u8 mask_color:3;
        alt_u8 rev_lpf_str:5;
        alt_u8 panasonic_hack:1;
        alt_u32 misc_rsv:19;
    } __attribute__((packed, __may_alias__));
    alt_u32 data;
} misc_config_reg;

typedef union {
    struct {
        alt_u32 sl_l_str_arr:20;
        alt_u8 sl_l_overlay:5;
        alt_u8 sl_hybr_str:5;
        alt_u8 sl_method:1;
        alt_u8 sl_no_altern:1;
    } __attribute__((packed, __may_alias__));
    alt_u32 data;
} sl_config_reg;

typedef union {
    struct {
        alt_u32 sl_c_str_arr:24;
        alt_u8 sl_c_overlay:6;
        alt_u8 sl_rsv:1;
        alt_u8 sl_altiv:1;
    } __attribute__((packed, __may_alias__));
    alt_u32 data;
} sl_config2_reg;

typedef struct {
    sc_status_reg sc_status;
    sc_status2_reg sc_status2;
    lt_status_reg lt_status;
    h_config_reg h_config;
    h_config2_reg h_config2;
    v_config_reg v_config;
    misc_config_reg misc_config;
    sl_config_reg sl_config;
    sl_config2_reg sl_config2;
} __attribute__((packed, __may_alias__)) sc_regs;

#endif //SC_CONFIG_REGS_H_
