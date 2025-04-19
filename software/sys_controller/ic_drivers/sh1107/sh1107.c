//
// Copyright (C) 2025  Balázs Triszka <info@balika011.hu>
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
#include "sh1107.h"
#include "alt_types.h"
#include "altera_avalon_pio_regs.h"
#include "i2c_opencores.h"
#include "av_controller.h"
#include "font12.h"

#define OLED_WIDTH 128
#define OLED_HEIGHT 64

extern alt_u32 sys_ctrl;

static void OLED_Write(uint8_t value)
{
	SPI_write(I2CA_BASE, &value, 1);
}

void sh1107_init()
{
	sys_ctrl &= ~(LCD_CS_N | LCD_RS);
	IOWR_ALTERA_AVALON_PIO_DATA(PIO_0_BASE, sys_ctrl);

    alt_u8 init[] = {
        0xae,       /*turn off OLED display*/
        0x00,       /*set lower column address*/
        0x10,       /*set higher column address*/
        0xb0,       /*set page address*/
        0xdc, 0x00, /*set display start line*/
        0x81,       /*contract control*/
        0x6f,       /*128*/
        0x21,       /* Set Memory addressing mode (0x20/0x21) */
        0xa1,       /*set segment remap*/
        0xc0,       /*Com scan direction*/
        0xa4,       /*Disable Entire Display On (0xA4/0xA5)*/
        0xa6,       /*normal / reverse*/
        0xa8,       /*multiplex ratio*/
        0x3f,       /*duty = 1/64*/
        0xd3, 0x60, /*set display offset*/
        0xd5, 0x41, /*set osc division*/
        0xd9, 0x22, /*set pre-charge period*/
        0xdb, 0x35, /*set vcomh*/
        0xad,       /*set charge pump enable*/
        0x8a        /*Set DC-DC enable (a=0:disable; a=1:enable) */
    };
    SPI_write(I2CA_BASE, init, sizeof(init));

    // Clear the screen
    OLED_Write(0xb0);
    for (alt_u8 i = 0; i < OLED_HEIGHT; i++)
    {
        sys_ctrl &= ~LCD_RS;
        IOWR_ALTERA_AVALON_PIO_DATA(PIO_0_BASE, sys_ctrl);
        OLED_Write(0x00 + (i & 0x0f));
        OLED_Write(0x10 + (i >> 4));

        sys_ctrl |= LCD_RS;
        IOWR_ALTERA_AVALON_PIO_DATA(PIO_0_BASE, sys_ctrl);
        for (alt_u8 i = 0; i < OLED_WIDTH / 8; i++)
            OLED_Write(0);
    }

    sys_ctrl &= ~LCD_RS;
    IOWR_ALTERA_AVALON_PIO_DATA(PIO_0_BASE, sys_ctrl);

    // Turn on
    OLED_Write(0xaf);

    sys_ctrl |= LCD_CS_N;
    IOWR_ALTERA_AVALON_PIO_DATA(PIO_0_BASE, sys_ctrl);
}

void sh1107_write(char *row1, char *row2)
{
    sys_ctrl &= ~(LCD_CS_N | LCD_RS);
    IOWR_ALTERA_AVALON_PIO_DATA(PIO_0_BASE, sys_ctrl);

    alt_u8 row1len = strnlen(row1, LCD_ROW_LEN);

    for (alt_u8 i = 0; i < 12; i++)
    {
        sys_ctrl &= ~LCD_RS;
        IOWR_ALTERA_AVALON_PIO_DATA(PIO_0_BASE, sys_ctrl);
        OLED_Write(0xb0);
        OLED_Write(0x00 + ((i + 13) & 0x0f));
        OLED_Write(0x10 + ((i + 13) >> 4));

        sys_ctrl |= LCD_RS;
        IOWR_ALTERA_AVALON_PIO_DATA(PIO_0_BASE, sys_ctrl);
        for (alt_u8 j = 0; j < row1len; j++)
            OLED_Write(Font12_Table[(row1[j] - 0x20) * 12 + i]);

        for (alt_u8 j = row1len; j < LCD_ROW_LEN; j++)
            OLED_Write(Font12_Table[i]);
    }

    alt_u8 row2len = strnlen(row2, LCD_ROW_LEN);

    for (alt_u8 i = 0; i < 12; i++)
    {
        sys_ctrl &= ~LCD_RS;
        IOWR_ALTERA_AVALON_PIO_DATA(PIO_0_BASE, sys_ctrl);
        OLED_Write(0xb0);
        OLED_Write(0x00 + ((i + 39) & 0x0f));
        OLED_Write(0x10 + ((i + 39) >> 4));

        sys_ctrl |= LCD_RS;
        IOWR_ALTERA_AVALON_PIO_DATA(PIO_0_BASE, sys_ctrl);
        for (alt_u8 j = 0; j < row2len; j++)
            OLED_Write(Font12_Table[(row2[j] - 0x20) * 12 + i]);

        for (alt_u8 j = row2len; j < LCD_ROW_LEN; j++)
            OLED_Write(Font12_Table[i]);
    }

    sys_ctrl |= LCD_CS_N;
    IOWR_ALTERA_AVALON_PIO_DATA(PIO_0_BASE, sys_ctrl);
}
