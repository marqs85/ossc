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

#include <unistd.h>
#include <string.h>
#include "system.h"
#include "flash.h"
#include "utils.h"

// save some code space
#define SINGLE_FLASH_INSTANCE

alt_flash_dev *epcq_dev;


int init_flash()
{
#ifdef SINGLE_FLASH_INSTANCE
    extern alt_llist alt_flash_dev_list;
    epcq_dev = (alt_flash_dev*)alt_flash_dev_list.next;
#else
    epcq_dev = alt_flash_open_dev(EPCQ_CONTROLLER_0_AVL_MEM_NAME);
#endif

    if (epcq_dev == NULL)
        return -1;

    return 0;
}

int verify_flash(alt_u32 offset, alt_u32 length, alt_u32 golden_crc, alt_u8 *tmpbuf)
{
    alt_u32 crcval=0, i, bytes_to_read;
    int retval;

    for (i=0; i<length; i=i+PAGESIZE) {
        bytes_to_read = ((length-i < PAGESIZE) ? (length-i) : PAGESIZE);

        //retval = read_flash(i, bytes_to_read, tmpbuf);
        retval = alt_epcq_controller_read(epcq_dev, offset+i, tmpbuf, bytes_to_read);
        if (retval != 0)
            return retval;

        crcval = crc32(tmpbuf, bytes_to_read, (i==0));
    }

    if (crcval != golden_crc)
        return -FLASH_VERIFY_ERROR;

    return 0;
}
