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

#include <string.h>
#include "userdata.h"
#include "flash.h"
#include "firmware.h"
#include "controls.h"
#include "av_controller.h"

extern alt_u16 rc_keymap[REMOTE_MAX_KEYS];
extern avconfig_t tc;

int write_userdata()
{
    alt_u8 databuf[PAGESIZE];
    int retval;

    strncpy((char*)databuf, "USRDATA", 8);
    databuf[8] = FW_VER_MAJOR;
    databuf[9] = FW_VER_MINOR;
    databuf[10] = 2;

    retval = write_flash_page(databuf, USERDATA_HDR_SIZE, (USERDATA_OFFSET/PAGESIZE));
    if (retval != 0) {
        return -1;
    }

    databuf[0] = UDE_REMOTE_MAP;
    databuf[1] = 4+sizeof(rc_keymap);
    databuf[2] = databuf[3] = 0;    //padding
    memcpy(databuf+4, rc_keymap, sizeof(rc_keymap));

    retval = write_flash_page(databuf, databuf[1], (USERDATA_OFFSET/PAGESIZE)+1);
    if (retval != 0) {
        return -1;
    }

    databuf[0] = UDE_AVCONFIG;
    databuf[1] = 4+sizeof(avconfig_t);
    databuf[2] = databuf[3] = 0;    //padding
    memcpy(databuf+4, &tc, sizeof(avconfig_t));

    retval = write_flash_page(databuf, databuf[1], (USERDATA_OFFSET/PAGESIZE)+2);
    if (retval != 0) {
        return -1;
    }

    return 0;
}

int read_userdata()
{
    int retval, i;
    alt_u8 databuf[PAGESIZE];
    userdata_hdr udhdr;
    userdata_entry udentry;

    retval = read_flash(USERDATA_OFFSET, USERDATA_HDR_SIZE, databuf);
    if (retval != 0) {
        printf("Flash read error\n");
        return -1;
    }

    strncpy(udhdr.userdata_key, (char*)databuf, 8);
    if (strncmp(udhdr.userdata_key, "USRDATA", 8)) {
        printf("No userdata found on flash\n");
        return 1;
    }

    udhdr.version_major = databuf[8];
    udhdr.version_minor = databuf[9];
    udhdr.num_entries = databuf[10];

    //TODO: check version compatibility
    printf("Userdata: v%u.%.2u, %u entries\n", udhdr.version_major, udhdr.version_minor, udhdr.num_entries);

    for (i=0; i<udhdr.num_entries; i++) {
        retval = read_flash(USERDATA_OFFSET+((i+1)*PAGESIZE), USERDATA_ENTRY_HDR_SIZE, databuf);
        if (retval != 0) {
            printf("Flash read error\n");
            return -1;
        }

        udentry.type = databuf[0];
        udentry.entry_len = databuf[1];

        retval = read_flash(USERDATA_OFFSET+((i+1)*PAGESIZE), udentry.entry_len, databuf);
        if (retval != 0) {
            printf("Flash read error\n");
            return -1;
        }

        switch (udentry.type) {
        case UDE_REMOTE_MAP:
            if ((udentry.entry_len-4) == sizeof(rc_keymap)) {
                memcpy(rc_keymap, databuf+4, sizeof(rc_keymap));
                printf("RC data read (%u bytes)\n", sizeof(rc_keymap));
            }
            break;
        case UDE_AVCONFIG:
            if ((udentry.entry_len-4) == sizeof(avconfig_t)) {
                memcpy(&tc, databuf+4, sizeof(avconfig_t));
                printf("Avconfig data read (%u bytes)\n", sizeof(avconfig_t));
            }
            break;
        default:
            printf("Unknown userdata entry\n");
            break;
        }
    }

    return 0;
}
