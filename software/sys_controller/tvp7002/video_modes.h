//
// Copyright (C) 2015-2017  Markus Hiienkari <mhiienka@niksula.hut.fi>
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

#ifndef VIDEO_MODES_H_
#define VIDEO_MODES_H_

#include <alt_types.h>
#include "sysconfig.h"

#define H_TOTAL_MIN 300
#define H_TOTAL_MAX 2300
#define H_SYNCLEN_MIN 10
#define H_SYNCLEN_MAX 200
#define H_BPORCH_MIN 1
#define H_BPORCH_MAX 255
#define H_ACTIVE_MIN 200
#define H_ACTIVE_MAX 1920
#define V_SYNCLEN_MIN 1
#define V_SYNCLEN_MAX 7
#define V_BPORCH_MIN 1
#define V_BPORCH_MAX 63
#define V_ACTIVE_MIN 200
#define V_ACTIVE_MAX 1200

typedef enum {
    FORMAT_RGBS = 0,
    FORMAT_RGBHV = 1,
    FORMAT_RGsB = 2,
    FORMAT_YPbPr = 3
} video_format;

typedef enum {
    VIDEO_LDTV      = (1<<0),
    VIDEO_SDTV      = (1<<1),
    VIDEO_EDTV      = (1<<2),
    VIDEO_HDTV      = (1<<3),
    VIDEO_PC        = (1<<4),
} video_type;

typedef enum {
    GROUP_NONE      = 0,
    GROUP_240P      = 1,
    GROUP_384P      = 2,
    GROUP_480I      = 3,
    GROUP_DTV480P   = 4,
    GROUP_VGA480P   = 5,
    GROUP_1080I     = 6,
} video_group;

typedef enum {
    MODE_INTERLACED     = (1<<0),
    MODE_PLLDIVBY2      = (1<<1),
    //at least one of the flags below must be set for each mode
    MODE_PT             = (1<<2),
    MODE_L2             = (1<<3),
    MODE_L2_320_COL     = (1<<4),
    MODE_L2_256_COL     = (1<<5),
    MODE_L3_GEN_16_9    = (1<<6),
    MODE_L3_GEN_4_3     = (1<<7),
    MODE_L3_320_COL     = (1<<8),
    MODE_L3_256_COL     = (1<<9),
    MODE_L4_GEN_4_3     = (1<<10),
    MODE_L4_320_COL     = (1<<11),
    MODE_L4_256_COL     = (1<<12),
    MODE_L5_GEN_4_3     = (1<<13),
    MODE_L5_320_COL     = (1<<14),
    MODE_L5_256_COL     = (1<<15),
} mode_flags;

typedef struct {
    char name[10];
    alt_u16 h_active;
    alt_u16 v_active;
    alt_u16 hz_x100;
    alt_u16 h_total;
    alt_u16 v_total;
    alt_u8 h_backporch;
    alt_u8 v_backporch;
    alt_u8 h_synclen;
    alt_u8 v_synclen;
    video_type type;
    video_group group;
    mode_flags flags;
} mode_data_t;


#define VIDEO_MODES_DEF { \
    /* 240p modes */ \
    { "1536x240",   1536,  240, 6000, 2046,   262, 234, 15, 150,  3, (VIDEO_SDTV | VIDEO_PC),   GROUP_240P,     (MODE_L5_GEN_4_3 | MODE_PLLDIVBY2) },                                                   \
    { "1280x240",   1280,  240, 6000, 1560,   262, 170, 15,  72,  3, (VIDEO_SDTV | VIDEO_PC),   GROUP_240P,     (MODE_L3_GEN_16_9 | MODE_L4_GEN_4_3 | MODE_PLLDIVBY2) },                                \
    { "960x240",     960,  240, 6000, 1170,   262, 128, 15,  54,  3, (VIDEO_SDTV | VIDEO_PC),   GROUP_240P,     (MODE_L3_GEN_4_3 | MODE_PLLDIVBY2) },                                                   \
    { "320x240",     320,  240, 6000,  426,   262,  49, 14,  31,  3, (VIDEO_SDTV | VIDEO_PC),   GROUP_240P,     (MODE_L2_320_COL | MODE_L3_320_COL | MODE_L4_320_COL | MODE_L5_320_COL) },              \
    { "256x240",     256,  240, 6000,  341,   262,  39, 14,  25,  3, (VIDEO_SDTV | VIDEO_PC),   GROUP_240P,     (MODE_L2_256_COL | MODE_L3_256_COL | MODE_L4_256_COL | MODE_L5_256_COL) },              \
    { "240p",        720,  240, 6000,  858,   262,  57, 15,  62,  3, (VIDEO_SDTV | VIDEO_PC),   GROUP_240P,     (MODE_PT | MODE_L2 | MODE_PLLDIVBY2) },                                                 \
    /* 288p modes */ \
    { "1536x240L",  1536,  240, 5000, 2046,   312, 234, 41, 150,  3, (VIDEO_SDTV | VIDEO_PC),   GROUP_240P,     (MODE_L5_GEN_4_3 | MODE_PLLDIVBY2) },                                                   \
    { "1280x288",   1280,  288, 5000, 1560,   312, 170, 15,  72,  3, (VIDEO_SDTV | VIDEO_PC),   GROUP_240P,     (MODE_L3_GEN_16_9 | MODE_L4_GEN_4_3 | MODE_PLLDIVBY2) },                                \
    { "960x288",     960,  288, 5000, 1170,   312, 128, 15,  54,  3, (VIDEO_SDTV | VIDEO_PC),   GROUP_240P,     (MODE_L3_GEN_4_3 | MODE_PLLDIVBY2) },                                                   \
    { "320x240LB",   320,  240, 5000,  426,   312,  49, 41,  31,  3, (VIDEO_SDTV | VIDEO_PC),   GROUP_240P,     (MODE_L2_320_COL | MODE_L3_320_COL | MODE_L4_320_COL | MODE_L5_320_COL) },              \
    { "256x240LB",   256,  240, 5000,  341,   312,  39, 41,  25,  3, (VIDEO_SDTV | VIDEO_PC),   GROUP_240P,     (MODE_L2_256_COL | MODE_L3_256_COL | MODE_L4_256_COL | MODE_L5_256_COL) },              \
    { "288p",        720,  288, 5000,  864,   312,  69, 19,  63,  3, (VIDEO_SDTV | VIDEO_PC),   GROUP_240P,     (MODE_PT | MODE_L2 | MODE_PLLDIVBY2) },                                                 \
    /* 384p: Sega Model 2 */ \
    { "384p",        496,  384, 5766,  640,   423,  50, 29,  62,  3, (VIDEO_EDTV),              GROUP_384P,     (MODE_PT | MODE_L2 | MODE_PLLDIVBY2) },                                                 \
    /* 384p: X68k @ 24kHz */ \
    { "640x384",     640,  384, 5500,  800,   492,  48, 63,  96,  2, VIDEO_PC,                  GROUP_384P,     (MODE_PT | MODE_L2 | MODE_PLLDIVBY2) },                                                 \
    /* ~525-line modes */ \
    { "480i",        720,  240, 5994,  858,   525,  57, 15,  62,  3, (VIDEO_SDTV | VIDEO_PC),   GROUP_480I,     (MODE_PT | MODE_L2 | MODE_L3_GEN_16_9 | MODE_L4_GEN_4_3 | MODE_PLLDIVBY2 | MODE_INTERLACED) },   \
    { "480p",        720,  480, 5994,  858,   525,  60, 30,  62,  6, (VIDEO_EDTV | VIDEO_PC),   GROUP_DTV480P,  (MODE_PT | MODE_L2) },                                                                          \
    { "640x480",     640,  480, 6000,  800,   525,  48, 33,  96,  2, (VIDEO_PC | VIDEO_EDTV),   GROUP_VGA480P,  (MODE_PT | MODE_L2) },                                                                          \
    /* X68k @ 31kHz */ \
    { "640x512",     640,  512, 6000,  800,   568,  48, 28,  96,  2, VIDEO_PC,                  GROUP_NONE,     MODE_PT },                                                                                      \
    /* ~625-line modes */ \
    { "576i",        720,  288, 5000,  864,   625,  69, 19,  63,  3, (VIDEO_SDTV | VIDEO_PC),   GROUP_480I,     (MODE_PT | MODE_L2 | MODE_L3_GEN_16_9 | MODE_L4_GEN_4_3 | MODE_PLLDIVBY2 | MODE_INTERLACED) },   \
    { "576p",        720,  576, 5000,  864,   625,  68, 39,  64,  5, VIDEO_EDTV,                GROUP_DTV480P,  (MODE_PT | MODE_L2) },                                                                  \
    { "800x600",     800,  600, 6000, 1056,   628,  88, 23, 128,  4, VIDEO_PC,                  GROUP_NONE,     MODE_PT },                                                                              \
    /* 720p modes */ \
    { "720p",       1280,  720, 5994, 1650,   750, 255, 20,  40,  5, VIDEO_HDTV,                GROUP_NONE,     MODE_PT },                                                                              \
    { "1280x720",   1280,  720, 6000, 1650,   750, 220, 20,  40,  5, VIDEO_PC,                  GROUP_NONE,     MODE_PT },                                                                              \
    /* VESA XGA and SXGA modes */ \
    { "1024x768",   1024,  768, 6000, 1344,   806, 160, 29, 136,  6, VIDEO_PC,                  GROUP_NONE,     MODE_PT },                                                                              \
    { "1280x1024",  1280, 1024, 6000, 1688,  1066, 248, 38, 112,  3, VIDEO_PC,                  GROUP_NONE,     MODE_PT },                                                                              \
    /* PS2 GSM 960i mode */ \
    { "640x960i",    640,  480, 5994,  800,  1050,  48, 33,  96,  2, (VIDEO_EDTV | VIDEO_PC),   GROUP_1080I,    (MODE_PT | MODE_L2 | MODE_INTERLACED) },                                                \
    /* 1080i/p HDTV modes */ \
    { "1080i",      1920,  540, 5994, 2200,  1125, 188, 16,  44,  5, (VIDEO_HDTV | VIDEO_PC),   GROUP_1080I,    (MODE_PT | MODE_L2 | MODE_INTERLACED) },                                                \
    { "1080p",      1920, 1080, 5994, 2200,  1125, 188, 36,  44,  5, VIDEO_HDTV,                GROUP_NONE,     MODE_PT },                                                                              \
    { "1920x1080",  1920, 1080, 6000, 2200,  1125, 148, 36,  44,  5, VIDEO_PC,                  GROUP_NONE,     MODE_PT },                                                                              \
    /* VESA UXGA with reduced h.backporch */ \
    { "1600x1200",  1600, 1200, 6000, 2160,  1250, 255, 46, 192,  3, VIDEO_PC,                  GROUP_NONE,     MODE_PT },                                                                              \
}

#define VIDEO_MODES_SIZE (sizeof((mode_data_t[])VIDEO_MODES_DEF))
#define VIDEO_MODES_CNT (sizeof((mode_data_t[])VIDEO_MODES_DEF)/sizeof(mode_data_t))

alt_8 get_mode_id(alt_u32 totlines, alt_u8 progressive, alt_u32 hz, video_type typemask);

#endif /* VIDEO_MODES_H_ */
