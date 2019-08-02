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

#include <stdio.h>
#include <unistd.h>
#include <string.h>
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
    mode_flags valid_lm[] = { MODE_PT, (MODE_L2 | (MODE_L2<<cm.cc.l2_mode)), (MODE_L3_GEN_16_9<<cm.cc.l3_mode), (MODE_L4_GEN_4_3<<cm.cc.l4_mode), (MODE_L5_GEN_4_3<<cm.cc.l5_mode) };
    mode_flags target_lm;
    alt_u8 pt_only = 0;

    // one for each video_group
    alt_u8* group_ptr[] = { &pt_only, &cm.cc.pm_240p, &cm.cc.pm_384p, &cm.cc.pm_480i, &cm.cc.pm_480p, &cm.cc.pm_1080i };

    for (i=0; i<num_modes; i++) {
        mode_type = video_modes[i].type;

        switch (video_modes[i].group) {
            case GROUP_NONE:
            case GROUP_240P:
                break;
            case GROUP_384P:
                //fixed Line2x/3x mode for 240x360p
                valid_lm[2] = MODE_L2_240x360;
                valid_lm[3] = MODE_L3_240x360;
                valid_lm[4] = MODE_L3_GEN_16_9;
                if (video_modes[i].v_total == 449) {
                    if (!strncmp(video_modes[i].name, "720x400", 7)) {
                        if (cm.cc.s400p_mode == 0)
                            continue;
                    } else if (!strncmp(video_modes[i].name, "640x400", 7)) {
                        if (cm.cc.s400p_mode == 1)
                            continue;
                    }
                }
                break;
            case GROUP_480I:
                //fixed Line3x/4x mode for 480i
                valid_lm[2] = MODE_L3_GEN_16_9;
                valid_lm[3] = MODE_L4_GEN_4_3;
                break;
            case GROUP_480P:
                if (video_modes[i].v_total == 525) {
                    if (video_modes[i-1].group == GROUP_480I) { // hit "480p" on the list
                        if (cm.cc.s480p_mode == 0) // Auto
                            mode_type &= ~VIDEO_PC;
                        else if (cm.cc.s480p_mode == 2) // VESA 640x480@60
                            continue;
                    } else { // "640x480" on the list
                        if (cm.cc.s480p_mode == 0) // Auto
                            mode_type &= ~VIDEO_EDTV;
                        else if (cm.cc.s480p_mode == 1) // DTV 480p
                            continue;
                    }
                }
                break;
            case GROUP_1080I:
                break;
            default:
                printf("WARNING: Corrupted mode (id %d)\n", i);
                continue;
                break;
        }

        target_lm = valid_lm[*group_ptr[video_modes[i].group]];

        if ((typemask & mode_type) && (target_lm & video_modes[i].flags) && (progressive == !(video_modes[i].flags & MODE_INTERLACED)) && (totlines <= (video_modes[i].v_total+LINECNT_MAX_TOLERANCE))) {

            // defaults
            cm.tx_pixelrep = TX_PIXELREP_DISABLE;
            cm.hdmitx_pixr_ifr = 0;
            cm.hdmitx_vic = HDMI_Unknown;
            cm.sample_mult = 1;
            cm.hsync_cut = 0;
            cm.target_lm = target_lm & video_modes[i].flags;    //ensure L2 mode uniqueness

            switch (cm.target_lm) {
                case MODE_PT:
                    cm.fpga_vmultmode = FPGA_V_MULTMODE_1X;
                    cm.fpga_hmultmode = FPGA_H_MULTMODE_FULLWIDTH;
                    cm.hdmitx_vic = video_modes[i].vic;
                    // Upsample / pixel-repeat horizontal resolution of 240p/480i modes to fulfill min. 25MHz TMDS clock requirement
                    if ((video_modes[i].group == GROUP_240P) || (video_modes[i].group == GROUP_480I)) {
                        if (cm.cc.upsample2x)
                            cm.sample_mult = 2;
                        else
                            cm.tx_pixelrep = TX_PIXELREP_2X;
                        cm.hdmitx_pixr_ifr = TX_PIXELREP_2X;
                    }
                    break;
                case MODE_L2:
                    cm.fpga_vmultmode = FPGA_V_MULTMODE_2X;
                    // Use native 2x sampling with low-res modes when possible to minimize jitter and generate min. 25MHz input pclk for FPGA PLL
                    if ((!cm.cc.vga_ilace_fix) && (video_modes[i].h_total < 1400) && ((video_modes[i].group == GROUP_240P) || (video_modes[i].group == GROUP_384P) || (video_modes[i].group == GROUP_480I))) {
                        cm.fpga_hmultmode = FPGA_H_MULTMODE_OPTIMIZED_1X;
                        cm.sample_mult = 2;
                    } else {
                        cm.fpga_hmultmode = FPGA_H_MULTMODE_FULLWIDTH;
                    }
                    // Upsample / pixel-repeat horizontal resolution of 384p/480p/960i modes
                    if ((video_modes[i].group == GROUP_384P) || (video_modes[i].group == GROUP_480P) || ((video_modes[i].group == GROUP_1080I) && (video_modes[i].h_total < 1200))) {
                        if (cm.cc.upsample2x) {
                            cm.fpga_hmultmode = FPGA_H_MULTMODE_FULLWIDTH;
                            cm.sample_mult = 2;
                        } else {
                            cm.tx_pixelrep = TX_PIXELREP_2X;
                        }
                    }
                    break;
                case MODE_L2_512_COL:
                    cm.fpga_vmultmode = FPGA_V_MULTMODE_2X;
                    cm.fpga_hmultmode = FPGA_H_MULTMODE_OPTIMIZED;
                    cm.sample_mult = 2;
                    break;
                case MODE_L2_256_COL:
                    cm.fpga_vmultmode = FPGA_V_MULTMODE_2X;
                    cm.fpga_hmultmode = FPGA_H_MULTMODE_OPTIMIZED_1X;
                    cm.sample_mult = 6;
                    break;
                case MODE_L2_320_COL:
                    cm.fpga_vmultmode = FPGA_V_MULTMODE_2X;
                    cm.fpga_hmultmode = FPGA_H_MULTMODE_OPTIMIZED_1X;
                    cm.sample_mult = 4;
                    break;
                case MODE_L2_240x360:
                    cm.fpga_vmultmode = FPGA_V_MULTMODE_2X;
                    cm.fpga_hmultmode = FPGA_H_MULTMODE_OPTIMIZED;
                    cm.sample_mult = 5;
                    break;
                case MODE_L3_GEN_16_9:
                    cm.fpga_vmultmode = FPGA_V_MULTMODE_3X;
                    cm.fpga_hmultmode = FPGA_H_MULTMODE_FULLWIDTH;
                    // Upsample / pixel-repeat horizontal resolution of 480i mode
                    if (video_modes[i].group == GROUP_480I) {
                        if (cm.cc.upsample2x)
                            cm.sample_mult = 2;
                        else
                            cm.tx_pixelrep = TX_PIXELREP_2X;
                    }
                    break;
                case MODE_L3_GEN_4_3:
                    cm.fpga_vmultmode = FPGA_V_MULTMODE_3X;
                    cm.fpga_hmultmode = FPGA_H_MULTMODE_ASPECTFIX;
                    break;
                case MODE_L3_512_COL:
                    cm.fpga_vmultmode = FPGA_V_MULTMODE_3X;
                    cm.fpga_hmultmode = FPGA_H_MULTMODE_OPTIMIZED;
                    cm.sample_mult = 2;
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
                case MODE_L3_240x360:
                    cm.fpga_vmultmode = FPGA_V_MULTMODE_3X;
                    cm.fpga_hmultmode = FPGA_H_MULTMODE_OPTIMIZED;
                    cm.sample_mult = 7;
                    cm.hsync_cut = 13;
                    break;
                case MODE_L4_GEN_4_3:
                    cm.fpga_vmultmode = FPGA_V_MULTMODE_4X;
                    cm.fpga_hmultmode = FPGA_H_MULTMODE_FULLWIDTH;
                    // Upsample / pixel-repeat horizontal resolution of 480i mode
                    if (video_modes[i].group == GROUP_480I) {
                        if (cm.cc.upsample2x)
                            cm.sample_mult = 2;
                        else
                            cm.tx_pixelrep = TX_PIXELREP_2X;
                    }
                    break;
                case MODE_L4_512_COL:
                    cm.fpga_vmultmode = FPGA_V_MULTMODE_4X;
                    cm.fpga_hmultmode = FPGA_H_MULTMODE_OPTIMIZED;
                    cm.sample_mult = 2;
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
                case MODE_L5_512_COL:
                    cm.fpga_vmultmode = FPGA_V_MULTMODE_5X;
                    cm.fpga_hmultmode = FPGA_H_MULTMODE_OPTIMIZED;
                    cm.sample_mult = 3;
                    cm.hsync_cut = 40;
                    break;
                case MODE_L5_320_COL:
                    cm.fpga_vmultmode = FPGA_V_MULTMODE_5X;
                    cm.fpga_hmultmode = FPGA_H_MULTMODE_OPTIMIZED;
                    cm.sample_mult = 5;
                    cm.hsync_cut = 24;
                    break;
                case MODE_L5_256_COL:
                    cm.fpga_vmultmode = FPGA_V_MULTMODE_5X;
                    cm.fpga_hmultmode = FPGA_H_MULTMODE_OPTIMIZED;
                    cm.sample_mult = 6;
                    cm.hsync_cut = 20;
                    break;
                default:
                    printf("WARNING: invalid target_lm\n");
                    continue;
                    break;
            }

            if (cm.hdmitx_vic == HDMI_Unknown)
                cm.hdmitx_vic = cm.cc.default_vic;

            return i;
        }
    }

    return -1;
}
