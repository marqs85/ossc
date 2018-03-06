//
// Copyright (C) 2015-2017  Markus Hiienkari <mhiienka@niksula.hut.fi>
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

#include "alt_types.h"
#include "sysconfig.h"

#define FW_VER_MAJOR            0
#define FW_VER_MINOR            81

#ifdef ENABLE_AUDIO
#define FW_SUFFIX1              "a"
#else
#define FW_SUFFIX1              ""
#endif

#ifdef OSDLANG_JP
#define FW_SUFFIX2              "j"
#else
#define FW_SUFFIX2              ""
#endif

#define FW_UPDATE_RETRIES       3

#define FW_IMAGE_ERROR          100
#define FW_HDR_ERROR            101
#define FW_HDR_CRC_ERROR        102
#define FW_DATA_CRC_ERROR       103
#define FW_UPD_CANCELLED        104

typedef struct {
    char fw_key[4];
    alt_u8 version_major;
    alt_u8 version_minor;
    char version_suffix[8];
    alt_u32 hdr_len;
    alt_u32 data_len;
    alt_u32 data_crc;
    alt_u32 hdr_crc;
} fw_hdr;

int fw_update();

#endif
