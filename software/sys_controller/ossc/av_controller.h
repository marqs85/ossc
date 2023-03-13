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

#ifndef AV_CONTROLLER_H_
#define AV_CONTROLLER_H_

#include "avconfig.h"
#include "sysconfig.h"
#include "sc_config_regs.h"
#include "osd_generator_regs.h"
#include "pll_reconfig_regs.h"

// sys_ctrl bits
#define LT_ACTIVE                   (1<<15)
#define LT_ARMED                    (1<<14)
#define LT_MODE_OFFS                12
#define VSYNC_I_TYPE                (1<<10)
#define PLL_BYPASS                  (1<<9)
#define REMOTE_EVENT                (1<<8)
#define SD_SPI_SS_N                 (1<<7)
#define LCD_CS_N                    (1<<6)
#define LCD_RS                      (1<<5)
#define LCD_BL                      (1<<4)
#define LCD_BL_TIMEOUT_OFFS         2
#define VIDGEN_OFF                  (1<<1)
#define AV_RESET_N                  (1<<0)

#define LT_CTRL_MASK                0xf000

#define HDMITX_MODE_MASK            0x00040000
#define PLL_ACTIVECLK_MASK          0x00080000
#define VSYNC_FLAG_MASK             0x00100000

#define AUTO_OFF                    0
#define AUTO_CURRENT_INPUT          1
#define AUTO_MAX_COUNT              100
#define AUTO_CURRENT_MAX_COUNT      6

#define PLL_CONFIG_VG               0

// In reverse order of importance
typedef enum {
    NO_CHANGE           = 0,
    SC_CONFIG_CHANGE    = 1,
    MODE_CHANGE         = 2,
    TX_MODE_CHANGE      = 3,
    ACTIVITY_CHANGE     = 4
} status_t;

typedef enum {
    TX_HDMI_RGB         = 0,
    TX_HDMI_YCBCR444    = 1,
    TX_DVI              = 2
} tx_mode_t;

typedef struct {
    alt_u32 data[5];
} pll_config_t;

//TODO: transform binary values into flags
typedef struct {
    alt_u32 totlines;
    alt_u32 pcnt_frame;
    alt_u32 clkcnt;
    alt_u8 progressive;
    alt_8 id;
    alt_u8 sync_active;
    alt_u8 tx_pixelrep;
    alt_u8 hdmitx_pixr_ifr;
    alt_u8 hdmitx_pclk_level;
    alt_u32 pclk_o_hz;
    avinput_t avinput;
    // Current configuration
    avconfig_t cc;
} avmode_t;

void ui_disp_menu(alt_u8 osd_mode);
void ui_disp_status(alt_u8 refresh_osd_timer);

void set_sampler_phase(uint8_t sampler_phase);

int load_profile();
int save_profile();

void print_vm_stats();
int latency_test();

#endif
