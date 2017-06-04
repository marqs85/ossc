//
// Copyright (C) 2015-2017  Markus Hiienkari <mhiienka@niksula.hut.fi>
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
#include "tvp7002.h"

#define SCANLINESTR_MAX     15
#define HV_MASK_MAX         63
#define HV_MASK_MAX_BR      15
#define VIDEO_LPF_MAX       5
#define SAMPLER_PHASE_MAX   31
#define SYNC_VTH_MAX        31
#define VSYNC_THOLD_MIN     10
#define VSYNC_THOLD_MAX     200
#define SD_SYNC_WIN_MAX     255
#define PLL_COAST_MAX       5

#define SL_MODE_MAX         2
#define SL_TYPE_MAX         2

#define L5FMT_1920x1080     0
#define L5FMT_1600x1200     1
#define L5FMT_1920x1200     2

typedef struct {
    alt_u8 sl_mode;
    alt_u8 sl_type;
    alt_u8 sl_str;
    alt_u8 sl_id;
    alt_u8 linemult_target;
    alt_u8 l2_mode;
    alt_u8 l3_mode;
    alt_u8 l4_mode;
    alt_u8 l5_mode;
    alt_u8 l5_fmt;
    alt_u8 pm_240p;
    alt_u8 pm_384p;
    alt_u8 pm_480i;
    alt_u8 pm_480p;
    alt_u8 pm_1080i;
    alt_u8 ar_256col;
    alt_u8 h_mask;
    alt_u8 v_mask;
    alt_u8 mask_br;
    alt_u8 tx_mode;
    alt_u8 hdmi_itc;
    alt_u8 s480p_mode;
    alt_u8 sampler_phase;
    alt_u8 tvp_hpll2x;
    alt_u8 ypbpr_cs;
    alt_u8 sync_vth;
    alt_u8 linelen_tol;
    alt_u8 vsync_thold;
    alt_u8 sync_lpf;
    alt_u8 video_lpf;
    alt_u8 pre_coast;
    alt_u8 post_coast;
    alt_u8 full_tx_setup;
    alt_u8 vga_ilace_fix;
#ifdef ENABLE_AUDIO
    alt_u8 audio_dw_sampl;
    alt_u8 audio_swap_lr;
#endif
    alt_u8 def_input;
    color_setup_t col;
} __attribute__((packed)) avconfig_t;

int set_default_avconfig();

#endif
