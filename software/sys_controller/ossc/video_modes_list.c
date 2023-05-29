//
// Copyright (C) 2020-2023  Markus Hiienkari <mhiienka@niksula.hut.fi>
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

/* Pure LM modes */
#ifdef VM_STATIC_INCLUDE
static
#endif
const mode_data_t video_modes_plm_default[] = {
    /* 240p modes */
    { "1600x240",      HDMI_Unknown,     {1600,  240,   6000,  2046, 0,  262,  202, 15,  150, 3,  0},  DEF_PHASE,  {{ 0,  0}},  VIDEO_SDTV,               GROUP_240P,   (MODE_L5_GEN_4_3),                                                        },
    { "1280x240",      HDMI_Unknown,     {1280,  240,   6000,  1560, 0,  262,  170, 15,   72, 3,  0},  DEF_PHASE,  {{ 0,  0}},  VIDEO_SDTV,               GROUP_240P,   (MODE_L3_GEN_16_9 | MODE_L4_GEN_4_3),                                     },
    { "960x240",       HDMI_Unknown,     { 960,  240,   6000,  1170, 0,  262,  128, 15,   54, 3,  0},  DEF_PHASE,  {{ 0,  0}},  VIDEO_SDTV,               GROUP_240P,   (MODE_L3_GEN_4_3),                                                        },
    { "512x240",       HDMI_Unknown,     { 512,  240,   6000,   682, 0,  262,   77, 14,   50, 3,  0},  DEF_PHASE,  {{ 0,  0}},  VIDEO_SDTV,               GROUP_240P,   (MODE_L2_512_COL | MODE_L3_512_COL | MODE_L4_512_COL | MODE_L5_512_COL),  },
    { "384x240",       HDMI_Unknown,     { 384,  240,   6000,   512, 0,  262,   59, 14,   37, 3,  0},  DEF_PHASE,  {{ 0,  0}},  VIDEO_SDTV,               GROUP_240P,   (MODE_L2_384_COL | MODE_L3_384_COL | MODE_L4_384_COL | MODE_L5_384_COL),  },
    { "320x240",       HDMI_Unknown,     { 320,  240,   6000,   426, 0,  262,   49, 14,   31, 3,  0},  DEF_PHASE,  {{ 0,  0}},  VIDEO_SDTV,               GROUP_240P,   (MODE_L2_320_COL | MODE_L3_320_COL | MODE_L4_320_COL | MODE_L5_320_COL),  },
    { "256x240",       HDMI_Unknown,     { 256,  240,   6000,   341, 0,  262,   39, 14,   25, 3,  0},  DEF_PHASE,  {{ 0,  0}},  VIDEO_SDTV,               GROUP_240P,   (MODE_L2_256_COL | MODE_L3_256_COL | MODE_L4_256_COL | MODE_L5_256_COL),  },
    { "240p",          HDMI_240p60,      { 720,  240,   6005,   858, 0,  262,   57, 15,   62, 3,  0},  DEF_PHASE,  {{ 0,  0}},  VIDEO_SDTV,               GROUP_240P,   (MODE_PT | MODE_L2),                                                      },
    /* 288p modes */
    { "1600x240L",     HDMI_Unknown,     {1600,  240,   5000,  2046, 0,  312,  202, 43,  150, 3,  0},  DEF_PHASE,  {{ 0,  0}},  VIDEO_SDTV,               GROUP_288P,   (MODE_L5_GEN_4_3),                                                        },
    { "1280x288",      HDMI_Unknown,     {1280,  288,   5000,  1560, 0,  312,  170, 19,   72, 3,  0},  DEF_PHASE,  {{ 0,  0}},  VIDEO_SDTV,               GROUP_288P,   (MODE_L3_GEN_16_9 | MODE_L4_GEN_4_3),                                     },
    { "960x288",       HDMI_Unknown,     { 960,  288,   5000,  1170, 0,  312,  128, 19,   54, 3,  0},  DEF_PHASE,  {{ 0,  0}},  VIDEO_SDTV,               GROUP_288P,   (MODE_L3_GEN_4_3),                                                        },
    { "512x240LB",     HDMI_Unknown,     { 512,  240,   5000,   682, 0,  312,   77, 41,   50, 3,  0},  DEF_PHASE,  {{ 0,  0}},  VIDEO_SDTV,               GROUP_288P,   (MODE_L2_512_COL | MODE_L3_512_COL | MODE_L4_512_COL | MODE_L5_512_COL),  },
    { "384x240LB",     HDMI_Unknown,     { 384,  240,   5000,   512, 0,  312,   59, 41,   37, 3,  0},  DEF_PHASE,  {{ 0,  0}},  VIDEO_SDTV,               GROUP_288P,   (MODE_L2_384_COL | MODE_L3_384_COL | MODE_L4_384_COL | MODE_L5_384_COL),  },
    { "320x240LB",     HDMI_Unknown,     { 320,  240,   5000,   426, 0,  312,   49, 41,   31, 3,  0},  DEF_PHASE,  {{ 0,  0}},  VIDEO_SDTV,               GROUP_288P,   (MODE_L2_320_COL | MODE_L3_320_COL | MODE_L4_320_COL | MODE_L5_320_COL),  },
    { "256x240LB",     HDMI_Unknown,     { 256,  240,   5000,   341, 0,  312,   39, 41,   25, 3,  0},  DEF_PHASE,  {{ 0,  0}},  VIDEO_SDTV,               GROUP_288P,   (MODE_L2_256_COL | MODE_L3_256_COL | MODE_L4_256_COL | MODE_L5_256_COL),  },
    { "288p",          HDMI_288p50,      { 720,  288,   5008,   864, 0,  312,   69, 19,   63, 3,  0},  DEF_PHASE,  {{ 0,  0}},  VIDEO_SDTV,               GROUP_288P,   (MODE_PT | MODE_L2),                                                      },
    /* 360p: GBI */
    { "480x360",       HDMI_Unknown,     { 480,  360,   6000,   600, 0,  375,   63, 10,   38, 3,  0},  DEF_PHASE,  {{ 0,  0}},  VIDEO_EDTV,               GROUP_384P,   (MODE_PT | MODE_L2),                                                      },
    { "240x360",       HDMI_Unknown,     { 256,  360,   6000,   300, 0,  375,   24, 10,   18, 3,  0},  DEF_PHASE,  {{ 0,  0}},  VIDEO_EDTV,               GROUP_384P,   (MODE_L2_240x360 | MODE_L3_240x360),                                      },
    /* 384p: Sega Model 2 */
    { "384p",          HDMI_Unknown,     { 496,  384,   5500,   640, 0,  423,   50, 29,   62, 3,  0},  DEF_PHASE,  {{ 0,  0}},  VIDEO_EDTV,               GROUP_384P,   (MODE_PT | MODE_L2),                                                      },
    /* 400p line3x */
    { "1600x400",      HDMI_Unknown,     {1600,  400,   7000,  2000, 0,  449,  120, 34,  240, 2,  0},  DEF_PHASE,  {{ 0,  0}},  VIDEO_PC,                 GROUP_384P,   (MODE_L3_GEN_16_9),                                                       },
    /* 720x400@70Hz, VGA Mode 3+/7+ */
    { "720x400_70",    HDMI_Unknown,     { 720,  400,   7000,   900, 0,  449,   64, 34,   96, 2,  0},  DEF_PHASE,  {{ 0,  0}},  VIDEO_PC,                 GROUP_384P,   (MODE_PT | MODE_L2),                                                      },
    /* 640x400@70Hz, VGA Mode 13h */
    { "640x400_70",    HDMI_Unknown,     { 640,  400,   7000,   800, 0,  449,   48, 34,   96, 2,  0},  DEF_PHASE,  {{ 0,  0}},  VIDEO_PC,                 GROUP_384P,   (MODE_PT | MODE_L2),                                                      },
    /* 384p: X68k @ 24kHz */
    { "640x384",       HDMI_Unknown,     { 640,  384,   5500,   800, 0,  492,   48, 63,   96, 2,  0},  DEF_PHASE,  {{ 0,  0}},  VIDEO_PC,                 GROUP_384P,   (MODE_PT | MODE_L2),                                                      },
    /* ~525-line modes */
    { "480i",          HDMI_480i60,      { 720,  240,   5994,   858, 0,  525,   57, 15,   62, 3,  1},  DEF_PHASE,  {{ 0,  0}},  VIDEO_SDTV,               GROUP_480I,   (MODE_PT | MODE_L2 | MODE_L3_GEN_16_9 | MODE_L4_GEN_4_3),                 },
    { "480p",          HDMI_480p60,      { 720,  480,   5994,   858, 0,  525,   60, 30,   62, 6,  0},  DEF_PHASE,  {{ 0,  0}},  VIDEO_EDTV,               GROUP_480P,   (MODE_PT | MODE_L2),                                                      },
    { "640x480_60",    HDMI_640x480p60,  { 640,  480,   6000,   800, 0,  525,   48, 33,   96, 2,  0},  DEF_PHASE,  {{ 0,  0}},  VIDEO_PC,                 GROUP_480P,   (MODE_PT | MODE_L2),                                                      },
     /* 480p PSP in-game */ \
    { "480x272",       HDMI_480p60_16x9, { 480,  272,   6000,   858, 0,  525,  177,134,   62, 6,  0},  DEF_PHASE,  {{ 0,  0}},  VIDEO_EDTV,               GROUP_480P,   (MODE_PT | MODE_L2)                                                       },                                                                  \
    /* X68k @ 31kHz */
    { "640x512",       HDMI_Unknown,     { 640,  512,   6000,   800, 0,  568,   48, 34,   96, 6,  0},  DEF_PHASE,  {{ 0,  0}},  VIDEO_PC,                 GROUP_480P,   (MODE_PT | MODE_L2),                                                      },
    /* ~625-line modes */
    { "576i",          HDMI_576i50,      { 720,  288,   5000,   864, 0,  625,   69, 19,   63, 3,  1},  DEF_PHASE,  {{ 0,  0}},  VIDEO_SDTV,               GROUP_576I,   (MODE_PT | MODE_L2 | MODE_L3_GEN_16_9 | MODE_L4_GEN_4_3),                 },
    { "576p",          HDMI_576p50,      { 720,  576,   5000,   864, 0,  625,   68, 39,   64, 5,  0},  DEF_PHASE,  {{ 0,  0}},  VIDEO_EDTV,               GROUP_576P,   (MODE_PT | MODE_L2),                                                      },
    { "800x600_60",    HDMI_Unknown,     { 800,  600,   6000,  1056, 0,  628,   88, 23,  128, 4,  0},  DEF_PHASE,  {{ 0,  0}},  VIDEO_PC,                 GROUP_NONE,   MODE_PT,                                                                  },
    /* CEA 720p modes */
    { "720p_50",       HDMI_720p50,      {1280,  720,   5000,  1980, 0,  750,  220, 20,   40, 5,  0},  DEF_PHASE,  {{ 0,  0}},  (VIDEO_HDTV | VIDEO_PC),  GROUP_720P,   MODE_PT,                                                                  },
    { "720p_60",       HDMI_720p60,      {1280,  720,   6000,  1650, 0,  750,  220, 20,   40, 5,  0},  DEF_PHASE,  {{ 0,  0}},  (VIDEO_HDTV | VIDEO_PC),  GROUP_720P,   MODE_PT,                                                                  },
    /* VESA XGA,1280x960 and SXGA modes */
    { "1024x768",      HDMI_Unknown,     {1024,  768,   6000,  1344, 0,  806,  160, 29,  136, 6,  0},  DEF_PHASE,  {{ 0,  0}},  VIDEO_PC,                 GROUP_NONE,   MODE_PT,                                                                  },
    { "1280x960",      HDMI_Unknown,     {1280,  960,   6000,  1800, 0, 1000,  312, 36,  112, 3,  0},  DEF_PHASE,  {{ 0,  0}},  VIDEO_PC,                 GROUP_NONE,   MODE_PT,                                                                  },
    { "1280x1024",     HDMI_Unknown,     {1280, 1024,   6000,  1688, 0, 1066,  248, 38,  112, 3,  0},  DEF_PHASE,  {{ 0,  0}},  VIDEO_PC,                 GROUP_NONE,   MODE_PT,                                                                  },
    /* PS2 GSM 960i mode */
    { "640x960i",      HDMI_Unknown,     { 640,  480,   6000,   800, 0, 1050,   48, 33,   96, 2,  1},  DEF_PHASE,  {{ 0,  0}},  VIDEO_EDTV,               GROUP_1080I,  (MODE_PT | MODE_L2),                                                      },
    /* CEA 1080i/p modes */
    { "1080i_50",      HDMI_1080i50,     {1920,  540,   5000,  2640, 0, 1125,  148, 15,   44, 5,  1},  DEF_PHASE,  {{ 0,  0}},  (VIDEO_HDTV | VIDEO_PC),  GROUP_1080I,  (MODE_PT | MODE_L2),                                                      },
    { "1080i_60",      HDMI_1080i60,     {1920,  540,   6000,  2200, 0, 1125,  148, 15,   44, 5,  1},  DEF_PHASE,  {{ 0,  0}},  (VIDEO_HDTV | VIDEO_PC),  GROUP_1080I,  (MODE_PT | MODE_L2),                                                      },
    { "1080p_50",      HDMI_1080p50,     {1920, 1080,   5000,  2640, 0, 1125,  148, 36,   44, 5,  0},  DEF_PHASE,  {{ 0,  0}},  (VIDEO_HDTV | VIDEO_PC),  GROUP_1080P,  MODE_PT,                                                                  },
    { "1080p_60",      HDMI_1080p60,     {1920, 1080,   6000,  2200, 0, 1125,  148, 36,   44, 5,  0},  DEF_PHASE,  {{ 0,  0}},  (VIDEO_HDTV | VIDEO_PC),  GROUP_1080P,  MODE_PT,                                                                  },
    /* VESA UXGA mode */
    { "1600x1200",     HDMI_Unknown,     {1600, 1200,   6000,  2160, 0, 1250,  304, 46,  192, 3,  0},  DEF_PHASE,  {{ 0,  0}},  VIDEO_PC,                 GROUP_NONE,   MODE_PT,                                                                  },
};
