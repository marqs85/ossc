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

#include <string.h>
#include "system.h"
#include "avconfig.h"
#include "av_controller.h"
#include "userdata.h"
#include "altera_avalon_pio_regs.h"
#include "tvp7002.h"

#define DEFAULT_ON              1

extern avmode_t cm;
extern alt_u8 update_cur_vm;
extern uint8_t sd_profile_sel_menu;

// Target configuration
avconfig_t tc;

// Default configuration
const avconfig_t tc_default = {
    .pm_240p = 1,
    .pm_384p = 1,
    .pm_480i = 1,
    .pm_1080i = 1,
    .l3_mode = 1,
    .clamp_offset = SIGNED_NUMVAL_ZERO,
    .sl_altern = 1,
    .sync_vth = DEFAULT_SYNC_VTH,
    .linelen_tol = DEFAULT_LINELEN_TOL,
    .vsync_thold = DEFAULT_VSYNC_THOLD,
    .pre_coast = DEFAULT_PRE_COAST,
    .post_coast = DEFAULT_POST_COAST,
    .adc_pll_bw = 1,
    .sync_lpf = DEFAULT_SYNC_LPF,
    .alc_h_filter = DEFAULT_ALC_H_FILTER,
    .alc_v_filter = DEFAULT_ALC_V_FILTER,
    .col = {
        .r_f_gain = DEFAULT_FINE_GAIN,
        .g_f_gain = DEFAULT_FINE_GAIN,
        .b_f_gain = DEFAULT_FINE_GAIN,
        .r_f_off = DEFAULT_FINE_OFFSET,
        .g_f_off = DEFAULT_FINE_OFFSET,
        .b_f_off = DEFAULT_FINE_OFFSET,
        .c_gain = DEFAULT_COARSE_GAIN,
    },
    .mask_br = 8,
    .shmask_str = 15,
    .audio_dw_sampl = DEFAULT_ON,
    .audio_gain = AUDIO_GAIN_0DB,
    .link_av = AV_LAST,
};

int set_default_profile(int update_cc)
{
    memcpy(&tc, &tc_default, sizeof(avconfig_t));

    tc.tx_mode = (IORD_ALTERA_AVALON_PIO_DATA(PIO_1_BASE) & HDMITX_MODE_MASK) ? TX_DVI : TX_HDMI_RGB;

    if (update_cc)
        memcpy(&cm.cc, &tc, sizeof(avconfig_t));

    set_default_c_shmask();
    set_default_c_lc_palette_set();
    set_default_vm_table();
    update_cur_vm = 1;

    return 0;
}

int reset_profile() {
    set_default_profile(0);

    return 0;
}

int load_profile_sd() {
    return read_userdata_sd(sd_profile_sel_menu, 0);
}

int save_profile_sd() {
    int retval;

    retval = write_userdata_sd(sd_profile_sel_menu);
    if (retval == 0)
        write_userdata_sd(SD_INIT_CONFIG_SLOT);

    return retval;
}
