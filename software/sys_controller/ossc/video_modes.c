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

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdint.h>
#include "system.h"
#include "video_modes.h"
#include "av_controller.h"
#include "avconfig.h"

#define VM_OUT_YMULT        (vm_conf->y_rpt+1)
#define VM_OUT_XMULT        (vm_conf->x_rpt+1)
#define VM_OUT_PCLKMULT     (((vm_conf->x_rpt+1)*(vm_conf->y_rpt+1))/(vm_conf->h_skip+1))

#include "video_modes_list.c"

const int num_video_modes_plm = sizeof(video_modes_plm_default)/sizeof(mode_data_t);

mode_data_t video_modes_plm[sizeof(video_modes_plm_default)/sizeof(mode_data_t)];

void set_default_vm_table() {
    memcpy(video_modes_plm, video_modes_plm_default, sizeof(video_modes_plm_default));
}

void vmode_hv_mult(mode_data_t *vmode, uint8_t h_mult, uint8_t v_mult) {
    uint32_t val, bp_extra;

    val = vmode->timings.h_synclen * h_mult;
    if (val > H_SYNCLEN_MAX) {
        vmode->timings.h_synclen = H_SYNCLEN_MAX;
        bp_extra = val - vmode->timings.h_synclen;
    } else {
        vmode->timings.h_synclen = val;
        bp_extra = 0;
    }

    val = (vmode->timings.h_backporch * h_mult) + bp_extra;
    if (val > H_BPORCH_MAX)
        vmode->timings.h_backporch = H_BPORCH_MAX;
    else
        vmode->timings.h_backporch = val;

    val = vmode->timings.h_active * h_mult;
    if (val > H_ACTIVE_MAX)
        vmode->timings.h_active = H_ACTIVE_MAX;
    else
        vmode->timings.h_active = val;

    vmode->timings.h_total = h_mult * vmode->timings.h_total + ((h_mult * vmode->timings.h_total_adj * 5 + 50) / 100);

    val = vmode->timings.v_synclen * v_mult;
    if (val > V_SYNCLEN_MAX) {
        vmode->timings.v_synclen = V_SYNCLEN_MAX;
        bp_extra = val - vmode->timings.v_synclen;
    } else {
        vmode->timings.v_synclen = val;
        bp_extra = 0;
    }

    val = (vmode->timings.v_backporch * v_mult) + bp_extra;
    if (val > V_BPORCH_MAX)
        vmode->timings.v_backporch = V_BPORCH_MAX;
    else
        vmode->timings.v_backporch = val;

    val = vmode->timings.v_active * v_mult;
    if (val > V_ACTIVE_MAX)
        vmode->timings.v_active = V_ACTIVE_MAX;
    else
        vmode->timings.v_active = val;

    if (vmode->timings.interlaced && ((v_mult % 2) == 0)) {
        vmode->timings.interlaced = 0;
        vmode->timings.v_total *= (v_mult / 2);
    } else {
        vmode->timings.v_total *= v_mult;
    }
}

uint32_t estimate_dotclk(mode_data_t *vm_in, uint32_t h_hz) {
    if ((vm_in->type & VIDEO_SDTV) ||
        (vm_in->type & VIDEO_EDTV))
    {
        return h_hz * 858;
    } else {
        return vm_in->timings.h_total * h_hz;
    }
}

uint32_t calculate_pclk(uint32_t src_clk_hz, mode_data_t *vm_out, vm_proc_config_t *vm_conf) {
    uint32_t pclk_hz;

    if (vm_conf->si_pclk_mult > 0) {
        pclk_hz = vm_conf->si_pclk_mult*src_clk_hz;
    } else if (vm_conf->si_pclk_mult < 0) {
        pclk_hz = src_clk_hz/((-1)*vm_conf->si_pclk_mult+1);
    } else {
        // Round to kHz but maximize accuracy without using 64-bit division
        pclk_hz = (((uint32_t)((((uint64_t)vm_out->timings.h_total*vm_out->timings.v_total*vm_out->timings.v_hz_x100)>>vm_out->timings.interlaced)/8)+6250)/12500)*1000;

        // Switch to integer mult if possible
        if (!vm_conf->framelock) {
            if ((pclk_hz >= src_clk_hz) && (pclk_hz % src_clk_hz == 0))
                vm_conf->si_pclk_mult = (pclk_hz/src_clk_hz);
            else if ((pclk_hz < src_clk_hz) && (src_clk_hz % pclk_hz == 0))
                vm_conf->si_pclk_mult = (-1)*((src_clk_hz/pclk_hz)-1);
        }
    }

    pclk_hz *= vm_conf->tx_pixelrep+1;

    return pclk_hz;
}

int get_pure_lm_mode(avconfig_t *cc, mode_data_t *vm_in, mode_data_t *vm_out, vm_proc_config_t *vm_conf)
{
    int i, diff_lines, diff_v_hz_x100, mindiff_id=0, mindiff_lines=1000, mindiff_v_hz_x100=10000;
    mode_data_t *mode_preset;
    mode_flags valid_lm[] = { MODE_PT, (MODE_L2 | (MODE_L2<<cc->l2_mode)), (MODE_L3_GEN_16_9<<cc->l3_mode), (MODE_L4_GEN_4_3<<cc->l4_mode), (MODE_L5_GEN_4_3<<cc->l5_mode), (MODE_L6_GEN_4_3<<cc->l6_mode) };
    mode_flags target_lm, mindiff_lm;
    uint8_t pt_only = 0;
    uint8_t upsample2x = cc->upsample2x;

    // one for each video_group
    uint8_t* group_ptr[] = { &pt_only, &cc->pm_240p, &cc->pm_240p, &cc->pm_384p, &cc->pm_480i, &cc->pm_480i, &cc->pm_480p, &cc->pm_480p, &pt_only, &cc->pm_1080i, &pt_only };

    for (i=0; i<num_video_modes_plm; i++) {
        mode_preset = &video_modes_plm[i];

        switch (mode_preset->group) {
            case GROUP_384P:
                //fixed Line2x/3x mode for 240x360p/400p
                valid_lm[2] = MODE_L3_GEN_16_9;
                valid_lm[3] = MODE_L2_240x360;
                valid_lm[4] = MODE_L3_240x360;
                if ((!vm_in->timings.h_total) && (mode_preset->timings.v_total == 449)) {
                    if ((video_modes_plm_default[i].timings.h_active == 720) && (video_modes_plm_default[i].timings.v_active == 400)) {
                        if (cc->s400p_mode == 0)
                            continue;
                    } else if ((video_modes_plm_default[i].timings.h_active == 640) && (video_modes_plm_default[i].timings.v_active == 400)) {
                        if (cc->s400p_mode == 1)
                            continue;
                    }
                }
                break;
            case GROUP_480I:
            case GROUP_576I:
                //fixed Line3x/4x mode for 480i
                valid_lm[2] = MODE_L3_GEN_16_9;
                valid_lm[3] = MODE_L4_GEN_4_3;
                break;
            case GROUP_480P:
                 if (mode_preset->vic == HDMI_480p60) {
                    switch (cc->s480p_mode) {
                        case 0: // Auto
                            if (vm_in->timings.h_synclen > 82)
                                continue;
                            break;
                        case 1: // DTV 480p
                            break;
                        default:
                            continue;
                    }
                } else if (mode_preset->vic == HDMI_640x480p60) {
                    switch (cc->s480p_mode) {
                        case 0: // Auto
                        case 2: // VESA 640x480@60
                            break;
                        default:
                            continue;
                    }
                }
                break;
            default:
                break;
        }

        target_lm = valid_lm[*group_ptr[mode_preset->group]];

        if ((target_lm & mode_preset->flags) &&
            (vm_in->timings.interlaced == mode_preset->timings.interlaced))
        {
            diff_lines = abs(vm_in->timings.v_total - mode_preset->timings.v_total);
            diff_v_hz_x100 = abs(vm_in->timings.v_hz_x100 - mode_preset->timings.v_hz_x100);

            if ((diff_lines < mindiff_lines) || ((mode_preset->group >= GROUP_720P) && (diff_lines == mindiff_lines) && (diff_v_hz_x100 < mindiff_v_hz_x100))) {
                mindiff_id = i;
                mindiff_lines = diff_lines;
                mindiff_v_hz_x100 = diff_v_hz_x100;
                mindiff_lm = target_lm;
            } else if ((mindiff_lines <= 2) && (diff_lines > mindiff_lines)) {
                // Break out if suitable mode already found
                break;
            }
        }
    }

    if (mindiff_lines >= 130)
        return -1;

    mode_preset = &video_modes_plm[mindiff_id];

    vm_in->timings.h_active = mode_preset->timings.h_active;
    vm_in->timings.v_active = mode_preset->timings.v_active;
    vm_in->timings.h_synclen = mode_preset->timings.h_synclen;
    vm_in->timings.v_synclen = mode_preset->timings.v_synclen;
    vm_in->timings.h_backporch = mode_preset->timings.h_backporch;
    vm_in->timings.v_backporch = mode_preset->timings.v_backporch;
    vm_in->timings.h_total = mode_preset->timings.h_total;
    vm_in->timings.h_total_adj = mode_preset->timings.h_total_adj;
    vm_in->sampler_phase = mode_preset->sampler_phase;
    vm_in->mask.h = mode_preset->mask.h;
    vm_in->mask.v = mode_preset->mask.v;
    vm_in->type = mode_preset->type;
    vm_in->group = mode_preset->group;
    vm_in->vic = mode_preset->vic;
    strncpy(vm_in->name, mode_preset->name, 10);

    memcpy(vm_out, vm_in, sizeof(mode_data_t));
    vm_out->vic = HDMI_Unknown;

    memset(vm_conf, 0, sizeof(vm_proc_config_t));
    vm_conf->si_pclk_mult = 1;

    mindiff_lm &= mode_preset->flags;    //ensure L2 mode uniqueness

    if (mindiff_lm >= MODE_L6_GEN_4_3)
        vm_conf->y_rpt = 5;
    else if (mindiff_lm >= MODE_L5_GEN_4_3)
        vm_conf->y_rpt = 4;
    else if (mindiff_lm >= MODE_L4_GEN_4_3)
        vm_conf->y_rpt = 3;
    else if (mindiff_lm >= MODE_L3_GEN_16_9)
        vm_conf->y_rpt = 2;
    else if (mindiff_lm >= MODE_L2)
        vm_conf->y_rpt = 1;

    switch (mindiff_lm) {
        case MODE_PT:
            vm_out->vic = vm_in->vic;

            // multiply horizontal resolution if necessary to fulfill min. 25MHz TMDS clock requirement. Tweak infoframe pixel repetition indicator later to make sink treat it as original resolution.
            while ((((vm_out->timings.v_hz_x100*vm_out->timings.v_total)/100)*vm_out->timings.h_total*(vm_conf->h_skip+1))>>vm_out->timings.interlaced < 25000000UL) {
                vm_conf->x_rpt = vm_conf->h_skip = 2*(vm_conf->h_skip+1)-1;
            }
            vmode_hv_mult(vm_out, VM_OUT_XMULT, VM_OUT_YMULT);
            break;
        case MODE_L2:
            // Upsample / pixel-repeat horizontal resolution of 384p/480p/960i modes
            if ((mode_preset->group == GROUP_384P) || (mode_preset->group == GROUP_480P) || (mode_preset->group == GROUP_576P) || ((mode_preset->group == GROUP_1080I) && (mode_preset->timings.h_total < 1200))) {
                if (upsample2x) {
                    vmode_hv_mult(vm_in, 2, 1);
                    vmode_hv_mult(vm_out, 2, VM_OUT_YMULT);
                } else {
                    vm_conf->x_rpt = 1;
                    vmode_hv_mult(vm_out, VM_OUT_XMULT, VM_OUT_YMULT);
                }
            } else {
                vmode_hv_mult(vm_out, VM_OUT_XMULT, VM_OUT_YMULT);
            }
            break;
        case MODE_L3_GEN_16_9:
            // Upsample / pixel-repeat horizontal resolution of 480i mode
            if ((mode_preset->group == GROUP_480I) || (mode_preset->group == GROUP_576I)) {
                if (upsample2x) {
                    vmode_hv_mult(vm_in, 2, 1);
                    vmode_hv_mult(vm_out, 2, VM_OUT_YMULT);
                } else {
                    vm_conf->x_rpt = 1;
                    vmode_hv_mult(vm_out, VM_OUT_XMULT, VM_OUT_YMULT);
                }
            } else {
                vmode_hv_mult(vm_out, VM_OUT_XMULT, VM_OUT_YMULT);
            }
            break;
        case MODE_L3_GEN_4_3:
            vm_conf->x_size = vm_out->timings.h_active-2*vm_in->mask.h;
            vm_out->timings.h_synclen /= 3;
            vm_out->timings.h_backporch /= 3;
            vm_out->timings.h_active /= 3;
            vm_out->timings.h_total /= 3;
            vm_out->timings.h_total_adj = 0;
            vmode_hv_mult(vm_out, 4, VM_OUT_YMULT);
            break;
        case MODE_L4_GEN_4_3:
            // Upsample / pixel-repeat horizontal resolution of 480i mode
            if ((mode_preset->group == GROUP_480I) || (mode_preset->group == GROUP_576I)) {
                if (upsample2x) {
                    vmode_hv_mult(vm_in, 2, 1);
                    vmode_hv_mult(vm_out, 2, VM_OUT_YMULT);
                } else {
                    vm_conf->x_rpt = 1;
                    vmode_hv_mult(vm_out, VM_OUT_XMULT, VM_OUT_YMULT);
                }
            } else {
                vmode_hv_mult(vm_out, VM_OUT_XMULT, VM_OUT_YMULT);
            }
            break;
        case MODE_L5_GEN_4_3:
        case MODE_L6_GEN_4_3:
            vmode_hv_mult(vm_out, VM_OUT_XMULT, VM_OUT_YMULT);
            break;
        case MODE_L2_512_COL:
        case MODE_L2_384_COL:
        case MODE_L2_320_COL:
        case MODE_L3_512_COL:
        case MODE_L4_512_COL:
        case MODE_L6_512_COL:
            vm_conf->x_rpt = vm_conf->h_skip = 1;
            vmode_hv_mult(vm_out, VM_OUT_XMULT, VM_OUT_YMULT);
            break;
        case MODE_L2_256_COL:
        case MODE_L3_384_COL:
        case MODE_L4_384_COL:
        case MODE_L5_512_COL:
        case MODE_L6_384_COL:
        case MODE_L6_320_COL:
            vm_conf->x_rpt = vm_conf->h_skip = 2;
            vmode_hv_mult(vm_out, VM_OUT_XMULT, VM_OUT_YMULT);
            break;
        case MODE_L3_320_COL:
        case MODE_L4_320_COL:
        case MODE_L5_384_COL:
        case MODE_L6_256_COL:
            vm_conf->x_rpt = vm_conf->h_skip = 3;
            vmode_hv_mult(vm_out, VM_OUT_XMULT, VM_OUT_YMULT);
            break;
        case MODE_L2_240x360:
        case MODE_L3_256_COL:
        case MODE_L4_256_COL:
        case MODE_L5_320_COL:
            vm_conf->x_rpt = vm_conf->h_skip = 4;
            vmode_hv_mult(vm_out, VM_OUT_XMULT, VM_OUT_YMULT);
            break;
        case MODE_L5_256_COL:
            vm_conf->x_rpt = vm_conf->h_skip = 5;
            vmode_hv_mult(vm_out, VM_OUT_XMULT, VM_OUT_YMULT);
            break;
        case MODE_L3_240x360:
            vm_conf->x_rpt = vm_conf->h_skip = 7;
            vmode_hv_mult(vm_out, VM_OUT_XMULT, VM_OUT_YMULT);
            break;
        default:
            printf("WARNING: invalid mindiff_lm\n");
            return -1;
            break;
    }

    // Set clock multiplication factor
    if (mindiff_lm == MODE_L3_GEN_4_3)
        vm_conf->si_pclk_mult = 4;
    else
        vm_conf->si_pclk_mult = VM_OUT_PCLKMULT;

    // Reduce x_rpt for 1:1 PAR 256col mode
    if (mindiff_lm & (MODE_L2_256_COL|MODE_L4_256_COL|MODE_L5_256_COL))
        vm_conf->x_rpt -= cc->ar_256col;
    else if (mindiff_lm & (MODE_L3_256_COL|MODE_L6_256_COL))
        vm_conf->x_rpt = cc->ar_256col ? 2 : 3;

    if (mindiff_lm & (MODE_L3_320_COL|MODE_L2_240x360))
        vm_conf->x_rpt--;
    else if (mindiff_lm & MODE_L3_240x360)
        vm_conf->x_rpt -= 2;

    if (mindiff_lm == MODE_L2_240x360) {
        vm_out->timings.h_active += 80;
        vm_out->timings.h_backporch -= 40;
    }

    // Force TX pixel-repeat for high bandwidth modes
    if (((mindiff_lm == MODE_L5_GEN_4_3) && (mode_preset->group == GROUP_288P)) || (mindiff_lm >= MODE_L6_GEN_4_3))
        vm_conf->tx_pixelrep = 1;

    sniprintf(vm_out->name, 11, "%s x%u", vm_in->name, vm_conf->y_rpt+1);

    if (vm_conf->x_size == 0)
        vm_conf->x_size = (vm_in->timings.h_active-2*vm_in->mask.h)*(vm_conf->x_rpt+1);
    if (vm_conf->y_size == 0)
        vm_conf->y_size = vm_out->timings.v_active-2*vm_in->mask.v*(vm_conf->y_rpt+1);

    vm_conf->x_offset = ((vm_out->timings.h_active-vm_conf->x_size)/2);
    vm_conf->x_start_lb = vm_in->mask.h;
    vm_conf->y_offset = ((vm_out->timings.v_active-vm_conf->y_size)/2);

    // Line5x format
    if ((vm_conf->y_rpt == 4) && !((mindiff_lm == MODE_L5_GEN_4_3) && (mode_preset->group == GROUP_288P))) {
        // adjust output width to 1920
        if (cc->l5_fmt != 1) {
            vm_conf->x_offset = (1920-vm_conf->x_size)/2;
            vm_out->timings.h_synclen = (vm_out->timings.h_total - 1920)/4;
            vm_out->timings.h_backporch = (vm_out->timings.h_total - 1920)/2;
            vm_out->timings.h_active = 1920;
        }

        // adjust output height to 1080
        if (cc->l5_fmt == 0) {
            vm_conf->y_start_lb = (vm_out->timings.v_active-1080)/10;
            vm_out->timings.v_backporch += 5*vm_conf->y_start_lb;
            vm_out->timings.v_active = 1080;
            vm_conf->y_size = vm_out->timings.v_active-2*vm_in->mask.v*(vm_conf->y_rpt+1);
        }
    }

    // Aspect
    if (vm_out->type & VIDEO_HDTV) {
        vm_out->ar.h = 16;
        vm_out->ar.v = 9;
    } else {
        vm_out->ar.h = 4;
        vm_out->ar.v = 3;
    }

#ifdef LM_EMIF_EXTRA_DELAY
    vm_conf->framesync_line = ((vm_out->timings.v_total>>vm_out->timings.interlaced)-(1+vm_out->timings.interlaced)*(vm_conf->y_rpt+1));
#else
    //vm_conf->framesync_line = vm_in->timings.interlaced ? ((vm_out->timings.v_total>>vm_out->timings.interlaced)-(vm_conf->y_rpt+1)) : 0;
    vm_conf->framesync_line = 0;
#endif
    vm_conf->framelock = 1;

    if (vm_out->vic == HDMI_Unknown)
        vm_out->vic = cc->default_vic;

    return mindiff_id;
}

int get_vmode(vmode_t vmode_id, mode_data_t *vm_in, mode_data_t *vm_out, vm_proc_config_t *vm_conf)
{
    memset(vm_conf, 0, sizeof(vm_proc_config_t));
    memset(vm_in, 0, sizeof(mode_data_t));
    memcpy(vm_out, &video_modes_plm_default[vmode_id], sizeof(mode_data_t));
    vm_out->ar.h = 4;
    vm_out->ar.v = 3;

    return 0;
}
