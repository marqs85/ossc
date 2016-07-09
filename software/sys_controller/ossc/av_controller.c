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
#include "i2c_opencores.h"
#include "av_controller.h"
#include "tvp7002.h"
#include "ths7353.h"
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

#define LINECNT_THOLD           1
#define STABLE_THOLD            1
#define MIN_LINES_PROGRESSIVE   200
#define MIN_LINES_INTERLACED    400
#define SYNC_LOSS_THOLD         5
#define STATUS_TIMEOUT          10000

// Current mode
avmode_t cm;

extern mode_data_t video_modes[];
extern ypbpr_to_rgb_csc_t csc_coeffs[];
extern alt_u16 rc_keymap[REMOTE_MAX_KEYS];
extern alt_u32 remote_code;
extern alt_u32 btn_code, btn_code_prev;
extern alt_u8 remote_rpt, remote_rpt_prev;
extern avconfig_t tc;

alt_u8 target_typemask;
alt_u8 target_type;
alt_u8 stable_frames;

char row1[LCD_ROW_LEN+1], row2[LCD_ROW_LEN+1], menu_row1[LCD_ROW_LEN+1], menu_row2[LCD_ROW_LEN+1];

extern alt_u8 menu_active;
avinput_t target_mode;

inline void lcd_write_menu()
{
    lcd_write((char*)&menu_row1, (char*)&menu_row2);
}

inline void lcd_write_status() {
    lcd_write((char*)&row1, (char*)&row2);
}

inline void TX_enable(tx_mode_t mode)
{
    // shut down TX before setting new config
    SetAVMute(TRUE);
    DisableVideoOutput();
    EnableAVIInfoFrame(FALSE, NULL);

    // re-setup
    EnableVideoOutput(PCLK_MEDIUM, COLOR_RGB444, COLOR_RGB444, !mode);
    //TODO: set correct VID based on mode
    if (mode == TX_HDMI)
        HDMITX_SetAVIInfoFrame(HDMI_480p60, F_MODE_RGB444, 0, 0);

    // start TX
    SetAVMute(FALSE);
}

void set_lpf(alt_u8 lpf)
{
    alt_u32 pclk;
    pclk = (clkrate[REFCLK_EXT27]/cm.clkcnt)*video_modes[cm.id].h_total;
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
    alt_u16 fpga_totlines;
    status_t status;
    static alt_u8 act_ctr;
    alt_u32 ctr;
    int valid_linecnt;

    status = NO_CHANGE;

    // Wait until vsync active (avoid noise coupled to I2C bus on earlier prototypes)
    for (ctr=0; ctr<STATUS_TIMEOUT; ctr++) {
        if (!(IORD_ALTERA_AVALON_PIO_DATA(PIO_4_BASE) & (1<<31))) {
            //printf("ctrval %u\n", ctr);
            break;
        }
    }

    sync_active = tvp_check_sync(input, format);
    vsyncmode = IORD_ALTERA_AVALON_PIO_DATA(PIO_4_BASE) >> 16;

    data1 = tvp_readreg(TVP_LINECNT1);
    data2 = tvp_readreg(TVP_LINECNT2);
    totlines = ((data2 & 0x0f) << 8) | data1;
    progressive = !!(data2 & (1<<5));
    cm.macrovis = !!(data2 & (1<<6));

    fpga_totlines = IORD_ALTERA_AVALON_PIO_DATA(PIO_4_BASE) & 0xffff;

    // NOTE: "progressive" may not have correct value if H-PLL is not locked (!cm.sync_active)
    if ((vsyncmode == 0x2) || (!cm.sync_active && (totlines < MIN_LINES_INTERLACED))) {
        progressive = 1;
    } else if ((vsyncmode == 0x1) && fpga_totlines > ((totlines-1)*2)) {
        progressive = 0;
        totlines = fpga_totlines; //ugly hack
    }

    valid_linecnt = check_linecnt(progressive, totlines);

    // TVP7002 may randomly report "no sync" (especially with arcade boards),
    // thus disable output only after N consecutive "no sync"-events
    if (!cm.sync_active && sync_active && valid_linecnt) {
        cm.sync_active = sync_active;
        status = ACTIVITY_CHANGE;
        act_ctr = 0;
    } else if (cm.sync_active && (!sync_active || !valid_linecnt)) {
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

    data1 = tvp_readreg(TVP_CLKCNT1);
    data2 = tvp_readreg(TVP_CLKCNT2);
    clkcnt = ((data2 & 0x0f) << 8) | data1;

    if (valid_linecnt) {
        if ((abs((alt_16)totlines - (alt_16)cm.totlines) > LINECNT_THOLD) || (clkcnt != cm.clkcnt) || (progressive != cm.progressive)) {
            printf("totlines: %u (cur) / %u (prev), clkcnt: %u (cur) / %u (prev). Data1: 0x%.2x, Data2: 0x%.2x\n", (unsigned)totlines, (unsigned)cm.totlines, (unsigned)clkcnt, (unsigned)cm.clkcnt, (unsigned)data1, (unsigned)data2);
            stable_frames = 0;
        } else if (stable_frames != STABLE_THOLD) {
            stable_frames++;
            if (stable_frames == STABLE_THOLD)
                status = (status < MODE_CHANGE) ? MODE_CHANGE : status;
        }

        if ((tc.linemult_target != cm.cc.linemult_target) || (tc.l3_mode != cm.cc.l3_mode))
            status = (status < MODE_CHANGE) ? MODE_CHANGE : status;

        if ((tc.s480p_mode != cm.cc.s480p_mode) && (video_modes[cm.id].flags & (MODE_DTV480P|MODE_VGA480P)))
            status = (status < MODE_CHANGE) ? MODE_CHANGE : status;

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
        (tc.edtv_l2x != cm.cc.edtv_l2x) ||
        (tc.interlace_pt != cm.cc.interlace_pt))
        status = (status < INFO_CHANGE) ? INFO_CHANGE : status;

    if (tc.sampler_phase != cm.cc.sampler_phase)
        tvp_set_hpll_phase(tc.sampler_phase);

    if (tc.sync_vth != cm.cc.sync_vth)
        tvp_set_sog_thold(tc.sync_vth);

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

    if (tc.en_alc != cm.cc.en_alc)
        tvp_set_alc(tc.en_alc, target_type);

    cm.cc = tc;

    return status;
}

// h_info:     [31:30]           [29:28]         [27:22]      [21]  [20:10]         [9:8]  [7:0]
//           | H_LINEMULT[1:0] | H_L3MODE[1:0] | H_MASK[5:0] |    | H_ACTIVE[10:0] |     | H_BACKPORCH[7:0] |
//
// v_info:     [31]          [30]            [29:28]        [27:24]              [23:18]       [17:7]          [6]  [5:0]
//           | V_SCANLINES | V_SCANLINEDIR | V_SCANLINEID | V_SCANLINESTR[3:0] | V_MASK[5:0] | V_ACTIVE[10:0] |   | V_BACKPORCH[5:0] |
void set_videoinfo()
{
    alt_u8 slid_target;
    alt_u8 sl_mode_fpga;

    if (video_modes[cm.id].flags & MODE_L3ENABLE_MASK) {
        cm.linemult = 2;
        slid_target = cm.cc.sl_id ? (cm.cc.sl_type == 1 ? 1 : 2) : 0;
    } else if ((video_modes[cm.id].flags & MODE_L2ENABLE) || (cm.cc.edtv_l2x && (video_modes[cm.id].type & VIDEO_EDTV))) {
        cm.linemult = 1;
        slid_target = cm.cc.sl_id;
    } else {
        cm.linemult = 0;
        slid_target = cm.cc.sl_id;
    }

    if (cm.cc.sl_mode == 2) {    //manual
        sl_mode_fpga = 1+cm.cc.sl_type;
    } else if (cm.cc.sl_mode == 1) {   //auto
        if (video_modes[cm.id].flags & MODE_INTERLACED)
            sl_mode_fpga = 3;
        else if (video_modes[cm.id].flags & (MODE_L2ENABLE|MODE_L3ENABLE_MASK))
            sl_mode_fpga = 1;
        else
            sl_mode_fpga = 0;
    } else {
        sl_mode_fpga = 0;
    }

    if ((cm.cc.interlace_pt) && (video_modes[cm.id].flags & MODE_INTERLACED)) {
        cm.linemult = 0;
        sl_mode_fpga = 0;
    }

    IOWR_ALTERA_AVALON_PIO_DATA(PIO_2_BASE, (cm.linemult<<30) | (cm.cc.l3_mode<<28) | (cm.cc.h_mask)<<22 | (video_modes[cm.id].h_active<<10) | video_modes[cm.id].h_backporch);
    IOWR_ALTERA_AVALON_PIO_DATA(PIO_3_BASE, (sl_mode_fpga<<30) | (slid_target<<28) | (cm.cc.sl_str<<24) | (cm.cc.v_mask<<18) | (video_modes[cm.id].v_active<<7) | video_modes[cm.id].v_backporch);

    if (video_modes[cm.id].type & VIDEO_EDTV)
        HDMITX_SetPixelRepetition(cm.cc.edtv_l2x, 0);
    else if (video_modes[cm.id].flags & MODE_INTERLACED)
        HDMITX_SetPixelRepetition(cm.cc.interlace_pt, (cm.cc.tx_mode==TX_HDMI));
    else
        HDMITX_SetPixelRepetition(0, 0);
}

// Configure TVP7002 and scan converter logic based on the video mode
void program_mode()
{
    alt_u32 data1, data2;
    alt_u32 h_hz, v_hz_x100;

    // Mark as stable (needed after sync up to avoid unnecessary mode switch)
    stable_frames = STABLE_THOLD;

    if ((cm.clkcnt != 0) && (cm.totlines != 0)) { //prevent div by 0
        h_hz = clkrate[REFCLK_EXT27]/cm.clkcnt;
        v_hz_x100 = cm.progressive ? (100*clkrate[REFCLK_EXT27]/cm.clkcnt)/cm.totlines : (2*(100*clkrate[REFCLK_EXT27]/cm.clkcnt))/cm.totlines;
    } else {
        h_hz = 15700;
        v_hz_x100 = 6000;
    }

    printf("\nLines: %u %c\n", (unsigned)cm.totlines, cm.progressive ? 'p' : 'i');
    printf("Clocks per line: %u : HS %u.%.3u kHz  VS %u.%.2u Hz\n", (unsigned)cm.clkcnt, (unsigned)(h_hz/1000), (unsigned)(h_hz%1000), (unsigned)(v_hz_x100/100), (unsigned)(v_hz_x100%100));

    data1 = tvp_readreg(TVP_HSINWIDTH);
    data2 = tvp_readreg(TVP_VSINWIDTH);
    printf("Hswidth: %u  Vswidth: %u  Macrovision: %u\n", (unsigned)data1, (unsigned)(data2 & 0x1F), (unsigned)cm.macrovis);

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

    tvp_source_setup(cm.id, target_type, cm.cc.en_alc, (cm.progressive ? cm.totlines : cm.totlines/2), v_hz_x100/100, cm.cc.pre_coast, cm.cc.post_coast, cm.cc.vsync_thold);
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

    if (check_flash() != 0) {
        printf("Error: incorrect flash type detected\n");
        return -1;
    }

    set_default_avconfig();

    // safe?
    read_userdata();

    if (!(IORD_ALTERA_AVALON_PIO_DATA(PIO_1_BASE) & PB1_BIT))
        setup_rc();

    // init always in HDMI mode (fixes yellow screen bug)
    TX_enable(TX_HDMI);

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
    TX_enable(tc.tx_mode);
}

int main()
{
    tvp_input_t target_input = 0;
    ths_input_t target_ths = 0;
    video_format target_format = 0;

    alt_u8 av_init = 0;
    status_t status;

    alt_u32 input_vec;

    int init_stat;

    init_stat = init_hw();

    if (init_stat >= 0) {
        printf("### DIY VIDEO DIGITIZER / SCANCONVERTER INIT OK ###\n\n");
        sniprintf(row1, LCD_ROW_LEN+1, "OSSC  fw. %u.%.2u", FW_VER_MAJOR, FW_VER_MINOR);
#ifndef DEBUG
        strncpy(row2, "2014-2016  marqs", LCD_ROW_LEN+1);
#else
        strncpy(row2, "** DEBUG BUILD *", LCD_ROW_LEN+1);
#endif
        lcd_write_status();
    } else {
        sniprintf(row1, LCD_ROW_LEN+1, "Init error  %d", init_stat);
        strncpy(row2, "", LCD_ROW_LEN+1);
        lcd_write_status();
        while (1) {}
    }

    // Mainloop
    while(1) {
        // Read remote control and PCB button status
        input_vec = IORD_ALTERA_AVALON_PIO_DATA(PIO_1_BASE);
        remote_code = input_vec & RC_MASK;
        btn_code = ~input_vec & PB_MASK;
        remote_rpt = input_vec >> 24;
        target_mode = AV_KEEP;

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
            break;
        case AV1_RGsB:
            target_input = TVP_INPUT1;
            target_format = FORMAT_RGsB;
            target_typemask = VIDEO_LDTV|VIDEO_SDTV|VIDEO_EDTV|VIDEO_HDTV;
            target_ths = THS_INPUT_B;
            break;
        case AV1_YPBPR:
            target_input = TVP_INPUT1;
            target_format = FORMAT_YPbPr;
            target_typemask = VIDEO_LDTV|VIDEO_SDTV|VIDEO_EDTV|VIDEO_HDTV;
            target_ths = THS_INPUT_B;
            break;
        case AV2_YPBPR:
            target_input = TVP_INPUT1;
            target_format = FORMAT_YPbPr;
            target_typemask = VIDEO_LDTV|VIDEO_SDTV|VIDEO_EDTV|VIDEO_HDTV;
            target_ths = THS_INPUT_A;
            break;
        case AV2_RGsB:
            target_input = TVP_INPUT1;
            target_format = FORMAT_RGsB;
            target_typemask = VIDEO_LDTV|VIDEO_SDTV|VIDEO_EDTV|VIDEO_HDTV;
            target_ths = THS_INPUT_A;
            break;
        case AV3_RGBHV:
            target_input = TVP_INPUT3;
            target_format = FORMAT_RGBHV;
            target_typemask = VIDEO_PC;
            target_ths = THS_STANDBY;
            break;
        case AV3_RGBs:
            target_input = TVP_INPUT3;
            target_format = FORMAT_RGBS;
            target_typemask = VIDEO_LDTV|VIDEO_SDTV|VIDEO_EDTV|VIDEO_HDTV;
            target_ths = THS_STANDBY;
            break;
        case AV3_RGsB:
            target_input = TVP_INPUT3;
            target_format = FORMAT_RGsB;
            target_typemask = VIDEO_LDTV|VIDEO_SDTV|VIDEO_EDTV|VIDEO_HDTV;
            target_ths = THS_STANDBY;
            break;
        case AV3_YPBPR:
            target_input = TVP_INPUT3;
            target_format = FORMAT_YPbPr;
            target_typemask = VIDEO_LDTV|VIDEO_SDTV|VIDEO_EDTV|VIDEO_HDTV;
            target_ths = THS_STANDBY;
            break;
        default:
            break;
        }

        if (target_mode != AV_KEEP) {
            printf("### SWITCH MODE TO %s ###\n", avinput_str[target_mode]);
            av_init = 1;
            cm.avinput = target_mode;
            cm.sync_active = 0;
            ths_source_sel(target_ths, (cm.cc.video_lpf > 1) ? (VIDEO_LPF_MAX-cm.cc.video_lpf) : THS_LPF_BYPASS);
            tvp_disable_output();
            tvp_source_sel(target_input, target_format);
            cm.clkcnt = 0; //TODO: proper invalidate
            strncpy(row1, avinput_str[cm.avinput], LCD_ROW_LEN+1);
            strncpy(row2, "    NO SYNC", LCD_ROW_LEN+1);
            if (!menu_active)
                lcd_write_status();
        }

        // Check here to enable regardless of av_init
        if (tc.tx_mode != cm.cc.tx_mode) {
            TX_enable(tc.tx_mode);
            cm.cc.tx_mode = tc.tx_mode;
            cm.clkcnt = 0; //TODO: proper invalidate
        }

        if (av_init) {
            status = get_status(target_input, target_format);

            switch (status) {
            case ACTIVITY_CHANGE:
                if (cm.sync_active) {
                    printf("Sync up\n");
                    IOWR_ALTERA_AVALON_PIO_DATA(PIO_0_BASE, (IORD_ALTERA_AVALON_PIO_DATA(PIO_0_BASE) | (1<<2)));  //disable videogen
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
        usleep(300);    // Avoid executing mainloop multiple times per vsync
    }

    return 0;
}
