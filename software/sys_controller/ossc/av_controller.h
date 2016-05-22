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

#ifndef AV_CONTROLLER_H_
#define AV_CONTROLLER_H_

#include "altera_epcq_controller_mod.h"
#include "typedef.h"
#include "cfg.h"
#include "lcd.h"
#include "tvp7002.h"

#define RC_MASK          0x0000ffff
#define PB_MASK          0x00030000
#define PB0_BIT          0x00010000
#define PB1_BIT          0x00020000
#define HDMITX_MODE_MASK 0x00040000

extern char row1[LCD_ROW_LEN+1], row2[LCD_ROW_LEN+1], menu_row1[LCD_ROW_LEN+1], menu_row2[LCD_ROW_LEN+1];

#define lcd_write_menu() lcd_write((char*)&menu_row1, (char*)&menu_row2)
#define lcd_write_status() lcd_write((char*)&row1, (char*)&row2)

// In reverse order of importance
typedef enum {
    NO_CHANGE       = 0,
    INFO_CHANGE     = 1,
    MODE_CHANGE     = 2,
    TX_MODE_CHANGE  = 3,
    ACTIVITY_CHANGE = 4
} status_t;

typedef enum {
    AV_KEEP   = 0,
    AV1_RGBs  = 1,
    AV1_RGsB  = 2,
    AV2_YPBPR = 3,
    AV2_RGsB  = 4,
    AV3_RGBHV = 5,
    AV3_RGBs  = 6,
    AV3_RGsB  = 7
} avinput_t;

//TODO: transform binary values into flags
typedef struct {
    alt_u32   totlines;
    alt_u32   clkcnt;
    alt_u8    progressive;
    alt_u8    macrovis;
    alt_u8    refclk;
    alt_8     id;
    alt_u8    sync_active;
    alt_u8    linemult;
    avinput_t avinput;
    // Current configuration
    avconfig_t cc;
} avmode_t;

typedef enum {
    TX_HDMI = 0,
    TX_DVI  = 1
} tx_mode_t;


status_t get_status(tvp_input_t input);

void TX_enable(tx_mode_t mode);

void program_mode(void);
void set_videoinfo(void);

int init_hw(void);
void enable_outputs(void);

#endif /* AV_CONTROLLER_H_ */
