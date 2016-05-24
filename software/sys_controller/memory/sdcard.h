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
#include "Altera_UP_SD_Card_Avalon_Interface_mod.h"

// SD controller uses 512-byte chunks
#define SD_BUFFER_SIZE 512

int read_sd_block(alt_u32 offset, alt_u32 size, alt_u8 *dstbuf);

int check_sdcard(alt_u8 *databuf);

#endif /* SDCARD_H_ */
