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

#include "alt_types.h"
#include "sysconfig.h"
#include "altera_epcq_controller_mod.h"

// EPCS16 pagesize is 256 bytes
// Flash is split 50-50 to FW and userdata, 1MB each
#define PAGESIZE 256
#define PAGES_PER_SECTOR 256
#define USERDATA_OFFSET 0x100000
#define USERDATA_MAX_SIZE 0x1000    //4KB should be enough

int check_flash();

int read_flash(alt_u32 offset, alt_u32 length, alt_u8 *dstbuf);

int write_flash_page(alt_u8 *pagedata, alt_u32 length, alt_u32 pagenum);

int verify_flash(alt_u32 offset, alt_u32 length, alt_u32 golden_crc, alt_u8 *tmpbuf);

#endif /* FLASH_H_ */
