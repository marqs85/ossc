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

#ifndef LCD_H_
#define LCD_H_

#include "system.h"
#include <stdio.h>
#include "sysconfig.h"

#define LCD_ROW_LEN 16

//#define I2C_DEBUG
#define I2CA_BASE I2C_OPENCORES_0_BASE

#define LCD_CS_N    (1<<0)
#define LCD_RS      (1<<1)

void lcd_init();

void lcd_write(char *row1, char *row2);

#endif /* LCD_H_ */
