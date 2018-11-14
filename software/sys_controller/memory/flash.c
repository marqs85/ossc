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

extern alt_epcq_controller_dev epcq_controller_0;

alt_epcq_controller_dev *epcq_controller_dev;


int check_flash()
{
    epcq_controller_dev = &epcq_controller_0;

    if ((epcq_controller_dev == NULL) || !(epcq_controller_dev->is_epcs && (epcq_controller_dev->page_size == PAGESIZE)))
        return -FLASH_DETECT_ERROR;

    printf("Flash size in bytes: %lu\nSector size: %lu (%lu pages)\nPage size: %lu\n",
            epcq_controller_dev->size_in_bytes, epcq_controller_dev->sector_size, epcq_controller_dev->sector_size/epcq_controller_dev->page_size, epcq_controller_dev->page_size);

    return 0;
}

int read_flash(alt_u32 offset, alt_u32 length, alt_u8 *dstbuf)
{
    int retval, i;

    retval = alt_epcq_controller_read(&epcq_controller_dev->dev, offset, dstbuf, length);
    if (retval != 0)
        return -FLASH_READ_ERROR;

    return 0;
}

int write_flash_page(alt_u8 *pagedata, alt_u32 length, alt_u32 pagenum)
{
    int retval, i;

    if ((pagenum % PAGES_PER_SECTOR) == 0) {
        printf("Erasing sector %u\n", (unsigned)(pagenum/PAGES_PER_SECTOR));
        retval = alt_epcq_controller_erase_block(&epcq_controller_dev->dev, pagenum*PAGESIZE);

        if (retval != 0) {
            printf("Flash erase error, sector %u\nRetval %d\n", (unsigned)(pagenum/PAGES_PER_SECTOR), retval);
            return -FLASH_ERASE_ERROR;
        }
    }

    retval = alt_epcq_controller_write_block(&epcq_controller_dev->dev, (pagenum/PAGES_PER_SECTOR)*PAGES_PER_SECTOR*PAGESIZE, pagenum*PAGESIZE, pagedata, length);

    if (retval != 0) {
        printf("Flash write error, page %u\nRetval %d\n", (unsigned)pagenum, retval);
        return -FLASH_WRITE_ERROR;
    }

    return 0;
}

int write_flash(alt_u8 *buf, alt_u32 length, alt_u32 pagenum)
{
    int retval;
    alt_u32 bytes_to_w;

    while (length > 0) {
        bytes_to_w = (length > PAGESIZE) ? PAGESIZE : length;

        retval = write_flash_page(buf, bytes_to_w, pagenum);
        if (retval != 0)
            return retval;

        buf += bytes_to_w;
        length -= bytes_to_w;
        ++pagenum;
    }

    return 0;
}

int verify_flash(alt_u32 offset, alt_u32 length, alt_u32 golden_crc, alt_u8 *tmpbuf)
{
    alt_u32 crcval=0, i, bytes_to_read;
    int retval;

    for (i=0; i<length; i=i+PAGESIZE) {
        bytes_to_read = ((length-i < PAGESIZE) ? (length-i) : PAGESIZE);

        retval = read_flash(i, bytes_to_read, tmpbuf);
        if (retval != 0)
            return retval;

        crcval = crc32(tmpbuf, bytes_to_read, (i==0));
    }

    if (crcval != golden_crc)
        return -FLASH_VERIFY_ERROR;

    return 0;
}
