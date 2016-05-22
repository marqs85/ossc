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
#include "altera_epcq_controller_mod.h"
#include "lcd.h"
#include "av_controller.h"
#include "controls.h"
#include "memory.h"
#include "menu.h"
#include "cfg.h"


typedef enum {
    NO_ACTION = 0,
    NEXT_PAGE = 1,
    PREV_PAGE = 2,
    VAL_PLUS  = 3,
    VAL_MINUS = 4,
} menucode_id;


typedef enum {
    SCANLINE_MODE,
    SCANLINE_STRENGTH,
    SCANLINE_ID,
    H_MASK,
    V_MASK,
    SAMPLER_480P,
    SAMPLER_PHASE,
    YPBPR_COLORSPACE,
    SYNC_THOLD,
    PRE_COAST,
    POST_COAST,
    SYNC_LPF,
    VIDEO_LPF,
    DISABLE_ALC,
    LINETRIPLE_ENABLE,
    LINETRIPLE_MODE,
    TX_MODE,
#ifndef DEBUG
    FW_UPDATE,
#endif
    SAVE_CONFIG
} menuitem_id;

typedef struct {
    const menuitem_id  id;
    const char        *desc;
} menuitem_t;

const menuitem_t menu[] = {
    { SCANLINE_MODE,     "Scanlines" },
    { SCANLINE_STRENGTH, "Scanline str." },
    { SCANLINE_ID,       "Scanline id" },
    { H_MASK,            "Horizontal mask" },
    { V_MASK,            "Vertical mask" },
    { SAMPLER_480P,      "480p in sampler" },
    { SAMPLER_PHASE,     "Sampling phase" },
    { YPBPR_COLORSPACE,  "YPbPr in ColSpa" },
    { SYNC_THOLD,        "Analog sync thld" },
    { PRE_COAST,         "H-PLL Pre-Coast" },
    { POST_COAST,        "H-PLL Post-Coast" },
    { SYNC_LPF,          "Analog sync LPF" },
    { VIDEO_LPF,         "Video LPF" },
    { DISABLE_ALC,       "Auto Lev. Contr." },
    { LINETRIPLE_ENABLE, "240p/288p lineX3" },
    { LINETRIPLE_MODE,   "Linetriple mode" },
    { TX_MODE,           "TX mode" },
#ifndef DEBUG
    { FW_UPDATE,         "Firmware update" },
#endif
    { SAVE_CONFIG,       "Save settings" },
};

#define MENUITEMS (sizeof(menu)/sizeof(menuitem_t))

const char const *l3_mode_desc[] = { "Generic 16:9", "Generic 4:3", "320x240 optim.", "256x240 optim." };
const char const *s480p_desc[] = { "Auto", "DTV 480p", "VGA 640x480" };
const char const *sl_mode_desc[] = { "Off", "Horizontal", "Vertical" };
const char const *sync_lpf_desc[] = { "Off", "33MHz", "10MHz", "2.5MHz" };
const char const *video_lpf_desc[] = { "Auto", "Off", "95MHz (HDTV II)", "35MHz (HDTV I)", "16MHz (EDTV)", "9MHz (SDTV)" };


extern char row1[LCD_ROW_LEN+1], row2[LCD_ROW_LEN+1], menu_row1[LCD_ROW_LEN+1], menu_row2[LCD_ROW_LEN+1];

extern alt_u16 rc_keymap[REMOTE_MAX_KEYS];
extern volatile alt_u32 remote_code;

extern ypbpr_to_rgb_csc_t csc_coeffs[];



// Target configuration
extern avconfig_t tc;

void display_menu(alt_u8 forcedisp)
{
    menucode_id code;
    int retval;
    static alt_u8 menu_page;

    if (remote_code == rc_keymap[RC_UP])
        code = PREV_PAGE;
    else if (remote_code == rc_keymap[RC_DOWN])
        code = NEXT_PAGE;
    else if (remote_code == rc_keymap[RC_RIGHT])
        code = VAL_PLUS;
    else if (remote_code == rc_keymap[RC_LEFT])
        code = VAL_MINUS;
    else
        code = NO_ACTION;

    if (!forcedisp && (code == NO_ACTION))
        return;

    if (code == PREV_PAGE)
        menu_page = (menu_page+MENUITEMS-1) % MENUITEMS;
    else if (code == NEXT_PAGE)
        menu_page = (menu_page+1) % MENUITEMS;

    strncpy(menu_row1, menu[menu_page].desc, LCD_ROW_LEN+1);

    switch (menu[menu_page].id) {
    case SCANLINE_MODE:
        if (code == VAL_MINUS)
            tc.sl_mode = tc.sl_mode ? tc.sl_mode-1 : SL_MODE_MAX;
        else if (code == VAL_PLUS)
            tc.sl_mode = tc.sl_mode < SL_MODE_MAX ? tc.sl_mode+1 : 0;
        strncpy(menu_row2, sl_mode_desc[tc.sl_mode], LCD_ROW_LEN+1);
        break;
    case SCANLINE_STRENGTH:
        if (code == VAL_MINUS)
        	tc.sl_str = tc.sl_str ? tc.sl_str-1 : SCANLINESTR_MAX;
        else if (code == VAL_PLUS)
        	tc.sl_str = tc.sl_str < SCANLINESTR_MAX ? tc.sl_str+1 : 0;
        sniprintf(menu_row2, LCD_ROW_LEN+1, "%u%%", ((tc.sl_str+1)*625)/100);
        break;
    case SCANLINE_ID:
        if ((code == VAL_MINUS) || (code == VAL_PLUS))
            tc.sl_id = !tc.sl_id;
        sniprintf(menu_row2, LCD_ROW_LEN+1, tc.sl_id ? "Odd" : "Even");
        break;
    case H_MASK:
    	if ((code == VAL_MINUS) && (tc.h_mask > 0))
            tc.h_mask--;
        else if ((code == VAL_PLUS) && (tc.h_mask < HV_MASK_MAX))
            tc.h_mask++;
        sniprintf(menu_row2, LCD_ROW_LEN+1, "%u pixels", tc.h_mask);
        break;
    case V_MASK:
        if ((code == VAL_MINUS) && (tc.v_mask > 0))
            tc.v_mask--;
        else if ((code == VAL_PLUS) && (tc.v_mask < HV_MASK_MAX))
            tc.v_mask++;
        sniprintf(menu_row2, LCD_ROW_LEN+1, "%u pixels", tc.v_mask);
        break;
    case SAMPLER_480P:
        if (code == VAL_MINUS)
        	tc.s480p_mode = tc.s480p_mode ? tc.s480p_mode-1 : S480P_MODE_MAX;
        else if (code == VAL_PLUS)
        	tc.s480p_mode = tc.s480p_mode < S480P_MODE_MAX ? tc.s480p_mode+1 : 0;
        strncpy(menu_row2, s480p_desc[tc.s480p_mode], LCD_ROW_LEN+1);
        break;
    case SAMPLER_PHASE:
        if (code == VAL_MINUS)
        	tc.sampler_phase = tc.sampler_phase > SAMPLER_PHASE_MIN ? tc.sampler_phase-1 : SAMPLER_PHASE_MAX;
        else if (code == VAL_PLUS)
        	tc.sampler_phase = tc.sampler_phase < SAMPLER_PHASE_MAX ? tc.sampler_phase+1 : SAMPLER_PHASE_MIN;
        sniprintf(menu_row2, LCD_ROW_LEN+1, "%d deg", ((tc.sampler_phase-SAMPLER_PHASE_MIN)*1125)/100);
        break;
    case YPBPR_COLORSPACE:
        if ((code == VAL_MINUS) || (code == VAL_PLUS))
            tc.ypbpr_cs = !tc.ypbpr_cs;
        strncpy(menu_row2, csc_coeffs[tc.ypbpr_cs].name, LCD_ROW_LEN+1);
        break;
    case SYNC_THOLD:
        if (code == VAL_MINUS)
        	tc.sync_thold = tc.sync_thold > SYNC_THOLD_MIN ? tc.sync_thold-1 : SYNC_THOLD_MAX;
        else if (code == VAL_PLUS)
        	tc.sync_thold = tc.sync_thold < SYNC_THOLD_MAX ? tc.sync_thold+1 : SYNC_THOLD_MIN;
        sniprintf(menu_row2, LCD_ROW_LEN+1, "%d mV", ((tc.sync_thold-SYNC_THOLD_MIN)*1127)/100);
        break;
    case PRE_COAST:
        if (code == VAL_MINUS)
            tc.pre_coast = tc.pre_coast > PLL_COAST_MIN ? tc.pre_coast-1 : PLL_COAST_MAX;
        else if (code == VAL_PLUS)
            tc.pre_coast = tc.pre_coast < PLL_COAST_MAX ? tc.pre_coast+1 : PLL_COAST_MIN;
        sniprintf(menu_row2, LCD_ROW_LEN+1, "%u lines", tc.pre_coast);
        break;
    case POST_COAST:
        if (code == VAL_MINUS)
            tc.post_coast = tc.post_coast > PLL_COAST_MIN ? tc.post_coast-1 : PLL_COAST_MAX;
        else if (code == VAL_PLUS)
            tc.post_coast = tc.post_coast < PLL_COAST_MAX ? tc.post_coast+1 : PLL_COAST_MIN;
        sniprintf(menu_row2, LCD_ROW_LEN+1, "%u lines", tc.post_coast);
        break;
    case SYNC_LPF:
        if (code == VAL_MINUS)
        	tc.sync_lpf = tc.sync_lpf ? tc.sync_lpf-1 : SYNC_LPF_MAX;
        else if (code == VAL_PLUS)
        	tc.sync_lpf = tc.sync_lpf < SYNC_LPF_MAX ? tc.sync_lpf+1 : 0;
        strncpy(menu_row2, sync_lpf_desc[tc.sync_lpf], LCD_ROW_LEN+1);
        break;
    case VIDEO_LPF:
        if (code == VAL_MINUS)
        	tc.video_lpf = tc.video_lpf ? tc.video_lpf-1 : VIDEO_LPF_MAX;
        else if (code == VAL_PLUS)
        	tc.video_lpf = tc.video_lpf < VIDEO_LPF_MAX ? tc.video_lpf+1 : 0;
        strncpy(menu_row2, video_lpf_desc[tc.video_lpf], LCD_ROW_LEN+1);
        break;
    case DISABLE_ALC:
        if ((code == VAL_MINUS) || (code == VAL_PLUS))
            tc.disable_alc = !tc.disable_alc;
        sniprintf(menu_row2, LCD_ROW_LEN+1, tc.disable_alc ? "Disabled" : "Enabled");
        break;
    case LINETRIPLE_ENABLE:
        if ((code == VAL_MINUS) || (code == VAL_PLUS))
            tc.linemult_target = !tc.linemult_target;
        sniprintf(menu_row2, LCD_ROW_LEN+1, tc.linemult_target ? "On" : "Off");
        break;
    case LINETRIPLE_MODE:
        if (code == VAL_MINUS)
        	tc.l3_mode = tc.l3_mode ? tc.l3_mode-1 : L3_MODE_MAX;
        else if (code == VAL_PLUS)
        	tc.l3_mode = tc.l3_mode < L3_MODE_MAX ? tc.l3_mode+1 : 0;
        strncpy(menu_row2, l3_mode_desc[tc.l3_mode], LCD_ROW_LEN+1);
        break;
    case TX_MODE:
        if (!(IORD_ALTERA_AVALON_PIO_DATA(PIO_1_BASE) & HDMITX_MODE_MASK) && ((code == VAL_MINUS) || (code == VAL_PLUS))) {
            tc.tx_mode = !tc.tx_mode;
            TX_enable(tc.tx_mode);
        }
        sniprintf(menu_row2, LCD_ROW_LEN+1, tc.tx_mode ? "DVI" : "HDMI");
        break;
#ifndef DEBUG
    case FW_UPDATE:
        if ((code == VAL_MINUS) || (code == VAL_PLUS)) {
            retval = fw_update();
            if (retval == 0) {
                sniprintf(menu_row1, LCD_ROW_LEN+1, "Fw update OK");
                sniprintf(menu_row2, LCD_ROW_LEN+1, "Please restart");
            } else {
                sniprintf(menu_row1, LCD_ROW_LEN+1, "FW not");
                sniprintf(menu_row2, LCD_ROW_LEN+1, "updated");
            }
        } else {
            sniprintf(menu_row2, LCD_ROW_LEN+1,  "press <- or ->");
        }
        break;
#endif
    case SAVE_CONFIG:
        if ((code == VAL_MINUS) || (code == VAL_PLUS)) {
            retval = write_userdata();
            if (retval == 0) {
                sniprintf(menu_row2, LCD_ROW_LEN+1, "Done");
            } else {
                sniprintf(menu_row2, LCD_ROW_LEN+1, "error");
            }
        } else {
            sniprintf(menu_row2, LCD_ROW_LEN+1,  "press <- or ->");
        }
        break;
    default:
        sniprintf(menu_row2, LCD_ROW_LEN+1, "MISSING ITEM");
        break;
    }

    lcd_write_menu();

    return;
}
