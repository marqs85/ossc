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

#include "alt_types.h"

#ifndef MENU_H_
#define MENU_H_

typedef enum {
    OPT_AVCONFIG_SELECTION,
    OPT_AVCONFIG_NUMVALUE,
    OPT_SUBMENU,
    OPT_FUNC_CALL,
} menuitem_type;

typedef int (*func_call)(void);
typedef void (*disp_func)(alt_u8);


typedef struct {
    alt_u8 *data;
    alt_u8 num_settings;
    const char **setting_str;
} opt_avconfig_selection;

typedef struct {
    alt_u8 *data;
    alt_u8 min;
    alt_u8 max;
    disp_func f;
} opt_avconfig_numvalue;

typedef struct {
    func_call f;
    char *text_success;
} opt_func_call;

typedef struct menustruct menu_t;

typedef struct {
    char *name;
    menuitem_type type;
    union {
        opt_avconfig_selection sel;
        opt_avconfig_numvalue num;
        const menu_t *sub;
        opt_func_call fun;
    };
} menuitem_t;

struct menustruct {
    alt_u8 num_items;
    menuitem_t *items;
};

typedef struct {
    menu_t *menu;
} opt_submenu;

#define SETTING_ITEM(x) sizeof(x)/sizeof(char*), x
#define MENU(X, Y) menuitem_t X##_items[] = Y; const menu_t X = { sizeof(X##_items)/sizeof(menuitem_t), X##_items };
#define P99_PROTECT(...) __VA_ARGS__

#define MAX_MENU_LEVELS 4

typedef enum {
    NO_ACTION           = 0,
    NEXT_PAGE           = 1,
    PREV_PAGE           = 2,
    VAL_PLUS            = 3,
    VAL_MINUS           = 4,
    OPT_SELECT          = 5,
    PREV_MENU           = 6,
} menucode_id;

typedef struct {
    const menu_t *m;
    alt_u8 mp;
} menunavi;

//TODO: move all below to separate header(s)

#define SCANLINESTR_MAX 15
#define VIDEO_LPF_MAX 5

typedef struct {
    alt_u8 sl_mode;
    alt_u8 sl_str;
    alt_u8 sl_id;
    alt_u8 linemult_target;
    alt_u8 l3_mode;
    alt_u8 h_mask;
    alt_u8 v_mask;
    alt_u8 tx_mode;
    alt_u8 s480p_mode;
    alt_u8 sampler_phase;
    alt_u8 ypbpr_cs;
    alt_u8 sync_vth;
    alt_u8 vsync_thold;
    alt_u8 sync_lpf;
    alt_u8 video_lpf;
    alt_u8 en_alc;
    alt_u8 pre_coast;
    alt_u8 post_coast;
} avconfig_t;

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
    RC_SL_TGL,
    RC_SL_PLUS,
    RC_SL_MINUS,
} rc_code_t;

int write_userdata();
int fw_update();
int set_default_avconfig();


#endif
