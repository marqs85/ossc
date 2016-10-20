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

#include "avconfig.h"
#include "sysconfig.h"

#define HDMITX_MODE_MASK        0x00040000

//sys_ctrl bits
#define SD_SPI_SS_N (1<<7)
#define LCD_CS_N    (1<<6)
#define LCD_RS      (1<<5)
#define LCD_BL      (1<<4)
#define VIDGEN_OFF  (1<<1)
#define AV_RESET_N  (1<<0)

static const char *avinput_str[] = { "Test pattern", "AV1: RGBS", "AV1: RGsB", "AV1: YPbPr", "AV2: YPbPr", "AV2: RGsB", "AV3: RGBHV", "AV3: RGBS", "AV3: RGsB", "AV3: YPbPr" };

typedef enum {
    AV_KEEP         = 0,
    AV1_RGBs        = 1,
    AV1_RGsB        = 2,
    AV1_YPBPR       = 3,
    AV2_YPBPR       = 4,
    AV2_RGsB        = 5,
    AV3_RGBHV       = 6,
    AV3_RGBs        = 7,
    AV3_RGsB        = 8,
    AV3_YPBPR       = 9
} avinput_t;

// In reverse order of importance
typedef enum {
    NO_CHANGE           = 0,
    INFO_CHANGE         = 1,
    MODE_CHANGE         = 2,
    TX_MODE_CHANGE      = 3,
    ACTIVITY_CHANGE     = 4
} status_t;

typedef enum {
    TX_HDMI             = 0,
    TX_DVI              = 1
} tx_mode_t;

//TODO: transform binary values into flags
typedef struct {
    alt_u32 totlines;
    alt_u32 clkcnt;
    alt_u8 progressive;
    alt_u8 macrovis;
    alt_8 id;
    alt_u8 sync_active;
    alt_u8 linemult;
    avinput_t avinput;
    // Current configuration
    avconfig_t cc;
} avmode_t;

inline void lcd_write_menu();
inline void lcd_write_status();

void vm_display(alt_u8 code);
void vm_tweak(alt_u16 v);

#endif
