//
// Copyright (C) 2015-2016  Markus Hiienkari <mhiienka@niksula.hut.fi>
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

#ifndef ST7032_H_
#define ST7032_H_

#include "system.h"
#include <stdio.h>
#include "sysconfig.h"

void st7032_init();

void st7032_write(char *row1, char *row2);

#endif /* st7032_H_ */
