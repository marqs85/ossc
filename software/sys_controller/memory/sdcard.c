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

SD_DEV sdcard_dev;

int check_sdcard(alt_u8 *databuf)
{
    SDRESULTS res;

    res = SD_Init(&sdcard_dev);
    printf("SD det status: %u\n", res);
    if (res) {
        sniprintf(menu_row1, LCD_ROW_LEN+1, "No SD card det.");
        menu_row2[0] = '\0';
        return 1;
    }

    return SD_Read(&sdcard_dev, databuf, 0, 0, 512);
}
