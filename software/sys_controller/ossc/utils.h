//
// Copyright (C) 2018  Markus Hiienkari <mhiienka@niksula.hut.fi>
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

#ifndef UTILS_H_
#define UTILS_H_

#include <alt_types.h>

#define PRINTF_BUFSIZE 512

inline unsigned char bitswap8(unsigned char v);

alt_u32 bswap32(alt_u32 w);

unsigned long crc32(unsigned char *input_data, unsigned long input_data_length, int do_initialize);

int dd_printf(const char *__restrict fmt, ...);

#endif
