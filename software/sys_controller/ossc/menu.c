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

#include <string.h>
#include "menu.h"
#include "av_controller.h"
#include "firmware.h"
#include "userdata.h"
#include "controls.h"
#include "lcd.h"
#include "tvp7002.h"

#define OPT_NOWRAP  0
#define OPT_WRAP    1

#ifdef OSDLANG_JP
#define LNG(e, j) j
#else
#define LNG(e, j) e
#endif

extern char row1[LCD_ROW_LEN+1], row2[LCD_ROW_LEN+1], menu_row1[LCD_ROW_LEN+1], menu_row2[LCD_ROW_LEN+1];
extern avconfig_t tc;
extern alt_u16 tc_h_samplerate, tc_h_synclen, tc_h_active, tc_v_active, tc_h_bporch, tc_v_bporch;
extern alt_u32 remote_code;
extern alt_u16 rc_keymap[REMOTE_MAX_KEYS];

alt_u8 menu_active;

static const char *off_on_desc[] = { LNG("Off","ｵﾌ"), LNG("On","ｵﾝ") };
static const char *video_lpf_desc[] = { LNG("Auto","ｵｰﾄ"), LNG("Off","ｵﾌ"), "95MHz (HDTV II)", "35MHz (HDTV I)", "16MHz (EDTV)", "9MHz (SDTV)" };
static const char *ypbpr_cs_desc[] = { "Rec. 601", "Rec. 709" };
static const char *s480p_mode_desc[] = { LNG("Auto","ｵｰﾄ"), "DTV 480p", "VESA 640x480@60" };
static const char *sync_lpf_desc[] = { LNG("Off","ｵﾌ"), LNG("33MHz (min)","33MHz (ｻｲｼｮｳ)"), LNG("10MHz (med)","10MHz (ﾁｭｳｲ)"), LNG("2.5MHz (max)","2.5MHz (ｻｲﾀﾞｲ)") };
static const char *l3_mode_desc[] = { LNG("Generic 16:9","ｼﾞｪﾈﾘｯｸ 16:9"), LNG("Generic 4:3","ｼﾞｪﾈﾘｯｸ 4:3"), LNG("320x240 optim.","320x240 ｻｲﾃｷ."), LNG("256x240 optim.","256x240 ｻｲﾃｷ.") };
static const char *l4l5_mode_desc[] = { LNG("Generic 4:3","ﾊﾝﾖｳ 4:3"), LNG("320x240 optim.","320x240 ｻｲﾃｷ."), LNG("256x240 optim.","256x240 ｻｲﾃｷ.") };
static const char *l5_fmt_desc[] = { "1920x1080", "1600x1200", "1920x1200" };
static const char *pm_240p_desc[] = { LNG("Passthru","ﾊﾟｽｽﾙｰ"), "Line2x", "Line3x", "Line4x", "Line5x" };
static const char *pm_480i_desc[] = { LNG("Passthru","ﾊﾟｽｽﾙｰ"), "Line2x (bob)" };
static const char *pm_384p_480p_desc[] = { LNG("Passthru","ﾊﾟｽｽﾙｰ"), "Line2x" };
static const char *ar_256col_desc[] = { "4:3", "8:7" };
static const char *tx_mode_desc[] = { "HDMI", "DVI" };
static const char *sl_mode_desc[] = { LNG("Off","ｵﾌ"), LNG("Auto","ｼﾞﾄﾞｳ"), LNG("Manual","ｼｭﾄﾞｳ") };
static const char *sl_type_desc[] = { LNG("Horizontal","ﾖｺ"), LNG("Vertical","ﾀﾃ"), LNG("Alternating","ｺｳｺﾞ") };
static const char *sl_id_desc[] = { LNG("Top","ｳｴ"), LNG("Bottom","ｼﾀ") };
static const char *audio_dw_sampl_desc[] = { LNG("Off (fs = 96kHz)","ｵﾌ (fs = 96kHz)"), "2x  (fs = 48kHz)" };

static void sampler_phase_disp(alt_u8 v) { sniprintf(menu_row2, LCD_ROW_LEN+1, LNG("%d deg","%d ﾄﾞ"), (v*1125)/100); }
static void sync_vth_disp(alt_u8 v) { sniprintf(menu_row2, LCD_ROW_LEN+1, "%d mV", (v*1127)/100); }
static void intclks_to_time_disp(alt_u8 v) { sniprintf(menu_row2, LCD_ROW_LEN+1, "%u.%.2u us", (unsigned)(((1000000U*v)/(TVP_INTCLK_HZ/1000))/1000), (unsigned)((((1000000U*v)/(TVP_INTCLK_HZ/1000))%1000)/10)); }
static void extclks_to_time_disp(alt_u8 v) { sniprintf(menu_row2, LCD_ROW_LEN+1, "%u.%.2u us", (unsigned)(((1000000U*v)/(TVP_EXTCLK_HZ/1000))/1000), (unsigned)((((1000000U*v)/(TVP_EXTCLK_HZ/1000))%1000)/10)); }
static void sl_str_disp(alt_u8 v) { sniprintf(menu_row2, LCD_ROW_LEN+1, "%u%%", ((v+1)*625)/100); }
static void lines_disp(alt_u8 v) { sniprintf(menu_row2, LCD_ROW_LEN+1, LNG("%u lines","%u ﾗｲﾝ"), v); }
static void pixels_disp(alt_u8 v) { sniprintf(menu_row2, LCD_ROW_LEN+1, LNG("%u pixels","%u ﾄﾞｯﾄ"), v); }
static void value_disp(alt_u8 v) { sniprintf(menu_row2, LCD_ROW_LEN+1, "    %u", v); }

MENU(menu_advtiming, P99_PROTECT({ \
    { LNG("H. samplerate","ｽｲﾍｲｻﾝﾌﾟﾙﾚﾄ"),        OPT_AVCONFIG_NUMVAL_U16,{ .num_u16 = { &tc_h_samplerate, H_TOTAL_MIN,   H_TOTAL_MAX, vm_tweak } } },
    { LNG("H. synclen","ｽｲﾍｲﾗｲﾝﾄﾞｳｷｼﾝｺﾞ"),       OPT_AVCONFIG_NUMVAL_U16,{ .num_u16 = { &tc_h_synclen,    H_SYNCLEN_MIN, H_SYNCLEN_MAX, vm_tweak } } },
    { LNG("H. active","ｽｲﾍｲｱｸﾃｨﾌﾞ"),             OPT_AVCONFIG_NUMVAL_U16,{ .num_u16 = { &tc_h_active,     H_ACTIVE_MIN,  H_ACTIVE_MAX, vm_tweak } } },
    { LNG("V. active","ｽｲﾁｮｸｱｸﾃｨﾌﾞ"),            OPT_AVCONFIG_NUMVAL_U16,{ .num_u16 = { &tc_v_active,     V_ACTIVE_MIN,  V_ACTIVE_MAX, vm_tweak } } },
    { LNG("H. backporch","ｽｲﾍｲﾊﾞｯｸﾎﾟｰﾁ"),        OPT_AVCONFIG_NUMVAL_U16,{ .num_u16 = { &tc_h_bporch,     H_BPORCH_MIN,  H_BPORCH_MAX, vm_tweak } } },
    { LNG("V. backporch","ｽｲﾁｮｸﾊﾞｯｸﾎﾟｰﾁ"),       OPT_AVCONFIG_NUMVAL_U16,{ .num_u16 = { &tc_v_bporch,     V_BPORCH_MIN,  V_BPORCH_MAX, vm_tweak } } },
}))


MENU(menu_vinputproc, P99_PROTECT({ \
    { LNG("Video LPF","ﾋﾞﾃﾞｵ LPF"),             OPT_AVCONFIG_SELECTION, { .sel = { &tc.video_lpf,     OPT_WRAP,   SETTING_ITEM(video_lpf_desc) } } },
    { LNG("YPbPr in ColSpa","ｲﾛｸｳｶﾝﾆYPbPr"),    OPT_AVCONFIG_SELECTION, { .sel = { &tc.ypbpr_cs,      OPT_WRAP,   SETTING_ITEM(ypbpr_cs_desc) } } },
    { LNG("R/Pr offset","R/Pr ｵﾌｾｯﾄ"),          OPT_AVCONFIG_NUMVALUE,  { .num = { &tc.col.r_f_off,   OPT_NOWRAP, 0, 0xFF, value_disp } } },
    { LNG("G/Y offset","G/Y ｵﾌｾｯﾄ"),            OPT_AVCONFIG_NUMVALUE,  { .num = { &tc.col.g_f_off,   OPT_NOWRAP, 0, 0xFF, value_disp } } },
    { LNG("B/Pb offset","B/Pb ｵﾌｾｯﾄ"),          OPT_AVCONFIG_NUMVALUE,  { .num = { &tc.col.b_f_off,   OPT_NOWRAP, 0, 0xFF, value_disp } } },
    { LNG("R/Pr gain","R/Pr ｹﾞｲﾝ"),             OPT_AVCONFIG_NUMVALUE,  { .num = { &tc.col.r_f_gain,  OPT_NOWRAP, 0, 0xFF, value_disp } } },
    { LNG("G/Y gain","G/Y ｹﾞｲﾝ"),               OPT_AVCONFIG_NUMVALUE,  { .num = { &tc.col.g_f_gain,  OPT_NOWRAP, 0, 0xFF, value_disp } } },
    { LNG("B/Pb gain","B/Pb ｹﾞｲﾝ"),             OPT_AVCONFIG_NUMVALUE,  { .num = { &tc.col.b_f_gain,  OPT_NOWRAP, 0, 0xFF, value_disp } } },
}))

MENU(menu_sampling, P99_PROTECT({ \
    { LNG("Sampling phase","ｻﾝﾌﾟﾘﾝｸﾞﾌｪｰｽﾞ"),     OPT_AVCONFIG_NUMVALUE,  { .num = { &tc.sampler_phase, OPT_WRAP, 0, SAMPLER_PHASE_MAX, sampler_phase_disp } } },
    { LNG("480p in sampler","ｻﾝﾌﾟﾗｰﾃﾞ480p"),     OPT_AVCONFIG_SELECTION, { .sel = { &tc.s480p_mode,    OPT_WRAP, SETTING_ITEM(s480p_mode_desc) } } },
    { LNG("Allow TVP HPLL2x","TVP HPLL2xｷｮﾖｳ"), OPT_AVCONFIG_SELECTION, { .sel = { &tc.tvp_hpll2x,   OPT_WRAP, SETTING_ITEM(off_on_desc) } } },
    { LNG("<Adv. timing   >","<ｶｸｼｭﾀｲﾐﾝｸﾞ>"),    OPT_SUBMENU,            { .sub = { &menu_advtiming, vm_display } } },
}))

MENU(menu_sync, P99_PROTECT({ \
    { LNG("Analog sync LPF","ｱﾅﾛｸﾞﾄﾞｳｷ LPF"),     OPT_AVCONFIG_SELECTION, { .sel = { &tc.sync_lpf,    OPT_WRAP,   SETTING_ITEM(sync_lpf_desc) } } },
    { LNG("Analog sync Vth","ｱﾅﾛｸﾞﾄﾞｳｷ Vth"),     OPT_AVCONFIG_NUMVALUE,  { .num = { &tc.sync_vth,    OPT_NOWRAP, 0, SYNC_VTH_MAX, sync_vth_disp } } },
    { LNG("Hsync tolerance","ｽｲﾍｲﾄﾞｳｷｺｳｻ"),      OPT_AVCONFIG_NUMVALUE,  { .num = { &tc.linelen_tol, OPT_NOWRAP, 0, 0xFF, intclks_to_time_disp } } },
    { LNG("Vsync threshold","ｽｲﾁｮｸﾄﾞｳｷｹﾞﾝｶｲﾃﾝ"),  OPT_AVCONFIG_NUMVALUE,  { .num = { &tc.vsync_thold, OPT_NOWRAP, VSYNC_THOLD_MIN, VSYNC_THOLD_MAX, intclks_to_time_disp } } },
    { "H-PLL Pre-Coast",                        OPT_AVCONFIG_NUMVALUE,  { .num = { &tc.pre_coast,   OPT_NOWRAP, 0, PLL_COAST_MAX, lines_disp } } },
    { "H-PLL Post-Coast",                       OPT_AVCONFIG_NUMVALUE,  { .num = { &tc.post_coast,  OPT_NOWRAP, 0, PLL_COAST_MAX, lines_disp } } },
}))

MENU(menu_output, P99_PROTECT({ \
    { LNG("240p/288p proc","240p/288pｼｮﾘ"),     OPT_AVCONFIG_SELECTION, { .sel = { &tc.pm_240p,         OPT_WRAP, SETTING_ITEM(pm_240p_desc) } } },
    { LNG("384p proc","384pｼｮﾘ"),               OPT_AVCONFIG_SELECTION, { .sel = { &tc.pm_384p,         OPT_WRAP, SETTING_ITEM(pm_384p_480p_desc) } } },
    { LNG("480i/576i proc","480i/576iｼｮﾘ"),     OPT_AVCONFIG_SELECTION, { .sel = { &tc.pm_480i,         OPT_WRAP, SETTING_ITEM(pm_480i_desc) } } },
    { LNG("480p/576p proc","480p/576pｼｮﾘ"),     OPT_AVCONFIG_SELECTION, { .sel = { &tc.pm_480p,         OPT_WRAP, SETTING_ITEM(pm_384p_480p_desc) } } },
    { LNG("960i/1080i proc","960i/1080iｼｮﾘ"),   OPT_AVCONFIG_SELECTION, { .sel = { &tc.pm_1080i,        OPT_WRAP, SETTING_ITEM(pm_480i_desc) } } },
    { LNG("Line3x mode","Line3xﾓｰﾄﾞ"),          OPT_AVCONFIG_SELECTION, { .sel = { &tc.l3_mode,         OPT_WRAP, SETTING_ITEM(l3_mode_desc) } } },
    { LNG("Line4x mode","Line4xﾓｰﾄﾞ"),          OPT_AVCONFIG_SELECTION, { .sel = { &tc.l4_mode,         OPT_WRAP, SETTING_ITEM(l4l5_mode_desc) } } },
    { LNG("Line5x mode","Line5xﾓｰﾄﾞ"),          OPT_AVCONFIG_SELECTION, { .sel = { &tc.l5_mode,         OPT_WRAP, SETTING_ITEM(l4l5_mode_desc) } } },
    { LNG("Line5x format","Line5xｹｲｼｷ"),        OPT_AVCONFIG_SELECTION, { .sel = { &tc.l5_fmt,          OPT_WRAP, SETTING_ITEM(l5_fmt_desc) } } },
    { LNG("256x240 aspect","256x240ｱｽﾍﾟｸﾄ"),    OPT_AVCONFIG_SELECTION, { .sel = { &tc.ar_256col,       OPT_WRAP, SETTING_ITEM(ar_256col_desc) } } },
    { LNG("TX mode","TXﾓｰﾄﾞ"),                  OPT_AVCONFIG_SELECTION, { .sel = { &tc.tx_mode,         OPT_WRAP, SETTING_ITEM(tx_mode_desc) } } },
    { LNG("Initial input","ｼｮｷﾆｭｳﾘｮｸ"),         OPT_AVCONFIG_SELECTION, { .sel = { &tc.def_input,       OPT_WRAP, SETTING_ITEM(avinput_str) } } },
}))

MENU(menu_postproc, P99_PROTECT({ \
    { LNG("Scanlines","ｽｷｬﾝﾗｲﾝ"),                  OPT_AVCONFIG_SELECTION, { .sel = { &tc.sl_mode,     OPT_WRAP,   SETTING_ITEM(sl_mode_desc) } } },
    { LNG("Scanline str.","ｽｷｬﾝﾗｲﾝﾂﾖｻ"),           OPT_AVCONFIG_NUMVALUE,  { .num = { &tc.sl_str,      OPT_NOWRAP, 0, SCANLINESTR_MAX, sl_str_disp } } },
    { LNG("Scanline type","ｽｷｬﾝﾗｲﾝﾙｲ"),            OPT_AVCONFIG_SELECTION, { .sel = { &tc.sl_type,     OPT_WRAP,   SETTING_ITEM(sl_type_desc) } } },
    { LNG("Scanline alignm.","ｽｷｬﾝﾗｲﾝﾎﾟｼﾞｼｮﾝ"),    OPT_AVCONFIG_SELECTION, { .sel = { &tc.sl_id,       OPT_WRAP,   SETTING_ITEM(sl_id_desc) } } },
    { LNG("Horizontal mask","ｽｲﾍｲﾏｽｸ"),          OPT_AVCONFIG_NUMVALUE,  { .num = { &tc.h_mask,      OPT_NOWRAP, 0, HV_MASK_MAX, pixels_disp } } },
    { LNG("Vertical mask","ｽｲﾁｮｸﾏｽｸ"),           OPT_AVCONFIG_NUMVALUE,  { .num = { &tc.v_mask,      OPT_NOWRAP, 0, HV_MASK_MAX, pixels_disp } } },
    { LNG("Mask brightness","ﾏｽｸｱｶﾙｻ"),          OPT_AVCONFIG_NUMVALUE,  { .num = { &tc.mask_br,     OPT_NOWRAP, 0, HV_MASK_MAX_BR, value_disp } } },
}))

#ifdef DIY_AUDIO
MENU(menu_audio, P99_PROTECT({ \
    { LNG("Down-sampling","ﾀﾞｳﾝｻﾝﾌﾟﾘﾝｸﾞ"),       OPT_AVCONFIG_SELECTION, { .sel = { &tc.audio_dw_sampl, OPT_WRAP, SETTING_ITEM(audio_dw_sampl_desc) } } },
    { LNG("Swap left/right","ﾋﾀﾞﾘ/ﾐｷﾞｽﾜｯﾌﾟ"),    OPT_AVCONFIG_SELECTION, { .sel = { &tc.audio_swap_lr,  OPT_WRAP, SETTING_ITEM(off_on_desc) } } },
}))
#define AUDIO_MENU { "Audio options  >",                       OPT_SUBMENU,            { .sub = { &menu_audio, NULL } } },
#else
#define AUDIO_MENU
#endif


MENU(menu_main, P99_PROTECT({ \
    { LNG("Video in proc  >","ﾀｲｵｳｴｲｿﾞｳ     >"),  OPT_SUBMENU,            { .sub = { &menu_vinputproc, NULL } } },
    { LNG("Sampling opt.  >","ｻﾝﾌﾟﾘﾝｸﾞｵﾌﾟｼｮﾝ>"),  OPT_SUBMENU,            { .sub = { &menu_sampling, NULL } } },
    { LNG("Sync opt.      >","ﾄﾞｳｷｵﾌﾟｼｮﾝ    >"),  OPT_SUBMENU,            { .sub = { &menu_sync, NULL } } },
    { LNG("Output opt.    >","ｼｭﾂﾘｮｸｵﾌﾟｼｮﾝ  >"),  OPT_SUBMENU,            { .sub = { &menu_output, NULL } } },
    { LNG("Post-proc.     >","ｱﾄｼｮﾘ         >"),  OPT_SUBMENU,            { .sub = { &menu_postproc, NULL } } },
    AUDIO_MENU
    { LNG("<Load profile >","<ﾌﾟﾛﾌｧｲﾙﾛｰﾄﾞ    >"), OPT_SUBMENU,            { .sub = { NULL, load_profile_disp } } },
    { LNG("<Save profile >","<ﾌﾟﾛﾌｧｲﾙｾｰﾌﾞ    >"), OPT_SUBMENU,            { .sub = { NULL, save_profile_disp } } },
    { LNG("<Reset settings>","<ｾｯﾃｲｵｼｮｷｶ    >"),  OPT_FUNC_CALL,          { .fun = { set_default_avconfig, LNG("Reset done","ｼｮｷｶｽﾐ"), "" } } },
    { LNG("<Fw. update    >","<ﾌｧｰﾑｳｪｱｱｯﾌﾟ  >"),  OPT_FUNC_CALL,          { .fun = { fw_update, LNG("OK - pls restart","OK - ｻｲｷﾄﾞｳｼﾃｸﾀﾞｻｲ"), LNG("failed","ｼｯﾊﾟｲ") } } },
}))

// Max 3 levels currently
menunavi navi[] = {{&menu_main, 0}, {NULL, 0}, {NULL, 0}};
alt_u8 navlvl = 0;


void display_menu(alt_u8 forcedisp)
{
    menucode_id code = NO_ACTION;
    menuitem_type type;
    alt_u8 *val, val_wrap, val_min, val_max;
    alt_u16 *val_u16;
    int i, retval = 0;

    for (i=RC_OK; i < RC_INFO; i++) {
        if (remote_code == rc_keymap[i]) {
            code = i;
            break;
        }
    }

    if (!forcedisp && !remote_code)
        return;

    type = navi[navlvl].m->items[navi[navlvl].mp].type;

    // Parse menu control
    switch (code) {
    case PREV_PAGE:
        navi[navlvl].mp = (navi[navlvl].mp == 0) ? navi[navlvl].m->num_items-1 : (navi[navlvl].mp-1);
        break;
    case NEXT_PAGE:
        navi[navlvl].mp = (navi[navlvl].mp+1) % navi[navlvl].m->num_items;
        break;
    case PREV_MENU:
        if (navlvl > 0) {
            navlvl--;
        } else {
            menu_active = 0;
            lcd_write_status();
            return;
        }
        break;
    case OPT_SELECT:
        switch (navi[navlvl].m->items[navi[navlvl].mp].type) {
            case OPT_SUBMENU:
                if (navi[navlvl].m->items[navi[navlvl].mp].sub.arg_f)
                    navi[navlvl].m->items[navi[navlvl].mp].sub.arg_f(code);
                if (navi[navlvl].m->items[navi[navlvl].mp].sub.menu) {
                    if (navi[navlvl+1].m != navi[navlvl].m->items[navi[navlvl].mp].sub.menu)
                        navi[navlvl+1].mp = 0;
                    navi[navlvl+1].m = navi[navlvl].m->items[navi[navlvl].mp].sub.menu;
                    navlvl++;
                }
                break;
            case OPT_FUNC_CALL:
                retval = navi[navlvl].m->items[navi[navlvl].mp].fun.f();
                break;
            default:
                break;
        }
        break;
    case VAL_MINUS:
    case VAL_PLUS:
        switch (navi[navlvl].m->items[navi[navlvl].mp].type) {
            case OPT_AVCONFIG_SELECTION:
            case OPT_AVCONFIG_NUMVALUE:
                val = navi[navlvl].m->items[navi[navlvl].mp].sel.data;
                val_wrap = navi[navlvl].m->items[navi[navlvl].mp].sel.wrap_cfg;
                val_min = navi[navlvl].m->items[navi[navlvl].mp].sel.min;
                val_max = navi[navlvl].m->items[navi[navlvl].mp].sel.max;

                if (code == VAL_MINUS)
                    *val = (*val > val_min) ? (*val-1) : (val_wrap ? val_max : val_min);
                else
                    *val = (*val < val_max) ? (*val+1) : (val_wrap ? val_min : val_max);
                break;
            case OPT_AVCONFIG_NUMVAL_U16:
                val_u16 = navi[navlvl].m->items[navi[navlvl].mp].num_u16.data;
                if (code == VAL_MINUS)
                    *val_u16 = (*val_u16 > navi[navlvl].m->items[navi[navlvl].mp].num_u16.min) ? (*val_u16-1) : *val_u16;
                else
                    *val_u16 = (*val_u16 < navi[navlvl].m->items[navi[navlvl].mp].num_u16.max) ? (*val_u16+1) : *val_u16;
                break;
            case OPT_SUBMENU:
                if (navi[navlvl].m->items[navi[navlvl].mp].sub.arg_f)
                    navi[navlvl].m->items[navi[navlvl].mp].sub.arg_f(code);
                break;
            default:
                break;
        }
        break;
    default:
        break;
    }

    // Generate menu text
    type = navi[navlvl].m->items[navi[navlvl].mp].type;
    strncpy(menu_row1, navi[navlvl].m->items[navi[navlvl].mp].name, LCD_ROW_LEN+1);
    switch (navi[navlvl].m->items[navi[navlvl].mp].type) {
        case OPT_AVCONFIG_SELECTION:
            strncpy(menu_row2, navi[navlvl].m->items[navi[navlvl].mp].sel.setting_str[*(navi[navlvl].m->items[navi[navlvl].mp].sel.data)], LCD_ROW_LEN+1);
            break;
        case OPT_AVCONFIG_NUMVALUE:
            navi[navlvl].m->items[navi[navlvl].mp].num.df(*(navi[navlvl].m->items[navi[navlvl].mp].num.data));
            break;
        case OPT_AVCONFIG_NUMVAL_U16:
            navi[navlvl].m->items[navi[navlvl].mp].num_u16.df(*(navi[navlvl].m->items[navi[navlvl].mp].num_u16.data));
            break;
        case OPT_SUBMENU:
            if (navi[navlvl].m->items[navi[navlvl].mp].sub.arg_f)
                navi[navlvl].m->items[navi[navlvl].mp].sub.arg_f(NO_ACTION);
            else
                menu_row2[0] = 0;
            break;
        case OPT_FUNC_CALL:
            if (code == OPT_SELECT)
                sniprintf(menu_row2, LCD_ROW_LEN+1, "%s", (retval==0) ? navi[navlvl].m->items[navi[navlvl].mp].fun.text_success : navi[navlvl].m->items[navi[navlvl].mp].fun.text_failure);
            else
                menu_row2[0] = 0;
            break;
        default:
            break;
    }

    lcd_write_menu();
}
