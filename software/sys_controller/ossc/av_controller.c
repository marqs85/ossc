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

#include <stdio.h>
#include <unistd.h>
#include "system.h"
#include "string.h"
#include "altera_avalon_pio_regs.h"
#include "i2c_opencores.h"
#include "av_controller.h"
#include "tvp7002.h"
#include "ths7353.h"
#include "pcm1862.h"
#include "video_modes.h"
#include "lcd.h"
#include "flash.h"
#include "sdcard.h"
#include "menu.h"
#include "avconfig.h"
#include "sysconfig.h"
#include "firmware.h"
#include "userdata.h"
#include "it6613.h"
#include "it6613_sys.h"
#include "HDMI_TX.h"
#include "hdmitx.h"
#include "sd_io.h"
#include "sys/alt_timestamp.h"

#define STABLE_THOLD            1
#define MIN_LINES_PROGRESSIVE   200
#define MIN_LINES_INTERLACED    400
#define SYNC_LOCK_THOLD         3
#define SYNC_LOSS_THOLD         -5
#define STATUS_TIMEOUT          100000

alt_u16 sys_ctrl;

// Current mode
avmode_t cm;

extern mode_data_t video_modes[];
extern ypbpr_to_rgb_csc_t csc_coeffs[];
extern alt_u16 rc_keymap[REMOTE_MAX_KEYS];
extern alt_u16 rc_keymap_default[REMOTE_MAX_KEYS];
extern alt_u32 remote_code;
extern alt_u32 btn_code, btn_code_prev;
extern alt_u8 remote_rpt, remote_rpt_prev;
extern avconfig_t tc, tc_default;
extern alt_u8 vm_sel;

alt_u8 target_typemask;
alt_u8 target_type;
alt_u8 stable_frames;
alt_u8 update_cur_vm;

alt_u8 profile_sel, profile_sel_menu, input_profiles[AV_LAST], lt_sel, def_input, profile_link, lcd_bl_timeout;
alt_u8 osd_enable, osd_enable_pre=1, osd_status_timeout, osd_status_timeout_pre=1;
alt_u8 auto_input, auto_av1_ypbpr, auto_av2_ypbpr = 1, auto_av3_ypbpr;

char row1[LCD_ROW_LEN+1], row2[LCD_ROW_LEN+1], menu_row1[LCD_ROW_LEN+1], menu_row2[LCD_ROW_LEN+1];

extern alt_u8 menu_active;
avinput_t target_input;

alt_u8 pcm1862_active;

alt_u32 pclk_out;
alt_u32 read_it2(alt_u32 regaddr);

// Manually (see cyiv-51005.pdf) or automatically (MIF/HEX from PLL megafunction) generated config may not
// provide fully correct scan chain data (e.g. mismatches in C3) and lead to incorrect PLL configuration.
// To get correct scan chain data, do the following:
//   1. Create a ALTPLL_RECONFIG instance with initial value read from your MIF/HEX file
//   2. Connect ALTPLL_RECONFIG to your PLL and set its reconfig input to something you can control easily (e.g. button)
//   3. Create a signaltap file and add all PLL signals to capture. Set sample depth to 256 and clock to scanclk
//   4. Compile the design and program the FPGA
//   5. Open signaltap and set trigger to scanclkena rising edge
//   6. Run signaltap and trigger PLL reconfiguration
//   7. Export VCD file for analysis
//   8. Compare your MIF/HEX to the captured scan chain and update it accordingly
//   9. Dump the updated scan chain data to an array like below (last 16 bits are 0)
//  10. PLL can be then reconfigured with custom pll_reconfig as shown in program_mode()
const pll_config_t pll_configs[] = { {{0x0d806000, 0x00402010, 0x08040220, 0x00004022, 0x00000000}},    // 1x, 1x (default)
                                     {{0x0dc06000, 0x00783c11, 0x070180e0, 0x0000180e, 0x00000000}},    // 2x, 5x
                                     {{0x0d806000, 0x00301804, 0x02014060, 0x00001406, 0x00000000}} };  // 3x, 4x

volatile sc_regs *sc = (volatile sc_regs*)SC_CONFIG_0_BASE;
volatile osd_regs *osd = (volatile osd_regs*)OSD_GENERATOR_0_BASE;
volatile pll_reconfig_regs *pll_reconfig = (volatile pll_reconfig_regs*)PLL_RECONFIG_0_BASE;

inline void lcd_write_menu()
{
    strncpy((char*)osd->osd_chars.row1, menu_row1, LCD_ROW_LEN);
    strncpy((char*)osd->osd_chars.row2, menu_row2, LCD_ROW_LEN);
    lcd_write((char*)&menu_row1, (char*)&menu_row2);
}

inline void lcd_write_status() {
    strncpy((char*)osd->osd_chars.row1, row1, LCD_ROW_LEN);
    strncpy((char*)osd->osd_chars.row2, row2, LCD_ROW_LEN);
    lcd_write((char*)&row1, (char*)&row2);
}

#ifdef ENABLE_AUDIO
inline void SetupAudio(tx_mode_t mode)
{
    // shut down audio-tx before setting new config (recommended for changing audio-tx config)
    DisableAudioOutput();
    EnableAudioInfoFrame(FALSE, NULL);

    if (mode != TX_DVI) {
        EnableAudioOutput4OSSC(pclk_out, tc.audio_dw_sampl, tc.audio_swap_lr);
        HDMITX_SetAudioInfoFrame((BYTE)tc.audio_dw_sampl);
#ifdef DEBUG
        Switch_HDMITX_Bank(1);
        usleep(1000);
        alt_u32 cts = 0;
        cts |= read_it2(0x35) >> 4;
        cts |= read_it2(0x36) << 4;
        cts |= read_it2(0x37) << 12;
        printf("CTS: %lu\n", cts);
#endif
    }
}
#endif

inline void TX_enable(tx_mode_t mode)
{
    // shut down TX before setting new config
    SetAVMute(TRUE);
    DisableVideoOutput();
    EnableAVIInfoFrame(FALSE, NULL);

    //Setup TX configuration
    //TODO: set pclk target and VIC dynamically
    EnableVideoOutput(cm.hdmitx_pclk_level ? PCLK_HIGH : PCLK_MEDIUM, COLOR_RGB444, (mode == TX_HDMI_YCBCR444) ? COLOR_YUV444 : COLOR_RGB444, (mode != TX_DVI));

    if (mode != TX_DVI) {
        HDMITX_SetAVIInfoFrame(cm.hdmitx_vic, (mode == TX_HDMI_RGB) ? F_MODE_RGB444 : F_MODE_YUV444, 0, 0, tc.hdmi_itc, cm.hdmitx_pixr_ifr);
        cm.cc.hdmi_itc = tc.hdmi_itc;
    }

#ifdef ENABLE_AUDIO
    SetupAudio(mode);
#endif

    // start TX
    SetAVMute(FALSE);
}

void pll_reconfigure(alt_u8 id)
{
    if ((id < sizeof(pll_configs)/sizeof(pll_config_t)) && (id != pll_reconfig->pll_config_status.c_config_id)) {
        memcpy((void*)pll_reconfig->pll_config_data.data, pll_configs[id].data, sizeof(pll_config_t));
        pll_reconfig->pll_config_status.t_config_id = id;

        printf("Reconfiguring PLL to config %u\n", id);

        // Try switching to fixed reference clock as otherwise reconfig may hang or corrupt configuration
        if (cm.avinput != AV_TESTPAT) {
            sys_ctrl &= ~VIDGEN_OFF;
            IOWR_ALTERA_AVALON_PIO_DATA(PIO_0_BASE, sys_ctrl);
            usleep(10);
        }

        // Do not reconfigure if clock switch failed
        if ((IORD_ALTERA_AVALON_PIO_DATA(PIO_1_BASE) & PLL_ACTIVECLK_MASK) == 0) {
            // reset state machine if previous reconfigure hanged (should not occur with stable refclk)
            if (pll_reconfig->pll_config_status.busy) {
                pll_reconfig->pll_config_status.reset = 1;
                usleep(1);
            }

            pll_reconfig->pll_config_status.reset = 0;
            pll_reconfig->pll_config_status.update = 1;
            usleep(10);
        }

        if (cm.avinput != AV_TESTPAT) {
            sys_ctrl |= VIDGEN_OFF;
            IOWR_ALTERA_AVALON_PIO_DATA(PIO_0_BASE, sys_ctrl);
        }
    }
}

void set_lpf(alt_u8 lpf)
{
    alt_u32 pclk;
    pclk = (TVP_EXTCLK_HZ/cm.clkcnt)*video_modes[cm.id].h_total;
    printf("PCLK_in: %luHz\n", pclk);

    //Auto
    if (lpf == 0) {
        switch (target_type) {
        case VIDEO_PC:
            tvp_set_lpf((pclk < 30000000) ? 0x0F : 0);
            ths_set_lpf(THS_LPF_BYPASS);
            break;
        case VIDEO_HDTV:
            tvp_set_lpf(0);
            ths_set_lpf((pclk < 80000000) ? THS_LPF_35MHZ : THS_LPF_BYPASS);
            break;
        case VIDEO_EDTV:
            tvp_set_lpf(0);
            ths_set_lpf(THS_LPF_16MHZ);
            break;
        case VIDEO_SDTV:
        case VIDEO_LDTV:
        default:
            tvp_set_lpf(0);
            ths_set_lpf(THS_LPF_9MHZ);
            break;
        }
    } else {
        tvp_set_lpf((tc.video_lpf == 2) ? 0x0F : 0);
        ths_set_lpf((tc.video_lpf > 2) ? (VIDEO_LPF_MAX-tc.video_lpf) : THS_LPF_BYPASS);
    }
}

void set_csc(alt_u8 csc)
{
    if (csc > 1) {
        if (target_type == VIDEO_HDTV)
            csc = 1;
        else
            csc = 0;
    }

    tvp_sel_csc(&csc_coeffs[csc]);
}

inline int check_linecnt(alt_u8 progressive, alt_u32 totlines) {
    if (progressive)
        return (totlines >= MIN_LINES_PROGRESSIVE);
    else
        return (totlines >= MIN_LINES_INTERLACED);
}

// Check if input video status / target configuration has changed
status_t get_status(tvp_sync_input_t syncinput)
{
    alt_u32 data1, data2;
    alt_u32 totlines, clkcnt;
    alt_u8 progressive;
    //alt_u8 refclk;
    alt_u8 sync_active;
    alt_u8 vsyncmode;
    alt_u16 totlines_tvp;
    alt_u16 h_samplerate;
    status_t status;
    static alt_8 act_ctr;
    alt_u32 ctr;
    int valid_linecnt;
    alt_u8 h_mult;

    status = NO_CHANGE;

    // Wait until vsync active (avoid noise coupled to I2C bus on earlier prototypes)
    for (ctr=0; ctr<STATUS_TIMEOUT; ctr++) {
        if (sc->sc_status.vsync_flag) {
            //printf("ctrval %u\n", ctr);
            break;
        }
    }

    sync_active = tvp_check_sync(syncinput);
    vsyncmode = cm.sync_active ? sc->sc_status.fpga_vsyncgen : 0;

    // Read sync information from TVP7002 status registers
    data1 = tvp_readreg(TVP_LINECNT1);
    data2 = tvp_readreg(TVP_LINECNT2);
    totlines = ((data2 & 0x0f) << 8) | data1;
    progressive = !!(data2 & (1<<5));
    cm.macrovis = !!(data2 & (1<<6));
    data1 = tvp_readreg(TVP_CLKCNT1);
    data2 = tvp_readreg(TVP_CLKCNT2);
    clkcnt = ((data2 & 0x0f) << 8) | data1;

    // Read how many lines TVP7002 outputs in reality (valid only if output enabled)
    totlines_tvp = sc->sc_status.vmax_tvp+1;

    // NOTE: "progressive" may not have correct value if H-PLL is not locked (!cm.sync_active)
    if ((vsyncmode == 0x2) || (!cm.sync_active && (totlines < MIN_LINES_INTERLACED))) {
        progressive = 1;
    } else if (vsyncmode == 0x1) {
        progressive = 0;
        totlines = totlines_tvp; //compensate skipped vsync
    }

    valid_linecnt = check_linecnt(progressive, totlines);

    // TVP7002 may randomly report "no sync" (especially with arcade boards),
    // thus disable output only after N consecutive "no sync"-events
    if (!cm.sync_active && sync_active && valid_linecnt) {
        printf("Sync up in %d...\n", SYNC_LOCK_THOLD-act_ctr);
        if (act_ctr >= SYNC_LOCK_THOLD) {
            act_ctr = 0;
            cm.sync_active = 1;
            status = ACTIVITY_CHANGE;
        } else {
            act_ctr++;
        }
    } else if (cm.sync_active && (!sync_active || !valid_linecnt)) {
        printf("Sync down in %d...\n", act_ctr-SYNC_LOSS_THOLD);
        if (act_ctr <= SYNC_LOSS_THOLD) {
            act_ctr = 0;
            cm.sync_active = 0;
            status = ACTIVITY_CHANGE;
        } else {
            act_ctr--;
        }
    } else {
        act_ctr = 0;
    }

    if (valid_linecnt) {
        // Line count reported in TVP7002 status registers is sometimes +-1 line off and may alternate with correct value. Ignore these events
        if ((totlines > cm.totlines+1) || (totlines+1 < cm.totlines) || (clkcnt != cm.clkcnt) || (progressive != cm.progressive)) {
            printf("totlines: %lu (cur) / %lu (prev), clkcnt: %lu (cur) / %lu (prev). totlines_tvp: %u, VSM: %u\n", totlines, cm.totlines, clkcnt, cm.clkcnt, totlines_tvp, vsyncmode);
            /*if (!cm.sync_active)
                act_ctr = 0;*/
            stable_frames = 0;
        } else if (stable_frames != STABLE_THOLD) {
            stable_frames++;
            if (stable_frames == STABLE_THOLD)
                status = (status < MODE_CHANGE) ? MODE_CHANGE : status;
        }

        if ((tc.pm_240p != cm.cc.pm_240p) ||
            (tc.pm_384p != cm.cc.pm_384p) ||
            (tc.pm_480i != cm.cc.pm_480i) ||
            (tc.pm_480p != cm.cc.pm_480p) ||
            (tc.pm_1080i != cm.cc.pm_1080i) ||
            (tc.l2_mode != cm.cc.l2_mode) ||
            (tc.l3_mode != cm.cc.l3_mode) ||
            (tc.l4_mode != cm.cc.l4_mode) ||
            (tc.l5_mode != cm.cc.l5_mode) ||
            (tc.l5_fmt != cm.cc.l5_fmt) ||
            (tc.tvp_hpll2x != cm.cc.tvp_hpll2x) ||
            (tc.upsample2x != cm.cc.upsample2x) ||
            (tc.vga_ilace_fix != cm.cc.vga_ilace_fix) ||
            (tc.default_vic != cm.cc.default_vic) ||
            (tc.clamp_offset != cm.cc.clamp_offset))
            status = (status < MODE_CHANGE) ? MODE_CHANGE : status;

        if ((tc.s480p_mode != cm.cc.s480p_mode) && (video_modes[cm.id].v_total == 525))
            status = (status < MODE_CHANGE) ? MODE_CHANGE : status;

        if ((tc.s400p_mode != cm.cc.s400p_mode) && (video_modes[cm.id].v_total == 449))
            status = (status < MODE_CHANGE) ? MODE_CHANGE : status;

        if (cm.pll_config != pll_reconfig->pll_config_status.c_config_id)
            status = (status < MODE_CHANGE) ? MODE_CHANGE : status;

        if (update_cur_vm) {
            cm.h_mult_total = (video_modes[cm.id].h_total*cm.sample_mult) + ((cm.sample_mult*video_modes[cm.id].h_total_adj*5 + 50) / 100);
            tvp_setup_hpll(cm.h_mult_total, clkcnt, cm.cc.tvp_hpll2x && (video_modes[cm.id].flags & MODE_PLLDIVBY2));
            cm.sample_sel = tvp_set_hpll_phase(video_modes[cm.id].sampler_phase, cm.sample_mult);
            status = (status < SC_CONFIG_CHANGE) ? SC_CONFIG_CHANGE : status;
        }

        cm.totlines = totlines;
        cm.clkcnt = clkcnt;
        cm.progressive = progressive;
    }

    if ((tc.sl_mode != cm.cc.sl_mode) ||
        (tc.sl_type != cm.cc.sl_type) ||
        (tc.sl_hybr_str != cm.cc.sl_hybr_str) ||
        (tc.sl_method != cm.cc.sl_method) ||
        (tc.sl_str != cm.cc.sl_str) ||
        memcmp(tc.sl_cust_l_str, cm.cc.sl_cust_l_str, 5) ||
        memcmp(tc.sl_cust_c_str, cm.cc.sl_cust_c_str, 6) ||
        (tc.sl_altern != cm.cc.sl_altern) ||
        (tc.sl_altiv != cm.cc.sl_altiv) ||
        (tc.sl_id != cm.cc.sl_id) ||
        (tc.h_mask != cm.cc.h_mask) ||
        (tc.v_mask != cm.cc.v_mask) ||
        (tc.mask_br != cm.cc.mask_br) ||
        (tc.mask_color != cm.cc.mask_color) ||
        (tc.ar_256col != cm.cc.ar_256col) ||
        (tc.reverse_lpf != cm.cc.reverse_lpf) ||
        (tc.panasonic_hack != cm.cc.panasonic_hack))
        status = (status < SC_CONFIG_CHANGE) ? SC_CONFIG_CHANGE : status;

    if (tc.sync_vth != cm.cc.sync_vth)
        tvp_set_sog_thold(tc.sync_vth);

    if (tc.linelen_tol != cm.cc.linelen_tol)
        tvp_set_linelen_tol(tc.linelen_tol);

    if (tc.vsync_thold != cm.cc.vsync_thold)
        tvp_set_ssthold(tc.vsync_thold);

    if ((tc.pre_coast != cm.cc.pre_coast) || (tc.post_coast != cm.cc.post_coast))
        tvp_set_hpllcoast(tc.pre_coast, tc.post_coast);

    if (tc.ypbpr_cs != cm.cc.ypbpr_cs)
        set_csc(tc.ypbpr_cs);

    if (tc.video_lpf != cm.cc.video_lpf)
        set_lpf(tc.video_lpf);

    if (tc.sync_lpf != cm.cc.sync_lpf)
        tvp_set_sync_lpf(tc.sync_lpf);

    if (tc.stc_lpf != cm.cc.stc_lpf)
        tvp_set_clp_lpf(tc.stc_lpf);

    if ((tc.alc_h_filter != cm.cc.alc_h_filter) || (tc.alc_v_filter != cm.cc.alc_v_filter))
        tvp_set_alcfilt(tc.alc_v_filter, tc.alc_h_filter);

    if (memcmp(&tc.col, &cm.cc.col, sizeof(color_setup_t)))
        tvp_set_gain_offset(&tc.col);

#ifdef ENABLE_AUDIO
    if ((tc.audio_dw_sampl != cm.cc.audio_dw_sampl) ||
#ifdef MANUAL_CTS
        update_cur_vm ||
#endif
        (tc.audio_swap_lr != cm.cc.audio_swap_lr))
        SetupAudio(tc.tx_mode);

    if (pcm1862_active && (tc.audio_gain != cm.cc.audio_gain))
        pcm_set_gain(tc.audio_gain-AUDIO_GAIN_0DB);
#endif

    cm.cc = tc;
    update_cur_vm = 0;

    return status;
}

void update_sc_config()
{
    h_config_reg h_config = {.data=0x00000000};
    h_config2_reg h_config2 = {.data=0x00000000};
    v_config_reg v_config = {.data=0x00000000};
    misc_config_reg misc_config = {.data=0x00000000};
    sl_config_reg sl_config = {.data=0x00000000};
    sl_config2_reg sl_config2 = {.data=0x00000000};

    alt_u8 sl_no_altern = 0;
    alt_u8 sl_l_overlay = 0, sl_c_overlay = 0;
    alt_u32 sl_l_str_arr = 0, sl_c_str_arr = 0;

    alt_u8 h_opt_scale = cm.sample_mult;
    alt_u16 h_opt_startoffs = 0;
    alt_u16 h_synclen = video_modes[cm.id].h_synclen;
    alt_u16 h_border, h_mask;
    alt_u16 v_active = video_modes[cm.id].v_active;
    alt_u16 v_backporch = video_modes[cm.id].v_backporch;

    int i;

    // construct default scanline overlay
    if ((cm.cc.sl_type == 0) || (cm.cc.sl_type == 2)) {
        if (cm.cc.sl_altiv) {
            sl_l_overlay = 1<<(cm.cc.sl_id);
        } else {
            switch (cm.fpga_vmultmode) {
                case FPGA_V_MULTMODE_3X:
                    sl_l_overlay = 1<<(2*(cm.cc.sl_id));
                    break;
                case FPGA_V_MULTMODE_4X:
                    sl_l_overlay = 3<<(2*(cm.cc.sl_id));
                    break;
                case FPGA_V_MULTMODE_5X:
                    sl_l_overlay = 3<<(3*(cm.cc.sl_id));
                    break;
                default: //1x, 2x
                    sl_l_overlay = 1<<(cm.cc.sl_id);
                    break;
            }
        }
    }
    if ((cm.cc.sl_type == 1) || (cm.cc.sl_type == 2)) {
        if (cm.sample_mult <= 4)
            sl_c_overlay = (1<<((cm.sample_mult-1)*(cm.cc.sl_id)));
        else
            sl_c_overlay = (3<<((cm.sample_mult-2)*(cm.cc.sl_id)));
    }
    // construct custom scanline overlay and strength arrays
    for (i=0; i<5; i++) {
        if (cm.cc.sl_type == 3) {
            sl_l_str_arr |= ((cm.cc.sl_cust_l_str[i]-1)&0xf)<<(4*i);
            sl_l_overlay |= (cm.cc.sl_cust_l_str[i]!=0)<<i;
        } else {
            sl_l_str_arr |= cm.cc.sl_str<<(4*i);
        }
    }
    for (i=0; i<6; i++) {
        if (cm.cc.sl_type == 3) {
            sl_c_str_arr |= ((cm.cc.sl_cust_c_str[i]-1)&0xf)<<(4*i);
            sl_c_overlay |= (cm.cc.sl_cust_c_str[i]!=0)<<i;
        } else {
            sl_c_str_arr |= cm.cc.sl_str<<(4*i);
        }
    }
    // enable/disable alternating scanlines
    if ((video_modes[cm.id].flags & MODE_INTERLACED) && (cm.fpga_vmultmode))
        sl_no_altern = !cm.cc.sl_altern;
    // oevrride scanline mode
    if (cm.cc.sl_mode == 1) {   //auto
        //disable scanlines
        if ((cm.fpga_vmultmode==0) || (video_modes[cm.id].group == GROUP_480P)) {
            sl_l_overlay = 0;
            sl_c_overlay = 0;
        }
    } else if (cm.cc.sl_mode == 0) { //off
        //disable scanlines
        sl_l_overlay = 0;
        sl_c_overlay = 0;
    }

    switch (cm.target_lm) {
        case MODE_L2_240x360:
            h_opt_scale = 4;
            break;
        case MODE_L2_256_COL:
            h_opt_scale = 6-2*cm.cc.ar_256col;
            break;
        case MODE_L3_320_COL:
            h_opt_scale = 3;
            break;
        case MODE_L3_256_COL:
            h_opt_scale = 4-cm.cc.ar_256col;
            break;
        case MODE_L3_240x360:
            h_opt_scale = 6;
            break;
        case MODE_L4_256_COL:
            h_opt_scale = 5-cm.cc.ar_256col;
            break;
        case MODE_L5_256_COL:
            h_opt_scale = 6-cm.cc.ar_256col;
            break;
        default:
            break;
    }

    if (cm.target_lm >= MODE_L5_GEN_4_3 && cm.cc.l5_fmt == L5FMT_1920x1080) {
        v_active -= 24;
        v_backporch += 12;
    }

    // CEA-770.3 HDTV modes use tri-level syncs which have twice the width of bi-level syncs of corresponding CEA-861 modes
    if (target_type == VIDEO_HDTV)
        h_synclen *= 2;

    // 1920x* modes need short hsync
    if (h_synclen > cm.hsync_cut)
        h_synclen -= cm.hsync_cut;
    else
        h_synclen = 1;

    h_border = (((cm.sample_mult-h_opt_scale)*video_modes[cm.id].h_active)/2);
    h_mask = h_border + h_opt_scale*cm.cc.h_mask;
    h_opt_startoffs = h_border + (cm.sample_mult-h_opt_scale)*(h_synclen+(alt_u16)video_modes[cm.id].h_backporch);
    printf("h_border: %u, h_opt_startoffs: %u\n", h_border, h_opt_startoffs);

    h_config.h_multmode = cm.fpga_hmultmode;
    h_config.h_l5fmt = (cm.cc.l5_fmt!=L5FMT_1600x1200);
    h_config.h_l3_240x360 = (cm.target_lm==MODE_L3_240x360);
    h_config.h_synclen = (cm.sample_mult*h_synclen);
    h_config.h_backporch = (cm.sample_mult*(alt_u16)video_modes[cm.id].h_backporch);
    h_config.h_active = (cm.sample_mult*video_modes[cm.id].h_active);

    h_config2.h_mask = h_mask;
    h_config2.h_opt_scale = h_opt_scale;
    h_config2.h_opt_sample_sel = cm.sample_sel;
    h_config2.h_opt_sample_mult = cm.sample_mult;
    h_config2.h_opt_startoff = h_opt_startoffs;

    v_config.v_multmode = cm.fpga_vmultmode;
    v_config.v_mask = cm.cc.v_mask;
    v_config.v_synclen = video_modes[cm.id].v_synclen;
    v_config.v_backporch = v_backporch;
    v_config.v_active = v_active;

    misc_config.rev_lpf_str = cm.cc.reverse_lpf;
    misc_config.mask_br = cm.cc.mask_br;
    misc_config.mask_color = cm.cc.mask_color;
    misc_config.panasonic_hack = cm.cc.panasonic_hack;

    sl_config.sl_l_str_arr = sl_l_str_arr;
    sl_config.sl_l_overlay = sl_l_overlay;
    sl_config.sl_hybr_str = cm.cc.sl_hybr_str;
    sl_config.sl_method = cm.cc.sl_method;
    sl_config.sl_no_altern = sl_no_altern;

    sl_config2.sl_c_str_arr = sl_c_str_arr;
    sl_config2.sl_c_overlay = sl_c_overlay;
    sl_config2.sl_altiv = cm.cc.sl_altiv;

    sc->h_config = h_config;
    sc->h_config2 = h_config2;
    sc->v_config = v_config;
    sc->misc_config = misc_config;
    sc->sl_config = sl_config;
    sc->sl_config2 = sl_config2;
}

// Configure TVP7002 and scan converter logic based on the video mode
void program_mode()
{
    alt_u8 h_syncinlen, v_syncinlen, hdmitx_pclk_level, osd_x_size, osd_y_size;
    alt_u32 h_hz, v_hz_x100, h_synclen_px;

    // Mark as stable (needed after sync up to avoid unnecessary mode switch)
    stable_frames = STABLE_THOLD;

    if ((cm.clkcnt != 0) && (cm.totlines != 0)) { //prevent div by 0
        h_hz = TVP_EXTCLK_HZ/cm.clkcnt;
        v_hz_x100 = cm.progressive ? ((100*TVP_EXTCLK_HZ)/cm.totlines)/cm.clkcnt : (2*((100*TVP_EXTCLK_HZ)/cm.totlines))/cm.clkcnt;
    } else {
        h_hz = 15700;
        v_hz_x100 = 6000;
    }

    printf("\nLines: %u %c\n", (unsigned)cm.totlines, cm.progressive ? 'p' : 'i');
    printf("Clocks per line: %u : HS %u.%.3u kHz  VS %u.%.2u Hz\n", (unsigned)cm.clkcnt, (unsigned)(h_hz/1000), (unsigned)(h_hz%1000), (unsigned)(v_hz_x100/100), (unsigned)(v_hz_x100%100));

    h_syncinlen = tvp_readreg(TVP_HSINWIDTH);
    v_syncinlen = tvp_readreg(TVP_VSINWIDTH);
    printf("Hswidth: %u  Vswidth: %u  Macrovision: %u\n", (unsigned)h_syncinlen, (unsigned)(v_syncinlen & 0x1F), (unsigned)cm.macrovis);

    sniprintf(row1, LCD_ROW_LEN+1, "%s %u%c", avinput_str[cm.avinput], (unsigned)cm.totlines, cm.progressive ? 'p' : 'i');
    sniprintf(row2, LCD_ROW_LEN+1, "%u.%.2ukHz %u.%.2uHz", (unsigned)(h_hz/1000), (unsigned)((h_hz%1000)/10), (unsigned)(v_hz_x100/100), (unsigned)(v_hz_x100%100));
    if (!menu_active) {
        osd->osd_config.status_refresh = 1;
        lcd_write_status();
    }

    //printf ("Get mode id with %u %u %f\n", totlines, progressive, hz);
    cm.id = get_mode_id(cm.totlines, cm.progressive, v_hz_x100/100, target_typemask);

    if (cm.id == -1) {
        printf ("Error: no suitable mode found, defaulting to 240p\n");
        cm.id = 4;
    }
    vm_sel = cm.id;

    cm.h_mult_total = (video_modes[cm.id].h_total*cm.sample_mult) + ((cm.sample_mult*video_modes[cm.id].h_total_adj*5 + 50) / 100);

    target_type = target_typemask & video_modes[cm.id].type;
    h_synclen_px = ((alt_u32)h_syncinlen * (alt_u32)cm.h_mult_total) / cm.clkcnt;

    printf("Mode %s selected - hsync width: %upx\n", video_modes[cm.id].name, (unsigned)h_synclen_px);

    tvp_source_setup(target_type,
                     cm.h_mult_total,
                     cm.clkcnt,
                     cm.cc.tvp_hpll2x && (video_modes[cm.id].flags & MODE_PLLDIVBY2),
                     (alt_u8)h_synclen_px,
                     (alt_8)(cm.cc.clamp_offset-SIGNED_NUMVAL_ZERO));
    set_lpf(cm.cc.video_lpf);
    set_csc(cm.cc.ypbpr_cs);
    cm.sample_sel = tvp_set_hpll_phase(video_modes[cm.id].sampler_phase, cm.sample_mult);

    pll_reconfig->pll_config_status.reset = (cm.fpga_vmultmode == FPGA_V_MULTMODE_1X);

    switch (cm.fpga_vmultmode) {
    case FPGA_V_MULTMODE_2X:
    case FPGA_V_MULTMODE_5X:
        cm.pll_config = PLL_CONFIG_2X_5X;
        break;
    case FPGA_V_MULTMODE_3X:
    case FPGA_V_MULTMODE_4X:
        cm.pll_config = PLL_CONFIG_3X_4X;
        break;
    default:
        break;
    }
    pll_reconfigure(cm.pll_config);

    if (cm.fpga_vmultmode == FPGA_V_MULTMODE_1X) {
        osd_x_size = (video_modes[cm.id].v_active > 700) ? 1 : 0;
        osd_y_size = osd_x_size;
    } else {
        osd_x_size = 1 - cm.tx_pixelrep + (cm.fpga_hmultmode == FPGA_H_MULTMODE_OPTIMIZED_1X) + (cm.fpga_vmultmode > FPGA_V_MULTMODE_3X);
        osd_y_size = 0;
    }
    osd->osd_config.x_size = osd_x_size;
    osd->osd_config.y_size = osd_y_size;

    update_sc_config();

    TX_SetPixelRepetition(cm.tx_pixelrep, ((cm.cc.tx_mode!=TX_DVI) && (cm.tx_pixelrep == cm.hdmitx_pixr_ifr)) ? 1 : 0);

    pclk_out = (TVP_EXTCLK_HZ/cm.clkcnt)*cm.h_mult_total*(cm.fpga_vmultmode+1);
    pclk_out *= 1+cm.tx_pixelrep;
    if (cm.fpga_hmultmode == FPGA_H_MULTMODE_OPTIMIZED_1X)
        pclk_out /= 2;
    else if (cm.fpga_hmultmode == FPGA_H_MULTMODE_ASPECTFIX)
        pclk_out = (pclk_out*4)/3;
    printf("PCLK_out: %luHz\n", pclk_out);

    if (pclk_out > 85000000)
        hdmitx_pclk_level = 1;
    else if (pclk_out < 75000000)
        hdmitx_pclk_level = 0;
    else
        hdmitx_pclk_level = cm.hdmitx_pclk_level;

    // Full TX initialization increases mode switch delay, use only when necessary
    if (cm.cc.full_tx_setup || (cm.hdmitx_pclk_level != hdmitx_pclk_level)) {
        cm.hdmitx_pclk_level = hdmitx_pclk_level;
        TX_enable(cm.cc.tx_mode);
    } else if (cm.cc.tx_mode!=TX_DVI) {
        HDMITX_SetAVIInfoFrame(cm.hdmitx_vic, (cm.cc.tx_mode == TX_HDMI_RGB) ? F_MODE_RGB444 : F_MODE_YUV444, 0, 0, cm.cc.hdmi_itc, cm.hdmitx_pixr_ifr);
#ifdef ENABLE_AUDIO
#ifdef MANUAL_CTS
        SetupAudio(cm.cc.tx_mode);
#endif
#endif
    }
}

int load_profile() {
    int retval;

    retval = read_userdata(profile_sel_menu, 0);
    if (retval == 0) {
        profile_sel = profile_sel_menu;

        // Change the input if the new profile demands it.
        if (tc.link_av != AV_LAST)
            target_input = tc.link_av;

        // Update profile link (also prevents the change of input from inducing a profile load).
        input_profiles[profile_link ? target_input : AV_TESTPAT] = profile_sel;
        write_userdata(INIT_CONFIG_SLOT);
    }

    return retval;
}

int save_profile() {
    int retval;

    retval = write_userdata(profile_sel_menu);
    if (retval == 0) {
        profile_sel = profile_sel_menu;

        input_profiles[profile_link ? cm.avinput : AV_TESTPAT] = profile_sel;
        write_userdata(INIT_CONFIG_SLOT);
    }

    return retval;
}

// Initialize hardware
int init_hw()
{
    alt_u32 chiprev;

    // Reset hardware
    IOWR_ALTERA_AVALON_PIO_DATA(PIO_0_BASE, AV_RESET_N|LCD_BL);
    IOWR_ALTERA_AVALON_PIO_DATA(PIO_0_BASE, 0x0000);
    sc->h_config.data = 0x00000000;
    sc->v_config.data = 0x00000000;
    usleep(10000);

    // unreset hw
    sys_ctrl = AV_RESET_N|LCD_BL|SD_SPI_SS_N|LCD_CS_N|REMOTE_EVENT;
    IOWR_ALTERA_AVALON_PIO_DATA(PIO_0_BASE, sys_ctrl);

    // Reload initial PLL config (needed after jtagm_reset_req if config has changed).
    // Note that test pattern gets restored only if pclk was active before jtagm_reset_req assertion.
    pll_reconfigure(PLL_CONFIG_VG);

    //wait >500ms for SD card interface to be stable
    //over 200ms and LCD may be buggy?
    usleep(200000);

    // IT6613 officially supports only 100kHz, but 400kHz seems to work
    I2C_init(I2CA_BASE,ALT_CPU_FREQ,400000);
    //I2C_init(I2C_OPENCORES_1_BASE,ALT_CPU_FREQ,400000);

    /* Initialize the character display */
    lcd_init();

    if (!ths_init()) {
        printf("Error: could not read from THS7353\n");
        return -2;
    }

    /* check if TVP is found */
    chiprev = tvp_readreg(TVP_CHIPREV);
    //printf("chiprev %d\n", chiprev);

    if (chiprev == 0xff) {
        printf("Error: could not read from TVP7002\n");
        return -3;
    }

    tvp_init();

    chiprev = HDMITX_ReadI2C_Byte(IT_DEVICEID);

    if (chiprev != 0x13) {
        printf("Error: could not read from IT6613\n");
        return -4;
    }

    InitIT6613();

#ifdef ENABLE_AUDIO
    if (pcm1862_init()) {
        printf("PCM1862 found\n");
        pcm1862_active = 1;
    }
#endif

    if (check_flash() != 0) {
        printf("Error: incorrect flash type detected\n");
        return -1;
    }

    // Set defaults
    set_default_avconfig();
    memcpy(&cm.cc, &tc_default, sizeof(avconfig_t));
    memcpy(rc_keymap, rc_keymap_default, sizeof(rc_keymap));

    // Load initconfig and profile
    read_userdata(INIT_CONFIG_SLOT, 0);
    read_userdata(profile_sel, 0);

    // Setup OSD
    osd_enable = osd_enable_pre;
    osd_status_timeout = osd_status_timeout_pre;
    osd->osd_config.x_size = 0;
    osd->osd_config.y_size = 0;
    osd->osd_config.x_offset = 3;
    osd->osd_config.y_offset = 3;
    osd->osd_config.enable = osd_enable;
    osd->osd_config.status_timeout = osd_status_timeout;

    // Setup remote keymap
    if (!(IORD_ALTERA_AVALON_PIO_DATA(PIO_1_BASE) & PB1_BIT))
        setup_rc();

    // init always in HDMI mode (fixes yellow screen bug)
    cm.hdmitx_vic = HDMI_480p60;
    TX_enable(TX_HDMI_RGB);

    return 0;
}

int latency_test() {
    lt_status_reg lt_status;
    alt_u16 latency_ms_x100, stb_ms_x100;
    alt_u32 btn_vec, btn_vec_prev=1;
    alt_u8 position = lt_sel+1;

    sys_ctrl |= LT_ACTIVE|(position<<LT_MODE_OFFS);
    IOWR_ALTERA_AVALON_PIO_DATA(PIO_0_BASE, sys_ctrl);
    sniprintf(menu_row2, LCD_ROW_LEN+1, "OK to init");
    lcd_write_menu();

    while (1) {
        btn_vec = IORD_ALTERA_AVALON_PIO_DATA(PIO_1_BASE) & RC_MASK;

        if ((btn_vec_prev == 0) && (btn_vec != 0)) {
            if (btn_vec == rc_keymap[RC_OK]) {
                sys_ctrl &= ~(3<<LT_MODE_OFFS);
                IOWR_ALTERA_AVALON_PIO_DATA(PIO_0_BASE, sys_ctrl);
                menu_row2[0] = 0;
                lcd_write_menu();
                usleep(400000);
                sys_ctrl |= LT_ARMED|(position<<LT_MODE_OFFS);
                IOWR_ALTERA_AVALON_PIO_DATA(PIO_0_BASE, sys_ctrl);
                //while (!sc->lt_status.lt_finished) {} //Hangs if sync is lost
                SPI_Timer_On(1000);
                while ((SPI_Timer_Status()==TRUE)) {
                    lt_status = sc->lt_status;
                    if (lt_status.lt_finished)
                        break;
                }
                SPI_Timer_Off();
                latency_ms_x100 = lt_status.lt_lat_result;
                stb_ms_x100 = lt_status.lt_stb_result;

                if (latency_ms_x100 == 0)
                    sniprintf(menu_row2, LCD_ROW_LEN+1, "False trigger");
                else if (latency_ms_x100 == 0xffff)
                    sniprintf(menu_row2, LCD_ROW_LEN+1, "Timeout");
                else if (stb_ms_x100 == 0xfff)
                    sniprintf(menu_row2, LCD_ROW_LEN+1, "%u.%.2ums", latency_ms_x100/100, latency_ms_x100%100);
                else
                    sniprintf(menu_row2, LCD_ROW_LEN+1, "%u.%.2ums/%u.%.2ums", latency_ms_x100/100, latency_ms_x100%100, stb_ms_x100/100, stb_ms_x100%100);
                lcd_write_menu();
            } else if (btn_vec == rc_keymap[RC_BACK]) {
                break;
            }

            sys_ctrl &= ~LT_ARMED;
            IOWR_ALTERA_AVALON_PIO_DATA(PIO_0_BASE, sys_ctrl);
        }

        btn_vec_prev = btn_vec;
        usleep(WAITLOOP_SLEEP_US);
    }

    sys_ctrl &= ~LT_CTRL_MASK;
    IOWR_ALTERA_AVALON_PIO_DATA(PIO_0_BASE, sys_ctrl);

    return 0;
}

// Enable chip outputs
void enable_outputs()
{
    // enable TVP output
    tvp_enable_output();
    // program video mode
    program_mode();

    // enable and unmute TX
    TX_enable(tc.tx_mode);
}

int main()
{
    tvp_input_t target_tvp = 0;
    tvp_sync_input_t target_tvp_sync = 0;
    ths_input_t target_ths = 0;
    pcm_input_t target_pcm = 0;
    video_format target_format = 0;

    status_t status;

    alt_u32 input_vec;

    alt_u32 auto_input_timestamp = 300 * (alt_timestamp_freq() >> 10);
    alt_u8 auto_input_changed = 0;
    alt_u8 auto_input_ctr = 0;
    alt_u8 auto_input_current_ctr = AUTO_CURRENT_MAX_COUNT;
    alt_u8 auto_input_keep_current = 0;

    int init_stat, man_input_change;

    init_stat = init_hw();

    if (init_stat >= 0) {
        printf("### DIY VIDEO DIGITIZER / SCANCONVERTER INIT OK ###\n\n");
        sniprintf(row1, LCD_ROW_LEN+1, "OSSC  fw. %u.%.2u" FW_SUFFIX1 FW_SUFFIX2, FW_VER_MAJOR, FW_VER_MINOR);
#ifndef DEBUG
        strncpy(row2, "2014-2019  marqs", LCD_ROW_LEN+1);
#else
        strncpy(row2, "** DEBUG BUILD *", LCD_ROW_LEN+1);
#endif
        osd->osd_config.status_refresh = 1;
        lcd_write_status();
        usleep(500000);
    } else {
        sniprintf(row1, LCD_ROW_LEN+1, "Init error  %d", init_stat);
        strncpy(row2, "", LCD_ROW_LEN+1);
        osd->osd_config.status_refresh = 1;
        lcd_write_status();
        while (1) {}
    }

    // start timer for auto input
    alt_timestamp_start();

    // Mainloop
    while(1) {
        // Read remote control and PCB button status
        input_vec = IORD_ALTERA_AVALON_PIO_DATA(PIO_1_BASE);
        remote_code = input_vec & RC_MASK;
        btn_code = ~input_vec & PB_MASK;
        remote_rpt = input_vec >> 24;

        if ((remote_rpt == 0) || ((remote_rpt > 1) && (remote_rpt < 6)) || (remote_rpt == remote_rpt_prev))
            remote_code = 0;

        remote_rpt_prev = remote_rpt;

        if (btn_code_prev == 0) {
            btn_code_prev = btn_code;
        } else {
            btn_code_prev = btn_code;
            btn_code = 0;
        }

        // Auto input switching
        if (auto_input != AUTO_OFF && cm.avinput != AV_TESTPAT && !cm.sync_active && !menu_active
            && alt_timestamp() >= auto_input_timestamp && auto_input_ctr < AUTO_MAX_COUNT) {

            // Keep switching on the same physical input when set to Current input or a short time after losing sync.
            auto_input_keep_current = (auto_input == AUTO_CURRENT_INPUT || auto_input_current_ctr < AUTO_CURRENT_MAX_COUNT);

            switch(cm.avinput) {
            case AV1_RGBs:
                target_input = auto_av1_ypbpr ? AV1_YPBPR : AV1_RGsB;
                break;
            case AV1_RGsB:
            case AV1_YPBPR:
                target_input = auto_input_keep_current ? AV1_RGBs : (auto_av2_ypbpr ? AV2_YPBPR : AV2_RGsB);
                break;
            case AV2_YPBPR:
            case AV2_RGsB:
                target_input = auto_input_keep_current ? target_input : AV3_RGBHV;
                break;
            case AV3_RGBHV:
                target_input = AV3_RGBs;
                break;
            case AV3_RGBs:
                target_input = auto_av3_ypbpr ? AV3_YPBPR : AV3_RGsB;
                break;
            case AV3_RGsB:
            case AV3_YPBPR:
                target_input = auto_input_keep_current ? AV3_RGBHV : AV1_RGBs;
                break;
            default:
                break;
            }

            auto_input_ctr++;

            if (auto_input_current_ctr < AUTO_CURRENT_MAX_COUNT)
                auto_input_current_ctr++;

            // For input linked profile loading below
            auto_input_changed = 1;

            // reset timer
            alt_timestamp_start();
        }

        man_input_change = parse_control();

        if (menu_active)
            display_menu(0);

        // Only auto load profile when input is manually changed or when sync is active after automatic switch.
        if ((target_input != cm.avinput && man_input_change) || (auto_input_changed && cm.sync_active))  {
            // The input changed, so load the appropriate profile if
            // input->profile link is enabled
            if (profile_link && (profile_sel != input_profiles[target_input])) {
                profile_sel = input_profiles[target_input];
                read_userdata(profile_sel, 0);
            }

            auto_input_changed = 0;
        }

        if ((target_input != cm.avinput) || ((target_tvp_sync >= TVP_HV_A) && ((tc.av3_alt_rgb != cm.cc.av3_alt_rgb)))) {

            target_tvp = TVP_INPUT1;
            target_tvp_sync = TVP_SOG1;
            target_typemask = VIDEO_LDTV|VIDEO_SDTV|VIDEO_EDTV|VIDEO_HDTV;

            if ((target_input <= AV1_YPBPR) || (tc.av3_alt_rgb==1 && ((target_input == AV3_RGBHV) || (target_input == AV3_RGBs)))) {
                target_ths = THS_INPUT_B;
                target_pcm = PCM_INPUT4;
            } else if ((target_input <= AV2_RGsB) || (tc.av3_alt_rgb==2 && ((target_input == AV3_RGBHV) || (target_input == AV3_RGBs)))) {
                target_ths = THS_INPUT_A;
                target_pcm = PCM_INPUT3;
            } else  { // if (target_input <= AV3_YPBPR) {
                target_tvp = TVP_INPUT3;
                target_ths = THS_STANDBY;
                target_pcm = PCM_INPUT2;
            }

            switch (target_input) {
            case AV1_RGBs:
            case AV3_RGBs:
                target_format = FORMAT_RGBS;
                break;
            case AV1_RGsB:
            case AV2_RGsB:
            case AV3_RGsB:
                target_format = FORMAT_RGsB;
                break;
            case AV1_YPBPR:
            case AV2_YPBPR:
            case AV3_YPBPR:
                target_format = FORMAT_YPbPr;
                break;
            case AV3_RGBHV:
                target_format = FORMAT_RGBHV;
                if (!tc.av3_alt_rgb)
                    target_typemask = VIDEO_PC;
                break;
            default:
                break;
            }

            switch (target_input) {
            case AV1_RGBs:
                target_tvp_sync = TVP_SOG2;
                break;
            case AV3_RGBHV:
                target_tvp_sync = TVP_HV_A;
                break;
            case AV3_RGBs:
                target_tvp_sync = TVP_CS_A;
                break;
            case AV3_RGsB:
            case AV3_YPBPR:
                target_tvp_sync = TVP_SOG3;
                break;
            default:
                break;
            }

            printf("### SWITCH MODE TO %s ###\n", avinput_str[target_input]);

            cm.avinput = target_input;
            cm.sync_active = 0;
            ths_source_sel(target_ths, (cm.cc.video_lpf > 1) ? (VIDEO_LPF_MAX-cm.cc.video_lpf) : THS_LPF_BYPASS);
            tvp_disable_output();
#ifdef ENABLE_AUDIO
            DisableAudioOutput();
            if (pcm1862_active)
                pcm_source_sel(target_pcm);
#endif
            tvp_source_sel(target_tvp, target_tvp_sync, target_format);
            cm.clkcnt = 0; //TODO: proper invalidate
            strncpy(row1, avinput_str[cm.avinput], LCD_ROW_LEN+1);
            strncpy(row2, "    NO SYNC", LCD_ROW_LEN+1);
            if (!menu_active) {
                osd->osd_config.status_refresh = 1;
                lcd_write_status();
            }
            if (man_input_change) {
                // record last input if it was selected manually
                if (def_input == AV_LAST)
                    write_userdata(INIT_CONFIG_SLOT);
                // Reset auto input timer when input is manually changed
                auto_input_ctr = 0;
                alt_timestamp_start();
            }
        }

        // Check here to enable regardless of input
        if (tc.tx_mode != cm.cc.tx_mode) {
            HDMITX_SetAVIInfoFrame(cm.hdmitx_vic, F_MODE_RGB444, 0, 0, 0, 0);
            TX_enable(tc.tx_mode);
            cm.cc.tx_mode = tc.tx_mode;
            cm.clkcnt = 0; //TODO: proper invalidate
        }
        if ((tc.tx_mode != TX_DVI) && (tc.hdmi_itc != cm.cc.hdmi_itc)) {
            //EnableAVIInfoFrame(FALSE, NULL);
            printf("setting ITC to %d\n", tc.hdmi_itc);
            HDMITX_SetAVIInfoFrame(cm.hdmitx_vic, (tc.tx_mode == TX_HDMI_RGB) ? F_MODE_RGB444 : F_MODE_YUV444, 0, 0, tc.hdmi_itc, cm.hdmitx_pixr_ifr);
            cm.cc.hdmi_itc = tc.hdmi_itc;
        }
        if (tc.av3_alt_rgb != cm.cc.av3_alt_rgb) {
            printf("Changing AV3 RGB source\n");
            cm.cc.av3_alt_rgb = tc.av3_alt_rgb;
        }
        if ((osd_enable != osd_enable_pre) || (osd_status_timeout != osd_status_timeout_pre)) {
            osd_enable = osd_enable_pre;
            osd_status_timeout = osd_status_timeout_pre;
            osd->osd_config.enable = osd_enable;
            osd->osd_config.status_timeout = osd_status_timeout;
        }

        if (cm.avinput != AV_TESTPAT) {
            status = get_status(target_tvp_sync);

            switch (status) {
            case ACTIVITY_CHANGE:
                if (cm.sync_active) {
                    printf("Sync up\n");
                    sys_ctrl |= VIDGEN_OFF;
                    IOWR_ALTERA_AVALON_PIO_DATA(PIO_0_BASE, sys_ctrl);
                    enable_outputs();
                } else {
                    printf("Sync lost\n");
                    cm.clkcnt = 0; //TODO: proper invalidate
                    tvp_disable_output();
                    //ths_source_sel(THS_STANDBY, 0);
                    strncpy(row1, avinput_str[cm.avinput], LCD_ROW_LEN+1);
                    strncpy(row2, "    NO SYNC", LCD_ROW_LEN+1);
                    if (!menu_active) {
                        osd->osd_config.status_refresh = 1;
                        lcd_write_status();
                    }
                    alt_timestamp_start();// reset auto input timer
                    auto_input_ctr = 0;
                    auto_input_current_ctr = 0;
                }
                break;
            case MODE_CHANGE:
                if (cm.sync_active) {
                    printf("Mode change\n");
                    program_mode();
                }
                break;
            case SC_CONFIG_CHANGE:
                if (cm.sync_active) {
                    printf("Scanconverter config change\n");
                    update_sc_config();
                }
                break;
            default:
                break;
            }
        }

        usleep(300);    // Avoid executing mainloop multiple times per vsync
    }

    return 0;
}
