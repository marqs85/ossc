//
// Copyright (C) 2015-2019  Markus Hiienkari <mhiienka@niksula.hut.fi>
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
#include "it6613_sys.h"

#define H_TOTAL_MIN 300
#define H_TOTAL_MAX 2800
#define H_TOTAL_ADJ_MAX 19
#define H_SYNCLEN_MIN 10
#define H_SYNCLEN_MAX 255
#define H_BPORCH_MIN 1
#define H_BPORCH_MAX 255
#define H_ACTIVE_MIN 200
#define H_ACTIVE_MAX 1920
#define V_SYNCLEN_MIN 1
#define V_SYNCLEN_MAX 7
#define V_BPORCH_MIN 1
#define V_BPORCH_MAX 236  // 255 - 12 for L5FMT_1920x1080 - 7 for V_SYNCLEN_MAX
#define V_ACTIVE_MIN 160
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
    GROUP_480P      = 4,
    GROUP_1080I     = 5,
} video_group;

typedef enum {
    MODE_INTERLACED     = (1<<0),
    MODE_PLLDIVBY2      = (1<<1),
    //at least one of the flags below must be set for each mode
    MODE_PT             = (1<<2),
    MODE_L2             = (1<<3),
    MODE_L2_512_COL     = (1<<4),
    MODE_L2_384_COL     = (1<<5),
    MODE_L2_320_COL     = (1<<6),
    MODE_L2_256_COL     = (1<<7),
    MODE_L2_240x360     = (1<<8),
    MODE_L2_480x272     = (1<<9),
    MODE_L3_GEN_16_9    = (1<<10),
    MODE_L3_GEN_4_3     = (1<<11),
    MODE_L3_512_COL     = (1<<12),
    MODE_L3_384_COL     = (1<<13),
    MODE_L3_320_COL     = (1<<14),
    MODE_L3_256_COL     = (1<<15),
    MODE_L3_240x360     = (1<<16),
    MODE_L4_GEN_4_3     = (1<<17),
    MODE_L4_512_COL     = (1<<18),
    MODE_L4_384_COL     = (1<<19),
    MODE_L4_320_COL     = (1<<20),
    MODE_L4_256_COL     = (1<<21),
    MODE_L5_GEN_4_3     = (1<<22),
    MODE_L5_512_COL     = (1<<23),
    MODE_L5_384_COL     = (1<<24),
    MODE_L5_320_COL     = (1<<25),
    MODE_L5_256_COL     = (1<<26),
} mode_flags;

typedef struct {
    char name[10];
    HDMI_Video_Type vic:5;
    alt_u16 h_active:11;
    alt_u16 v_active;
    alt_u16 h_total;
    alt_u8  h_total_adj:5;
    alt_u16 v_total:11;
    alt_u8 h_backporch;
    alt_u8 v_backporch;
    alt_u8 h_synclen;
    alt_u8 v_synclen;
    alt_u8 sampler_phase;
    video_type type:5;
    video_group group:3;
    mode_flags flags;
} mode_data_t;


#define VIDEO_MODES_DEF { \
    /* 240p modes */ \
    { "1600x240",  HDMI_Unknown,     1600,  240,  2046, 0,  262,  202, 15,  150, 3,  DEFAULT_SAMPLER_PHASE, VIDEO_SDTV,                 GROUP_240P,     (MODE_L5_GEN_4_3 | MODE_PLLDIVBY2) },                                                           \
    { "1280x240",  HDMI_Unknown,     1280,  240,  1560, 0,  262,  170, 15,   72, 3,  DEFAULT_SAMPLER_PHASE, VIDEO_SDTV,                 GROUP_240P,     (MODE_L3_GEN_16_9 | MODE_L4_GEN_4_3 | MODE_PLLDIVBY2) },                                        \
    { "960x240",   HDMI_Unknown,      960,  240,  1170, 0,  262,  128, 15,   54, 3,  DEFAULT_SAMPLER_PHASE, VIDEO_SDTV,                 GROUP_240P,     (MODE_L3_GEN_4_3 | MODE_PLLDIVBY2) },                                                           \
    { "512x240",   HDMI_Unknown,      512,  240,   682, 0,  262,   77, 14,   50, 3,  DEFAULT_SAMPLER_PHASE, VIDEO_SDTV,                 GROUP_240P,     (MODE_L2_512_COL | MODE_L3_512_COL | MODE_L4_512_COL | MODE_L5_512_COL) },                      \
    { "384x240",   HDMI_Unknown,      384,  240,   512, 0,  262,   59, 14,   37, 3,  DEFAULT_SAMPLER_PHASE, VIDEO_SDTV,                 GROUP_240P,     (MODE_L2_384_COL | MODE_L3_384_COL | MODE_L4_384_COL | MODE_L5_384_COL) },                      \
    { "320x240",   HDMI_Unknown,      320,  240,   426, 0,  262,   49, 14,   31, 3,  DEFAULT_SAMPLER_PHASE, VIDEO_SDTV,                 GROUP_240P,     (MODE_L2_320_COL | MODE_L3_320_COL | MODE_L4_320_COL | MODE_L5_320_COL) },                      \
    { "256x240",   HDMI_Unknown,      256,  240,   341, 0,  262,   39, 14,   25, 3,  DEFAULT_SAMPLER_PHASE, VIDEO_SDTV,                 GROUP_240P,     (MODE_L2_256_COL | MODE_L3_256_COL | MODE_L4_256_COL | MODE_L5_256_COL) },                      \
    { "240p",      HDMI_240p60,       720,  240,   858, 0,  262,   57, 15,   62, 3,  DEFAULT_SAMPLER_PHASE, VIDEO_SDTV,                 GROUP_240P,     (MODE_PT | MODE_L2 | MODE_PLLDIVBY2) },                                                         \
    /* 288p modes */ \
    { "1600x240L", HDMI_Unknown,     1600,  240,  2046, 0,  312,  202, 41,  150, 3,  DEFAULT_SAMPLER_PHASE, VIDEO_SDTV,                 GROUP_240P,     (MODE_L5_GEN_4_3 | MODE_PLLDIVBY2) },                                                           \
    { "1280x288",  HDMI_Unknown,     1280,  288,  1560, 0,  312,  170, 15,   72, 3,  DEFAULT_SAMPLER_PHASE, VIDEO_SDTV,                 GROUP_240P,     (MODE_L3_GEN_16_9 | MODE_L4_GEN_4_3 | MODE_PLLDIVBY2) },                                        \
    { "960x288",   HDMI_Unknown,      960,  288,  1170, 0,  312,  128, 15,   54, 3,  DEFAULT_SAMPLER_PHASE, VIDEO_SDTV,                 GROUP_240P,     (MODE_L3_GEN_4_3 | MODE_PLLDIVBY2) },                                                           \
    { "512x240LB", HDMI_Unknown,      512,  240,   682, 0,  312,   77, 41,   50, 3,  DEFAULT_SAMPLER_PHASE, VIDEO_SDTV,                 GROUP_240P,     (MODE_L2_512_COL | MODE_L3_512_COL | MODE_L4_512_COL | MODE_L5_512_COL) },                      \
    { "384x240LB", HDMI_Unknown,      384,  240,   512, 0,  312,   59, 41,   37, 3,  DEFAULT_SAMPLER_PHASE, VIDEO_SDTV,                 GROUP_240P,     (MODE_L2_384_COL | MODE_L3_384_COL | MODE_L4_384_COL | MODE_L5_384_COL) },                      \
    { "320x240LB", HDMI_Unknown,      320,  240,   426, 0,  312,   49, 41,   31, 3,  DEFAULT_SAMPLER_PHASE, VIDEO_SDTV,                 GROUP_240P,     (MODE_L2_320_COL | MODE_L3_320_COL | MODE_L4_320_COL | MODE_L5_320_COL) },                      \
    { "256x240LB", HDMI_Unknown,      256,  240,   341, 0,  312,   39, 41,   25, 3,  DEFAULT_SAMPLER_PHASE, VIDEO_SDTV,                 GROUP_240P,     (MODE_L2_256_COL | MODE_L3_256_COL | MODE_L4_256_COL | MODE_L5_256_COL) },                      \
    { "288p",      HDMI_288p50,       720,  288,   864, 0,  312,   69, 19,   63, 3,  DEFAULT_SAMPLER_PHASE, VIDEO_SDTV,                 GROUP_240P,     (MODE_PT | MODE_L2 | MODE_PLLDIVBY2) },                                                         \
    /* 360p: GBI */ \
    { "480x360",   HDMI_Unknown,      480,  360,   600, 0,  375,   63, 10,   38, 3,  DEFAULT_SAMPLER_PHASE, VIDEO_EDTV,                 GROUP_384P,     (MODE_PT | MODE_L2 | MODE_PLLDIVBY2) },                                                         \
    { "240x360",   HDMI_Unknown,      256,  360,   300, 0,  375,   24, 10,   18, 3,  DEFAULT_SAMPLER_PHASE, VIDEO_EDTV,                 GROUP_384P,     (MODE_L2_240x360 | MODE_L3_240x360) },                                                          \
    /* 384p: Sega Model 2 (real vtotal=423, avoid collision with PC88/98 and VGA400p) */ \
    { "384p",      HDMI_Unknown,      496,  384,   640, 0,  408,   50, 29,   62, 3,  DEFAULT_SAMPLER_PHASE, VIDEO_EDTV,                 GROUP_384P,     (MODE_PT | MODE_L2 | MODE_PLLDIVBY2) },                                                         \
    /* 400p line3x */ \
    { "1600x400",  HDMI_Unknown,     1600,  400,  2000, 0,  449,  120, 34,  240, 2,  DEFAULT_SAMPLER_PHASE, VIDEO_PC,                   GROUP_384P,     (MODE_L3_GEN_16_9) },                                                                           \
    /* 720x400@70Hz, VGA Mode 3+/7+ */ \
    { "720x400",   HDMI_Unknown,      720,  400,   900, 0,  449,   64, 34,   96, 2,  DEFAULT_SAMPLER_PHASE, VIDEO_PC,                   GROUP_384P,     (MODE_PT | MODE_L2) },                                                                          \
    /* 640x400@70Hz, VGA Mode 13h */ \
    { "640x400",   HDMI_Unknown,      640,  400,   800, 0,  449,   48, 34,   96, 2,  DEFAULT_SAMPLER_PHASE, VIDEO_PC,                   GROUP_384P,     (MODE_PT | MODE_L2) },                                                                          \
    /* 384p: X68k @ 24kHz */ \
    { "640x384",   HDMI_Unknown,      640,  384,   800, 0,  492,   48, 63,   96, 2,  DEFAULT_SAMPLER_PHASE, VIDEO_PC,                   GROUP_384P,     (MODE_PT | MODE_L2 | MODE_PLLDIVBY2) },                                                         \
    /* ~525-line modes */ \
    { "480i",      HDMI_480i60,       720,  240,   858, 0,  525,   57, 15,   62, 3,  DEFAULT_SAMPLER_PHASE, VIDEO_SDTV,                 GROUP_480I,     (MODE_PT | MODE_L2 | MODE_L3_GEN_16_9 | MODE_L4_GEN_4_3 | MODE_PLLDIVBY2 | MODE_INTERLACED) },  \
    { "480p",      HDMI_480p60,       720,  480,   858, 0,  525,   60, 30,   62, 6,  DEFAULT_SAMPLER_PHASE, VIDEO_EDTV,                 GROUP_480P,     (MODE_PT | MODE_L2) },                                                                          \
    /* 480p PSP in-game */ \
    { "480x272",   HDMI_480p60_16x9,  480,  272,   858, 0,  525,  177,134,   62, 6,  DEFAULT_SAMPLER_PHASE, VIDEO_EDTV,                 GROUP_480P,     (MODE_PT | MODE_L2_480x272) },                                                                  \
    { "640x480",   HDMI_640x480p60,   640,  480,   800, 0,  525,   48, 33,   96, 2,  DEFAULT_SAMPLER_PHASE, VIDEO_PC,                   GROUP_480P,     (MODE_PT | MODE_L2) },                                                                          \
    /* X68k @ 31kHz */ \
    { "640x512",   HDMI_Unknown,      640,  512,   800, 0,  568,   48, 28,   96, 2,  DEFAULT_SAMPLER_PHASE, VIDEO_PC,                   GROUP_480P,     (MODE_PT | MODE_L2) },                                                                          \
    /* ~625-line modes */ \
    { "576i",      HDMI_576i50,       720,  288,   864, 0,  625,   69, 19,   63, 3,  DEFAULT_SAMPLER_PHASE, VIDEO_SDTV,                 GROUP_480I,     (MODE_PT | MODE_L2 | MODE_L3_GEN_16_9 | MODE_L4_GEN_4_3 | MODE_PLLDIVBY2 | MODE_INTERLACED) },  \
    { "576p",      HDMI_576p50,       720,  576,   864, 0,  625,   68, 39,   64, 5,  DEFAULT_SAMPLER_PHASE, VIDEO_EDTV,                 GROUP_480P,     (MODE_PT | MODE_L2) },                                                                          \
    { "800x600",   HDMI_Unknown,      800,  600,  1056, 0,  628,   88, 23,  128, 4,  DEFAULT_SAMPLER_PHASE, VIDEO_PC,                   GROUP_NONE,     MODE_PT },                                                                                      \
    /* 720p modes */ \
    { "720p_50",   HDMI_720p50,      1280,  720,  1980, 0,  750,  220, 20,   40, 5,  DEFAULT_SAMPLER_PHASE, (VIDEO_HDTV | VIDEO_PC),    GROUP_NONE,     MODE_PT },                                                                                      \
    { "720p_60",   HDMI_720p60,      1280,  720,  1650, 0,  750,  220, 20,   40, 5,  DEFAULT_SAMPLER_PHASE, (VIDEO_HDTV | VIDEO_PC),    GROUP_NONE,     MODE_PT },                                                                                      \
    /* VESA XGA and SXGA modes */ \
    { "1024x768",  HDMI_Unknown,     1024,  768,  1344, 0,  806,  160, 29,  136, 6,  DEFAULT_SAMPLER_PHASE, VIDEO_PC,                   GROUP_NONE,     MODE_PT },                                                                                      \
    { "1280x1024", HDMI_Unknown,     1280, 1024,  1688, 0, 1066,  248, 38,  112, 3,  DEFAULT_SAMPLER_PHASE, VIDEO_PC,                   GROUP_NONE,     MODE_PT },                                                                                      \
    /* PS2 GSM 960i mode */ \
    { "640x960i",  HDMI_Unknown,      640,  480,   800, 0, 1050,   48, 33,   96, 2,  DEFAULT_SAMPLER_PHASE, VIDEO_EDTV,                 GROUP_1080I,    (MODE_PT | MODE_L2 | MODE_INTERLACED) },                                                        \
    /* 1080i/p modes */ \
    { "1080i_50",  HDMI_1080i50,     1920,  540,  2640, 0, 1125,  148, 15,   44, 5,  DEFAULT_SAMPLER_PHASE, (VIDEO_HDTV | VIDEO_PC),    GROUP_1080I,    (MODE_PT | MODE_L2 | MODE_INTERLACED) },                                                        \
    { "1080i_60",  HDMI_1080i60,     1920,  540,  2200, 0, 1125,  148, 16,   44, 5,  DEFAULT_SAMPLER_PHASE, (VIDEO_HDTV | VIDEO_PC),    GROUP_1080I,    (MODE_PT | MODE_L2 | MODE_INTERLACED) },                                                        \
    { "1080p_50",  HDMI_1080p50,     1920, 1080,  2640, 0, 1125,  148, 36,   44, 5,  DEFAULT_SAMPLER_PHASE, (VIDEO_HDTV | VIDEO_PC),    GROUP_NONE,     MODE_PT },                                                                                      \
    { "1080p_60",  HDMI_1080p60,     1920, 1080,  2200, 0, 1125,  148, 36,   44, 5,  DEFAULT_SAMPLER_PHASE, (VIDEO_HDTV | VIDEO_PC),    GROUP_NONE,     MODE_PT },                                                                                      \
    /* VESA UXGA with 49 H.backporch cycles exchanged for H.synclen */ \
    { "1600x1200", HDMI_Unknown,     1600, 1200,  2160, 0, 1250,  255, 46,  241, 3,  DEFAULT_SAMPLER_PHASE, VIDEO_PC,                   GROUP_NONE,     MODE_PT },                                                                                      \
}

#define VIDEO_MODES_SIZE (sizeof((mode_data_t[])VIDEO_MODES_DEF))
#define VIDEO_MODES_CNT (sizeof((mode_data_t[])VIDEO_MODES_DEF)/sizeof(mode_data_t))

alt_8 get_mode_id(alt_u32 totlines, alt_u8 progressive, alt_u32 hz, alt_u8 h_syncinlen);

#endif /* VIDEO_MODES_H_ */
