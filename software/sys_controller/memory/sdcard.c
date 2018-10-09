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
#include "sdcard.h"
#include "flash.h"
#include "lcd.h"

extern char menu_row1[LCD_ROW_LEN+1], menu_row2[LCD_ROW_LEN+1];

SD_DEV sdcard_dev;

int check_sdcard(alt_u8 *databuf)
{
    SDRESULTS res;

    res = SD_Init(&sdcard_dev);
    printf("SD det status: %u\n", res);
    if (res != SD_OK)
        return res;

    return SD_Read(&sdcard_dev, databuf, 0, 0, 512);
}

int copy_sd_to_flash(alt_u32 sd_blknum, alt_u32 flash_pagenum, alt_u32 length, alt_u8 *tmpbuf)
{
    int retval;
    alt_u32 bytes_to_rw;

    while (length > 0) {
        bytes_to_rw = (length < SD_BLK_SIZE) ? length : SD_BLK_SIZE;
        retval = SD_Read(&sdcard_dev, tmpbuf, sd_blknum, 0, bytes_to_rw);
        if (retval != 0) {
            printf("Failed to read SD card\n");
            return -retval;
        }

        retval = write_flash(tmpbuf, bytes_to_rw, flash_pagenum, NULL);
        if (retval != 0)
            return retval;

        ++sd_blknum;
        flash_pagenum += bytes_to_rw/PAGESIZE;
        length -= bytes_to_rw;
    }

    return 0;
}
