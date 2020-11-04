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

#ifndef SDCARD_H_
#define SDCARD_H_

#include "alt_types.h"
#include "sysconfig.h"
#include "sd_io.h"

int check_sdcard(alt_u8 *databuf);
int copy_sd_to_flash(alt_u32 sd_blknum, alt_u32 flash_pagenum, alt_u32 length, alt_u8 *tmpbuf);
int copy_flash_to_sd(alt_u32 flash_pagenum, alt_u32 sd_blknum, alt_u32 length, alt_u8 *tmpbuf);

#endif /* SDCARD_H_ */
