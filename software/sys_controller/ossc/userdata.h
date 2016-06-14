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

#ifndef USERDATA_H_
#define USERDATA_H_

#include "alt_types.h"

#define USERDATA_HDR_SIZE 11
typedef struct {
    char userdata_key[8];
    alt_u8 version_major;
    alt_u8 version_minor;
    alt_u8 num_entries;
} userdata_hdr;

#define USERDATA_ENTRY_HDR_SIZE 2
typedef struct {
    alt_u8 type;
    alt_u8 entry_len;
} userdata_entry;

typedef enum {
    UDE_REMOTE_MAP  = 0,
    UDE_AVCONFIG,
} userdata_entry_type;

#endif

int write_userdata();
int read_userdata();
