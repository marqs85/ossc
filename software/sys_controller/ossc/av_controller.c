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
#include "string.h"
#include "altera_avalon_pio_regs.h"
#include "Altera_UP_SD_Card_Avalon_Interface_mod.h"
#include "altera_epcq_controller_mod.h"
#include "i2c_opencores.h"
#include "tvp7002.h"
#include "ths7353.h"
#include "video_modes.h"
#include "lcd.h"
#include "sysconfig.h"
#include "it6613.h"
#include "it6613_sys.h"
#include "HDMI_TX.h"
#include "hdmitx.h"
#include "ci_crc.h"
#include "cfg.h"
#include "av_controller.h"
#include "controls.h"
#include "memory.h"
#include "menu.h"

#define LINECNT_THOLD             1
#define STABLE_THOLD              1
#define MIN_LINES_PROGRESSIVE   200
#define MIN_LINES_INTERLACED    400
#define SYNC_LOSS_THOLD           5
#define STATUS_TIMEOUT        10000

extern alt_u16 rc_keymap[REMOTE_MAX_KEYS];

extern const alt_u32 clkrate[];

const char const *avinput_str[] = { "NO MODE", "AV1: RGBS", "AV1: RGsB", "AV2: YPbPr", "AV2: RGsB", "AV3: RGBHV", "AV3: RGBS", "AV3: RGsB" };

// Target configuration
avconfig_t tc;

// Current mode
avmode_t cm;

extern mode_data_t video_modes[];
extern ypbpr_to_rgb_csc_t csc_coeffs[];

alt_u8 target_typemask;
alt_u8 target_type;
alt_u8 stable_frames;

char row1[LCD_ROW_LEN+1], row2[LCD_ROW_LEN+1], menu_row1[LCD_ROW_LEN+1], menu_row2[LCD_ROW_LEN+1];


short int sd_fw_handle;

extern volatile alt_u8 menu_active;
extern volatile alt_u32 remote_code;
extern volatile alt_u8 remote_rpt, remote_rpt_prev;
extern volatile alt_u32 btn_code, btn_code_prev;


void TX_enable(tx_mode_t mode)
{
    //SetAVMute(TRUE);
    DisableVideoOutput();
    EnableAVIInfoFrame(FALSE, NULL);

    EnableVideoOutput(PCLK_MEDIUM, COLOR_RGB444, COLOR_RGB444, mode == TX_HDMI);
    if (!(IORD_ALTERA_AVALON_PIO_DATA(PIO_1_BASE) & HDMITX_MODE_MASK) && mode == TX_HDMI)
        HDMITX_SetAVIInfoFrame(HDMI_480p60, F_MODE_RGB444, 0, 0);

    // start Tx
    SetAVMute(FALSE);
}


void set_lpf(alt_u8 lpf)
{
    alt_u32 pclk;
    pclk = (clkrate[cm.refclk]/cm.clkcnt)*video_modes[cm.id].h_total;
    printf("PCLK: %uHz\n", pclk);

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
            tvp_set_lpf(0);
            ths_set_lpf(0);
            break;
        default:
            break;
        }
    } else {
        tvp_set_lpf((tc.video_lpf == 2) ? 0x0F : 0);
        ths_set_lpf((tc.video_lpf > 2) ? (VIDEO_LPF_MAX-tc.video_lpf) : THS_LPF_BYPASS);
    }
}

// Check if input video status / target configuration has changed
status_t get_status(tvp_input_t input)
{
    alt_u32 data1, data2;
    alt_u32 totlines, clkcnt;
    alt_u8 progressive;
    //alt_u8 refclk;
    alt_u8 sync_active;
    alt_u8 vsyncmode;
    alt_u16 fpga_totlines;
    status_t status;
    static alt_u8 act_ctr;
    alt_u32 ctr;

    status = NO_CHANGE;

    // Wait until vsync active (avoid noise coupled to I2C bus on earlier prototypes)
    for (ctr=0; ctr<STATUS_TIMEOUT; ctr++) {
        if (!(IORD_ALTERA_AVALON_PIO_DATA(PIO_4_BASE) & (1<<31))) {
            //printf("ctrval %u\n", ctr);
            break;
        }
    }

    sync_active = tvp_check_sync(input);
    vsyncmode = IORD_ALTERA_AVALON_PIO_DATA(PIO_4_BASE) >> 16;

    // TVP7002 may randomly report "no sync" (especially with arcade boards),
    // thus disable output only after N consecutive "no sync"-events
    if (!cm.sync_active && sync_active) {
        cm.sync_active = sync_active;
        status = ACTIVITY_CHANGE;
        act_ctr = 0;
    } else if (cm.sync_active && !sync_active) {
        printf("Sync down in %u...\n", SYNC_LOSS_THOLD-act_ctr);
        if (act_ctr >= SYNC_LOSS_THOLD) {
            act_ctr = 0;
            cm.sync_active = sync_active;
            status = ACTIVITY_CHANGE;
        } else {
            act_ctr++;
        }
    } else {
        act_ctr = 0;
    }


    data1 = tvp_readreg(TVP_LINECNT1);
    data2 = tvp_readreg(TVP_LINECNT2);
    totlines = ((data2 & 0x0f) << 8) | data1;
    progressive = !!(data2 & (1<<5));
    cm.macrovis = !!(data2 & (1<<6));

    fpga_totlines = IORD_ALTERA_AVALON_PIO_DATA(PIO_4_BASE) & 0xffff;

    //TODO: check flags instead
    if (vsyncmode == 0x2) {
        progressive = 1;
    } else if ((vsyncmode == 0x1) && fpga_totlines > ((totlines-1)*2)) {
        progressive = 0;
        totlines = fpga_totlines; //ugly hack
    }

    data1 = tvp_readreg(TVP_CLKCNT1);
    data2 = tvp_readreg(TVP_CLKCNT2);
    clkcnt = ((data2 & 0x0f) << 8) | data1;

    if ((progressive && (totlines > MIN_LINES_PROGRESSIVE)) || (!progressive && (totlines > MIN_LINES_INTERLACED))) {
        if ((abs((alt_16)totlines - (alt_16)cm.totlines) > LINECNT_THOLD) || (clkcnt != cm.clkcnt) || (progressive != cm.progressive)) {
            printf("totlines: %u (cur) / %u (prev), clkcnt: %u (cur) / %u (prev). Data1: 0x%.2x, Data2: 0x%.2x\n", (unsigned)totlines, (unsigned)cm.totlines, (unsigned)clkcnt, (unsigned)cm.clkcnt, (unsigned)data1, (unsigned)data2);
            stable_frames = 0;
        } else if (stable_frames != STABLE_THOLD) {
            stable_frames++;
            if (stable_frames == STABLE_THOLD)
                status = (status < MODE_CHANGE) ? MODE_CHANGE : status;
        }

        
        if ((tc.linemult_target != cm.cc.linemult_target) ||
	    (tc.l3_mode != cm.cc.l3_mode) ||
	    ((tc.s480p_mode != cm.cc.s480p_mode) && (video_modes[cm.id].flags & (MODE_DTV480P|MODE_VGA480P))))
            status = (status < MODE_CHANGE) ? MODE_CHANGE : status;


        if (tc.disable_alc != cm.cc.disable_alc)
            tvp_set_alc(tc.disable_alc, target_type);


        cm.totlines = totlines;
        cm.clkcnt = clkcnt;
        cm.progressive = progressive;
    }

    if ((tc.sl_mode != cm.cc.sl_mode) ||
        (tc.sl_str != cm.cc.sl_str) ||
        (tc.sl_id != cm.cc.sl_id) ||
        (tc.h_mask != cm.cc.h_mask) ||
        (tc.v_mask != cm.cc.v_mask))
        status = (status < INFO_CHANGE) ? INFO_CHANGE : status;

    if (tc.sampler_phase != cm.cc.sampler_phase)
        tvp_set_hpll_phase(tc.sampler_phase-SAMPLER_PHASE_MIN);

    if (tc.sync_thold != cm.cc.sync_thold)
        tvp_set_sog_thold(tc.sync_thold-SYNC_THOLD_MIN);

    if ((tc.pre_coast != cm.cc.pre_coast) || (tc.post_coast != cm.cc.post_coast))
        tvp_set_hpllcoast(tc.pre_coast, tc.post_coast);

    if (tc.ypbpr_cs != cm.cc.ypbpr_cs)
        tvp_sel_csc(&csc_coeffs[tc.ypbpr_cs]);

    if (tc.video_lpf != cm.cc.video_lpf)
        set_lpf(tc.video_lpf);

    if (tc.sync_lpf != cm.cc.sync_lpf)
        tvp_set_sync_lpf(tc.sync_lpf);

    // use memcpy instead?
    cm.cc = tc;

    return status;
}

// h_info:     [31:30]           [29:28]         [27:22]      [21]  [20:10]          [7:0]
//           | H_LINEMULT[1:0] | H_L3MODE[1:0] | H_MASK[5:0] |    | H_ACTIVE[10:0] | H_BACKPORCH[7:0] |
//
// v_info:     [31]          [30]            [29]           [28:25]              [24:19]      [18]  [17:7]          [6]  [5:0]
//           | V_SCANLINES | V_SCANLINEDIR | V_SCANLINEID | V_SCANLINESTR[3:0] | V_MASK[5:0] |    | V_ACTIVE[10:0] |   | V_BACKPORCH[5:0] |
void set_videoinfo()
{
    alt_u8 slid_target;

    if (video_modes[cm.id].flags & MODE_L3ENABLE_MASK) {
        cm.linemult = 2;
        slid_target = cm.cc.sl_id ? 2 : 0;
    } else if (video_modes[cm.id].flags & MODE_L2ENABLE) {
        cm.linemult = 1;
        slid_target = cm.cc.sl_id;
    } else {
        cm.linemult = 0;
        slid_target = cm.cc.sl_id;
    }

    IOWR_ALTERA_AVALON_PIO_DATA(PIO_2_BASE, (cm.linemult<<30) | (cm.cc.l3_mode<<28) | (cm.cc.h_mask)<<22 | (video_modes[cm.id].h_active<<10) | video_modes[cm.id].h_backporch);
    IOWR_ALTERA_AVALON_PIO_DATA(PIO_3_BASE, ((!!cm.cc.sl_mode)<<31) | (cm.cc.sl_mode > 0 ? (cm.cc.sl_mode-1)<<30 : 0) | (slid_target<<19) | (cm.cc.sl_str<<25) | (cm.cc.v_mask<<19) | (video_modes[cm.id].v_active<<7) | video_modes[cm.id].v_backporch);
}

// Configure TVP7002 and scan converter logic based on the video mode
void program_mode()
{
    alt_u32 data1, data2;
    alt_u32 h_hz, v_hz_x100;

    // Mark as stable (needed after sync up to avoid unnecessary mode switch)
    stable_frames = STABLE_THOLD;

    if ((cm.clkcnt != 0) && (cm.totlines != 0)) { //prevent div by 0
        h_hz = clkrate[cm.refclk]/cm.clkcnt;
        v_hz_x100 = cm.progressive ? (100*clkrate[cm.refclk]/cm.clkcnt)/cm.totlines : (2*(100*clkrate[cm.refclk]/cm.clkcnt))/cm.totlines;
    } else {
        h_hz = 15700;
        v_hz_x100 = 6000;
    }

    printf("\nLines: %u %c\n", (unsigned)cm.totlines, cm.progressive ? 'p' : 'i');
    printf("Clocks per line: %u : HS %u.%.3u kHz  VS %u.%.2u Hz\n", (unsigned)cm.clkcnt, (unsigned)(h_hz/1000), (unsigned)(h_hz%1000), (unsigned)(v_hz_x100/100), (unsigned)(v_hz_x100%100));

    data1 = tvp_readreg(TVP_HSINWIDTH);
    data2 = tvp_readreg(TVP_VSINWIDTH);
    printf("Hswidth: %u  Vswidth: %u  Macrovision: %u\n", (unsigned)data1, (unsigned)(data2 & 0x1F), (unsigned)cm.macrovis);

    //TODO: rewrite with strncpy to reduce code size
    sniprintf(row1, LCD_ROW_LEN+1, "%s %u%c", avinput_str[cm.avinput], (unsigned)cm.totlines, cm.progressive ? 'p' : 'i');
    sniprintf(row2, LCD_ROW_LEN+1, "%u.%.2ukHz %u.%.2uHz", (unsigned)(h_hz/1000), (unsigned)((h_hz%1000)/10), (unsigned)(v_hz_x100/100), (unsigned)(v_hz_x100%100));
    //strncpy(row1, avinput_str[cm.avinput], LCD_ROW_LEN+1);
    //strncpy(row2, avinput_str[cm.avinput], LCD_ROW_LEN+1);
    if (!menu_active)
        lcd_write_status();

    //printf ("Get mode id with %u %u %f\n", totlines, progressive, hz);
    cm.id = get_mode_id(cm.totlines, cm.progressive, v_hz_x100/100, target_typemask, cm.cc.linemult_target, cm.cc.l3_mode, cm.cc.s480p_mode);

    if ( cm.id == -1) {
        printf ("Error: no suitable mode found, defaulting to 240p\n");
        cm.id = 4;
    }

    target_type = target_typemask & video_modes[cm.id].type;

    printf("Mode %s selected\n", video_modes[cm.id].name);

    tvp_source_setup(cm.id, target_type, cm.cc.disable_alc, (cm.progressive ? cm.totlines : cm.totlines/2), v_hz_x100/100, cm.refclk, cm.cc.pre_coast, cm.cc.post_coast);
    set_lpf(cm.cc.video_lpf);
    set_videoinfo();
}

// Initialize hardware
int init_hw()
{
    alt_u32 chiprev;

    // Reset error vector and scan converter
    IOWR_ALTERA_AVALON_PIO_DATA(PIO_0_BASE, 0x03);
    IOWR_ALTERA_AVALON_PIO_DATA(PIO_0_BASE, 0x00);
    IOWR_ALTERA_AVALON_PIO_DATA(PIO_2_BASE, 0x00000000);
    IOWR_ALTERA_AVALON_PIO_DATA(PIO_3_BASE, 0x00000000);
    usleep(10000);

    // unreset hw
    IOWR_ALTERA_AVALON_PIO_DATA(PIO_0_BASE, 0x03);

    //wait >500ms for SD card interface to be stable
    //over 200ms and LCD may be buggy?
    usleep(200000);

    // IT6613 officially supports only 100kHz, but 400kHz seems to work
    I2C_init(I2CA_BASE,ALT_CPU_FREQ,400000);


    set_default_avconfig();
    // safe?
    read_userdata();

    /* Initialize the character display */
    lcd_init();


    if (!ths_init()) {
        printf("Error: could not read from THS7353\n");
        return -2;
    }

    /* check if TVP is found */
    chiprev = tvp_readreg(TVP_CHIPREV);
    //printf("chiprev %d\n", chiprev);

    if ( chiprev == 0xff) {
        printf("Error: could not read from TVP7002\n");
        return -3;
    }

    tvp_init();

    chiprev = HDMITX_ReadI2C_Byte(IT_DEVICEID);

    if ( chiprev != 0x13) {
        printf("Error: could not read from IT6613\n");
        return -4;
    }

    InitIT6613();

    if (check_flash() != 0) {
        printf("Error: incorrect flash type detected\n");
        return -1;
    }

    // enforce DVI mode on non-DIY boards
    if ((IORD_ALTERA_AVALON_PIO_DATA(PIO_1_BASE) & HDMITX_MODE_MASK)) {
        cm.cc.tx_mode = TX_DVI;
        tc.tx_mode = TX_DVI;
    }

    if (!(IORD_ALTERA_AVALON_PIO_DATA(PIO_1_BASE) & PB1_BIT))
        setup_rc();

    // init always is HDMI mode (fixes yellow screen bug)
    TX_enable(TX_HDMI);
    if (cm.cc.tx_mode)
        TX_enable(cm.cc.tx_mode);

    SetAVMute(FALSE);
    return 0;
}

// Enable chip outputs
void enable_outputs()
{
    // program video mode
    program_mode();
    // enable TVP output
    tvp_enable_output();

    // enable and unmute HDMITX
    // TODO: check pclk
    TX_enable(cm.cc.tx_mode);
}

