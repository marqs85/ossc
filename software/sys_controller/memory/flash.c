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
#include "flash.h"
#include "lcd.h"
#include "ci_crc.h"

extern alt_epcq_controller_dev epcq_controller_0;
extern char menu_row1[LCD_ROW_LEN+1], menu_row2[LCD_ROW_LEN+1];

alt_epcq_controller_dev *epcq_controller_dev;


int check_flash()
{
    epcq_controller_dev = &epcq_controller_0;

    if ((epcq_controller_dev == NULL) || !(epcq_controller_dev->is_epcs && (epcq_controller_dev->page_size == PAGESIZE)))
        return -1;

    printf("Flash size in bytes: %d\nSector size: %d (%d pages)\nPage size: %d\n",
            epcq_controller_dev->size_in_bytes, epcq_controller_dev->sector_size, epcq_controller_dev->sector_size/epcq_controller_dev->page_size, epcq_controller_dev->page_size);

    return 0;
}

int read_flash(alt_u32 offset, alt_u32 length, alt_u8 *dstbuf)
{
    int retval, i;

    retval = alt_epcq_controller_read(&epcq_controller_dev->dev, offset, dstbuf, length);
    if (retval != 0)
        return -1;

    for (i=0; i<length; i++)
        dstbuf[i] = ALT_CI_NIOS_CUSTOM_INSTR_BITSWAP_0(dstbuf[i]) >> 24;

    return 0;
}

int write_flash_page(alt_u8 *pagedata, alt_u32 length, alt_u32 pagenum)
{
    int retval, i;

    if ((pagenum % PAGES_PER_SECTOR) == 0) {
        printf("Erasing sector %u\n", (unsigned)(pagenum/PAGES_PER_SECTOR));
        retval = alt_epcq_controller_erase_block(&epcq_controller_dev->dev, pagenum*PAGESIZE);

        if (retval != 0) {
            strncpy(menu_row1, "Flash erase", LCD_ROW_LEN+1);
            sniprintf(menu_row1, LCD_ROW_LEN+1, "error %d", retval);
            menu_row2[0] = '\0';
            printf("Flash erase error, sector %u\nRetval %d\n", (unsigned)(pagenum/PAGES_PER_SECTOR), retval);
            return -200;
        }
    }

    // Bit-reverse bytes for flash
    for (i=0; i<length; i++)
        pagedata[i] = ALT_CI_NIOS_CUSTOM_INSTR_BITSWAP_0(pagedata[i]) >> 24;

    retval = alt_epcq_controller_write_block(&epcq_controller_dev->dev, (pagenum/PAGES_PER_SECTOR)*PAGES_PER_SECTOR*PAGESIZE, pagenum*PAGESIZE, pagedata, length);

    if (retval != 0) {
        strncpy(menu_row1, "Flash write", LCD_ROW_LEN+1);
        strncpy(menu_row2, "error", LCD_ROW_LEN+1);
        printf("Flash write error, page %u\nRetval %d\n", (unsigned)pagenum, retval);
        return -201;
    }

    return retval;
}

int verify_flash(alt_u32 offset, alt_u32 length, alt_u32 golden_crc, alt_u8 *tmpbuf)
{
    alt_u32 crcval=0, i, bytes_to_read;
    int retval;

    for (i=0; i<length; i=i+PAGESIZE) {
        bytes_to_read = ((length-i < PAGESIZE) ? (length-i) : PAGESIZE);

        retval = read_flash(i, bytes_to_read, tmpbuf);
        if (retval != 0)
            return -202;

        crcval = crcCI(tmpbuf, bytes_to_read, (i==0));
    }

    if (crcval != golden_crc) {
        sniprintf(menu_row1, LCD_ROW_LEN+1, "Flash verif fail");
        menu_row2[0] = '\0';
        return -203;
    }

    return 0;
}
