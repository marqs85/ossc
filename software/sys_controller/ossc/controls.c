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

#include <unistd.h>
#include "string.h"
#include "altera_avalon_pio_regs.h"
#include "altera_epcq_controller_mod.h"
#include "cfg.h"
#include "av_controller.h"
#include "lcd.h"
#include "video_modes.h"
#include "controls.h"
#include "menu.h"

const char const *rc_keydesc[] = { "1", "2", "3", "MENU", "BACK", "UP", "DOWN", "LEFT", "RIGHT", "INFO", "LCD_BACKLIGHT", "HOTKEY1", "HOTKEY2", "HOTKEY3"};

volatile alt_u8 remote_rpt, remote_rpt_prev;
volatile alt_u32 btn_code, btn_code_prev;

volatile alt_u16 rc_keymap[REMOTE_MAX_KEYS] = {0x3E29, 0x3EA9, 0x3E69, 0x3E4D, 0x3EED, 0x3E2D, 0x3ECD, 0x3EAD, 0x3E6D, 0x3E65, 0x3E01, 0x3EC1, 0x3E41, 0x3EA1};
extern char menu_row1[LCD_ROW_LEN+1], menu_row2[LCD_ROW_LEN+1];
volatile alt_u32 remote_code;

alt_u8 menu_active;
extern volatile avconfig_t tc;
extern volatile avmode_t cm;

extern mode_data_t video_modes[];

void setup_rc()
{
    int i, confirm;
    alt_u32 remote_code_prev = 0xFFFF;

    for (i=0; i<REMOTE_MAX_KEYS; i++) {
        sniprintf(menu_row1, LCD_ROW_LEN+1, "Press");
        strncpy(menu_row2, rc_keydesc[i], LCD_ROW_LEN+1);
        lcd_write_menu();
        confirm = 0;

        while (1) {
            remote_code = IORD_ALTERA_AVALON_PIO_DATA(PIO_1_BASE) & RC_MASK;

            if (remote_code && (remote_code != remote_code_prev)) {
                if (confirm == 0) {
                    rc_keymap[i] = remote_code;
                    sniprintf(menu_row1, LCD_ROW_LEN+1, "Confirm");
                    lcd_write_menu();
                    confirm = 1;
                } else {
                    if (remote_code == rc_keymap[i]) {
                        confirm = 2;
                    } else {
                        sniprintf(menu_row1, LCD_ROW_LEN+1, "Mismatch, retry");
                        lcd_write_menu();
                        confirm = 0;
                    }
                }

            }

            remote_code_prev = remote_code;

            if (confirm == 2)
                break;

            usleep(MAINLOOP_SLEEP_US);
        }
    }
}

void read_control()
{
    if (remote_code == rc_keymap[RC_MENU]) {
        menu_active = !menu_active;

        if (menu_active)
            display_menu(1);
        else
            lcd_write_status();
    } else if (remote_code == rc_keymap[RC_INFO]) {
        sniprintf(menu_row1, LCD_ROW_LEN+1, "VMod: %s", video_modes[cm.id].name);
        //sniprintf(menu_row1, LCD_ROW_LEN+1, "0x%x 0x%x 0x%x", ths_readreg(THS_CH1), ths_readreg(THS_CH2), ths_readreg(THS_CH3));
        sniprintf(menu_row2, LCD_ROW_LEN+1, "LO: %u VSM: %u", IORD_ALTERA_AVALON_PIO_DATA(PIO_4_BASE) & 0xffff, (IORD_ALTERA_AVALON_PIO_DATA(PIO_4_BASE) >> 16) & 0x3);
        lcd_write_menu();
        printf("Mod: %s\n", video_modes[cm.id].name);
        printf("Lines: %u M: %u\n", IORD_ALTERA_AVALON_PIO_DATA(PIO_4_BASE) & 0xffff, cm.macrovis);
    } else if (remote_code == rc_keymap[RC_LCDBL]) {
        IOWR_ALTERA_AVALON_PIO_DATA(PIO_0_BASE, (IORD_ALTERA_AVALON_PIO_DATA(PIO_0_BASE) ^ (1<<1)));
    } else if (remote_code == rc_keymap[RC_HOTKEY1]) {
        //tc.sl_mode = (tc.sl_mode > 0) ? 0 : 1;
        tc.sl_mode = tc.sl_mode < SL_MODE_MAX ? tc.sl_mode+1 : 0;
    } else if (remote_code == rc_keymap[RC_HOTKEY2]) {
        //if (tc.sl_str > 0)
        //    tc.sl_str--;
            tc.sl_str = tc.sl_str ? tc.sl_str-1 : SCANLINESTR_MAX;
    } else if (remote_code == rc_keymap[RC_HOTKEY3]) {
        //if (tc.sl_str < SCANLINESTR_MAX)
        //    tc.sl_str++;
        tc.sl_str = tc.sl_str < SCANLINESTR_MAX ? tc.sl_str+1 : 0;
    }

    if (btn_code_prev == 0) {
        if (btn_code & PB1_BIT)
            tc.sl_mode = (tc.sl_mode > 0) ? 0 : 1;
    }

    if (menu_active)
        display_menu(0);
}
