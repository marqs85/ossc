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
extern avmode_t cm;
extern avconfig_t tc;
extern mode_data_t video_modes[];
extern alt_u32 remote_code;
extern alt_u16 rc_keymap[REMOTE_MAX_KEYS];
extern alt_u8 vm_sel, profile_sel_menu, lt_sel, def_input, profile_link, lcd_bl_timeout;
extern alt_u8 auto_input, auto_av1_ypbpr, auto_av2_ypbpr, auto_av3_ypbpr;
extern alt_u8 update_cur_vm;
extern alt_u8 osd_enable, osd_enable_pre, osd_status_timeout_pre;
extern char target_profile_name[PROFILE_NAME_LEN+1];
extern volatile osd_regs *osd;

alt_u16 tc_h_samplerate, tc_h_samplerate_adj, tc_h_synclen, tc_h_bporch, tc_h_active, tc_v_synclen, tc_v_bporch, tc_v_active, tc_sampler_phase;
alt_u8 menu_active;
alt_u8 vm_sel, vm_edit;

static const char *off_on_desc[] = { LNG("Off","ｵﾌ"), LNG("On","ｵﾝ") };
static const char *video_lpf_desc[] = { LNG("Auto","ｵｰﾄ"), LNG("Off","ｵﾌ"), "95MHz (HDTV II)", "35MHz (HDTV I)", "16MHz (EDTV)", "9MHz (SDTV)" };
static const char *ypbpr_cs_desc[] = { "Rec. 601", "Rec. 709", "Auto" };
static const char *s480p_mode_desc[] = { LNG("Auto","ｵｰﾄ"), "DTV 480p", "VESA 640x480@60", "PSP 480x272" };
static const char *s400p_mode_desc[] = { "VGA 640x400@70", "VGA 720x400@70" };
static const char *sync_lpf_desc[] = { LNG("2.5MHz (max)","2.5MHz (ｻｲﾀﾞｲ)"), LNG("10MHz (med)","10MHz (ﾁｭｳｲ)"), LNG("33MHz (min)","33MHz (ｻｲｼｮｳ)"), LNG("Off","ｵﾌ") };
static const char *stc_lpf_desc[] = { "4.8MHz (HDTV/PC)", "0.5MHz (SDTV)", "1.7MHz (EDTV)" };
static const char *l3_mode_desc[] = { LNG("Generic 16:9","ｼﾞｪﾈﾘｯｸ 16:9"), LNG("Generic 4:3","ｼﾞｪﾈﾘｯｸ 4:3"), LNG("512x240 optim.","512x240 ｻｲﾃｷｶ."), LNG("384x240 optim.","384x240 ｻｲﾃｷｶ."), LNG("320x240 optim.","320x240 ｻｲﾃｷｶ."), LNG("256x240 optim.","256x240 ｻｲﾃｷｶ.") };
static const char *l2l4l5_mode_desc[] = { LNG("Generic 4:3","ｼﾞｪﾈﾘｯｸ 4:3"), LNG("512x240 optim.","512x240 ｻｲﾃｷｶ."), LNG("384x240 optim.","384x240 ｻｲﾃｷｶ."), LNG("320x240 optim.","320x240 ｻｲﾃｷｶ."), LNG("256x240 optim.","256x240 ｻｲﾃｷｶ.") };
static const char *l5_fmt_desc[] = { "1920x1080", "1600x1200", "1920x1200" };
static const char *pm_240p_desc[] = { LNG("Passthru","ﾊﾟｽｽﾙｰ"), "Line2x", "Line3x", "Line4x", "Line5x" };
static const char *pm_480i_desc[] = { LNG("Passthru","ﾊﾟｽｽﾙｰ"), "Line2x (bob)", "Line3x (laced)", "Line4x (bob)" };
static const char *pm_384p_desc[] = { LNG("Passthru","ﾊﾟｽｽﾙｰ"), "Line2x", "Line2x 240x360", "Line3x 240x360", "Line3x Generic" };
static const char *pm_480p_desc[] = { LNG("Passthru","ﾊﾟｽｽﾙｰ"), "Line2x" };
static const char *pm_1080i_desc[] = { LNG("Passthru","ﾊﾟｽｽﾙｰ"), "Line2x (bob)" };
static const char *ar_256col_desc[] = { "4:3", "8:7" };
static const char *tx_mode_desc[] = { "HDMI (RGB)", "HDMI (YCbCr444)", "DVI" };
static const char *sl_mode_desc[] = { LNG("Off","ｵﾌ"), LNG("Auto","ｵｰﾄ"), LNG("On","ｵﾝ") };
static const char *sl_method_desc[] = { LNG("Multiplication","Multiplication"), LNG("Subtraction","Subtraction") };
static const char *sl_type_desc[] = { LNG("Horizontal","ﾖｺ"), LNG("Vertical","ﾀﾃ"), "Horiz. + Vert.", "Custom" };
static const char *sl_id_desc[] = { LNG("Top","ｳｴ"), LNG("Bottom","ｼﾀ") };
static const char *audio_dw_sampl_desc[] = { LNG("Off (fs = 96kHz)","ｵﾌ (fs = 96kHz)"), "2x  (fs = 48kHz)" };
static const char *lt_desc[] = { "Top-left", "Center", "Bottom-right" };
static const char *lcd_bl_timeout_desc[] = { "Off", "3s", "10s", "30s" };
static const char *osd_enable_desc[] = { "Off", "Full", "Simple" };
static const char *osd_status_desc[] = { "2s", "5s", "10s", "Off" };
static const char *rgsb_ypbpr_desc[] = { "RGsB", "YPbPr" };
static const char *auto_input_desc[] = { "Off", "Current input", "All inputs" };
static const char *mask_color_desc[] = { "Black", "Blue", "Green", "Cyan", "Red", "Magenta", "Yellow", "White" };
static const char *av3_alt_rgb_desc[] = { "Off", "AV1", "AV2" };

static void sync_vth_disp(alt_u8 v) { sniprintf(menu_row2, LCD_ROW_LEN+1, "%d mV", (v*1127)/100); }
static void intclks_to_time_disp(alt_u8 v) { sniprintf(menu_row2, LCD_ROW_LEN+1, "%u.%.2u us", (unsigned)(((1000000U*v)/(TVP_INTCLK_HZ/1000))/1000), (unsigned)((((1000000U*v)/(TVP_INTCLK_HZ/1000))%1000)/10)); }
static void extclks_to_time_disp(alt_u8 v) { sniprintf(menu_row2, LCD_ROW_LEN+1, "%u.%.2u us", (unsigned)(((1000000U*v)/(TVP_EXTCLK_HZ/1000))/1000), (unsigned)((((1000000U*v)/(TVP_EXTCLK_HZ/1000))%1000)/10)); }
static void sl_str_disp(alt_u8 v) { sniprintf(menu_row2, LCD_ROW_LEN+1, "%u%%", ((v+1)*625)/100); }
static void sl_cust_str_disp(alt_u8 v) { sniprintf(menu_row2, LCD_ROW_LEN+1, "%u%%", ((v)*625)/100); }
static void sl_hybr_str_disp(alt_u8 v) { sniprintf(menu_row2, LCD_ROW_LEN+1, "%u%%", (v*625)/100); }
static void lines_disp(alt_u8 v) { sniprintf(menu_row2, LCD_ROW_LEN+1, LNG("%u lines","%u ﾗｲﾝ"), v); }
static void pixels_disp(alt_u8 v) { sniprintf(menu_row2, LCD_ROW_LEN+1, LNG("%u pixels","%u ﾄﾞｯﾄ"), v); }
static void value_disp(alt_u8 v) { sniprintf(menu_row2, LCD_ROW_LEN+1, "%u", v); }
static void signed_disp(alt_u8 v) { sniprintf(menu_row2, LCD_ROW_LEN+1, "%d", (alt_8)(v-SIGNED_NUMVAL_ZERO)); }
static void lt_disp(alt_u8 v) { strncpy(menu_row2, lt_desc[v], LCD_ROW_LEN+1); }
static void aud_db_disp(alt_u8 v) { sniprintf(menu_row2, LCD_ROW_LEN+1, "%d dB", ((alt_8)v-AUDIO_GAIN_0DB)); }
static void vm_display_name (alt_u8 v) { strncpy(menu_row2, video_modes[v].name, LCD_ROW_LEN+1); }
static void link_av_desc (avinput_t v) { strncpy(menu_row2, v == AV_LAST ? "No link" : avinput_str[v], LCD_ROW_LEN+1); }
static void profile_disp(alt_u8 v) { read_userdata(v, 1); sniprintf(menu_row2, LCD_ROW_LEN+1, "%u: %s", v, (target_profile_name[0] == 0) ? "<empty>" : target_profile_name); }
static void alc_v_filter_disp(alt_u8 v) { sniprintf(menu_row2, LCD_ROW_LEN+1, LNG("%u lines","%u ﾗｲﾝ"), (1<<v)); }
static void alc_h_filter_disp(alt_u8 v) { sniprintf(menu_row2, LCD_ROW_LEN+1, LNG("%u pixels","%u ﾄﾞｯﾄ"), (1<<(v+1))); }
//static void coarse_gain_disp(alt_u8 v) { sniprintf(menu_row2, LCD_ROW_LEN+1, "%u.%u", ((v*10)+50)/100, (((v*10)+50)%100)/10); }

static const arg_info_t vm_arg_info = {&vm_sel, VIDEO_MODES_CNT-1, vm_display_name};
static const arg_info_t profile_arg_info = {&profile_sel_menu, MAX_PROFILE, profile_disp};
static const arg_info_t lt_arg_info = {&lt_sel, (sizeof(lt_desc)/sizeof(char*))-1, lt_disp};


MENU(menu_advtiming, P99_PROTECT({ \
    { LNG("H. samplerate","H. ｻﾝﾌﾟﾙﾚｰﾄ"),        OPT_AVCONFIG_NUMVAL_U16,{ .num_u16 = { &tc_h_samplerate, H_TOTAL_MIN,   H_TOTAL_MAX, vm_tweak } } },
    { "H. s.rate frac",                           OPT_AVCONFIG_NUMVAL_U16,{ .num_u16 = { &tc_h_samplerate_adj, 0,  H_TOTAL_ADJ_MAX, vm_tweak } } },
    { LNG("H. synclen","H. ﾄﾞｳｷﾅｶﾞｻ"),       OPT_AVCONFIG_NUMVAL_U16,{ .num_u16 = { &tc_h_synclen,    H_SYNCLEN_MIN, H_SYNCLEN_MAX, vm_tweak } } },
    { LNG("H. backporch","H. ﾊﾞｯｸﾎﾟｰﾁ"),        OPT_AVCONFIG_NUMVAL_U16,{ .num_u16 = { &tc_h_bporch,     H_BPORCH_MIN,  H_BPORCH_MAX, vm_tweak } } },
    { LNG("H. active","H. ｱｸﾃｨﾌﾞ"),             OPT_AVCONFIG_NUMVAL_U16,{ .num_u16 = { &tc_h_active,     H_ACTIVE_MIN,  H_ACTIVE_MAX, vm_tweak } } },
    { LNG("V. synclen","V. ﾄﾞｳｷﾅｶﾞｻ"),       OPT_AVCONFIG_NUMVAL_U16,{ .num_u16 = { &tc_v_synclen,    V_SYNCLEN_MIN, V_SYNCLEN_MAX, vm_tweak } } },
    { LNG("V. backporch","V. ﾊﾞｯｸﾎﾟｰﾁ"),       OPT_AVCONFIG_NUMVAL_U16,{ .num_u16 = { &tc_v_bporch,     V_BPORCH_MIN,  V_BPORCH_MAX, vm_tweak } } },
    { LNG("V. active","V. ｱｸﾃｨﾌﾞ"),            OPT_AVCONFIG_NUMVAL_U16,{ .num_u16 = { &tc_v_active,     V_ACTIVE_MIN,  V_ACTIVE_MAX, vm_tweak } } },
    { LNG("Sampling phase","ｻﾝﾌﾟﾘﾝｸﾞﾌｪｰｽﾞ"),     OPT_AVCONFIG_NUMVAL_U16,  { .num_u16 = { &tc_sampler_phase, 0, SAMPLER_PHASE_MAX, vm_tweak } } },
}))

MENU(menu_cust_sl, P99_PROTECT({ \
    { "Sub-line 1 str",                       OPT_AVCONFIG_NUMVALUE,  { .num = { &tc.sl_cust_l_str[0],      OPT_NOWRAP, 0, SCANLINESTR_MAX+1, sl_cust_str_disp } } },
    { "Sub-line 2 str",                       OPT_AVCONFIG_NUMVALUE,  { .num = { &tc.sl_cust_l_str[1],      OPT_NOWRAP, 0, SCANLINESTR_MAX+1, sl_cust_str_disp } } },
    { "Sub-line 3 str",                       OPT_AVCONFIG_NUMVALUE,  { .num = { &tc.sl_cust_l_str[2],      OPT_NOWRAP, 0, SCANLINESTR_MAX+1, sl_cust_str_disp } } },
    { "Sub-line 4 str",                       OPT_AVCONFIG_NUMVALUE,  { .num = { &tc.sl_cust_l_str[3],      OPT_NOWRAP, 0, SCANLINESTR_MAX+1, sl_cust_str_disp } } },
    { "Sub-line 5 str",                       OPT_AVCONFIG_NUMVALUE,  { .num = { &tc.sl_cust_l_str[4],      OPT_NOWRAP, 0, SCANLINESTR_MAX+1, sl_cust_str_disp } } },
    { "Sub-column 1 str",                     OPT_AVCONFIG_NUMVALUE,  { .num = { &tc.sl_cust_c_str[0],      OPT_NOWRAP, 0, SCANLINESTR_MAX+1, sl_cust_str_disp } } },
    { "Sub-column 2 str",                     OPT_AVCONFIG_NUMVALUE,  { .num = { &tc.sl_cust_c_str[1],      OPT_NOWRAP, 0, SCANLINESTR_MAX+1, sl_cust_str_disp } } },
    { "Sub-column 3 str",                     OPT_AVCONFIG_NUMVALUE,  { .num = { &tc.sl_cust_c_str[2],      OPT_NOWRAP, 0, SCANLINESTR_MAX+1, sl_cust_str_disp } } },
    { "Sub-column 4 str",                     OPT_AVCONFIG_NUMVALUE,  { .num = { &tc.sl_cust_c_str[3],      OPT_NOWRAP, 0, SCANLINESTR_MAX+1, sl_cust_str_disp } } },
    { "Sub-column 5 str",                     OPT_AVCONFIG_NUMVALUE,  { .num = { &tc.sl_cust_c_str[4],      OPT_NOWRAP, 0, SCANLINESTR_MAX+1, sl_cust_str_disp } } },
    { "Sub-column 6 str",                     OPT_AVCONFIG_NUMVALUE,  { .num = { &tc.sl_cust_c_str[5],      OPT_NOWRAP, 0, SCANLINESTR_MAX+1, sl_cust_str_disp } } },
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
    { LNG("Pre-ADC Gain","Pre-ADC Gain"),       OPT_AVCONFIG_NUMVALUE,  { .num = { &tc.col.c_gain,    OPT_NOWRAP, 0, COARSE_GAIN_MAX, value_disp } } },
    { "Clamp/ALC offset",                       OPT_AVCONFIG_NUMVALUE,  { .num = { &tc.clamp_offset,  OPT_NOWRAP, CLAMP_OFFSET_MIN, CLAMP_OFFSET_MAX, signed_disp } } },
    { "ALC V filter",                           OPT_AVCONFIG_NUMVALUE,  { .num = { &tc.alc_v_filter,  OPT_NOWRAP, 0, ALC_V_FILTER_MAX, alc_v_filter_disp } } },
    { "ALC H filter",                           OPT_AVCONFIG_NUMVALUE,  { .num = { &tc.alc_h_filter,  OPT_NOWRAP, 0, ALC_H_FILTER_MAX, alc_h_filter_disp } } },
}))

MENU(menu_sampling, P99_PROTECT({ \
    { LNG("480p in sampler","ｻﾝﾌﾟﾗｰﾃﾞ480p"),     OPT_AVCONFIG_SELECTION, { .sel = { &tc.s480p_mode,    OPT_WRAP, SETTING_ITEM(s480p_mode_desc) } } },
    { LNG("400p in sampler","ｻﾝﾌﾟﾗｰﾃﾞ400p"),     OPT_AVCONFIG_SELECTION, { .sel = { &tc.s400p_mode,    OPT_WRAP, SETTING_ITEM(s400p_mode_desc) } } },
    { LNG("Allow TVP HPLL2x","TVP HPLL2xｷｮﾖｳ"), OPT_AVCONFIG_SELECTION, { .sel = { &tc.tvp_hpll2x,   OPT_WRAP, SETTING_ITEM(off_on_desc) } } },
    { LNG("Allow upsample2x","ｱｯﾌﾟｻﾝﾌﾟﾙ2xｷｮﾖｳ"), OPT_AVCONFIG_SELECTION, { .sel = { &tc.upsample2x,   OPT_WRAP, SETTING_ITEM(off_on_desc) } } },
    { LNG("<Adv. timing   >","<ｶｸｼｭﾀｲﾐﾝｸﾞ>"),    OPT_SUBMENU,            { .sub = { &menu_advtiming, &vm_arg_info, vm_select } } },
}))

MENU(menu_sync, P99_PROTECT({ \
    { LNG("Analog sync LPF","ｱﾅﾛｸﾞﾄﾞｳｷ LPF"),    OPT_AVCONFIG_SELECTION, { .sel = { &tc.sync_lpf,    OPT_WRAP,   SETTING_ITEM(sync_lpf_desc) } } },
    { "Analog STC LPF",                         OPT_AVCONFIG_SELECTION, { .sel = { &tc.stc_lpf,    OPT_WRAP,   SETTING_ITEM(stc_lpf_desc) } } },
    { LNG("Analog sync Vth","ｱﾅﾛｸﾞﾄﾞｳｷ Vth"),    OPT_AVCONFIG_NUMVALUE,  { .num = { &tc.sync_vth,    OPT_NOWRAP, 0, SYNC_VTH_MAX, sync_vth_disp } } },
    { LNG("Hsync tolerance","Hsyncｺｳｻ"),        OPT_AVCONFIG_NUMVALUE,  { .num = { &tc.linelen_tol, OPT_NOWRAP, 0, 0xFF, intclks_to_time_disp } } },
    { LNG("Vsync threshold","Vsyncｼｷｲﾁ"),       OPT_AVCONFIG_NUMVALUE,  { .num = { &tc.vsync_thold, OPT_NOWRAP, VSYNC_THOLD_MIN, VSYNC_THOLD_MAX, intclks_to_time_disp } } },
    { "H-PLL Pre-Coast",                        OPT_AVCONFIG_NUMVALUE,  { .num = { &tc.pre_coast,   OPT_NOWRAP, 0, PLL_COAST_MAX, lines_disp } } },
    { "H-PLL Post-Coast",                       OPT_AVCONFIG_NUMVALUE,  { .num = { &tc.post_coast,  OPT_NOWRAP, 0, PLL_COAST_MAX, lines_disp } } },
}))

MENU(menu_output, P99_PROTECT({ \
    { LNG("240p/288p proc","240p/288pｼｮﾘ"),     OPT_AVCONFIG_SELECTION, { .sel = { &tc.pm_240p,         OPT_WRAP, SETTING_ITEM(pm_240p_desc) } } },
    { LNG("384p/400p proc","384p/400pｼｮﾘ"),     OPT_AVCONFIG_SELECTION, { .sel = { &tc.pm_384p,         OPT_WRAP, SETTING_ITEM(pm_384p_desc) } } },
    { LNG("480i/576i proc","480i/576iｼｮﾘ"),     OPT_AVCONFIG_SELECTION, { .sel = { &tc.pm_480i,         OPT_WRAP, SETTING_ITEM(pm_480i_desc) } } },
    { LNG("480p/576p proc","480p/576pｼｮﾘ"),     OPT_AVCONFIG_SELECTION, { .sel = { &tc.pm_480p,         OPT_WRAP, SETTING_ITEM(pm_480p_desc) } } },
    { LNG("960i/1080i proc","960i/1080iｼｮﾘ"),   OPT_AVCONFIG_SELECTION, { .sel = { &tc.pm_1080i,        OPT_WRAP, SETTING_ITEM(pm_1080i_desc) } } },
    { LNG("Line2x mode","Line2xﾓｰﾄﾞ"),          OPT_AVCONFIG_SELECTION, { .sel = { &tc.l2_mode,         OPT_WRAP, SETTING_ITEM(l2l4l5_mode_desc) } } },
    { LNG("Line3x mode","Line3xﾓｰﾄﾞ"),          OPT_AVCONFIG_SELECTION, { .sel = { &tc.l3_mode,         OPT_WRAP, SETTING_ITEM(l3_mode_desc) } } },
    { LNG("Line4x mode","Line4xﾓｰﾄﾞ"),          OPT_AVCONFIG_SELECTION, { .sel = { &tc.l4_mode,         OPT_WRAP, SETTING_ITEM(l2l4l5_mode_desc) } } },
    { LNG("Line5x mode","Line5xﾓｰﾄﾞ"),          OPT_AVCONFIG_SELECTION, { .sel = { &tc.l5_mode,         OPT_WRAP, SETTING_ITEM(l2l4l5_mode_desc) } } },
    { LNG("Line5x format","Line5xｹｲｼｷ"),        OPT_AVCONFIG_SELECTION, { .sel = { &tc.l5_fmt,          OPT_WRAP, SETTING_ITEM(l5_fmt_desc) } } },
    { LNG("256x240 aspect","256x240ｱｽﾍﾟｸﾄ"),    OPT_AVCONFIG_SELECTION, { .sel = { &tc.ar_256col,       OPT_WRAP, SETTING_ITEM(ar_256col_desc) } } },
    { LNG("TX mode","TXﾓｰﾄﾞ"),                  OPT_AVCONFIG_SELECTION, { .sel = { &tc.tx_mode,         OPT_WRAP, SETTING_ITEM(tx_mode_desc) } } },
    { "HDMI ITC",                              OPT_AVCONFIG_SELECTION, { .sel = { &tc.hdmi_itc,        OPT_WRAP, SETTING_ITEM(off_on_desc) } } },
}))

MENU(menu_scanlines, P99_PROTECT({ \
    { LNG("Scanlines","ｽｷｬﾝﾗｲﾝ"),                  OPT_AVCONFIG_SELECTION, { .sel = { &tc.sl_mode,     OPT_WRAP,   SETTING_ITEM(sl_mode_desc) } } },
    { LNG("Sl. strength","ｽｷｬﾝﾗｲﾝﾂﾖｻ"),            OPT_AVCONFIG_NUMVALUE,  { .num = { &tc.sl_str,      OPT_NOWRAP, 0, SCANLINESTR_MAX, sl_str_disp } } },
    { "Sl. hybrid str.",                          OPT_AVCONFIG_NUMVALUE,  { .num = { &tc.sl_hybr_str, OPT_NOWRAP, 0, SL_HYBRIDSTR_MAX, sl_hybr_str_disp } } },
    { "Sl. method",                               OPT_AVCONFIG_SELECTION, { .sel = { &tc.sl_method,   OPT_WRAP,   SETTING_ITEM(sl_method_desc) } } },
    { "Sl. alternating",                          OPT_AVCONFIG_SELECTION, { .sel = { &tc.sl_altern,   OPT_WRAP,   SETTING_ITEM(off_on_desc) } } },
    { LNG("Sl. alignment","ｽｷｬﾝﾗｲﾝﾎﾟｼﾞｼｮﾝ"),        OPT_AVCONFIG_SELECTION, { .sel = { &tc.sl_id,       OPT_WRAP,   SETTING_ITEM(sl_id_desc) } } },
    { "Sl. alt interval",                         OPT_AVCONFIG_SELECTION, { .sel = { &tc.sl_altiv,    OPT_WRAP,   SETTING_ITEM(off_on_desc) } } },
    { LNG("Sl. type","ｽｷｬﾝﾗｲﾝﾙｲ"),                 OPT_AVCONFIG_SELECTION, { .sel = { &tc.sl_type,     OPT_WRAP,   SETTING_ITEM(sl_type_desc) } } },
    { "<  Custom Sl.  >",                         OPT_SUBMENU,            { .sub = { &menu_cust_sl, NULL, NULL } } },
}))

MENU(menu_postproc, P99_PROTECT({ \
    { LNG("Horizontal mask","ｽｲﾍｲﾏｽｸ"),           OPT_AVCONFIG_NUMVALUE,  { .num = { &tc.h_mask,      OPT_NOWRAP, 0, H_MASK_MAX, pixels_disp } } },
    { LNG("Vertical mask","ｽｲﾁｮｸﾏｽｸ"),            OPT_AVCONFIG_NUMVALUE,  { .num = { &tc.v_mask,      OPT_NOWRAP, 0, V_MASK_MAX, pixels_disp } } },
    { "Mask color",                              OPT_AVCONFIG_SELECTION, { .sel = { &tc.mask_color,  OPT_NOWRAP,   SETTING_ITEM(mask_color_desc) } } },
    { LNG("Mask brightness","ﾏｽｸｱｶﾙｻ"),           OPT_AVCONFIG_NUMVALUE,  { .num = { &tc.mask_br,     OPT_NOWRAP, 0, HV_MASK_MAX_BR, value_disp } } },
    { LNG("Reverse LPF","ｷﾞｬｸLPF"),              OPT_AVCONFIG_NUMVALUE,  { .num = { &tc.reverse_lpf, OPT_NOWRAP, 0, REVERSE_LPF_MAX, value_disp } } },
    { LNG("<DIY lat. test>","DIYﾁｴﾝﾃｽﾄ"),         OPT_FUNC_CALL,          { .fun = { latency_test, &lt_arg_info } } },
}))

MENU(menu_compatibility, P99_PROTECT({ \
    { LNG("Full TX setup","ﾌﾙTXｾｯﾄｱｯﾌﾟ"),         OPT_AVCONFIG_SELECTION, { .sel = { &tc.full_tx_setup,    OPT_WRAP, SETTING_ITEM(off_on_desc) } } },
    { LNG("AV3 interlacefix","AV3ｲﾝﾀｰﾚｰｽｼｭｳｾｲ"),  OPT_AVCONFIG_SELECTION, { .sel = { &tc.vga_ilace_fix,   OPT_WRAP, SETTING_ITEM(off_on_desc) } } },
    { "AV3 use alt. RGB",                        OPT_AVCONFIG_SELECTION, { .sel = { &tc.av3_alt_rgb,     OPT_WRAP, SETTING_ITEM(av3_alt_rgb_desc) } } },
    { "Default HDMI VIC",                       OPT_AVCONFIG_NUMVALUE,  { .num = { &tc.default_vic,     OPT_NOWRAP, 0, HDMI_1080p50, value_disp } } },
    { "Panasonic hack",                        OPT_AVCONFIG_SELECTION, { .sel = { &tc.panasonic_hack,   OPT_WRAP, SETTING_ITEM(off_on_desc) } } },
}))

#ifdef ENABLE_AUDIO
MENU(menu_audio, P99_PROTECT({ \
    { LNG("Down-sampling","ﾀﾞｳﾝｻﾝﾌﾟﾘﾝｸﾞ"),       OPT_AVCONFIG_SELECTION, { .sel = { &tc.audio_dw_sampl, OPT_WRAP, SETTING_ITEM(audio_dw_sampl_desc) } } },
    { LNG("Swap left/right","ﾋﾀﾞﾘ/ﾐｷﾞｽﾜｯﾌﾟ"),    OPT_AVCONFIG_SELECTION, { .sel = { &tc.audio_swap_lr,  OPT_WRAP, SETTING_ITEM(off_on_desc) } } },
    { "Pre-ADC gain",                           OPT_AVCONFIG_NUMVALUE,  { .num = { &tc.audio_gain,     OPT_NOWRAP, 0, AUDIO_GAIN_MAX, aud_db_disp } } },
}))
#define AUDIO_MENU { LNG("Audio options  >","ｵｰﾃﾞｨｵｵﾌﾟｼｮﾝ     >"),                  OPT_SUBMENU,            { .sub = { &menu_audio, NULL, NULL } } },
#else
#define AUDIO_MENU
#endif

MENU(menu_settings, P99_PROTECT({ \
    { LNG("<Load profile >","<ﾌﾟﾛﾌｧｲﾙﾛｰﾄﾞ    >"),   OPT_FUNC_CALL,         { .fun = { load_profile, &profile_arg_info } } },
    { LNG("<Save profile >","<ﾌﾟﾛﾌｧｲﾙｾｰﾌﾞ    >"),  OPT_FUNC_CALL,          { .fun = { save_profile, &profile_arg_info } } },
    { LNG("<Reset settings>","<ｾｯﾃｲｵｼｮｷｶ    >"),  OPT_FUNC_CALL,          { .fun = { set_default_avconfig, NULL } } },
    { LNG("Link prof->input","Link prof->input"), OPT_AVCONFIG_NUMVALUE,  { .num = { &tc.link_av,  OPT_WRAP, AV1_RGBs, AV_LAST, link_av_desc } } },
    { LNG("Link input->prof","Link input->prof"),   OPT_AVCONFIG_SELECTION, { .sel = { &profile_link,  OPT_WRAP, SETTING_ITEM(off_on_desc) } } },
    { LNG("Initial input","ｼｮｷﾆｭｳﾘｮｸ"),          OPT_AVCONFIG_SELECTION, { .sel = { &def_input,       OPT_WRAP, SETTING_ITEM(avinput_str) } } },
    { "Autodetect input",                          OPT_AVCONFIG_SELECTION, { .sel = { &auto_input,     OPT_WRAP, SETTING_ITEM(auto_input_desc) } } },
    { "Auto AV1 Y/Gs",                          OPT_AVCONFIG_SELECTION, { .sel = { &auto_av1_ypbpr,     OPT_WRAP, SETTING_ITEM(rgsb_ypbpr_desc) } } },
    { "Auto AV2 Y/Gs",                          OPT_AVCONFIG_SELECTION, { .sel = { &auto_av2_ypbpr,     OPT_WRAP, SETTING_ITEM(rgsb_ypbpr_desc) } } },
    { "Auto AV3 Y/Gs",                          OPT_AVCONFIG_SELECTION, { .sel = { &auto_av3_ypbpr,     OPT_WRAP, SETTING_ITEM(rgsb_ypbpr_desc) } } },
    { "LCD BL timeout",                         OPT_AVCONFIG_SELECTION, { .sel = { &lcd_bl_timeout,  OPT_WRAP, SETTING_ITEM(lcd_bl_timeout_desc) } } },
    { "OSD",                                    OPT_AVCONFIG_SELECTION, { .sel = { &osd_enable_pre,   OPT_WRAP,   SETTING_ITEM(osd_enable_desc) } } },
    { "OSD status disp.",                       OPT_AVCONFIG_SELECTION, { .sel = { &osd_status_timeout_pre,   OPT_WRAP,   SETTING_ITEM(osd_status_desc) } } },
#ifndef DEBUG
    { LNG("<Import sett.  >","<ｾｯﾃｲﾖﾐｺﾐ      >"), OPT_FUNC_CALL,        { .fun = { import_userdata, NULL } } },
    { LNG("<Export sett.  >","<ｾｯﾃｲｶｷｺﾐ      >"), OPT_FUNC_CALL,        { .fun = { export_userdata, NULL } } },
    { LNG("<Fw. update    >","<ﾌｧｰﾑｳｪｱｱｯﾌﾟﾃﾞｰﾄ>"), OPT_FUNC_CALL,        { .fun = { fw_update, NULL } } },
#endif
}))


MENU(menu_main, P99_PROTECT({ \
    { LNG("Video in proc  >","ﾀｲｵｳｴｲｿﾞｳ     >"),  OPT_SUBMENU,            { .sub = { &menu_vinputproc, NULL, NULL } } },
    { LNG("Sampling opt.  >","ｻﾝﾌﾟﾘﾝｸﾞｵﾌﾟｼｮﾝ>"),  OPT_SUBMENU,            { .sub = { &menu_sampling, NULL, NULL } } },
    { LNG("Sync opt.      >","ﾄﾞｳｷｵﾌﾟｼｮﾝ    >"),  OPT_SUBMENU,            { .sub = { &menu_sync, NULL, NULL } } },
    { LNG("Output opt.    >","ｼｭﾂﾘｮｸｵﾌﾟｼｮﾝ  >"),  OPT_SUBMENU,            { .sub = { &menu_output, NULL, NULL } } },
    { LNG("Scanline opt.  >","ｽｷｬﾝﾗｲﾝｵﾌﾟｼｮﾝ >"),  OPT_SUBMENU,            { .sub = { &menu_scanlines, NULL, NULL } } },
    { LNG("Post-proc.     >","ｱﾄｼｮﾘ         >"),  OPT_SUBMENU,            { .sub = { &menu_postproc, NULL, NULL } } },
    { LNG("Compatibility  >","ｺﾞｶﾝｾｲ        >"), OPT_SUBMENU,             { .sub = { &menu_compatibility, NULL, NULL } } },
    AUDIO_MENU
    { "Settings opt   >",                       OPT_SUBMENU,             { .sub = { &menu_settings, NULL, NULL } } },
}))

// Max 3 levels currently
menunavi navi[] = {{&menu_main, 0}, {NULL, 0}, {NULL, 0}};
alt_u8 navlvl = 0;


menunavi* get_current_menunavi() {
    return &navi[navlvl];
}

void write_option_value(menuitem_t *item, int func_called, int retval)
{
    switch (item->type) {
        case OPT_AVCONFIG_SELECTION:
            strncpy(menu_row2, item->sel.setting_str[*(item->sel.data)], LCD_ROW_LEN+1);
            break;
        case OPT_AVCONFIG_NUMVALUE:
            item->num.df(*(item->num.data));
            break;
        case OPT_AVCONFIG_NUMVAL_U16:
            item->num_u16.df(item->num_u16.data);
            break;
        case OPT_SUBMENU:
            if (item->sub.arg_info)
                item->sub.arg_info->df(*item->sub.arg_info->data);
            else
                menu_row2[0] = 0;
            break;
        case OPT_FUNC_CALL:
            if (func_called) {
                if (retval == 0)
                    strncpy(menu_row2, "Done", LCD_ROW_LEN+1);
                else if (retval < 0)
                    sniprintf(menu_row2, LCD_ROW_LEN+1, "Failed (%d)", retval);
            } else if (item->fun.arg_info) {
                item->fun.arg_info->df(*item->fun.arg_info->data);
            } else {
                menu_row2[0] = 0;
            }
            break;
        default:
            break;
    }
}

void render_osd_page() {
    int i;
    menuitem_t *item;
    uint32_t row_mask[2] = {0, 0};

    if (!menu_active || (osd_enable != 1))
        return;

    for (i=0; i < navi[navlvl].m->num_items; i++) {
        item = &navi[navlvl].m->items[i];
        strncpy((char*)osd->osd_array.data[i][0], item->name, OSD_CHAR_COLS);
        row_mask[0] |= (1<<i);

        if ((item->type != OPT_SUBMENU) && (item->type != OPT_FUNC_CALL)) {
            write_option_value(item, 0, 0);
            strncpy((char*)osd->osd_array.data[i][1], menu_row2, OSD_CHAR_COLS);
            row_mask[1] |= (1<<i);
        }
    }

    osd->osd_sec_enable[0].mask = row_mask[0];
    osd->osd_sec_enable[1].mask = row_mask[1];
}

void display_menu(alt_u8 forcedisp)
{
    menucode_id code = NO_ACTION;
    menuitem_t *item;
    alt_u8 *val, val_wrap, val_min, val_max;
    alt_u16 *val_u16, val_u16_min, val_u16_max;
    int i, func_called = 0, retval = 0;

    for (i=RC_OK; i < RC_INFO; i++) {
        if (remote_code == rc_keymap[i]) {
            code = i;
            break;
        }
    }

    if (!forcedisp && !remote_code)
        return;

    item = &navi[navlvl].m->items[navi[navlvl].mp];

    // Parse menu control
    switch (code) {
    case PREV_PAGE:
    case NEXT_PAGE:
        if ((item->type == OPT_FUNC_CALL) || (item->type == OPT_SUBMENU))
            osd->osd_sec_enable[1].mask &= ~(1<<navi[navlvl].mp);

        if (code == PREV_PAGE)
            navi[navlvl].mp = (navi[navlvl].mp == 0) ? navi[navlvl].m->num_items-1 : (navi[navlvl].mp-1);
        else
            navi[navlvl].mp = (navi[navlvl].mp+1) % navi[navlvl].m->num_items;
        break;
    case PREV_MENU:
        if (navlvl > 0) {
            navlvl--;
            render_osd_page();
        } else {
            menu_active = 0;
            osd->osd_config.menu_active = 0;
            ui_disp_status(0);
            return;
        }
        break;
    case OPT_SELECT:
        switch (item->type) {
            case OPT_SUBMENU:
                if (item->sub.arg_f)
                    item->sub.arg_f();

                if (navi[navlvl+1].m != item->sub.menu)
                    navi[navlvl+1].mp = 0;
                navi[navlvl+1].m = item->sub.menu;
                navlvl++;
                render_osd_page();

                break;
            case OPT_FUNC_CALL:
                retval = item->fun.f();
                func_called = 1;
                break;
            default:
                break;
        }
        break;
    case VAL_MINUS:
    case VAL_PLUS:
        switch (item->type) {
            case OPT_AVCONFIG_SELECTION:
            case OPT_AVCONFIG_NUMVALUE:
                val = item->sel.data;
                val_wrap = item->sel.wrap_cfg;
                val_min = item->sel.min;
                val_max = item->sel.max;

                if (code == VAL_MINUS)
                    *val = (*val > val_min) ? (*val-1) : (val_wrap ? val_max : val_min);
                else
                    *val = (*val < val_max) ? (*val+1) : (val_wrap ? val_min : val_max);
                break;
            case OPT_AVCONFIG_NUMVAL_U16:
                val_u16 = item->num_u16.data;
                val_u16_min = item->num_u16.min;
                val_u16_max = item->num_u16.max;
                val_wrap = (val_u16_min == 0);
                if (code == VAL_MINUS)
                    *val_u16 = (*val_u16 > val_u16_min) ? (*val_u16-1) : (val_wrap ? val_u16_max : val_u16_min);
                else
                    *val_u16 = (*val_u16 < val_u16_max) ? (*val_u16+1) : (val_wrap ? val_u16_min : val_u16_max);
                break;
            case OPT_SUBMENU:
                val = item->sub.arg_info->data;
                val_max = item->sub.arg_info->max;

                if (item->sub.arg_info) {
                    if (code == VAL_MINUS)
                        *val = (*val > 0) ? (*val-1) : 0;
                    else
                        *val = (*val < val_max) ? (*val+1) : val_max;
                }
                break;
            case OPT_FUNC_CALL:
                val = item->fun.arg_info->data;
                val_max = item->fun.arg_info->max;

                if (item->fun.arg_info) {
                    if (code == VAL_MINUS)
                        *val = (*val > 0) ? (*val-1) : 0;
                    else
                        *val = (*val < val_max) ? (*val+1) : val_max;
                }
                break;
            default:
                break;
        }
        break;
    default:
        break;
    }

    // Generate menu text
    item = &navi[navlvl].m->items[navi[navlvl].mp];
    strncpy(menu_row1, item->name, LCD_ROW_LEN+1);
    write_option_value(item, func_called, retval);
    strncpy((char*)osd->osd_array.data[navi[navlvl].mp][1], menu_row2, OSD_CHAR_COLS);
    osd->osd_row_color.mask = (1<<navi[navlvl].mp);
    if (func_called || ((item->type == OPT_FUNC_CALL) && item->fun.arg_info != NULL) || ((item->type == OPT_SUBMENU) && item->sub.arg_info != NULL))
        osd->osd_sec_enable[1].mask |= (1<<navi[navlvl].mp);

    ui_disp_menu(0);
}

static void vm_select() {
    vm_edit = vm_sel;
    tc_h_samplerate = video_modes[vm_edit].h_total;
    tc_h_samplerate_adj = (alt_u16)video_modes[vm_edit].h_total_adj;
    tc_h_synclen = (alt_u16)video_modes[vm_edit].h_synclen;
    tc_h_bporch = (alt_u16)video_modes[vm_edit].h_backporch;
    tc_h_active = video_modes[vm_edit].h_active;
    tc_v_synclen = (alt_u16)video_modes[vm_edit].v_synclen;
    tc_v_bporch = (alt_u16)video_modes[vm_edit].v_backporch;
    tc_v_active = video_modes[vm_edit].v_active;
    tc_sampler_phase = video_modes[vm_edit].sampler_phase;
}

static void vm_tweak(alt_u16 *v) {
    if (cm.sync_active && (cm.id == vm_edit)) {
        if ((video_modes[cm.id].h_total != tc_h_samplerate) ||
            (video_modes[cm.id].h_total_adj != (alt_u8)tc_h_samplerate_adj) ||
            (video_modes[cm.id].h_synclen != tc_h_synclen) ||
            (video_modes[cm.id].h_backporch != (alt_u8)tc_h_bporch) ||
            (video_modes[cm.id].h_active != tc_h_active) ||
            (video_modes[cm.id].v_synclen != tc_v_synclen) ||
            (video_modes[cm.id].v_backporch != (alt_u8)tc_v_bporch) ||
            (video_modes[cm.id].v_active != tc_v_active) ||
            (video_modes[cm.id].sampler_phase != tc_sampler_phase))
            update_cur_vm = 1;
    }
    video_modes[vm_edit].h_total = tc_h_samplerate;
    video_modes[vm_edit].h_total_adj = (alt_u8)tc_h_samplerate_adj;
    video_modes[vm_edit].h_synclen = (alt_u8)tc_h_synclen;
    video_modes[vm_edit].h_backporch = (alt_u8)tc_h_bporch;
    video_modes[vm_edit].h_active = tc_h_active;
    video_modes[vm_edit].v_synclen = (alt_u8)tc_v_synclen;
    video_modes[vm_edit].v_backporch = (alt_u8)tc_v_bporch;
    video_modes[vm_edit].v_active = tc_v_active;
    video_modes[vm_edit].sampler_phase = tc_sampler_phase;

    if (v == &tc_sampler_phase)
        sniprintf(menu_row2, LCD_ROW_LEN+1, LNG("%d deg","%d ﾄﾞ"), ((*v)*1125)/100);
    else if (v == &tc_h_samplerate)
        sniprintf(menu_row2, LCD_ROW_LEN+1, "%u", video_modes[vm_edit].h_total);
    else if (v == &tc_h_samplerate_adj)
        sniprintf(menu_row2, LCD_ROW_LEN+1, ".%.2u", video_modes[vm_edit].h_total_adj*5);
    else
        sniprintf(menu_row2, LCD_ROW_LEN+1, "%u", *v);
}
