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

#include <stdio.h>
#include <unistd.h>
#include "system.h"
#include "video_modes.h"

#define LINECNT_MAX_TOLERANCE   30

const mode_data_t video_modes_default[] = VIDEO_MODES_DEF;
mode_data_t video_modes[VIDEO_MODES_CNT];

/* TODO: rewrite, check hz etc. */
alt_8 get_mode_id(alt_u32 totlines, alt_u8 progressive, alt_u32 hz, video_type typemask, alt_u8 linemult_target, alt_u8 l3_mode, alt_u8 s480p_mode)
{
    alt_8 i;
    alt_u8 num_modes = sizeof(video_modes)/sizeof(mode_data_t);
    video_type mode_type;

    // TODO: a better check
    for (i=0; i<num_modes; i++) {
        mode_type = video_modes[i].type;

        // disable particular 480p mode based on input and user preference
        if (video_modes[i].flags & MODE_DTV480P) {
            if (s480p_mode == 0)  // auto
                mode_type &= ~VIDEO_PC;
            else if (s480p_mode == 2)  // VGA 640x480
                mode_type = 0;
        } else if (video_modes[i].flags & MODE_VGA480P) {
            if (s480p_mode == 0)  // auto
                mode_type &= ~VIDEO_EDTV;
            else if (s480p_mode == 1) // DTV 480P
                mode_type = 0;
        }

        if ((typemask & mode_type) && (progressive == !(video_modes[i].flags & MODE_INTERLACED)) && (totlines <= (video_modes[i].v_total+LINECNT_MAX_TOLERANCE))) {
            if (linemult_target && (video_modes[i].flags & MODE_L3ENABLE_MASK) && ((video_modes[i].flags & MODE_L3ENABLE_MASK) == (1<<l3_mode))) {
                return i;
            } else if (!(video_modes[i].flags & MODE_L3ENABLE_MASK)) {
                return i;
            }
        }
    }

    return -1;
}
