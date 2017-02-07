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

#include <stdio.h>
#include <unistd.h>
#include "system.h"
#include "av_controller.h"
#include "video_modes.h"

#define LINECNT_MAX_TOLERANCE   30

extern avmode_t cm;

const mode_data_t video_modes_default[] = VIDEO_MODES_DEF;
mode_data_t video_modes[VIDEO_MODES_CNT];

/* TODO: rewrite, check hz etc. */
alt_8 get_mode_id(alt_u32 totlines, alt_u8 progressive, alt_u32 hz, video_type typemask)
{
    alt_8 i;
    alt_u8 num_modes = sizeof(video_modes)/sizeof(mode_data_t);
    video_type mode_type;
    mode_flags valid_lm[] = { MODE_PT, MODE_L2, (MODE_L3_GEN_16_9<<cm.cc.l3_mode), (MODE_L4_GEN_4_3<<cm.cc.l4_mode), (MODE_L5_GEN_4_3<<cm.cc.l5_mode) };
    mode_flags target_lm;
    alt_u8 pt_only = 0;

    // one for each video_group
    alt_u8* group_ptr[] = { &pt_only, &cm.cc.pm_240p, &cm.cc.pm_384p, &cm.cc.pm_480i, &cm.cc.pm_480p, &cm.cc.pm_480p, &cm.cc.pm_1080i };

    for (i=0; i<num_modes; i++) {
        mode_type = video_modes[i].type;

        // disable particular mode based on input and user preference
        if (video_modes[i].group == GROUP_DTV480P) {
            if (cm.cc.s480p_mode == 0)  // auto
                mode_type &= ~VIDEO_PC;
            else if (cm.cc.s480p_mode == 2)  // VGA 640x480
                continue;
        } else if (video_modes[i].group == GROUP_VGA480P) {
            if (cm.cc.s480p_mode == 0)  // auto
                mode_type &= ~VIDEO_EDTV;
            else if (cm.cc.s480p_mode == 1) // DTV 480P
                continue;
        } else if (video_modes[i].group > GROUP_1080I) {
            printf("WARNING: Corrupted mode (id %d)\n", i);
            continue;
        }

        target_lm = valid_lm[*group_ptr[video_modes[i].group]];

        if ((typemask & mode_type) && (target_lm & video_modes[i].flags) && (progressive == !(video_modes[i].flags & MODE_INTERLACED)) && (totlines <= (video_modes[i].v_total+LINECNT_MAX_TOLERANCE))) {

            // defaults
            cm.hdmitx_pixelrep = HDMITX_PIXELREP_DISABLE;
            cm.hdmitx_pixr_ifr = 0;
            cm.sample_mult = 1;
            cm.hsync_cut = 0;
            cm.target_lm = target_lm;

            switch (target_lm) {
                case MODE_PT:
                    cm.fpga_vmultmode = FPGA_V_MULTMODE_1X;
                    cm.fpga_hmultmode = FPGA_H_MULTMODE_FULLWIDTH;
                    cm.hdmitx_pixelrep = ((video_modes[i].group == GROUP_240P) || (video_modes[i].group == GROUP_480I)) ? HDMITX_PIXELREP_2X : HDMITX_PIXELREP_DISABLE;
                    cm.hdmitx_pixr_ifr = cm.hdmitx_pixelrep;
                    break;
                case MODE_L2:
                    cm.fpga_vmultmode = FPGA_V_MULTMODE_2X;
                    if ((video_modes[i].group == GROUP_240P) || (video_modes[i].group == GROUP_384P) || (video_modes[i].group == GROUP_480I)) {
                        cm.fpga_hmultmode = FPGA_H_MULTMODE_OPTIMIZED;
                        cm.sample_mult = 2;
                    } else {
                        cm.fpga_hmultmode = FPGA_H_MULTMODE_FULLWIDTH;
                    }
                    cm.hdmitx_pixelrep = ((video_modes[i].group == GROUP_384P) ||
                                          (video_modes[i].group == GROUP_DTV480P) ||
                                          (video_modes[i].group == GROUP_VGA480P) ||
                                          ((video_modes[i].group == GROUP_1080I) && (video_modes[i].h_total < 1200))) ? HDMITX_PIXELREP_2X : HDMITX_PIXELREP_DISABLE;
                    break;
                case MODE_L3_GEN_16_9:
                    cm.fpga_vmultmode = FPGA_V_MULTMODE_3X;
                    cm.fpga_hmultmode = FPGA_H_MULTMODE_FULLWIDTH;
                    break;
                case MODE_L3_GEN_4_3:
                    cm.fpga_vmultmode = FPGA_V_MULTMODE_3X;
                    cm.fpga_hmultmode = FPGA_H_MULTMODE_ASPECTFIX;
                    break;
                case MODE_L3_320_COL:
                    cm.fpga_vmultmode = FPGA_V_MULTMODE_3X;
                    cm.fpga_hmultmode = FPGA_H_MULTMODE_OPTIMIZED;
                    cm.sample_mult = 4;
                    break;
                case MODE_L3_256_COL:
                    cm.fpga_vmultmode = FPGA_V_MULTMODE_3X;
                    cm.fpga_hmultmode = FPGA_H_MULTMODE_OPTIMIZED;
                    cm.sample_mult = 5;
                    break;
                case MODE_L4_GEN_4_3:
                    cm.fpga_vmultmode = FPGA_V_MULTMODE_4X;
                    cm.fpga_hmultmode = FPGA_H_MULTMODE_FULLWIDTH;
                    break;
                case MODE_L4_320_COL:
                    cm.fpga_vmultmode = FPGA_V_MULTMODE_4X;
                    cm.fpga_hmultmode = FPGA_H_MULTMODE_OPTIMIZED;
                    cm.sample_mult = 4;
                    break;
                case MODE_L4_256_COL:
                    cm.fpga_vmultmode = FPGA_V_MULTMODE_4X;
                    cm.fpga_hmultmode = FPGA_H_MULTMODE_OPTIMIZED;
                    cm.sample_mult = 5;
                    break;
                case MODE_L5_GEN_4_3:
                    cm.fpga_vmultmode = FPGA_V_MULTMODE_5X;
                    cm.fpga_hmultmode = FPGA_H_MULTMODE_FULLWIDTH;
                    cm.hsync_cut = 120;
                    break;
                case MODE_L5_320_COL:
                    cm.fpga_vmultmode = FPGA_V_MULTMODE_5X;
                    cm.fpga_hmultmode = FPGA_H_MULTMODE_OPTIMIZED;
                    cm.sample_mult = 5;
                    cm.hsync_cut = 120;
                    break;
                case MODE_L5_256_COL:
                    cm.fpga_vmultmode = FPGA_V_MULTMODE_5X;
                    cm.fpga_hmultmode = FPGA_H_MULTMODE_OPTIMIZED;
                    cm.sample_mult = 6;
                    cm.hsync_cut = 120;
                    break;
                default:
                    printf("WARNING: invalid target_lm\n");
                    continue;
                    break;
            }

            return i;
        }
    }

    return -1;
}
