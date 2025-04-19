//
// Copyright (C) 2017  Markus Hiienkari <mhiienka@niksula.hut.fi>
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

#ifndef PCM1862_H_
#define PCM1862_H_

#include "pcm1862_regs.h"
#include "sysconfig.h"

typedef enum {
    PCM_INPUT1 = 0,
    PCM_INPUT2 = 1,
    PCM_INPUT3 = 2,
    PCM_INPUT4 = 3
} pcm_input_t;

void pcm_source_sel(pcm_input_t input);

void pcm_set_stereo_mode(int mono_enable);

void pcm_set_gain(alt_8 db_gain);

int pcm1862_init();

#endif /* PCM1862_H_ */
