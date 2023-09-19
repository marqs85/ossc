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
#include "firmware.h"
#include "userdata.h"
#include "it6613.h"
#include "it6613_sys.h"
#include "HDMI_TX.h"
#include "hdmitx.h"
#include "sd_io.h"
#include "sys/alt_timestamp.h"

#define MIN_LINES_PROGRESSIVE   200
#define MIN_LINES_INTERLACED    400
#define STATUS_TIMEOUT_US       25000
#define MAINLOOP_INTERVAL_US    10000

#define PCNT_TOLERANCE 50
#define HSYNC_WIDTH_TOLERANCE 8

alt_u32 sys_ctrl;

// Current mode
avmode_t cm;

extern mode_data_t video_modes_plm[];
extern ypbpr_to_rgb_csc_t csc_coeffs[];
extern alt_u16 rc_keymap[REMOTE_MAX_KEYS];
extern alt_u16 rc_keymap_default[REMOTE_MAX_KEYS];
extern alt_u32 remote_code;
extern alt_u32 btn_code, btn_code_prev;
extern alt_u8 remote_rpt, remote_rpt_prev;
extern avconfig_t tc, tc_default;
extern alt_u8 vm_sel;
extern char target_profile_name[PROFILE_NAME_LEN+1];

tvp_input_t target_tvp;
tvp_sync_input_t target_tvp_sync;
alt_u8 target_type;
alt_u8 update_cur_vm;

alt_u8 profile_sel, profile_sel_menu, input_profiles[AV_LAST], lt_sel, def_input, profile_link, lcd_bl_timeout;
alt_u8 osd_enable=1, osd_status_timeout=1;
alt_u8 auto_input, auto_av1_ypbpr, auto_av2_ypbpr = 1, auto_av3_ypbpr;

char row1[LCD_ROW_LEN+1], row2[LCD_ROW_LEN+1], menu_row1[LCD_ROW_LEN+1], menu_row2[LCD_ROW_LEN+1];

extern alt_u8 menu_active;
avinput_t target_input;

alt_u8 pcm1862_active;

uint8_t sl_def_iv_x, sl_def_iv_y;

alt_u32 read_it2(alt_u32 regaddr);

mode_data_t vmode_in, vmode_out;
vm_proc_config_t vm_conf;

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
const pll_config_t pll_configs[] = { {{0x0d806000, 0x00402010, 0x08800020, 0x00080002, 0x00000000}},    // 1x (default)
                                     {{0x0d806000, 0x00402008, 0x04800020, 0x00080002, 0x00000000}},    // 2x (~20-40MHz)
                                     {{0x0d806000, 0x00441c07, 0x02800020, 0x00080002, 0x00000000}},    // 3x (~20-40MHz)
                                     {{0x0d806000, 0x00402004, 0x02800020, 0x00080002, 0x00000000}},    // 4x (~20-40MHz)
                                     {{0x0d806000, 0x00441c05, 0x01800020, 0x00080002, 0x00000000}},    // 5x (~20-40MHz)
                                     {{0x0d806000, 0x00301802, 0x01800020, 0x00080002, 0x00000000}},    // 6x (~20-40MHz)
                                     {{0x0e406000, 0x00281407, 0x02800020, 0x00080002, 0x00000000}} };  // 2x (~75MHz)

volatile sc_regs *sc = (volatile sc_regs*)SC_CONFIG_0_BASE;
volatile osd_regs *osd = (volatile osd_regs*)OSD_GENERATOR_0_BASE;
volatile pll_reconfig_regs *pll_reconfig = (volatile pll_reconfig_regs*)PLL_RECONFIG_0_BASE;

void ui_disp_menu(alt_u8 osd_mode)
{
    alt_u8 menu_page;

    if ((osd_mode == 1) || (osd_enable == 2)) {
        strncpy((char*)osd->osd_array.data[0][0], menu_row1, OSD_CHAR_COLS);
        strncpy((char*)osd->osd_array.data[1][0], menu_row2, OSD_CHAR_COLS);
        osd->osd_row_color.mask = 0;
        osd->osd_sec_enable[0].mask = 3;
        osd->osd_sec_enable[1].mask = 0;
    } else if (osd_mode == 2) {
        menu_page = get_current_menunavi()->mp;
        strncpy((char*)osd->osd_array.data[menu_page][1], menu_row2, OSD_CHAR_COLS);
        osd->osd_sec_enable[1].mask |= (1<<menu_page);
    }

    lcd_write((char*)&menu_row1, (char*)&menu_row2);
}

void ui_disp_status(alt_u8 refresh_osd_timer) {
    if (!menu_active) {
        if (refresh_osd_timer)
            osd->osd_config.status_refresh = 1;

        strncpy((char*)osd->osd_array.data[0][0], row1, OSD_CHAR_COLS);
        strncpy((char*)osd->osd_array.data[1][0], row2, OSD_CHAR_COLS);
        osd->osd_row_color.mask = 0;
        osd->osd_sec_enable[0].mask = 3;
        osd->osd_sec_enable[1].mask = 0;

        lcd_write((char*)&row1, (char*)&row2);
    }
}

#ifdef ENABLE_AUDIO
inline void SetupAudio(tx_mode_t mode)
{
    // shut down audio-tx before setting new config (recommended for changing audio-tx config)
    DisableAudioOutput();
    EnableAudioInfoFrame(FALSE, NULL);

    if (mode != TX_DVI) {
        EnableAudioOutput4OSSC(cm.pclk_o_hz, tc.audio_dw_sampl, tc.audio_swap_lr);
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
        HDMITX_SetAVIInfoFrame(vmode_out.vic, (mode == TX_HDMI_RGB) ? F_MODE_RGB444 : F_MODE_YUV444, 0, 0, tc.hdmi_itc, vm_conf.hdmitx_pixr_ifr);
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
    pclk = estimate_dotclk(&vmode_in, (TVP_EXTCLK_HZ/cm.clkcnt));

    //Auto
    if (lpf == 0) {
        if (target_tvp == TVP_INPUT3) {
            tvp_set_lpf((pclk < 30000000) ? 0x0F : 0);
            ths_set_lpf(THS_LPF_BYPASS);
        } else {
            tvp_set_lpf(0);

            switch (target_type) {
            case VIDEO_PC:
            case VIDEO_HDTV:
                ths_set_lpf((pclk < 80000000) ? THS_LPF_35MHZ : THS_LPF_BYPASS);
                break;
            case VIDEO_EDTV:
                ths_set_lpf(THS_LPF_16MHZ);
                break;
            case VIDEO_SDTV:
            default:
                ths_set_lpf(THS_LPF_9MHZ);
                break;
            }
        }
    } else {
        if (target_tvp == TVP_INPUT3) {
            tvp_set_lpf((lpf == 2) ? 0x0F : 0);
            ths_set_lpf(THS_LPF_BYPASS);
        } else {
            tvp_set_lpf(0);
            ths_set_lpf((lpf > 2) ? (VIDEO_LPF_MAX-lpf) : THS_LPF_BYPASS);
        }
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

void set_sampler_phase(uint8_t sampler_phase) {
    uint32_t sample_rng_x1000;
    uint8_t tvp_phase;

    vmode_in.sampler_phase = sampler_phase;

    if (vm_conf.h_skip == 0) {
        vm_conf.h_sample_sel = 0;
        tvp_phase = sampler_phase;
    } else {
        sample_rng_x1000 = 360000 / (vm_conf.h_skip+1);
        vm_conf.h_sample_sel = (sampler_phase*11250)/sample_rng_x1000;
        tvp_phase = ((((sampler_phase*11250) % sample_rng_x1000)*32)/sample_rng_x1000);
    }

    if (vm_conf.h_skip > 0)
        printf("Sample sel: %u/%u\n", (vm_conf.h_sample_sel+1), (vm_conf.h_skip+1));

    tvp_set_hpll_phase(tvp_phase);
}

// Check if input video status / target configuration has changed
status_t get_status(tvp_sync_input_t syncinput)
{
    alt_u32 totlines, clkcnt, pcnt_frame;
    alt_u8 progressive, sync_active, valid_linecnt, hsync_width;
    status_t status = NO_CHANGE;
    alt_timestamp_type start_ts = alt_timestamp();

    // Wait until vsync active (avoid noise coupled to I2C bus on earlier prototypes)
    while (alt_timestamp() < start_ts + STATUS_TIMEOUT_US*(TIMER_0_FREQ/1000000)) {
        if (IORD_ALTERA_AVALON_PIO_DATA(PIO_1_BASE) & VSYNC_FLAG_MASK)
            break;
    }

    //sync_active = tvp_check_sync(syncinput);
    sync_active = sc->fe_status.sync_active;

    // Read sync information from TVP7002 frontend
    totlines = sc->fe_status.vtotal;
    progressive = !sc->fe_status.interlace_flag;
    pcnt_frame = (unsigned long)sc->fe_status2.pcnt_frame;
    hsync_width = (unsigned long)sc->fe_status2.hsync_width;
    clkcnt = pcnt_frame/(totlines>>!progressive);

    valid_linecnt = check_linecnt(progressive, totlines);

    // Check sync activity
    if (!cm.sync_active && sync_active && valid_linecnt) {
        cm.sync_active = 1;
        status = ACTIVITY_CHANGE;
    } else if (cm.sync_active && (!sync_active || !valid_linecnt)) {
        cm.sync_active = 0;
        status = ACTIVITY_CHANGE;
    }

    if (valid_linecnt) {
        if ((totlines != cm.totlines) ||
            (progressive != cm.progressive) ||
            (pcnt_frame < (cm.pcnt_frame - PCNT_TOLERANCE)) ||
            (pcnt_frame > (cm.pcnt_frame + PCNT_TOLERANCE)) ||
            (abs(((int)hsync_width - (int)cm.hsync_width)) > HSYNC_WIDTH_TOLERANCE)) {
            printf("totlines: %lu (cur) / %lu (prev), pcnt_frame: %lu (cur) / %lu (prev), hsync_width: %lu (cur) / %lu (prev)\n", totlines, cm.totlines, pcnt_frame, cm.pcnt_frame, hsync_width, cm.hsync_width);

            status = (status < MODE_CHANGE) ? MODE_CHANGE : status;
        }

        if (memcmp(&tc, &cm.cc, offsetof(avconfig_t, sl_mode)) || (update_cur_vm == 1))
            status = (status < MODE_CHANGE) ? MODE_CHANGE : status;

        if ((vm_conf.si_pclk_mult > 1) && (pll_reconfig->pll_config_status.c_config_id != 5) && (vm_conf.si_pclk_mult-1 != pll_reconfig->pll_config_status.c_config_id))
            status = (status < MODE_CHANGE) ? MODE_CHANGE : status;

        cm.totlines = totlines;
        cm.clkcnt = clkcnt;
        cm.pcnt_frame = pcnt_frame;
        cm.hsync_width = hsync_width;
        cm.progressive = progressive;
    }

    if (memcmp(&tc.sl_mode, &cm.cc.sl_mode, offsetof(avconfig_t, sync_vth) - offsetof(avconfig_t, sl_mode)))
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

    if (pcm1862_active && (tc.audio_mono != cm.cc.audio_mono)) {
        DisableAudioOutput();
        pcm_set_stereo_mode(tc.audio_mono);
        SetupAudio(cm.cc.tx_mode);
    }
#endif

    cm.cc = tc;
    update_cur_vm = 0;

    return status;
}

void update_sc_config(mode_data_t *vm_in, mode_data_t *vm_out, vm_proc_config_t *vm_conf, avconfig_t *avconfig)
{
    int i;

    hv_config_reg hv_in_config = {.data=0x00000000};
    hv_config2_reg hv_in_config2 = {.data=0x00000000};
    hv_config3_reg hv_in_config3 = {.data=0x00000000};
    hv_config_reg hv_out_config = {.data=0x00000000};
    hv_config2_reg hv_out_config2 = {.data=0x00000000};
    hv_config3_reg hv_out_config3 = {.data=0x00000000};
    xy_config_reg xy_out_config = {.data=0x00000000};
    xy_config2_reg xy_out_config2 = {.data=0x00000000};
    misc_config_reg misc_config = {.data=0x00000000};
    sl_config_reg sl_config = {.data=0x00000000};
    sl_config2_reg sl_config2 = {.data=0x00000000};
    sl_config3_reg sl_config3 = {.data=0x00000000};

    // Set input params
    hv_in_config.h_total = vm_in->timings.h_total;
    hv_in_config.h_active = vm_in->timings.h_active;
    hv_in_config.h_synclen = vm_in->timings.h_synclen;
    hv_in_config2.h_backporch = vm_in->timings.h_backporch;
    hv_in_config2.v_active = vm_in->timings.v_active;
    hv_in_config3.v_synclen = vm_in->timings.v_synclen;
    hv_in_config3.v_backporch = vm_in->timings.v_backporch;
    hv_in_config2.interlaced = vm_in->timings.interlaced;
    hv_in_config3.v_startline = vm_in->timings.v_synclen+vm_in->timings.v_backporch+12;
    hv_in_config3.h_skip = vm_conf->h_skip;
    hv_in_config3.h_sample_sel = vm_conf->h_sample_sel;

    // Set output params
    hv_out_config.h_total = vm_out->timings.h_total;
    hv_out_config.h_active = vm_out->timings.h_active;
    hv_out_config.h_synclen = vm_out->timings.h_synclen;
    hv_out_config2.h_backporch = vm_out->timings.h_backporch;
    hv_out_config2.v_total = vm_out->timings.v_total;
    hv_out_config2.v_active = vm_out->timings.v_active;
    hv_out_config3.v_synclen = vm_out->timings.v_synclen;
    hv_out_config3.v_backporch = vm_out->timings.v_backporch;
    hv_out_config2.interlaced = vm_out->timings.interlaced;
    hv_out_config3.v_startline = vm_conf->framesync_line;

    xy_out_config.x_size = vm_conf->x_size;
    xy_out_config.y_size = vm_conf->y_size;
    xy_out_config.y_offset = vm_conf->y_offset;
    xy_out_config2.x_offset = vm_conf->x_offset;
    xy_out_config2.x_start_lb = vm_conf->x_start_lb;
    xy_out_config2.y_start_lb = vm_conf->y_start_lb;
    xy_out_config2.x_rpt = vm_conf->x_rpt;
    xy_out_config2.y_rpt = vm_conf->y_rpt;

    misc_config.mask_br = avconfig->mask_br;
    misc_config.mask_color = avconfig->mask_color;
    misc_config.reverse_lpf = avconfig->reverse_lpf;
    misc_config.lm_deint_mode = 0;
    misc_config.nir_even_offset = 0;
    misc_config.ypbpr_cs = (avconfig->ypbpr_cs == 0) ? ((vm_in->type & VIDEO_HDTV) ? 1 : 0) : avconfig->ypbpr_cs-1;
    misc_config.vip_enable = 0;
    misc_config.bfi_enable = 0;
    misc_config.bfi_str = 0;

    // set default/custom scanline interval
    sl_def_iv_y = (vm_conf->y_rpt > 0) ? vm_conf->y_rpt : 1;
    sl_def_iv_x = (vm_conf->x_rpt > 0) ? vm_conf->x_rpt : sl_def_iv_y;
    sl_config3.sl_iv_x = ((avconfig->sl_type == 3) && (avconfig->sl_cust_iv_x)) ? avconfig->sl_cust_iv_x : sl_def_iv_x;
    sl_config3.sl_iv_y = ((avconfig->sl_type == 3) && (avconfig->sl_cust_iv_y)) ? avconfig->sl_cust_iv_y : sl_def_iv_y;

    // construct custom/default scanline overlay
    for (i=0; i<6; i++) {
        if (avconfig->sl_type == 3) {
            sl_config.sl_l_str_arr |= ((avconfig->sl_cust_l_str[i]-1)&0xf)<<(4*i);
            sl_config.sl_l_overlay |= (avconfig->sl_cust_l_str[i]!=0)<<i;
        } else {
            sl_config.sl_l_str_arr |= avconfig->sl_str<<(4*i);

            if ((i==5) && ((avconfig->sl_type == 0) || (avconfig->sl_type == 2))) {
                sl_config.sl_l_overlay = (1<<((sl_config3.sl_iv_y+1)/2))-1;
                if (avconfig->sl_id)
                    sl_config.sl_l_overlay <<= (sl_config3.sl_iv_y+2)/2;
            }
        }
    }
    for (i=0; i<10; i++) {
        if (avconfig->sl_type == 3) {
            if (i<8)
                sl_config2.sl_c_str_arr_l |= ((avconfig->sl_cust_c_str[i]-1)&0xf)<<(4*i);
            else
                sl_config3.sl_c_str_arr_h |= ((avconfig->sl_cust_c_str[i]-1)&0xf)<<(4*(i-8));
            sl_config3.sl_c_overlay |= (avconfig->sl_cust_c_str[i]!=0)<<i;
        } else {
            if (i<8)
                sl_config2.sl_c_str_arr_l |= avconfig->sl_str<<(4*i);
            else
                sl_config3.sl_c_str_arr_h |= avconfig->sl_str<<(4*(i-8));

            if ((i==9) && ((avconfig->sl_type == 1) || (avconfig->sl_type == 2)))
                sl_config3.sl_c_overlay = (1<<((sl_config3.sl_iv_x+1)/2))-1;
        }
    }
    sl_config.sl_method = avconfig->sl_method;
    sl_config.sl_altern = avconfig->sl_altern;
    sl_config3.sl_hybr_str = avconfig->sl_hybr_str;

    // disable scanlines if configured so
    if (((avconfig->sl_mode == 1) && (!vm_conf->y_rpt)) || (avconfig->sl_mode == 0)) {
        sl_config.sl_l_overlay = 0;
        sl_config3.sl_c_overlay = 0;
    }

    sc->hv_in_config = hv_in_config;
    sc->hv_in_config2 = hv_in_config2;
    sc->hv_in_config3 = hv_in_config3;
    sc->hv_out_config = hv_out_config;
    sc->hv_out_config2 = hv_out_config2;
    sc->hv_out_config3 = hv_out_config3;
    sc->xy_out_config = xy_out_config;
    sc->xy_out_config2 = xy_out_config2;
    sc->misc_config = misc_config;
    sc->sl_config = sl_config;
    sc->sl_config2 = sl_config2;
    sc->sl_config3 = sl_config3;
}

// Configure TVP7002 and scan converter logic based on the video mode
void program_mode()
{
    int retval;
    alt_u8 h_syncinlen, v_syncinlen, macrovis, hdmitx_pclk_level, osd_x_size, osd_y_size;
    alt_u32 h_hz, h_synclen_px, pclk_i_hz, dotclk_hz, pll_h_total;

    memset(&vmode_in, 0, sizeof(mode_data_t));

    vmode_in.timings.v_hz_x100 = (100*27000000UL)/cm.pcnt_frame;
    h_hz = (100*27000000UL)/((100*cm.pcnt_frame*(1+!cm.progressive))/cm.totlines);

    printf("\nLines: %u %c\n", (unsigned)cm.totlines, cm.progressive ? 'p' : 'i');
    printf("Clocks per line: %u\n", (unsigned)cm.clkcnt);

    //h_syncinlen = tvp_readreg(TVP_HSINWIDTH);
    h_syncinlen = cm.hsync_width;
#ifdef DEBUG
    v_syncinlen = tvp_readreg(TVP_VSINWIDTH);
    macrovis = !!(tvp_readreg(TVP_LINECNT2) & (1<<6));
    printf("Hswidth: %u  Vswidth: %u  Macrovision: %u\n", (unsigned)h_syncinlen, (unsigned)(v_syncinlen & 0x1F), (unsigned)macrovis);
#endif

    vmode_in.timings.h_synclen = h_syncinlen;
    vmode_in.timings.v_total = cm.totlines;
    vmode_in.timings.interlaced = !cm.progressive;

    sniprintf(row1, LCD_ROW_LEN+1, "%s %u-%c", avinput_str[cm.avinput], (unsigned)cm.totlines, cm.progressive ? 'p' : 'i');
    sniprintf(row2, LCD_ROW_LEN+1, "%u.%.2ukHz %u.%.2uHz", (unsigned)(h_hz/1000), (unsigned)((h_hz%1000)/10), (unsigned)(vmode_in.timings.v_hz_x100/100), (unsigned)(vmode_in.timings.v_hz_x100%100));
    ui_disp_status(1);

    retval = get_pure_lm_mode(&cm.cc, &vmode_in, &vmode_out, &vm_conf);

    if (retval == -1) {
        printf ("ERROR: no suitable mode preset found\n");
        vm_conf.si_pclk_mult = 0;
        return;
    }
    cm.id = retval;
    vm_sel = cm.id;

    // Double TVP7002 PLL sampling rate when possible to minimize jitter
    while (1) {
        pll_h_total = (vm_conf.h_skip+1) * vmode_in.timings.h_total + (((vm_conf.h_skip+1) * vmode_in.timings.h_total_adj * 5 + 50) / 100);
        pclk_i_hz = h_hz * pll_h_total;

        if ((pclk_i_hz < 25000000UL) && ((vm_conf.si_pclk_mult % 2) == 0)) {
            vm_conf.h_skip = 2*(vm_conf.h_skip+1)-1;
            vm_conf.si_pclk_mult /= 2;
        } else {
            break;
        }
    }

    // Tweak infoframe pixel repetition indicator if passing thru horizontally multiplied mode
    if ((vm_conf.y_rpt == 0) && (vm_conf.h_skip > 0))
        vm_conf.hdmitx_pixr_ifr = vm_conf.h_skip;

    dotclk_hz = estimate_dotclk(&vmode_in, h_hz);
    cm.pclk_o_hz = calculate_pclk(pclk_i_hz, &vmode_out, &vm_conf);

    printf("H: %lu.%.2lukHz V: %u.%.2uHz\n", (h_hz+5)/1000, ((h_hz+5)%1000)/10, (vmode_in.timings.v_hz_x100/100), (vmode_in.timings.v_hz_x100%100));
    printf("Estimated source dot clock: %lu.%.2luMHz\n", (dotclk_hz+5000)/1000000, ((dotclk_hz+5000)%1000000)/10000);
    printf("PCLK_IN: %luHz PCLK_OUT: %luHz\n", pclk_i_hz, cm.pclk_o_hz);

    // Trilevel sync is used with HDTV modes using composite sync
    // CEA-770.3 HDTV modes use tri-level syncs which have twice the width of bi-level syncs of corresponding CEA-861 modes
    if (video_modes_plm[cm.id].type & VIDEO_HDTV) {
        target_type = (target_tvp_sync <= TVP_SOG3) ? VIDEO_HDTV : VIDEO_PC;
        if (target_type == VIDEO_HDTV)
            vmode_in.timings.h_synclen *= 2;
    } else {
        target_type = video_modes_plm[cm.id].type;
    }

    h_synclen_px = ((alt_u32)h_syncinlen * pll_h_total) / cm.clkcnt;

    printf("Mode %s selected - hsync width: %upx\n", video_modes_plm[cm.id].name, (unsigned)h_synclen_px);

    tvp_source_setup(target_type,
                     pll_h_total,
                     cm.clkcnt,
                     0,
                     (alt_u8)h_synclen_px,
                     (alt_8)(cm.cc.clamp_offset-SIGNED_NUMVAL_ZERO));
    set_lpf(cm.cc.video_lpf);
    set_csc(cm.cc.ypbpr_cs);

    set_sampler_phase(video_modes_plm[cm.id].sampler_phase);

    pll_reconfig->pll_config_status.reset = (vm_conf.si_pclk_mult <= 1);

    if (vm_conf.si_pclk_mult > 1) {
        if ((vm_conf.si_pclk_mult == 2) && (pclk_i_hz > 50000000UL))
            pll_reconfigure(6);
        else
            pll_reconfigure(vm_conf.si_pclk_mult-1);
        sys_ctrl &= ~PLL_BYPASS;
    } else {
        sys_ctrl |= PLL_BYPASS;
    }
    IOWR_ALTERA_AVALON_PIO_DATA(PIO_0_BASE, sys_ctrl);

    update_osd_size(&vmode_out, &vm_conf);

    update_sc_config(&vmode_in, &vmode_out, &vm_conf, &cm.cc);

    TX_SetPixelRepetition(vm_conf.tx_pixelrep, ((cm.cc.tx_mode!=TX_DVI) && (vm_conf.tx_pixelrep == vm_conf.hdmitx_pixr_ifr)) ? 1 : 0);

    if (cm.pclk_o_hz > 85000000)
        hdmitx_pclk_level = 1;
    else if (cm.pclk_o_hz < 75000000)
        hdmitx_pclk_level = 0;
    else
        hdmitx_pclk_level = cm.hdmitx_pclk_level;

    printf("PCLK level: %u, PR: %u, IPR: %u, ITC: %u\n", hdmitx_pclk_level, vm_conf.tx_pixelrep, vm_conf.hdmitx_pixr_ifr, cm.cc.hdmi_itc);

    // Full TX initialization increases mode switch delay, use only when necessary
    if (cm.cc.full_tx_setup || (cm.hdmitx_pclk_level != hdmitx_pclk_level)) {
        cm.hdmitx_pclk_level = hdmitx_pclk_level;
        TX_enable(cm.cc.tx_mode);
    } else if (cm.cc.tx_mode!=TX_DVI) {
        HDMITX_SetAVIInfoFrame(vmode_out.vic, (cm.cc.tx_mode == TX_HDMI_RGB) ? F_MODE_RGB444 : F_MODE_YUV444, 0, 0, cm.cc.hdmi_itc, vm_conf.hdmitx_pixr_ifr);
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
    sc->hv_in_config.data = 0x00000000;
    sc->hv_in_config2.data = 0x00000000;
    sc->hv_in_config3.data = 0x00000000;
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

    if (init_flash() != 0) {
        printf("Error: could not find flash\n");
        return -1;
    }

    // Set defaults
    set_default_avconfig();
    memcpy(&cm.cc, &tc_default, sizeof(avconfig_t));
    memcpy(rc_keymap, rc_keymap_default, sizeof(rc_keymap));

    // Init menu
    init_menu();

    // Load initconfig and profile
    read_userdata(INIT_CONFIG_SLOT, 0);
    read_userdata(profile_sel, 0);

    // Setup test pattern
    get_vmode(VMODE_480p, &vmode_in, &vmode_out, &vm_conf);
    update_sc_config(&vmode_in, &vmode_out, &vm_conf, &cm.cc);

    // init always in HDMI mode (fixes yellow screen bug)
    TX_enable(TX_HDMI_RGB);

    // Setup remote keymap
    if (!(IORD_ALTERA_AVALON_PIO_DATA(PIO_1_BASE) & PB1_BIT))
        setup_rc();

    return 0;
}

void print_vm_stats() {
    int row = 0;

    if (!menu_active) {
        memset((void*)osd->osd_array.data, 0, sizeof(osd_char_array));
        read_userdata(profile_sel, 1);

        sniprintf((char*)osd->osd_array.data[row][0], OSD_CHAR_COLS, "Mode preset:");
        sniprintf((char*)osd->osd_array.data[row][1], OSD_CHAR_COLS, "%s", vmode_out.name);
        sniprintf((char*)osd->osd_array.data[++row][0], OSD_CHAR_COLS, "Refresh rate:");
        sniprintf((char*)osd->osd_array.data[row][1], OSD_CHAR_COLS, "%u.%.2uHz", vmode_out.timings.v_hz_x100/100, vmode_out.timings.v_hz_x100%100);
        sniprintf((char*)osd->osd_array.data[++row][0], OSD_CHAR_COLS, "H/V synclen:");
        sniprintf((char*)osd->osd_array.data[row][1], OSD_CHAR_COLS, "%-5u %-5u", vmode_out.timings.h_synclen, vmode_out.timings.v_synclen);
        sniprintf((char*)osd->osd_array.data[++row][0], OSD_CHAR_COLS, "H/V backporch:");
        sniprintf((char*)osd->osd_array.data[row][1], OSD_CHAR_COLS, "%-5u %-5u", vmode_out.timings.h_backporch, vmode_out.timings.v_backporch);
        sniprintf((char*)osd->osd_array.data[++row][0], OSD_CHAR_COLS, "H/V active:");
        sniprintf((char*)osd->osd_array.data[row][1], OSD_CHAR_COLS, "%-5u %-5u", vmode_out.timings.h_active, vmode_out.timings.v_active);
        sniprintf((char*)osd->osd_array.data[++row][0], OSD_CHAR_COLS, "H/V total:");
        sniprintf((char*)osd->osd_array.data[row][1], OSD_CHAR_COLS, "%-5u %-5u", vmode_out.timings.h_total, vmode_out.timings.v_total);
        row++;

        sniprintf((char*)osd->osd_array.data[++row][0], OSD_CHAR_COLS, "Profile:");
        sniprintf((char*)osd->osd_array.data[row][1], OSD_CHAR_COLS, "%u: %s", profile_sel, (target_profile_name[0] == 0) ? "<empty>" : target_profile_name);
        sniprintf((char*)osd->osd_array.data[++row][0], OSD_CHAR_COLS, "Firmware:");
        sniprintf((char*)osd->osd_array.data[row][1], OSD_CHAR_COLS, "%u.%.2u" FW_SUFFIX1 FW_SUFFIX2 " @ " __DATE__, FW_VER_MAJOR, FW_VER_MINOR);

        osd->osd_config.status_refresh = 1;
        osd->osd_row_color.mask = 0;
        osd->osd_sec_enable[0].mask = (1<<(row+1))-1;
        osd->osd_sec_enable[1].mask = (1<<(row+1))-1;
    }
}

int latency_test() {
    lt_status_reg lt_status;
    alt_u16 latency_ms_x100, stb_ms_x100;
    alt_u32 btn_vec, btn_vec_prev=1;
    alt_u8 position = lt_sel+1;

    sys_ctrl |= LT_ACTIVE|(position<<LT_MODE_OFFS);
    IOWR_ALTERA_AVALON_PIO_DATA(PIO_0_BASE, sys_ctrl);
    sniprintf(menu_row2, LCD_ROW_LEN+1, "OK to init");
    ui_disp_menu(0);

    while (1) {
        btn_vec = IORD_ALTERA_AVALON_PIO_DATA(PIO_1_BASE) & RC_MASK;

        if ((btn_vec_prev == 0) && (btn_vec != 0)) {
            if (btn_vec == rc_keymap[RC_OK]) {
                sys_ctrl &= ~(3<<LT_MODE_OFFS);
                IOWR_ALTERA_AVALON_PIO_DATA(PIO_0_BASE, sys_ctrl);
                menu_row2[0] = 0;
                ui_disp_menu(0);
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
                ui_disp_menu(0);
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

int main()
{
    ths_input_t target_ths = 0;
    pcm_input_t target_pcm = 0;
    video_format target_format = 0;

    status_t status;

    alt_u32 input_vec;

    alt_timestamp_type start_ts;
    alt_timestamp_type auto_input_timestamp = 0;
    alt_u8 auto_input_changed = 0;
    alt_u8 auto_input_ctr = 0;
    alt_u8 auto_input_current_ctr = AUTO_CURRENT_MAX_COUNT;
    alt_u8 auto_input_keep_current = 0;

    int init_stat, man_input_change;

    // Start system timer
    alt_timestamp_start();

    init_stat = init_hw();

    if (init_stat >= 0) {
        printf("### DIY VIDEO DIGITIZER / SCANCONVERTER INIT OK ###\n\n");
        sniprintf(row1, LCD_ROW_LEN+1, "OSSC  fw. %u.%.2u" FW_SUFFIX1 FW_SUFFIX2, FW_VER_MAJOR, FW_VER_MINOR);
#ifndef DEBUG
        strncpy(row2, "2014-2023  marqs", LCD_ROW_LEN+1);
#else
        strncpy(row2, "** DEBUG BUILD *", LCD_ROW_LEN+1);
#endif
        ui_disp_status(1);
        usleep(500000);
    } else {
        sniprintf(row1, LCD_ROW_LEN+1, "Init error  %d", init_stat);
        strncpy(row2, "", LCD_ROW_LEN+1);
        ui_disp_status(1);
        while (1) {}
    }

    // Mainloop
    while(1) {
        start_ts = alt_timestamp();

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
        if ((auto_input != AUTO_OFF) && (cm.avinput != AV_TESTPAT) && !cm.sync_active && !menu_active
            && (alt_timestamp() >= auto_input_timestamp + 300 * (alt_timestamp_freq() >> 10)) && (auto_input_ctr < AUTO_MAX_COUNT)) {

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

            // set auto_input_timestamp
            auto_input_timestamp = alt_timestamp();
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
            tvp_powerdown();
#ifdef ENABLE_AUDIO
            DisableAudioOutput();
            if (pcm1862_active)
                pcm_source_sel(target_pcm);
#endif
            tvp_source_sel(target_tvp, target_tvp_sync, target_format);
            cm.clkcnt = 0; //TODO: proper invalidate
            sys_ctrl &= ~VSYNC_I_TYPE;
            if (target_format == FORMAT_RGBHV)
                sys_ctrl |= VSYNC_I_TYPE;
            IOWR_ALTERA_AVALON_PIO_DATA(PIO_0_BASE, sys_ctrl);
            strncpy(row1, avinput_str[cm.avinput], LCD_ROW_LEN+1);
            strncpy(row2, "    NO SYNC", LCD_ROW_LEN+1);
            ui_disp_status(1);
            if (man_input_change) {
                // record last input if it was selected manually
                if (def_input == AV_LAST)
                    write_userdata(INIT_CONFIG_SLOT);
                // Set auto_input_timestamp when input is manually changed
                auto_input_ctr = 0;
                auto_input_timestamp = alt_timestamp();
            }
        }

        // Check here to enable regardless of input
        if (tc.tx_mode != cm.cc.tx_mode) {
            HDMITX_SetAVIInfoFrame(vmode_out.vic, F_MODE_RGB444, 0, 0, 0, 0);
            TX_enable(tc.tx_mode);
            cm.cc.tx_mode = tc.tx_mode;
            cm.clkcnt = 0; //TODO: proper invalidate
        }
        if (tc.tx_mode != TX_DVI) {
            if (tc.hdmi_itc != cm.cc.hdmi_itc) {
                //EnableAVIInfoFrame(FALSE, NULL);
                printf("setting ITC to %d\n", tc.hdmi_itc);
                HDMITX_SetAVIInfoFrame(vmode_out.vic, (tc.tx_mode == TX_HDMI_RGB) ? F_MODE_RGB444 : F_MODE_YUV444, 0, 0, tc.hdmi_itc, vm_conf.hdmitx_pixr_ifr);
                cm.cc.hdmi_itc = tc.hdmi_itc;
            }
            if (tc.hdmi_hdr != cm.cc.hdmi_hdr) {
                printf("setting HDR flag to %d\n", tc.hdmi_hdr);
                HDMITX_SetHDRInfoFrame(tc.hdmi_hdr ? 3 : 0);
                cm.cc.hdmi_hdr = tc.hdmi_hdr;
            }
        }
        if (tc.av3_alt_rgb != cm.cc.av3_alt_rgb) {
            printf("Changing AV3 RGB source\n");
            cm.cc.av3_alt_rgb = tc.av3_alt_rgb;
        }
        if ((!!osd_enable != osd->osd_config.enable) || (osd_status_timeout != osd->osd_config.status_timeout)) {
            osd->osd_config.enable = !!osd_enable;
            osd->osd_config.status_timeout = osd_status_timeout;
            if (menu_active) {
                remote_code = 0;
                render_osd_page();
                display_menu(1);
            }
        }

        if (cm.avinput != AV_TESTPAT) {
            status = get_status(target_tvp_sync);

            switch (status) {
            case ACTIVITY_CHANGE:
                if (cm.sync_active) {
                    printf("Sync up\n");
                    sys_ctrl |= VIDGEN_OFF;
                    IOWR_ALTERA_AVALON_PIO_DATA(PIO_0_BASE, sys_ctrl);
                    tvp_powerup();
                    program_mode();
#ifdef ENABLE_AUDIO
                    SetupAudio(cm.cc.tx_mode);
#endif
                } else {
                    printf("Sync lost\n");
                    cm.clkcnt = 0; //TODO: proper invalidate
                    tvp_powerdown();
                    //ths_source_sel(THS_STANDBY, 0);
                    strncpy(row1, avinput_str[cm.avinput], LCD_ROW_LEN+1);
                    strncpy(row2, "    NO SYNC", LCD_ROW_LEN+1);
                    ui_disp_status(1);
                    // Set auto_input_timestamp
                    auto_input_timestamp = alt_timestamp();
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
                    update_sc_config(&vmode_in, &vmode_out, &vm_conf, &cm.cc);
                }
                break;
            default:
                break;
            }
        }

        while (alt_timestamp() < start_ts + MAINLOOP_INTERVAL_US*(TIMER_0_FREQ/1000000)) {}

        // restart timer if past half-range
        if (start_ts > 0x7fffffff) {
            alt_timestamp_start();
            if (auto_input_timestamp > start_ts)
                auto_input_timestamp -= start_ts;
            else
                auto_input_timestamp = 0;
            //printf("Timer restart\n");
        }
    }

    return 0;
}
