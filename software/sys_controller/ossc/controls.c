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

#include <string.h>
#include <unistd.h>
#include "alt_types.h"
#include "controls.h"
#include "menu.h"
#include "av_controller.h"
#include "video_modes.h"
#include "userdata.h"
#include "lcd.h"
#include "altera_avalon_pio_regs.h"

static const char *rc_keydesc[REMOTE_MAX_KEYS] = { "1", "2", "3", "4", "5", "6", "7", "8", "9", "0", \
                                                   "MENU", "OK", "BACK", "UP", "DOWN", "LEFT", "RIGHT", "INFO", "LCD_BACKLIGHT", "SCANLINE_MODE", \
                                                   "SCANLINE_TYPE", "SCANLINE_INT+", "SCANLINE_INT-", "LINEMULT_MODE", "PHASE+", "PHASE-", "PROFILE_HOTKEY"};
const alt_u16 rc_keymap_default[REMOTE_MAX_KEYS] = {0x3E29, 0x3EA9, 0x3E69, 0x3EE9, 0x3E19, 0x3E99, 0x3E59, 0x3ED9, 0x3E39, 0x3EC9, \
                                              0x3E4D, 0x3E1D, 0x3EED, 0x3E2D, 0x3ECD, 0x3EAD, 0x3E6D, 0x3E65, 0x3E01, 0x1C48, \
                                              0x1C18, 0x1C50, 0x1CD0, 0x1CC8, 0x5E58, 0x5ED8, 0x3EB9};
alt_u16 rc_keymap[REMOTE_MAX_KEYS];

extern char menu_row1[LCD_ROW_LEN+1], menu_row2[LCD_ROW_LEN+1];
extern const mode_data_t video_modes[];
extern avmode_t cm;
extern avconfig_t tc;
extern avinput_t target_mode;
extern alt_u8 menu_active;
extern alt_u16 sys_ctrl;
extern alt_u8 profile_sel;

alt_u32 remote_code;
alt_u8 remote_rpt, remote_rpt_prev;
alt_u32 btn_code, btn_code_prev;

void setup_rc()
{
    int i, confirm;
    alt_u32 remote_code_prev = 0;

    for (i=0; i<REMOTE_MAX_KEYS; i++) {
        strncpy(menu_row1, "Press", LCD_ROW_LEN+1);
        strncpy(menu_row2, rc_keydesc[i], LCD_ROW_LEN+1);
        lcd_write_menu();
        confirm = 0;

        while (1) {
            remote_code = IORD_ALTERA_AVALON_PIO_DATA(PIO_1_BASE) & RC_MASK;
            btn_code = ~IORD_ALTERA_AVALON_PIO_DATA(PIO_1_BASE) & PB_MASK;

            if (remote_code && (remote_code != remote_code_prev)) {
                if (confirm == 0) {
                    rc_keymap[i] = remote_code;
                    strncpy(menu_row1, "Confirm", LCD_ROW_LEN+1);
                    lcd_write_menu();
                    confirm = 1;
                } else {
                    if (remote_code == rc_keymap[i]) {
                        confirm = 2;
                    } else {
                        strncpy(menu_row1, "Mismatch, retry", LCD_ROW_LEN+1);
                        lcd_write_menu();
                        confirm = 0;
                    }
                }
            }

            if ((btn_code_prev == 0) && (btn_code == PB0_BIT)) {
                if (i == 0) {
                    memcpy(rc_keymap, rc_keymap_default, sizeof(rc_keymap));
                    i=REMOTE_MAX_KEYS;
                } else {
                    i-=2;
                }
                confirm = 2;
            }

            remote_code_prev = remote_code;
            btn_code_prev = btn_code;

            if (confirm == 2)
                break;

            usleep(WAITLOOP_SLEEP_US);
        }
    }
    write_userdata(INIT_CONFIG_SLOT);
}

void parse_control()
{
    int i;
    alt_u32 btn_vec;
    alt_u8 pt_only = 0;

    // one for each video_group
    alt_u8* pmcfg_ptr[] = { &pt_only, &tc.pm_240p, &tc.pm_384p, &tc.pm_480i, &tc.pm_480p, &tc.pm_480p, &tc.pm_1080i };
    alt_u8 valid_pm[] = { 0x1, 0x1f, 0x3, 0xf, 0x3, 0x3, 0x3 };

    if (remote_code)
        printf("RCODE: 0x%.4lx, %d\n", remote_code, remote_rpt);

    if (btn_code_prev == 0 && btn_code != 0)
        printf("BCODE: 0x%.2lx\n", btn_code>>16);

    for (i = RC_BTN1; i < REMOTE_MAX_KEYS; i++) {
        if (remote_code == rc_keymap[i])
            break;
        if (i == REMOTE_MAX_KEYS - 1)
            goto Button_Check;
    }

    switch (i) {
        case RC_BTN1: target_mode = AV1_RGBs; break;
        case RC_BTN4: target_mode = AV1_RGsB; break;
        case RC_BTN7: target_mode = AV1_YPBPR; break;
        case RC_BTN2: target_mode = AV2_YPBPR; break;
        case RC_BTN5: target_mode = AV2_RGsB; break;
        case RC_BTN3: target_mode = AV3_RGBHV; break;
        case RC_BTN6: target_mode = AV3_RGBs; break;
        case RC_BTN9: target_mode = AV3_RGsB; break;
        case RC_BTN0: target_mode = AV3_YPBPR; break;
        case RC_MENU:
            menu_active = !menu_active;

            if (menu_active)
                display_menu(1);
            else
                lcd_write_status();

            break;
        case RC_INFO:
            sniprintf(menu_row1, LCD_ROW_LEN+1, "VMod: %s", video_modes[cm.id].name);
            sniprintf(menu_row2, LCD_ROW_LEN+1, "LC: %u VSM: %u", (IORD_ALTERA_AVALON_PIO_DATA(PIO_2_BASE) & 0x7ff)+1, (IORD_ALTERA_AVALON_PIO_DATA(PIO_2_BASE) >> 16) & 0x3);
            lcd_write_menu();
            printf("Mod: %s\n", video_modes[cm.id].name);
            printf("Lines: %u M: %u\n", (IORD_ALTERA_AVALON_PIO_DATA(PIO_2_BASE) & 0x7ff)+1, cm.macrovis);
            break;
        case RC_LCDBL:
            sys_ctrl ^= LCD_BL;
            IOWR_ALTERA_AVALON_PIO_DATA(PIO_0_BASE, sys_ctrl);
            break;
        case RC_SL_MODE: tc.sl_mode = (tc.sl_mode < SL_MODE_MAX) ? (tc.sl_mode + 1) : 0; break;
        case RC_SL_TYPE: tc.sl_type = (tc.sl_type < SL_TYPE_MAX) ? (tc.sl_type + 1) : 0; break;
        case RC_SL_MINUS: tc.sl_str = tc.sl_str ? (tc.sl_str - 1) : 0; break;
        case RC_SL_PLUS: tc.sl_str = (tc.sl_str < SCANLINESTR_MAX) ? (tc.sl_str + 1) : SCANLINESTR_MAX; break;
        case RC_LM_MODE:
            strncpy(menu_row1, "Linemult mode:", LCD_ROW_LEN+1);
            strncpy(menu_row2, "press 1-5", LCD_ROW_LEN+1);
            lcd_write_menu();

            while (1) {
                btn_vec = IORD_ALTERA_AVALON_PIO_DATA(PIO_1_BASE) & RC_MASK;
                for (i = RC_BTN1; i < REMOTE_MAX_KEYS; i++) {
                    if (btn_vec == rc_keymap[i])
                        break;
                }

                if (video_modes[cm.id].group > GROUP_1080I) {
                    printf("WARNING: Corrupted mode (id %d)\n", cm.id);
                    break;
                }

                if (i <= RC_BTN5) {
                    if ((1<<i) & valid_pm[video_modes[cm.id].group]) {
                        *pmcfg_ptr[video_modes[cm.id].group] = i;
                    } else {
                        sniprintf(menu_row2, LCD_ROW_LEN+1, "%ux unsupported", i+1);
                        lcd_write_menu();
                        usleep(500000);
                    }
                    break;
                } else if (i == RC_BACK) {
                    break;
                }

                usleep(WAITLOOP_SLEEP_US);
            }
            lcd_write_status();
            menu_active = 0;
            break;
        case RC_PHASE_PLUS: tc.sampler_phase = (tc.sampler_phase < SAMPLER_PHASE_MAX) ? (tc.sampler_phase + 1) : 0; break;
        case RC_PHASE_MINUS: tc.sampler_phase = tc.sampler_phase ? (tc.sampler_phase - 1) : SAMPLER_PHASE_MAX; break;
        case RC_PROF_HOTKEY:
            strncpy(menu_row1, "Profile load:", LCD_ROW_LEN+1);
            strncpy(menu_row2, "press 0-9", LCD_ROW_LEN+1);
            lcd_write_menu();

            while (1) {
                btn_vec = IORD_ALTERA_AVALON_PIO_DATA(PIO_1_BASE) & RC_MASK;
                for (i = RC_BTN1; i < REMOTE_MAX_KEYS; i++) {
                    if (btn_vec == rc_keymap[i])
                        break;
                }

                if (i <= RC_BTN0) {
                    profile_sel = (i+1)%10;
                    load_profile();
                    break;
                } else if (i == RC_BACK) {
                    break;
                }

                usleep(WAITLOOP_SLEEP_US);
            }
            lcd_write_status();
            menu_active = 0;
            break;
        default: break;
    }

Button_Check:
    if (btn_code_prev == 0) {
        if (btn_code & PB0_BIT)
            target_mode = (cm.avinput == AV3_YPBPR) ? AV1_RGBs : (cm.avinput+1);
        if (btn_code & PB1_BIT)
            tc.sl_mode = tc.sl_mode < SL_MODE_MAX ? tc.sl_mode + 1 : 0;
    }
}
