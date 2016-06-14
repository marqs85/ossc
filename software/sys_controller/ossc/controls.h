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

#ifndef CONTROLS_H_
#define CONTROLS_H_

#define RC_MASK             0x0000ffff
#define PB_MASK             0x00030000
#define PB0_BIT             0x00010000
#define PB1_BIT             0x00020000

typedef enum {
    RC_BTN1                 = 0,
    RC_BTN2,
    RC_BTN3,
    RC_BTN4,
    RC_BTN5,
    RC_BTN6,
    RC_BTN7,
    RC_BTN8,
    RC_BTN9,
    RC_BTN0,
    RC_MENU,
    RC_OK,
    RC_BACK,
    RC_UP,
    RC_DOWN,
    RC_LEFT,
    RC_RIGHT,
    RC_INFO,
    RC_LCDBL,
    RC_SL_MODE,
    RC_SL_TYPE,
    RC_SL_PLUS,
    RC_SL_MINUS,
    RC_LM_MODE,
} rc_code_t;

#define REMOTE_MAX_KEYS RC_LM_MODE-RC_BTN1+1

void setup_rc();
void parse_control();

#endif
