//
// Copyright (C) 2015-2022  Markus Hiienkari <mhiienka@niksula.hut.fi>
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

#include <stdint.h>

// bit-fields coded as little-endian
typedef union {
    struct {
        uint16_t vmax:11;
        uint8_t interlace_flag:1;
        uint8_t sc_rsv2:4;
        uint8_t fpga_vsyncgen:2;
        uint16_t vmax_tvp:11;
        uint8_t sc_rsv:2;
        uint8_t vsync_flag:1;
    } __attribute__((packed, __may_alias__));
    uint32_t data;
} sc_status_reg;

typedef union {
    struct {
        uint32_t pcnt_frame:20;
        uint16_t sc_rsv:12;
    } __attribute__((packed, __may_alias__));
    uint32_t data;
} sc_status2_reg;

typedef union {
    struct {
        uint16_t lt_lat_result:16;
        uint16_t lt_stb_result:12;
        uint8_t lt_rsv:3;
        uint8_t lt_finished:1;
    } __attribute__((packed, __may_alias__));
    uint32_t data;
} lt_status_reg;

typedef union {
    struct {
        uint16_t h_total:12;
        uint16_t h_active:12;
        uint16_t h_synclen:8;
    } __attribute__((packed, __may_alias__));
    uint32_t data;
} hv_config_reg;

typedef union {
    struct {
        uint16_t h_backporch:9;
        uint16_t v_total:11;
        uint16_t v_active:11;
        uint8_t interlaced:1;
    } __attribute__((packed, __may_alias__));
    uint32_t data;
} hv_config2_reg;

typedef union {
    struct {
        uint8_t v_synclen:4;
        uint16_t v_backporch:9;
        uint16_t v_startline:11;
        uint8_t h_skip:4;
        uint8_t h_sample_sel:4;
    } __attribute__((packed, __may_alias__));
    uint32_t data;
} hv_config3_reg;

typedef union {
    struct {
        uint8_t mask_br:4;
        uint8_t mask_color:3;
        uint8_t rev_lpf_str:5;
        uint8_t panasonic_hack:1;
        /* temp */
        uint8_t h_l3_240x360:1;
        uint16_t h_opt_startoff:10;
        //uint8_t h_l5fmt:1;
        uint8_t h_multmode:2;
        uint8_t v_multmode:3;
        uint8_t h_opt_scale:3;
    } __attribute__((packed, __may_alias__));
    uint32_t data;
} misc_config_reg;

typedef union {
    struct {
        uint32_t sl_l_str_arr:20;
        uint8_t sl_l_overlay:5;
        uint8_t sl_hybr_str:5;
        uint8_t sl_method:1;
        uint8_t sl_no_altern:1;
    } __attribute__((packed, __may_alias__));
    uint32_t data;
} sl_config_reg;

typedef union {
    struct {
        uint32_t sl_c_str_arr:24;
        uint8_t sl_c_overlay:6;
        uint8_t sl_rsv:1;
        uint8_t sl_altiv:1;
    } __attribute__((packed, __may_alias__));
    uint32_t data;
} sl_config2_reg;

typedef struct {
    sc_status_reg sc_status;
    sc_status2_reg sc_status2;
    lt_status_reg lt_status;
    hv_config_reg hv_in_config;
    hv_config2_reg hv_in_config2;
    hv_config3_reg hv_in_config3;
    misc_config_reg misc_config;
    sl_config_reg sl_config;
    sl_config2_reg sl_config2;
} __attribute__((packed, __may_alias__)) sc_regs;

#endif //SC_CONFIG_REGS_H_
