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
#include "lcd.h"
#include "alt_types.h"
#include "altera_avalon_pio_regs.h"
#include "i2c_opencores.h"

#define LCD_CMD     0x00
#define LCD_DATA    0x40

#define WRDELAY     20
#define CLEARDELAY  800

void lcd_init()
{
    alt_u8 lcd_ctrl = 0x00;
    IOWR_ALTERA_AVALON_PIO_DATA(PIO_5_BASE, lcd_ctrl);
    usleep(WRDELAY);

    SPI_write(I2CA_BASE, 0x38);    // function set
    usleep(WRDELAY);
    SPI_write(I2CA_BASE, 0x39);    // function set, select extended table (IS=1)
    usleep(WRDELAY);
    SPI_write(I2CA_BASE, 0x14);    // osc freq
    usleep(WRDELAY);
    SPI_write(I2CA_BASE, 0x71);    // contrast set
    usleep(WRDELAY);
    SPI_write(I2CA_BASE, 0x5E);    // power/icon/cont
    usleep(WRDELAY);
    SPI_write(I2CA_BASE, 0x6D);    // follower control
    usleep(WRDELAY);
    SPI_write(I2CA_BASE, 0x0C);    // display on
    usleep(WRDELAY);
    SPI_write(I2CA_BASE, 0x01);    // clear display
    usleep(CLEARDELAY);
    SPI_write(I2CA_BASE, 0x06);    // entry mode set
    usleep(WRDELAY);
    SPI_write(I2CA_BASE, 0x02);    // return home
    usleep(CLEARDELAY);

    lcd_ctrl |= LCD_CS_N;
    IOWR_ALTERA_AVALON_PIO_DATA(PIO_5_BASE, lcd_ctrl);
}

void lcd_write(char *row1, char *row2)
{
    alt_u8 i, rowlen;
    alt_u8 lcd_ctrl = 0x00;

    IOWR_ALTERA_AVALON_PIO_DATA(PIO_5_BASE, lcd_ctrl);

    SPI_write(I2CA_BASE, 0x01);    // clear display
    usleep(CLEARDELAY);

    // Set RS to enter data write mode
    lcd_ctrl |= LCD_RS;
    IOWR_ALTERA_AVALON_PIO_DATA(PIO_5_BASE, lcd_ctrl);

    //ensure no empty row
    rowlen = strnlen(row1, LCD_ROW_LEN);
    if (rowlen == 0) {
        strncpy(row1, " ", LCD_ROW_LEN+1);
        rowlen++;
    }

    for (i=0; i<rowlen; i++) {
        SPI_write(I2CA_BASE, row1[i]);
        usleep(WRDELAY);
    }

    // second row
    lcd_ctrl &= ~LCD_RS;
    IOWR_ALTERA_AVALON_PIO_DATA(PIO_5_BASE, lcd_ctrl);
    SPI_write(I2CA_BASE, (1<<7)|0x40);
    usleep(WRDELAY);

    lcd_ctrl |= LCD_RS;
    IOWR_ALTERA_AVALON_PIO_DATA(PIO_5_BASE, lcd_ctrl);

    //ensure no empty row
    rowlen = strnlen(row2, LCD_ROW_LEN);
    if (rowlen == 0) {
        strncpy(row2, " ", LCD_ROW_LEN+1);
        rowlen++;
    }

    for (i=0; i<rowlen; i++) {
        SPI_write(I2CA_BASE, row2[i]);
        usleep(WRDELAY);
    }

    lcd_ctrl |= LCD_CS_N;
    IOWR_ALTERA_AVALON_PIO_DATA(PIO_5_BASE, lcd_ctrl);
}
