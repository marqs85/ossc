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
#include "lcd.h"

extern char menu_row1[LCD_ROW_LEN+1], menu_row2[LCD_ROW_LEN+1];

alt_up_sd_card_dev *sdcard_dev;


int read_sd_block(alt_u32 offset, alt_u32 size, alt_u8 *dstbuf)
{
    /*int i;
    alt_u32 tmp;

    if ((offset % SD_BUFFER_SIZE) || (size > 512)) {
        sniprintf(menu_row1, LCD_ROW_LEN+1, "Invalid read cmd");
        menu_row2[0] = '\0';
        return -1;
    }

    if (!Read_Sector_Data((offset/SD_BUFFER_SIZE), 0)) {
        sniprintf(menu_row1, LCD_ROW_LEN+1, "SD read failure");
        menu_row2[0] = '\0';
        return -2;
    }

    // Copy buffer to SW
    for (i=0; i<size; i=i+4) {
        tmp = IORD_32DIRECT(sdcard_dev->base, i);
        *((alt_u32*)(dstbuf+i)) = tmp;
    }
*/
    return 0;
}

int check_sdcard(alt_u8 *databuf)
{
  /*  sdcard_dev = alt_up_sd_card_open_dev(ALTERA_UP_SD_CARD_AVALON_INTERFACE_0_NAME);

    if ((sdcard_dev == NULL) || !alt_up_sd_card_is_Present()) {
        sniprintf(menu_row1, LCD_ROW_LEN+1, "No SD card det.");
        menu_row2[0] = '\0';
        return 1;
    }

    return read_sd_block(0, 512, databuf);*/
    return 0;
}
