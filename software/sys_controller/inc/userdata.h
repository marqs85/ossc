//
// Copyright (C) 2015-2025  Markus Hiienkari <mhiienka@niksula.hut.fi>
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

#include <stdint.h>
#include "sysconfig.h"

#define USERDATA_NAME_LEN 13
#define MAX_USERDATA_ENTRY 15
#define MAX_SD_USERDATA_ENTRY 100

#define MAX_PROFILE (MAX_USERDATA_ENTRY-1)
#define MAX_SD_PROFILE (MAX_SD_USERDATA_ENTRY-1)
#define INIT_CONFIG_SLOT MAX_USERDATA_ENTRY
#define SD_INIT_CONFIG_SLOT MAX_SD_USERDATA_ENTRY

typedef enum {
    UDE_INITCFG  = 0,
    UDE_PROFILE,
} ude_type;

typedef struct {
    char userdata_key[8];
    char name[USERDATA_NAME_LEN+1];
    ude_type type;
    uint8_t num_items;
} __attribute__((packed, __may_alias__)) ude_hdr;

typedef struct {
    uint16_t id;
    uint16_t version;
    uint16_t data_size;
} __attribute__((packed, __may_alias__)) ude_item_hdr;

typedef struct {
    ude_item_hdr hdr;
    void *data;
} ude_item_map;

int write_userdata(uint8_t entry);
int read_userdata(uint8_t entry, int dry_run);
int write_userdata_sd(uint8_t entry);
int read_userdata_sd(uint8_t entry, int dry_run);
int import_userdata();
int export_userdata();

#endif
