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

#ifndef FIRMWARE_H_
#define FIRMWARE_H_

#include <stdint.h>
#include "sysconfig.h"

#define FW_VER_MAJOR            1
#define FW_VER_MINOR            20

#ifdef OSDLANG_JP
#define FW_SUFFIX              "j"
#else
#define FW_SUFFIX              ""
#endif

typedef struct {
    char fw_key[4];
    uint8_t version_major;
    uint8_t version_minor;
    char version_suffix[8];
    uint32_t hdr_len;
    uint32_t data_len;
    uint32_t data_crc;
    char padding[482];
    uint32_t hdr_crc;
} __attribute__((packed)) fw_hdr;

typedef struct {
    uint32_t sm_cur_state[4];
    uint32_t force_early_confdone[4];
    uint32_t wdog_timeout[4];
    uint32_t wdog_enable[4];
    uint32_t image_addr[4];
    uint32_t force_int_osc[4];
    uint32_t reg_trig_cnd[4];
    uint32_t reset_timer;
    uint32_t reconfig_start;
} rem_update_regs;

typedef struct {
    volatile rem_update_regs *regs;
} rem_update_dev;

int fw_init_secondary();
int fw_update();
void fw_update_commit(uint32_t* cluster_idx, uint32_t bytes_to_copy, uint16_t fs_csize, uint16_t fs_startsec, uint32_t flash_addr);

#endif
