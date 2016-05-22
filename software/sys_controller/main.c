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
#include "lcd.h"
#include "sysconfig.h"
#include "hdmitx.h"
#include "ci_crc.h"
#include "cfg.h"
#include "av_controller.h"
#include "controls.h"
#include "memory.h"
#include "menu.h"
#include "fw_version.h"


extern const char const *avinput_str[];

extern char row1[LCD_ROW_LEN+1], row2[LCD_ROW_LEN+1], menu_row1[LCD_ROW_LEN+1], menu_row2[LCD_ROW_LEN+1];

extern alt_u16 rc_keymap[REMOTE_MAX_KEYS];

extern volatile alt_u32 remote_code;
extern volatile alt_u8 remote_rpt, remote_rpt_prev;
extern volatile alt_u32 btn_code, btn_code_prev;

extern volatile alt_u8 menu_active;

extern alt_u8 target_typemask;

// Target configuration
extern volatile avconfig_t tc;

// Current mode
extern volatile avmode_t cm;

int main()
{
    tvp_input_t target_input = 0;
    ths_input_t target_ths = 0;
    video_format target_format = 0;
    avinput_t target_mode;

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

    while(1) {
        // Select target input and mode
        input_vec = IORD_ALTERA_AVALON_PIO_DATA(PIO_1_BASE);
        remote_code = input_vec & RC_MASK;
        btn_code = ~input_vec & PB_MASK;
        remote_rpt = input_vec >> 24;

        if ((remote_rpt == 0) || ((remote_rpt > 1) && (remote_rpt < 6)) || (remote_rpt == remote_rpt_prev))
            remote_code = 0;
        /*else if ((remote_rpt >= 6) && (remote_rpt % 2))
            remote_code = 0;*/

        if (remote_code)
            printf("RCODE: 0x%.4x, %u\n", remote_code, remote_rpt);

        if (btn_code_prev == 0 && btn_code != 0)
            printf("BCODE: 0x%.2x\n", btn_code>>16);

        target_mode = AV_KEEP;

        if (remote_code == rc_keymap[RC_BTN1]) {
            if (cm.avinput == AV1_RGBs)
                target_mode = AV1_RGsB;
            else
                target_mode = AV1_RGBs;
        } else if (remote_code == rc_keymap[RC_BTN2]) {
            if (cm.avinput == AV2_YPBPR)
                target_mode = AV2_RGsB;
            else
                target_mode = AV2_YPBPR;
        } else if (remote_code == rc_keymap[RC_BTN3]) {
            if (cm.avinput == AV3_RGBHV)
                target_mode = AV3_RGBs;
            else if (cm.avinput == AV3_RGBs)
                target_mode = AV3_RGsB;
            else
                target_mode = AV3_RGBHV;
        }

        if ((btn_code_prev == 0) && (btn_code & PB0_BIT)) {
            target_mode = (cm.avinput == AV3_RGsB) ? AV1_RGBs : (cm.avinput+1);
        }

        if (target_mode == cm.avinput)
            target_mode = AV_KEEP;

        if (target_mode != AV_KEEP)
            printf("### SWITCH MODE TO %s ###\n", avinput_str[target_mode]);

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
        default:
            break;
        }

        if (target_mode != AV_KEEP) {
            av_init = 1;
            cm.avinput = target_mode;
            cm.sync_active = 0;
            ths_source_sel(target_ths, (cm.cc.video_lpf > 1) ? (VIDEO_LPF_MAX-cm.cc.video_lpf) : THS_LPF_BYPASS);
            tvp_disable_output();
            DisableAudioOutput();
            tvp_source_sel(target_input, target_format, cm.refclk);
            cm.clkcnt = 0; //TODO: proper invalidate
            strncpy(row1, avinput_str[cm.avinput], LCD_ROW_LEN+1);
            strncpy(row2, "    NO SYNC", LCD_ROW_LEN+1);
            if (!menu_active)
                lcd_write_status();
        }

        //usleep(MAINLOOP_SLEEP_US);
        usleep(300);    //avoid triggering multiple times per vsync
        read_control();

        if (av_init) {
            status = get_status(target_input);

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
    }

    return 0;
}
