//
// Copyright (C) 2015-2023  Markus Hiienkari <mhiienka@niksula.hut.fi>
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

#ifndef VIDEO_MODES_H_
#define VIDEO_MODES_H_

#include <stdint.h>
#include "sysconfig.h"
#include "avconfig.h"
#include "it6613_sys.h"

#define DEF_PHASE   0x10

#define H_TOTAL_MIN 300
#define H_TOTAL_MAX 2800
#define H_TOTAL_ADJ_MAX 19
#define H_SYNCLEN_MIN 10
#define H_SYNCLEN_MAX 255
#define H_BPORCH_MIN 0
#define H_BPORCH_MAX 511
#define H_ACTIVE_MIN 200
#define H_ACTIVE_MAX 2560
#define H_ACTIVE_SMP_MAX 2048
#define V_SYNCLEN_MIN 1
#define V_SYNCLEN_MAX 15
#define V_BPORCH_MIN 0
#define V_BPORCH_MAX 511
#define V_ACTIVE_MIN 160
#define V_ACTIVE_MAX 1440

typedef enum {
    FORMAT_RGBS = 0,
    FORMAT_RGBHV = 1,
    FORMAT_RGsB = 2,
    FORMAT_YPbPr = 3
} video_format;

typedef enum {
    VIDEO_SDTV      = (1<<0),
    VIDEO_EDTV      = (1<<1),
    VIDEO_HDTV      = (1<<2),
    VIDEO_PC        = (1<<3),
} video_type;

typedef enum {
    GROUP_NONE      = 0,
    GROUP_240P      = 1,
    GROUP_288P      = 2,
    GROUP_384P      = 3,
    GROUP_480I      = 4,
    GROUP_576I      = 5,
    GROUP_480P      = 6,
    GROUP_576P      = 7,
    GROUP_720P      = 8,
    GROUP_1080I     = 9,
    GROUP_1080P     = 10,
} video_group;

typedef enum {
    MODE_INTERLACED     = (1<<0), //deprecated
    MODE_CRT            = (1<<1),
    //at least one of the flags below must be set for each P-LM mode
    MODE_PT             = (1<<2),
    MODE_L2             = (1<<3),
    MODE_L2_512_COL     = (1<<4),
    MODE_L2_384_COL     = (1<<5),
    MODE_L2_320_COL     = (1<<6),
    MODE_L2_256_COL     = (1<<7),
    MODE_L2_240x360     = (1<<8),
    MODE_L3_GEN_16_9    = (1<<9),
    MODE_L3_GEN_4_3     = (1<<10),
    MODE_L3_512_COL     = (1<<11),
    MODE_L3_384_COL     = (1<<12),
    MODE_L3_320_COL     = (1<<13),
    MODE_L3_256_COL     = (1<<14),
    MODE_L3_240x360     = (1<<15),
    MODE_L4_GEN_4_3     = (1<<16),
    MODE_L4_512_COL     = (1<<17),
    MODE_L4_384_COL     = (1<<18),
    MODE_L4_320_COL     = (1<<19),
    MODE_L4_256_COL     = (1<<20),
    MODE_L5_GEN_4_3     = (1<<21),
    MODE_L5_512_COL     = (1<<22),
    MODE_L5_384_COL     = (1<<23),
    MODE_L5_320_COL     = (1<<24),
    MODE_L5_256_COL     = (1<<25),
} mode_flags;

typedef enum {
    VMODE_480p = 24,
} vmode_t;

typedef struct {
    uint16_t h_active;
    uint16_t v_active;
    uint16_t v_hz_x100;
    uint16_t h_total;
    uint8_t  h_total_adj;
    uint16_t v_total;
    uint16_t h_backporch;
    uint16_t v_backporch;
    uint8_t h_synclen;
    uint8_t v_synclen;
    uint8_t interlaced;
} sync_timings_t;

typedef struct {
    uint8_t h;
    uint8_t v;
} aspect_ratio_t;

typedef struct {
    uint8_t h;
    uint8_t v;
} mask_t;

typedef enum {
    TX_1X   = 0,
    TX_2X   = 1,
    TX_4X   = 2
} HDMI_pixelrep_t;

typedef struct {
    char name[10];
    HDMI_Video_Type vic;
    sync_timings_t timings;
    uint8_t sampler_phase;
    union {
        aspect_ratio_t ar;
        mask_t mask;
    };
    video_type type;
    video_group group;
    mode_flags flags;
} mode_data_t;

typedef struct {
    int8_t x_rpt;
    int8_t y_rpt;
    uint8_t h_skip;
    uint8_t h_sample_sel;
    int16_t x_offset;
    int16_t y_offset;
    uint16_t x_size;
    uint16_t y_size;
    uint16_t framesync_line;
    uint8_t x_start_lb;
    int8_t y_start_lb;
    uint8_t framelock;
    // for generation from 27MHz clock
    int8_t si_pclk_mult;
} vm_proc_config_t;


void set_default_vm_table();

uint32_t estimate_dotclk(mode_data_t *vm_in, uint32_t h_hz);

uint32_t calculate_pclk(uint32_t src_clk_hz, mode_data_t *vm_out, vm_proc_config_t *vm_conf);

int get_pure_lm_mode(avconfig_t *cc, mode_data_t *vm_in, mode_data_t *vm_out, vm_proc_config_t *vm_conf);

int get_vmode(vmode_t vmode_id, mode_data_t *vm_in, mode_data_t *vm_out, vm_proc_config_t *vm_conf);

#endif /* VIDEO_MODES_H_ */
