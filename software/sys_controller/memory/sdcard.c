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

#include <io.h>
#include <string.h>
#include "sdcard.h"
#include "flash.h"
#include "lcd.h"

extern alt_flash_dev *epcq_dev;

SD_DEV sdcard_dev;

int check_sdcard(alt_u8 *databuf)
{
    SDRESULTS res;

    res = SD_Init(&sdcard_dev);
    printf("SD det status: %u\n", res);
    if (res == SD_OK)
        res = SD_Read(&sdcard_dev, databuf, 0, 0, 512);

    return -res;
}

int copy_sd_to_flash(alt_u32 sd_blknum, alt_u32 flash_pagenum, alt_u32 length, alt_u8 *tmpbuf)
{
    SDRESULTS res;
    int retval;
    alt_u32 bytes_to_rw;

    while (length > 0) {
        bytes_to_rw = (length < SD_BLK_SIZE) ? length : SD_BLK_SIZE;
        res = SD_Read(&sdcard_dev, tmpbuf, sd_blknum, 0, bytes_to_rw);
        if (res != SD_OK) {
            printf("Failed to read SD card\n");
            return -res;
        }

        if ((flash_pagenum % PAGES_PER_SECTOR) == 0) {
            retval = alt_epcq_controller_erase_block(epcq_dev, flash_pagenum*PAGESIZE);
            if (retval != 0)
                return retval;
        }

        retval = alt_epcq_controller_write_block(epcq_dev, ((flash_pagenum/PAGES_PER_SECTOR)*SECTORSIZE), flash_pagenum*PAGESIZE, tmpbuf, bytes_to_rw);
        if (retval != 0)
            return retval;

        ++sd_blknum;
        flash_pagenum += bytes_to_rw/PAGESIZE;
        length -= bytes_to_rw;
    }

    return 0;
}

int copy_flash_to_sd(alt_u32 flash_pagenum, alt_u32 sd_blknum, alt_u32 length, alt_u8 *tmpbuf)
{
    SDRESULTS res;
    int retval;
    alt_u32 bytes_to_rw;

    while (length > 0) {
        bytes_to_rw = (length < SD_BLK_SIZE) ? length : SD_BLK_SIZE;
        retval = alt_epcq_controller_read(epcq_dev, flash_pagenum*PAGESIZE, tmpbuf, bytes_to_rw);
        if (retval != 0)
            return retval;

        if (bytes_to_rw < SD_BLK_SIZE)
            memset(tmpbuf+bytes_to_rw, 0, SD_BLK_SIZE-bytes_to_rw);

        res = SD_Write(&sdcard_dev, tmpbuf, sd_blknum);
        if (res != SD_OK) {
            printf("Failed to write to SD card\n");
            return -res;
        }

        ++sd_blknum;
        flash_pagenum += bytes_to_rw/PAGESIZE;
        length -= bytes_to_rw;
    }

    return 0;

}
