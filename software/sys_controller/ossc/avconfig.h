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

#ifndef AVCONFIG_H_
#define AVCONFIG_H_

#include "alt_types.h"
#include "sysconfig.h"

#define SIGNED_NUMVAL_ZERO  128

#define SCANLINESTR_MAX     15
#define SL_HYBRIDSTR_MAX    28
#define H_MASK_MAX          255
#define V_MASK_MAX          63
#define HV_MASK_MAX_BR      15
#define VIDEO_LPF_MAX       5
#define SAMPLER_PHASE_MAX   31
#define SYNC_VTH_MAX        31
#define VSYNC_THOLD_MIN     10
#define VSYNC_THOLD_MAX     200
#define SD_SYNC_WIN_MAX     255
#define PLL_COAST_MAX       5
#define REVERSE_LPF_MAX     31
#define COARSE_GAIN_MAX     15
#define ALC_H_FILTER_MAX    7
#define ALC_V_FILTER_MAX    10
#define CLAMP_OFFSET_MIN    (SIGNED_NUMVAL_ZERO-100)
#define CLAMP_OFFSET_MAX    (SIGNED_NUMVAL_ZERO+100)

#define SL_MODE_MAX         2
#define SL_TYPE_MAX         2

#define AUDIO_GAIN_M12DB    0
#define AUDIO_GAIN_0DB      12
#define AUDIO_GAIN_12DB     24
#define AUDIO_GAIN_MAX      AUDIO_GAIN_12DB

#define L5FMT_1920x1080     0
#define L5FMT_1600x1200     1
#define L5FMT_1920x1200     2

static const char *avinput_str[] = { "Test pattern", "AV1_RGBS", "AV1_RGsB", "AV1_YPbPr", "AV2_YPbPr", "AV2_RGsB", "AV3_RGBHV", "AV3_RGBS", "AV3_RGsB", "AV3_YPbPr", "Last used" };

typedef enum {
    AV_TESTPAT      = 0,
    AV1_RGBs        = 1,
    AV1_RGsB        = 2,
    AV1_YPBPR       = 3,
    AV2_YPBPR       = 4,
    AV2_RGsB        = 5,
    AV3_RGBHV       = 6,
    AV3_RGBs        = 7,
    AV3_RGsB        = 8,
    AV3_YPBPR       = 9,
    AV_LAST         = 10
} avinput_t;

typedef struct {
    alt_u8 r_f_off;
    alt_u8 g_f_off;
    alt_u8 b_f_off;
    alt_u8 r_f_gain;
    alt_u8 g_f_gain;
    alt_u8 b_f_gain;
    alt_u8 c_gain;
} __attribute__((packed)) color_setup_t;

typedef struct {
    /* P-LM mode options */
    alt_u8 pm_240p;
    alt_u8 pm_384p;
    alt_u8 pm_480i;
    alt_u8 pm_480p;
    alt_u8 pm_1080i;
    alt_u8 l2_mode;
    alt_u8 l3_mode;
    alt_u8 l4_mode;
    alt_u8 l5_mode;
    alt_u8 l6_mode;
    alt_u8 l5_fmt;
    alt_u8 s480p_mode;
    alt_u8 s400p_mode;
    alt_u8 upsample2x;
    alt_u8 ar_256col;
    alt_u8 default_vic;
    alt_u8 clamp_offset;

    /* Postprocessing settings */
    alt_u8 sl_mode;
    alt_u8 sl_type;
    alt_u8 sl_hybr_str;
    alt_u8 sl_method;
    alt_u8 sl_altern;
    alt_u8 sl_str;
    alt_u8 sl_id;
    alt_u8 sl_cust_l_str[5];
    alt_u8 sl_cust_c_str[6];
    alt_u8 sl_cust_iv_x;
    alt_u8 sl_cust_iv_y;
    alt_u8 mask_br;
    alt_u8 mask_color;
    alt_u8 reverse_lpf;

    /* AFE settings */
    alt_u8 sync_vth;
    alt_u8 linelen_tol;
    alt_u8 vsync_thold;
    alt_u8 pre_coast;
    alt_u8 post_coast;
    alt_u8 ypbpr_cs;
    alt_u8 video_lpf;
    alt_u8 sync_lpf;
    alt_u8 stc_lpf;
    alt_u8 alc_h_filter;
    alt_u8 alc_v_filter;
    color_setup_t col;

    /* Audio settings */
    alt_u8 audio_dw_sampl;
    alt_u8 audio_swap_lr;
    alt_u8 audio_gain;
    alt_u8 audio_mono;

    /* TX / extra settings */
    alt_u8 tx_mode;
    alt_u8 hdmi_itc;
    alt_u8 full_tx_setup;
    alt_u8 av3_alt_rgb;
    avinput_t link_av;
} __attribute__((packed)) avconfig_t;

int set_default_avconfig();

#endif
