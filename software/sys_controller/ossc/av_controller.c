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

#define STABLE_THOLD            1
#define MIN_LINES_PROGRESSIVE   200
#define MIN_LINES_INTERLACED    400
#define SYNC_LOCK_THOLD         3
#define SYNC_LOSS_THOLD         -5
#define STATUS_TIMEOUT          10000

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
extern avconfig_t tc;

alt_u8 target_typemask;
alt_u8 target_type;
alt_u8 stable_frames;
alt_u8 update_cur_vm;

alt_u8 vm_sel, vm_edit, profile_sel, input_profiles[3], lt_sel;
alt_u16 tc_h_samplerate, tc_h_synclen, tc_h_bporch, tc_h_active, tc_v_synclen, tc_v_bporch, tc_v_active;

char row1[LCD_ROW_LEN+1], row2[LCD_ROW_LEN+1], menu_row1[LCD_ROW_LEN+1], menu_row2[LCD_ROW_LEN+1];

extern alt_u8 menu_active;
avinput_t target_mode;
phyinput_t phy_input_sel;

alt_u8 pcm1862_active;


inline void lcd_write_menu()
{
    lcd_write((char*)&menu_row1, (char*)&menu_row2);
}

inline void lcd_write_status() {
    lcd_write((char*)&row1, (char*)&row2);
}

#ifdef ENABLE_AUDIO
inline void SetupAudio(tx_mode_t mode)
{
    // shut down audio-tx before setting new config (recommended for changing audio-tx config)
    DisableAudioOutput();
    EnableAudioInfoFrame(FALSE, NULL);

    if (mode == TX_HDMI) {
        alt_u32 pclk_out = (TVP_EXTCLK_HZ/cm.clkcnt)*video_modes[cm.id].h_total*cm.sample_mult*(cm.fpga_vmultmode+1);

        pclk_out *= 1+cm.tx_pixelrep;

        printf("PCLK_out: %luHz\n", pclk_out);
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
    EnableVideoOutput(PCLK_MEDIUM, COLOR_RGB444, COLOR_RGB444, !mode);

    if (mode == TX_HDMI) {
        HDMITX_SetAVIInfoFrame(HDMI_Unkown, 0, 0, tc.hdmi_itc, cm.hdmitx_pixr_ifr);
        cm.cc.hdmi_itc = tc.hdmi_itc;
    }

#ifdef ENABLE_AUDIO
    SetupAudio(mode);
#endif

    // start TX
    SetAVMute(FALSE);
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
            ths_set_lpf(THS_LPF_BYPASS);
            break;
        case VIDEO_EDTV:
            tvp_set_lpf(0);
            ths_set_lpf(1);
            break;
        case VIDEO_SDTV:
        case VIDEO_LDTV:
        default:
            tvp_set_lpf(0);
            ths_set_lpf(0);
            break;
        }
    } else {
        tvp_set_lpf((tc.video_lpf == 2) ? 0x0F : 0);
        ths_set_lpf((tc.video_lpf > 2) ? (VIDEO_LPF_MAX-tc.video_lpf) : THS_LPF_BYPASS);
    }
}

inline int check_linecnt(alt_u8 progressive, alt_u32 totlines) {
    if (progressive)
        return (totlines >= MIN_LINES_PROGRESSIVE);
    else
        return (totlines >= MIN_LINES_INTERLACED);
}

// Check if input video status / target configuration has changed
status_t get_status(tvp_input_t input, video_format format)
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
        if (!(IORD_ALTERA_AVALON_PIO_DATA(PIO_2_BASE) & (1<<31))) {
            //printf("ctrval %u\n", ctr);
            break;
        }
    }

    sync_active = tvp_check_sync(input, format);
    vsyncmode = cm.sync_active ? ((IORD_ALTERA_AVALON_PIO_DATA(PIO_2_BASE) >> 16) & 0x3) : 0;

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
    totlines_tvp = ((IORD_ALTERA_AVALON_PIO_DATA(PIO_2_BASE) >> 18) & 0x7ff)+1;

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
            (tc.vga_ilace_fix != cm.cc.vga_ilace_fix))
            status = (status < MODE_CHANGE) ? MODE_CHANGE : status;

        if ((tc.s480p_mode != cm.cc.s480p_mode) && (video_modes[cm.id].v_total == 525))
            status = (status < MODE_CHANGE) ? MODE_CHANGE : status;

        if (update_cur_vm) {
            tvp_setup_hpll(cm.sample_mult*video_modes[cm.id].h_total, clkcnt, cm.cc.tvp_hpll2x && (video_modes[cm.id].flags & MODE_PLLDIVBY2));
            status = (status < INFO_CHANGE) ? INFO_CHANGE : status;
        }

        cm.totlines = totlines;
        cm.clkcnt = clkcnt;
        cm.progressive = progressive;
    }

    if ((tc.sl_mode != cm.cc.sl_mode) ||
        (tc.sl_type != cm.cc.sl_type) ||
        (tc.sl_str != cm.cc.sl_str) ||
        (tc.sl_id != cm.cc.sl_id) ||
        (tc.h_mask != cm.cc.h_mask) ||
        (tc.v_mask != cm.cc.v_mask) ||
        (tc.mask_br != cm.cc.mask_br) ||
        (tc.ar_256col != cm.cc.ar_256col) ||
        (tc.reverse_lpf != cm.cc.reverse_lpf))
        status = (status < INFO_CHANGE) ? INFO_CHANGE : status;

    if (tc.sampler_phase != cm.cc.sampler_phase) {
        cm.sample_sel = tvp_set_hpll_phase(tc.sampler_phase, cm.sample_mult);
        status = (status < INFO_CHANGE) ? INFO_CHANGE : status;
    }

    if (tc.sync_vth != cm.cc.sync_vth)
        tvp_set_sog_thold(tc.sync_vth);

    if (tc.linelen_tol != cm.cc.linelen_tol)
        tvp_set_linelen_tol(tc.linelen_tol);

    if (tc.vsync_thold != cm.cc.vsync_thold)
        tvp_set_ssthold(tc.vsync_thold);

    if ((tc.pre_coast != cm.cc.pre_coast) || (tc.post_coast != cm.cc.post_coast))
        tvp_set_hpllcoast(tc.pre_coast, tc.post_coast);

    if (tc.ypbpr_cs != cm.cc.ypbpr_cs)
        tvp_sel_csc(&csc_coeffs[tc.ypbpr_cs]);

    if (tc.video_lpf != cm.cc.video_lpf)
        set_lpf(tc.video_lpf);

    if (tc.sync_lpf != cm.cc.sync_lpf)
        tvp_set_sync_lpf(tc.sync_lpf);

    if (!memcmp(&tc.col, &cm.cc.col, sizeof(color_setup_t)))
        tvp_set_fine_gain_offset(&cm.cc.col);

#ifdef ENABLE_AUDIO
    if ((tc.audio_dw_sampl != cm.cc.audio_dw_sampl) ||
#ifdef MANUAL_CTS
        (tc.edtv_l2x != cm.cc.edtv_l2x) ||
        (tc.interlace_pt != cm.cc.interlace_pt) ||
        update_cur_vm ||
#endif
        (tc.audio_swap_lr != cm.cc.audio_swap_lr))
        SetupAudio(tc.tx_mode);
#endif

    cm.cc = tc;
    update_cur_vm = 0;

    return status;
}

// h_info:     [31:30]           [29]     [28]  [27:20]          [19:11]            [10:0]
//           | H_MULTMODE[1:0] | H_L5FMT |    | H_SYNCLEN[7:0] | H_BACKPORCH[8:0] | H_ACTIVE[10:0] |
//
// h_info2:   [31:29]  [28:19]       [18:16]            [15:13]                 [12:10]                  [9:0]
//           |       | H_MASK[9:0] | H_OPT_SCALE[2:0] | H_OPT_SAMPLE_SEL[2:0] | H_OPT_SAMPLE_MULT[2:0] | H_OPT_STARTOFF[9:0] |
//
// v_info:     [31:29]           [28:27]               [26]           [25:20]       [19:17]          [16:11]            [10:0]
//           | V_MULTMODE[2:0] | V_SCANLINEMODE[1:0] | V_SCANLINEID | V_MASK[5:0] | V_SYNCLEN[2:0] | V_BACKPORCH[5:0] | V_ACTIVE[10:0] |
//
// extra:      [31:13]  [12:8]          [7:4]            [3:0]
//           |        | X_REV_LPF_STR | H_MASK_BR[3:0] | H_SCANLINESTR[3:0] |
//
void set_videoinfo()
{
    alt_u8 sl_mode_fpga;
    alt_u8 h_opt_scale = cm.sample_mult;
    alt_u16 h_opt_startoffs = 0;
    alt_u16 h_synclen = video_modes[cm.id].h_synclen;
    alt_u16 h_border, h_mask;
    alt_u16 v_active = video_modes[cm.id].v_active;
    alt_u16 v_backporch = video_modes[cm.id].v_backporch;

    if (cm.cc.sl_mode == 2) {    //manual
        sl_mode_fpga = 1+cm.cc.sl_type;
    } else if (cm.cc.sl_mode == 1) {   //auto
        if (video_modes[cm.id].flags & MODE_INTERLACED)
            sl_mode_fpga = cm.fpga_vmultmode ? FPGA_SCANLINEMODE_ALT : FPGA_SCANLINEMODE_OFF;
        else if ((cm.fpga_vmultmode) && (video_modes[cm.id].group != GROUP_480P))
            sl_mode_fpga = FPGA_SCANLINEMODE_H;
        else
            sl_mode_fpga = FPGA_SCANLINEMODE_OFF;
    } else {
        sl_mode_fpga = FPGA_SCANLINEMODE_OFF;
    }

    switch (cm.target_lm) {
        case MODE_L2_320_COL:
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
        case MODE_L4_320_COL:
            h_opt_scale = 4;
            break;
        case MODE_L4_256_COL:
            h_opt_scale = 5-cm.cc.ar_256col;
            break;
        case MODE_L5_GEN_4_3:
            if (cm.cc.l5_fmt == L5FMT_1920x1080) {
                v_active -= 24;
                v_backporch += 12;
            }
            break;
        case MODE_L5_320_COL:
            h_opt_scale = 5;
            if (cm.cc.l5_fmt == L5FMT_1920x1080) {
                v_active -= 24;
                v_backporch += 12;
            }
            break;
        case MODE_L5_256_COL:
            h_opt_scale = 6-cm.cc.ar_256col;
            if (cm.cc.l5_fmt == L5FMT_1920x1080) {
                v_active -= 24;
                v_backporch += 12;
            }
            break;
        default:
            break;
    }

    // CEA-770.3 HDTV modes use tri-level syncs which have twice the width of bi-level syncs of corresponding CEA-861 modes
    if (target_type == VIDEO_HDTV)
        h_synclen *= 2;

    h_border = (((cm.sample_mult-h_opt_scale)*video_modes[cm.id].h_active)/2);
    h_mask = h_border + h_opt_scale*cm.cc.h_mask;
    h_opt_startoffs = h_border + (cm.sample_mult-h_opt_scale)*(h_synclen+(alt_u16)video_modes[cm.id].h_backporch);
    h_opt_startoffs = (h_opt_startoffs/cm.sample_mult)*cm.sample_mult;
    printf("h_border: %u, h_opt_startoffs: %u\n", h_border, h_opt_startoffs);

    IOWR_ALTERA_AVALON_PIO_DATA(PIO_3_BASE, (cm.fpga_hmultmode<<30) |
                                            ((cm.cc.l5_fmt!=L5FMT_1600x1200)<<29) |
                                            ((((cm.sample_mult*h_synclen)-cm.hsync_cut)&0xff)<<20) |
                                            (((cm.sample_mult*(alt_u16)video_modes[cm.id].h_backporch)&0x1ff)<<11) |
                                            ((cm.sample_mult*video_modes[cm.id].h_active)&0x7ff));
    IOWR_ALTERA_AVALON_PIO_DATA(PIO_4_BASE, (h_mask<<19) |
                                            (h_opt_scale<<16) |
                                            (cm.sample_sel<<13) |
                                            (cm.sample_mult<<10) |
                                            h_opt_startoffs);
    IOWR_ALTERA_AVALON_PIO_DATA(PIO_5_BASE, (cm.fpga_vmultmode<<29) |
                                            (sl_mode_fpga<<27) |
                                            (cm.cc.sl_id<<26) |
                                            (cm.cc.v_mask<<20) |
                                            (video_modes[cm.id].v_synclen<<17) |
                                            (v_backporch<<11) |
                                            v_active);
    IOWR_ALTERA_AVALON_PIO_DATA(PIO_6_BASE, (cm.cc.reverse_lpf<<8) |
                                            (cm.cc.mask_br<<4) |
                                            cm.cc.sl_str);
}

// Configure TVP7002 and scan converter logic based on the video mode
void program_mode()
{
    alt_u8 h_syncinlen, v_syncinlen;
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
    if (!menu_active)
        lcd_write_status();

    //printf ("Get mode id with %u %u %f\n", totlines, progressive, hz);
    cm.id = get_mode_id(cm.totlines, cm.progressive, v_hz_x100/100, target_typemask);

    if (cm.id == -1) {
        printf ("Error: no suitable mode found, defaulting to 240p\n");
        cm.id = 4;
    }
    vm_sel = cm.id;

    target_type = target_typemask & video_modes[cm.id].type;
    h_synclen_px = ((alt_u32)h_syncinlen * (alt_u32)video_modes[cm.id].h_total*cm.sample_mult) / cm.clkcnt;

    printf("Mode %s selected - hsync width: %upx\n", video_modes[cm.id].name, (unsigned)h_synclen_px);

    tvp_source_setup(target_type,
                     cm.sample_mult*video_modes[cm.id].h_total,
                     cm.clkcnt,
                     cm.cc.tvp_hpll2x && (video_modes[cm.id].flags & MODE_PLLDIVBY2),
                     (alt_u8)h_synclen_px,
                     cm.cc.pre_coast,
                     cm.cc.post_coast,
                     cm.cc.vsync_thold);
    set_lpf(cm.cc.video_lpf);
    cm.sample_sel = tvp_set_hpll_phase(cm.cc.sampler_phase, cm.sample_mult);

    set_videoinfo();

    TX_SetPixelRepetition(cm.tx_pixelrep, ((cm.cc.tx_mode==TX_HDMI) && (cm.tx_pixelrep == cm.hdmitx_pixr_ifr)) ? 1 : 0);

    // Full TX initialization increases mode switch delay, use only for compatibility
    if (cm.cc.full_tx_setup) {
        TX_enable(cm.cc.tx_mode);
    } else if (cm.cc.tx_mode==TX_HDMI) {
        HDMITX_SetAVIInfoFrame(HDMI_Unkown, 0, 0, cm.cc.hdmi_itc, cm.hdmitx_pixr_ifr);
#ifdef ENABLE_AUDIO
#ifdef MANUAL_CTS
        SetupAudio(cm.cc.tx_mode);
#endif
#endif
    }
}

int load_profile() {
    int retval;

    input_profiles[phy_input_sel] = profile_sel;
    retval = read_userdata(profile_sel);
    if (retval == 0)
        write_userdata(INIT_CONFIG_SLOT);
    return retval;
}

int save_profile() {
    int retval;

    input_profiles[phy_input_sel] = profile_sel;
    retval = write_userdata(profile_sel);
    if (retval == 0)
        write_userdata(INIT_CONFIG_SLOT);
    return retval;
}

void vm_select() {
    vm_edit = vm_sel;
    tc_h_samplerate = video_modes[vm_edit].h_total;
    tc_h_synclen = (alt_u16)video_modes[vm_edit].h_synclen;
    tc_h_bporch = (alt_u16)video_modes[vm_edit].h_backporch;
    tc_h_active = video_modes[vm_edit].h_active;
    tc_v_synclen = (alt_u16)video_modes[vm_edit].v_synclen;
    tc_v_bporch = (alt_u16)video_modes[vm_edit].v_backporch;
    tc_v_active = video_modes[vm_edit].v_active;
}

void vm_tweak(alt_u16 v) {
    if (cm.sync_active && (cm.id == vm_edit)) {
        if ((video_modes[cm.id].h_total != tc_h_samplerate) ||
            (video_modes[cm.id].h_synclen != tc_h_synclen) ||
            (video_modes[cm.id].h_backporch != (alt_u8)tc_h_bporch) ||
            (video_modes[cm.id].h_active != tc_h_active) ||
            (video_modes[cm.id].v_synclen != tc_v_synclen) ||
            (video_modes[cm.id].v_backporch != (alt_u8)tc_v_bporch) ||
            (video_modes[cm.id].v_active != tc_v_active))
            update_cur_vm = 1;
    }
    video_modes[vm_edit].h_total = tc_h_samplerate;
    video_modes[vm_edit].h_synclen = (alt_u8)tc_h_synclen;
    video_modes[vm_edit].h_backporch = (alt_u8)tc_h_bporch;
    video_modes[vm_edit].h_active = tc_h_active;
    video_modes[vm_edit].v_synclen = (alt_u8)tc_v_synclen;
    video_modes[vm_edit].v_backporch = (alt_u8)tc_v_bporch;
    video_modes[vm_edit].v_active = tc_v_active;

    sniprintf(menu_row2, LCD_ROW_LEN+1, "%u", v);
}

// Initialize hardware
int init_hw()
{
    alt_u32 chiprev;

    // Reset hardware
    IOWR_ALTERA_AVALON_PIO_DATA(PIO_0_BASE, AV_RESET_N|LCD_BL);
    IOWR_ALTERA_AVALON_PIO_DATA(PIO_0_BASE, 0x0000);
    IOWR_ALTERA_AVALON_PIO_DATA(PIO_3_BASE, 0x00000000);
    IOWR_ALTERA_AVALON_PIO_DATA(PIO_5_BASE, 0x00000000);
    usleep(10000);

    // unreset hw
    sys_ctrl = AV_RESET_N|LCD_BL|SD_SPI_SS_N|LCD_CS_N;
    IOWR_ALTERA_AVALON_PIO_DATA(PIO_0_BASE, sys_ctrl);

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
    memcpy(rc_keymap, rc_keymap_default, sizeof(rc_keymap));

    // Load initconfig and profile
    read_userdata(INIT_CONFIG_SLOT);
    read_userdata(profile_sel);

    // Setup remote keymap
    if (!(IORD_ALTERA_AVALON_PIO_DATA(PIO_1_BASE) & PB1_BIT))
        setup_rc();

    // init always in HDMI mode (fixes yellow screen bug)
    TX_enable(TX_HDMI);

    return 0;
}

#ifdef DEBUG
int latency_test()
{
    sniprintf(menu_row2, LCD_ROW_LEN+1, "Unavailable");
    lcd_write_menu();
    usleep(1000000);
    return -1;
}
#else
int latency_test() {
    alt_u32 lt_status, btn_vec, btn_vec_prev=1;
    alt_u16 latency_ms_x100, stb_ms_x100;
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
                //while (!((lt_status = IORD_ALTERA_AVALON_PIO_DATA(PIO_7_BASE)) & (1<<31))) {} //Hangs if sync is lost
                SPI_Timer_On(1000);
                while ((SPI_Timer_Status()==TRUE)) {
                    lt_status = IORD_ALTERA_AVALON_PIO_DATA(PIO_7_BASE);
                    if (lt_status & (1<<31))
                        break;
                }
                SPI_Timer_Off();
                latency_ms_x100 = lt_status & 0xffff;
                stb_ms_x100 = (lt_status >> 16) & 0xfff;
                if ((latency_ms_x100 == 0) || (latency_ms_x100 == 0xffff))
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
#endif

// Enable chip outputs
void enable_outputs()
{
    // program video mode
    program_mode();
    // enable TVP output
    tvp_enable_output();

    // enable and unmute TX
    TX_enable(tc.tx_mode);
}

int main()
{
    tvp_input_t target_input = 0;
    ths_input_t target_ths = 0;
    pcm_input_t target_pcm = 0;
    video_format target_format = 0;

    alt_u8 av_init = 0;
    status_t status;

    alt_u32 input_vec;

    int init_stat;

    init_stat = init_hw();

    if (init_stat >= 0) {
        printf("### DIY VIDEO DIGITIZER / SCANCONVERTER INIT OK ###\n\n");
        sniprintf(row1, LCD_ROW_LEN+1, "OSSC  fw. %u.%.2u" FW_SUFFIX1 FW_SUFFIX2, FW_VER_MAJOR, FW_VER_MINOR);
#ifndef DEBUG
        strncpy(row2, "2014-2017  marqs", LCD_ROW_LEN+1);
#else
        strncpy(row2, "** DEBUG BUILD *", LCD_ROW_LEN+1);
#endif
        lcd_write_status();
        usleep(500000);
    } else {
        sniprintf(row1, LCD_ROW_LEN+1, "Init error  %d", init_stat);
        strncpy(row2, "", LCD_ROW_LEN+1);
        lcd_write_status();
        while (1) {}
    }

    if (tc.def_input < AV_LAST)
        target_mode = tc.def_input;

    // Mainloop
    while(1) {
        // Read remote control and PCB button status
        input_vec = IORD_ALTERA_AVALON_PIO_DATA(PIO_1_BASE);
        remote_code = input_vec & RC_MASK;
        btn_code = ~input_vec & PB_MASK;
        remote_rpt = input_vec >> 24;

        if ((remote_rpt == 0) || ((remote_rpt > 1) && (remote_rpt < 6)) || (remote_rpt == remote_rpt_prev))
            remote_code = 0;

        parse_control();

        if (menu_active)
            display_menu(0);

        if (target_mode == cm.avinput)
            target_mode = AV_KEEP;

        switch (target_mode) {
        case AV1_RGBs:
            target_input = TVP_INPUT1;
            target_format = FORMAT_RGBS;
            target_typemask = VIDEO_LDTV|VIDEO_SDTV|VIDEO_EDTV|VIDEO_HDTV;
            target_ths = THS_INPUT_B;
            target_pcm = PCM_INPUT4;
            break;
        case AV1_RGsB:
            target_input = TVP_INPUT1;
            target_format = FORMAT_RGsB;
            target_typemask = VIDEO_LDTV|VIDEO_SDTV|VIDEO_EDTV|VIDEO_HDTV;
            target_ths = THS_INPUT_B;
            target_pcm = PCM_INPUT4;
            break;
        case AV1_YPBPR:
            target_input = TVP_INPUT1;
            target_format = FORMAT_YPbPr;
            target_typemask = VIDEO_LDTV|VIDEO_SDTV|VIDEO_EDTV|VIDEO_HDTV;
            target_ths = THS_INPUT_B;
            target_pcm = PCM_INPUT4;
            break;
        case AV2_YPBPR:
            target_input = TVP_INPUT1;
            target_format = FORMAT_YPbPr;
            target_typemask = VIDEO_LDTV|VIDEO_SDTV|VIDEO_EDTV|VIDEO_HDTV;
            target_ths = THS_INPUT_A;
            target_pcm = PCM_INPUT3;
            break;
        case AV2_RGsB:
            target_input = TVP_INPUT1;
            target_format = FORMAT_RGsB;
            target_typemask = VIDEO_LDTV|VIDEO_SDTV|VIDEO_EDTV|VIDEO_HDTV;
            target_ths = THS_INPUT_A;
            target_pcm = PCM_INPUT3;
            break;
        case AV3_RGBHV:
            target_input = TVP_INPUT3;
            target_format = FORMAT_RGBHV;
            target_typemask = VIDEO_PC;
            target_ths = THS_STANDBY;
            target_pcm = PCM_INPUT2;
            break;
        case AV3_RGBs:
            target_input = TVP_INPUT3;
            target_format = FORMAT_RGBS;
            target_typemask = VIDEO_LDTV|VIDEO_SDTV|VIDEO_EDTV|VIDEO_HDTV;
            target_ths = THS_STANDBY;
            target_pcm = PCM_INPUT2;
            break;
        case AV3_RGsB:
            target_input = TVP_INPUT3;
            target_format = FORMAT_RGsB;
            target_typemask = VIDEO_LDTV|VIDEO_SDTV|VIDEO_EDTV|VIDEO_HDTV;
            target_ths = THS_STANDBY;
            target_pcm = PCM_INPUT2;
            break;
        case AV3_YPBPR:
            target_input = TVP_INPUT3;
            target_format = FORMAT_YPbPr;
            target_typemask = VIDEO_LDTV|VIDEO_SDTV|VIDEO_EDTV|VIDEO_HDTV;
            target_ths = THS_STANDBY;
            target_pcm = PCM_INPUT2;
            break;
        default:
            break;
        }

        if (target_mode != AV_KEEP) {
            printf("### SWITCH MODE TO %s ###\n", avinput_str[target_mode]);

            phy_input_sel = avinput_to_phyinput[target_mode];

            // The input changed, so load the appropriate profile
            if (profile_sel != input_profiles[phy_input_sel]) {
                profile_sel = input_profiles[phy_input_sel];
                read_userdata(profile_sel);
            }

            cm.avinput = target_mode;
            cm.sync_active = 0;
            ths_source_sel(target_ths, (cm.cc.video_lpf > 1) ? (VIDEO_LPF_MAX-cm.cc.video_lpf) : THS_LPF_BYPASS);
            tvp_disable_output();
#ifdef ENABLE_AUDIO
            DisableAudioOutput();
            if (pcm1862_active)
                pcm_source_sel(target_pcm);
#endif
            tvp_source_sel(target_input, target_format);
            cm.clkcnt = 0; //TODO: proper invalidate
            strncpy(row1, avinput_str[cm.avinput], LCD_ROW_LEN+1);
            strncpy(row2, "    NO SYNC", LCD_ROW_LEN+1);
            if (!menu_active)
                lcd_write_status();
            if (av_init && (tc.def_input == AV_LAST))
                write_userdata(INIT_CONFIG_SLOT);
            av_init = 1;
        }

        // Check here to enable regardless of av_init
        if (tc.tx_mode != cm.cc.tx_mode) {
            HDMITX_SetAVIInfoFrame(HDMI_Unkown, 0, 0, 0, 0);
            TX_enable(tc.tx_mode);
            cm.cc.tx_mode = tc.tx_mode;
            cm.clkcnt = 0; //TODO: proper invalidate
        }
        if ((tc.tx_mode == TX_HDMI) && (tc.hdmi_itc != cm.cc.hdmi_itc)) {
            //EnableAVIInfoFrame(FALSE, NULL);
            printf("setting ITC to %d\n", tc.hdmi_itc);
            HDMITX_SetAVIInfoFrame(HDMI_Unkown, 0, 0, tc.hdmi_itc, cm.hdmitx_pixr_ifr);
            cm.cc.hdmi_itc = tc.hdmi_itc;
        }

        if (av_init) {
            status = get_status(target_input, target_format);

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
                    if (!menu_active)
                        lcd_write_status();
                }
                break;
            case MODE_CHANGE:
                if (cm.sync_active) {
                    printf("Mode change\n");
                    program_mode();
                }
                break;
            case INFO_CHANGE:
                if (cm.sync_active) {
                    printf("Info change\n");
                    set_videoinfo();
                }
                break;
            default:
                break;
            }
        }

        btn_code_prev = btn_code;
        remote_rpt_prev = remote_rpt;
        target_mode = AV_KEEP;
        usleep(300);    // Avoid executing mainloop multiple times per vsync
    }

    return 0;
}
