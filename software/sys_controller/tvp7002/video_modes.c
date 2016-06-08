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

const mode_data_t video_modes[] = {
    { "240p_L3M0",  1280,  240, 6000, 1560,   262, 170, 16,  72,  3, (VIDEO_SDTV|VIDEO_PC),   (MODE_L3_MODE0|MODE_PLLDIVBY2) },
    { "240p_L3M1",   960,  240, 6000, 1170,   262, 128, 16,  54,  3, (VIDEO_SDTV|VIDEO_PC),   (MODE_L3_MODE1|MODE_PLLDIVBY2) },
    //{ "240p_L3M2",   384,  240, 6000,  512,   262,  66, 16,  31,  3, (VIDEO_LDTV|VIDEO_PC),   (MODE_L3_MODE2|MODE_PLLDIVBY2) },                 //CPS2
    { "240p_L3M2",   320,  240, 6000,  426,   262,  49, 16,  31,  3, (VIDEO_LDTV|VIDEO_PC),   (MODE_L3_MODE2|MODE_PLLDIVBY2) },
    { "240p_L3M3",   256,  240, 6000,  341,   262,  39, 16,  25,  3, (VIDEO_LDTV|VIDEO_PC),   (MODE_L3_MODE3|MODE_PLLDIVBY2) },
    { "240p",        720,  240, 6000,  858,   262,  65, 16,  60,  3, (VIDEO_SDTV|VIDEO_PC),   (MODE_L2ENABLE|MODE_PLLDIVBY2) },
    { "288p_L3M0",  1280,  288, 5000, 1560,   312, 170, 16,  72,  3, (VIDEO_SDTV|VIDEO_PC),   (MODE_L3_MODE0|MODE_PLLDIVBY2) },
    { "288p_L3M1",   960,  288, 5000, 1170,   312, 128, 16,  54,  3, (VIDEO_SDTV|VIDEO_PC),   (MODE_L3_MODE1|MODE_PLLDIVBY2) },
    { "288p_L3M2",   320,  240, 5000,  426,   312,  49, 41,  31,  3, (VIDEO_LDTV|VIDEO_PC),   (MODE_L3_MODE2|MODE_PLLDIVBY2) },
    { "288p_L3M3",   256,  240, 5000,  341,   312,  39, 41,  25,  3, (VIDEO_LDTV|VIDEO_PC),   (MODE_L3_MODE3|MODE_PLLDIVBY2) },
    { "288p",        720,  288, 5000,  864,   312,  65, 16,  60,  3, (VIDEO_SDTV|VIDEO_PC),   (MODE_L2ENABLE|MODE_PLLDIVBY2) },
    { "384p",        496,  384, 5766,  640,   423,  50, 29,  62,  3, VIDEO_EDTV,              (MODE_L2ENABLE|MODE_PLLDIVBY2) },                 //Sega Model 2
    { "640x384",     640,  384, 5500,  800,   492,  48, 63,  96,  2, VIDEO_PC,                (MODE_L2ENABLE) },                                //X68k @ 24kHz
    { "480i",        720,  240, 5994,  858,   525,  65, 16,  60,  3, (VIDEO_SDTV|VIDEO_PC),   (MODE_L2ENABLE|MODE_PLLDIVBY2|MODE_INTERLACED) },
    { "480p",        720,  480, 5994,  858,   525,  60, 30,  62,  6, (VIDEO_EDTV|VIDEO_PC),   (MODE_DTV480P) },
    { "640x480",     640,  480, 6000,  800,   525,  48, 33,  96,  2, (VIDEO_PC|VIDEO_EDTV),   (MODE_VGA480P) },
    { "640x512",     640,  512, 6000,  800,   568,  48, 28,  96,  2, VIDEO_PC,                0 },                                              //X68k @ 31kHz
    { "576i",        720,  288, 5000,  864,   625,  65, 16,  60,  3, (VIDEO_SDTV|VIDEO_PC),   (MODE_L2ENABLE|MODE_PLLDIVBY2|MODE_INTERLACED) },
    { "576p",        720,  576, 5000,  864,   625,  65, 32,  60,  6, VIDEO_EDTV,              0 },
    { "800x600",     800,  600, 6000, 1056,   628,  88, 23, 128,  4, VIDEO_PC,                0 },
    { "720p",       1280,  720, 5994, 1650,   750, 255, 20,  40,  5, VIDEO_HDTV,              0 },
    { "1280x720",   1280,  720, 6000, 1650,   750, 220, 20,  40,  5, VIDEO_PC,                0 },
    { "1024x768",   1024,  768, 6000, 1344,   806, 160, 29, 136,  6, VIDEO_PC,                0 },
    { "1280x1024",  1280, 1024, 6000, 1688,  1066, 248, 38, 112,  3, VIDEO_PC,                0 },
    { "1080i",      1920, 1080, 5994, 2200,  1125, 148, 16,  44,  5, VIDEO_HDTV,              (MODE_INTERLACED) },                              //Too high freq for L2 PLL
    { "1080p",      1920, 1080, 5994, 2200,  1125, 188, 36,  44,  5, VIDEO_HDTV,              0 },
    { "1920x1080",  1920, 1080, 6000, 2200,  1125, 148, 36,  44,  5, VIDEO_PC,                0 },
};

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
