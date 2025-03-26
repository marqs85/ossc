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

#ifndef FLASH_H_
#define FLASH_H_

#include <stdint.h>
#include "sysconfig.h"

#define FLASH_SECTOR_SIZE 65536

typedef struct {
    uint32_t ctrl;
    uint32_t baud_rate;
    uint32_t cs_delay;
    uint32_t read_capture;
    uint32_t oper_mode;
    uint32_t read_instr;
    uint32_t write_instr;
    uint32_t flash_cmd_cfg;
    uint32_t flash_cmd_ctrl;
    uint32_t flash_cmd_addr;
    uint32_t flash_cmd_wrdata[2];
    uint32_t flash_cmd_rddata[2];
} gen_flash_if_regs;

typedef struct {
    volatile gen_flash_if_regs *regs;
    uint32_t flash_size;
} flash_ctrl_dev;

void flash_write_protect(flash_ctrl_dev *dev, int enable);
void flash_sector_erase(flash_ctrl_dev *dev, uint32_t addr);

#endif /* FLASH_H_ */
