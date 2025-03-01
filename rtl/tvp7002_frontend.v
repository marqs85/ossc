//
// Copyright (C) 2022-2023 Markus Hiienkari <mhiienka@niksula.hut.fi>
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

module tvp7002_frontend (
    input PCLK_i,
    input CLK_MEAS_i,
    input reset_n,
    input [7:0] R_i,
    input [7:0] G_i,
    input [7:0] B_i,
    input HS_i,
    input VS_i,
    input HSYNC_i,
    input VSYNC_i,
    input DE_i,
    input FID_i,
    input sogref_update_i,
    input vsync_i_type,
    input [31:0] hv_in_config,
    input [31:0] hv_in_config2,
    input [31:0] hv_in_config3,
    input [31:0] misc_config,
    output [7:0] R_o,
    output [7:0] G_o,
    output [7:0] B_o,
    output HSYNC_o,
    output VSYNC_o,
    output DE_o,
    output FID_o,
    output reg interlace_flag,
    output datavalid_o,
    output [10:0] xpos_o,
    output [10:0] ypos_o,
    output reg [10:0] vtotal,
    output reg frame_change,
    output reg sof_scaler,
    output reg [19:0] pcnt_field,
    output reg [7:0] hsync_width,
    output reg sync_active
);

localparam FID_EVEN = 1'b0;
localparam FID_ODD = 1'b1;

localparam VSYNC_SEPARATED = 1'b0;
localparam VSYNC_RAW = 1'b1;

localparam PP_PL_START      = 1;
localparam PP_DE_POS_START  = PP_PL_START;
localparam PP_DE_POS_LENGTH = 1;
localparam PP_DE_POS_END    = PP_DE_POS_START + PP_DE_POS_LENGTH;
localparam PP_RLPF_START    = PP_DE_POS_END;
localparam PP_RLPF_LENGTH   = 3;
localparam PP_RLPF_END      = PP_RLPF_START + PP_RLPF_LENGTH;
localparam PP_PL_END        = PP_RLPF_END;

reg [11:0] h_cnt, h_cnt_sogref;
reg [10:0] v_cnt;
reg [10:0] vmax_cnt;
reg HS_i_prev, VS_i_np_prev;
reg HSYNC_i_np_prev, VSYNC_i_np_prev;
reg [1:0] fid_next_ctr;
reg fid_next;
reg [3:0] h_ctr;

reg [7:0] R_pp[PP_PL_START:PP_PL_END] /* synthesis ramstyle = "logic" */;
reg [7:0] G_pp[PP_PL_START:PP_PL_END] /* synthesis ramstyle = "logic" */;
reg [7:0] B_pp[PP_PL_START:PP_PL_END] /* synthesis ramstyle = "logic" */;
reg HSYNC_pp[PP_PL_START:PP_PL_END] /* synthesis ramstyle = "logic" */;
reg VSYNC_pp[PP_PL_START:PP_PL_END] /* synthesis ramstyle = "logic" */;
reg FID_pp[PP_PL_START:PP_PL_END] /* synthesis ramstyle = "logic" */;
reg DE_pp[PP_DE_POS_END:PP_PL_END] /* synthesis ramstyle = "logic" */;
reg datavalid_pp[PP_DE_POS_END:PP_PL_END] /* synthesis ramstyle = "logic" */;
reg [10:0] xpos_pp[PP_DE_POS_END:PP_PL_END] /* synthesis ramstyle = "logic" */;
reg [10:0] ypos_pp[PP_DE_POS_END:PP_PL_END] /* synthesis ramstyle = "logic" */;

// Reverse LPF
wire rlpf_trigger_act;
reg signed [14:0] R_diff_s15_pre, G_diff_s15_pre, B_diff_s15_pre, R_diff_s15, G_diff_s15, B_diff_s15;
reg [7:0] R_pp_prev_rlpf, G_pp_prev_rlpf, B_pp_prev_rlpf;

// Lumacode
reg [1:0] lc_code[1:4];
reg [2:0] lc_ctr;
reg [2:0] lc_cnt;
reg [2:0] lc_emp_nes;
reg [3:0] lc_atari_hue, lc_atari_luma;
reg lc_atari_ctr;

// Measurement registers
reg [20:0] pcnt_frame_ctr;
reg [17:0] syncpol_det_ctr, hsync_hpol_ctr, vsync_hpol_ctr;
reg [2:0] sync_inactive_ctr;
reg [11:0] pcnt_line, pcnt_line_ctr, meas_h_cnt, meas_h_cnt_sogref;
reg [7:0] hs_ctr;
reg pcnt_line_stored;
reg [10:0] meas_v_cnt;
reg meas_hl_det, meas_fid;
reg hsync_i_pol, vsync_i_pol;

wire [11:0] H_TOTAL = hv_in_config[11:0];
wire [11:0] H_ACTIVE = hv_in_config[23:12];
wire [7:0] H_SYNCLEN = hv_in_config[31:24];
wire [8:0] H_BACKPORCH = hv_in_config2[8:0];

wire [10:0] V_ACTIVE = hv_in_config2[30:20];
wire [3:0] V_SYNCLEN = hv_in_config3[3:0];
wire [8:0] V_BACKPORCH = hv_in_config3[12:4];

wire [5:0] MISC_REV_LPF_STR = (misc_config[11:7] + 6'd16);
wire MISC_REV_LPF_ENABLE = (misc_config[11:7] != 5'h0);
wire [2:0] MISC_LUMACODE_MODE = misc_config[25:23];
wire [1:0] MISC_LUMACODE_NES_PALETTE = misc_config[27:26];

wire [11:0] h_cnt_ref = (vsync_i_type == VSYNC_SEPARATED) ? h_cnt_sogref : h_cnt;
wire [11:0] even_min_thold = (H_TOTAL / 12'd4);
wire [11:0] even_max_thold = (H_TOTAL / 12'd2) + (H_TOTAL / 12'd4);

wire [11:0] meas_h_cnt_ref = (vsync_i_type == VSYNC_SEPARATED) ? meas_h_cnt_sogref : meas_h_cnt;
wire [11:0] meas_even_min_thold = (pcnt_line / 12'd4);
wire [11:0] meas_even_max_thold = (pcnt_line / 12'd2) + (pcnt_line / 12'd4);
wire meas_vblank_region = (pcnt_frame_ctr < 8*pcnt_line) | (pcnt_frame_ctr > (({1'b0, pcnt_field}<<interlace_flag) - 4*pcnt_line)) |
                          (interlace_flag & (pcnt_frame_ctr < (pcnt_field+8*pcnt_line)) & (pcnt_frame_ctr > (pcnt_field - 4*pcnt_line)));
wire [11:0] glitch_filt_thold = meas_vblank_region ? (pcnt_line/4) : (pcnt_line/8);

// TODO: calculate H/V polarity independently
wire VS_i_np = (VS_i ^ ~vsync_i_pol);
wire VSYNC_i_np = (VSYNC_i ^ ~vsync_i_pol);
wire HSYNC_i_np = (HSYNC_i ^ ~hsync_i_pol);

// Sample skip for low-res optimized modes
wire [3:0] H_SKIP = hv_in_config3[27:24];
wire [3:0] H_SAMPLE_SEL = hv_in_config3[31:28];

// Lumacode uses 2 samples for {C64, C128, VIC20, Spectrum, TMS99xxA}, 3 samples for NES, 6 samples for Atari 8bit (3 per pixel) and 4 samples for VCS (2 per half-pixel)
wire [2:0] LC_SAMPLES = (MISC_LUMACODE_MODE <= 3) ? 2 : ((MISC_LUMACODE_MODE <= 5) ? 3 : 2);
wire [2:0] LC_H_SKIP = ((H_SKIP+1) / LC_SAMPLES) - 1;

// Lumacode palettes for 2-sample index-based sources (C64, Spectrum, Coleco/MSX)
wire [23:0] lumacode_data_2s[0:2][0:15] = '{'{ 24'h000000,24'h2a1b9d,24'h7d202c,24'h84258c,24'h4c2e00,24'h3c3c3c,24'h646464,24'h4fb3a5,24'h7f410d,24'h6351db,24'h939393,24'hbfd04a,24'h339840,24'hb44f5c,24'h7ce587,24'hffffff},
                                            '{ 24'h000000,24'h000000,24'h0200FD,24'hCF01CE,24'h0100CE,24'hCF0100,24'hFF02FD,24'h01CFCF,24'hFF0201,24'h00CF15,24'h02FFFF,24'hFFFF1D,24'h00FF1C,24'hCFCF15,24'hCFCFCF,24'hFFFFFF},
                                            '{ 24'h000000,24'h5455ed,24'hfc5554,24'hff7978,24'h000000,24'hd4524d,24'h7d76fc,24'h42ebf5,24'h21b03b,24'h21c842,24'hff7978,24'hcccccc,24'hc95bba,24'hd4c154,24'he6ce80,24'hffffff}};

// Lumacode palette for NES, default
wire [23:0] lumacode_data_3s_default[0:63] = '{ 24'h000000, 24'h000000, 24'h000000, 24'h000000, 24'h000000, 24'h000000, 24'h000000, 24'h000000,
                                        24'h626262, 24'h001fb2, 24'h2404c8, 24'h5200b2, 24'h730076, 24'h800024, 24'h730b00, 24'h522800, 24'h244400, 24'h005700, 24'h005c00, 24'h005324, 24'h003c76, 24'h000000, 
                                        24'hababab, 24'h0d57ff, 24'h4b30ff, 24'h8a13ff, 24'hbc08d6, 24'hd21269, 24'hc72e00, 24'h9d5400, 24'h607b00, 24'h209800, 24'h00a300, 24'h009942, 24'h007db4, 24'h000000, 
                                        24'hffffff, 24'h53aeff, 24'h9085ff, 24'hd365ff, 24'hff57ff, 24'hff5dcf, 24'hff7757, 24'hfa9e00, 24'hbdc700, 24'h7ae700, 24'h43f611, 24'h26ef7e, 24'h2cd5f6, 24'h4e4e4e, 
                                        24'hffffff, 24'hb6e1ff, 24'hced1ff, 24'he9c3ff, 24'hffbcff, 24'hffbdf4, 24'hffc6c3, 24'hffd59a, 24'he9e681, 24'hcef481, 24'hb6fb9a, 24'ha9fac3, 24'ha9f0f4, 24'hb8b8b8};
// Firebrand X smooth
wire [23:0] lumacode_data_3s_palette2[0:63] = '{ 24'h000000, 24'h000000, 24'h000000, 24'h000000, 24'h000000, 24'h000000, 24'h000000, 24'h000000,
                                        24'h6A6D6A, 24'h001380, 24'h1E008A, 24'h39007A, 24'h550056, 24'h5A0018, 24'h4F1000, 24'h3D1C00, 24'h253200, 24'h003D00, 24'h004000, 24'h003924, 24'h002E55, 24'h000000,
                                        24'hB9BCB9, 24'h1850C7, 24'h4B30E3, 24'h7322D6, 24'h951FA9, 24'h9D285C, 24'h983700, 24'h7F4C00, 24'h5E6400, 24'h227700, 24'h027E02, 24'h007645, 24'h006E8A, 24'h000000,
                                        24'hFFFFFF, 24'h68A6FF, 24'h8C9CFF, 24'hB586FF, 24'hD975FD, 24'hE377B9, 24'hE58D68, 24'hD49D29, 24'hB3AF0C, 24'h7BC211, 24'h55CA47, 24'h46CB81, 24'h47C1C5, 24'h4A4D4A,
                                        24'hFFFFFF, 24'hCCEAFF, 24'hDDDEFF, 24'hECDAFF, 24'hF8D7FE, 24'hFCD6F5, 24'hFDDBCF, 24'hF9E7B5, 24'hF1F0AA, 24'hDAFAA9, 24'hC9FFBC, 24'hC3FBD7, 24'hC4F6F6, 24'hBEC1BE};
// Kitrinx34
wire [23:0] lumacode_data_3s_palette3[0:63] = '{ 24'h000000, 24'h000000, 24'h000000, 24'h000000, 24'h000000, 24'h000000, 24'h000000, 24'h000000,
                                        24'h666666, 24'h01247B, 24'h1B1489, 24'h39087C, 24'h520257, 24'h5C0725, 24'h571300, 24'h472300, 24'h2D3300, 24'h0E4000, 24'h004500, 24'h004124, 24'h003456, 24'h000000,
                                        24'hADADAD, 24'h2759C9, 24'h4845DB, 24'h6F34CA, 24'h922B9B, 24'hA1305A, 24'h9B4018, 24'h885400, 24'h686700, 24'h3E7A00, 24'h1B8213, 24'h0D7C57, 24'h136C99, 24'h000000,
                                        24'hFFFFFF, 24'h78ABFF, 24'h9897FF, 24'hC086FF, 24'hE27DEF, 24'hF281AF, 24'hED916D, 24'hDBA43B, 24'hBDB825, 24'h92CB33, 24'h6DD463, 24'h5ECEA8, 24'h65BEEA, 24'h525252,
                                        24'hFFFFFF, 24'hCADBFF, 24'hD8D2FF, 24'hE7CCFF, 24'hF4C9F9, 24'hFACBDF, 24'hF7D2C4, 24'hEEDAAF, 24'hE1E3A5, 24'hD0EBAB, 24'hC2EEBF, 24'hBDEBDB, 24'hC0E4F7, 24'hB8B8B8};
// FCEUX					
wire [23:0] lumacode_data_3s_palette4[0:63] = '{ 24'h000000, 24'h000000, 24'h000000, 24'h000000, 24'h000000, 24'h000000, 24'h000000, 24'h000000,
                                        24'h747474, 24'h24188C, 24'h0000A8, 24'h44009C, 24'h8C0074, 24'hA80010, 24'hA40000, 24'h7C0800, 24'h402C00, 24'h004400, 24'h005000, 24'h003C14, 24'h183C5C, 24'h000000,
                                        24'hBCBCBC, 24'h0070EC, 24'h2038EC, 24'h8000F0, 24'hBC00BC, 24'hE40058, 24'hD82800, 24'hC84C0C, 24'h887000, 24'h009400, 24'h00A800, 24'h009038, 24'h008088, 24'h000000,
                                        24'hFCFCFC, 24'h3CBCFC, 24'h5C94FC, 24'hCC88FC, 24'hF478FC, 24'hFC74B4, 24'hFC7460, 24'hFC9838, 24'hF0BC3C, 24'h80D010, 24'h4CDC48, 24'h58F898, 24'h00E8D8, 24'h787878,
                                        24'hFCFCFC, 24'hA8E4FC, 24'hC4D4FC, 24'hD4C8FC, 24'hFCC4FC, 24'hFCC4D8, 24'hFCBCB0, 24'hFCD8A8, 24'hFCE4A0, 24'hE0FCA0, 24'hA8F0BC, 24'hB0FCCC, 24'h9CFCF0, 24'hC4C4C4};

// Lumacode palette Atari GTIA
wire [23:0] lumacode_data_gtia[0:255] = '{
24'h000000, 24'h111111, 24'h222222, 24'h333333, 24'h444444, 24'h555555, 24'h666666, 24'h777777, 24'h888888, 24'h999999, 24'haaaaaa, 24'hbbbbbb, 24'hcccccc, 24'hdddddd, 24'heeeeee, 24'hffffff,
24'h091900, 24'h192806, 24'h29370d, 24'h3a4714, 24'h4a561b, 24'h5a6522, 24'h6b7529, 24'h7b8430, 24'h8c9336, 24'h9ca33d, 24'hacb244, 24'hbdc14b, 24'hcdd152, 24'hdee059, 24'heeef60, 24'hffff67,
24'h300000, 24'h3d1108, 24'h4b2211, 24'h593319, 24'h674422, 24'h75552a, 24'h826633, 24'h90773b, 24'h9e8844, 24'hac994c, 24'hbaaa55, 24'hc7bb5d, 24'hd5cc66, 24'he3dd6e, 24'hf1ee77, 24'hffff80,
24'h4b0000, 24'h570f0c, 24'h631e18, 24'h6f2e24, 24'h7a3d30, 24'h874d3c, 24'h935c49, 24'h9f6b55, 24'hab7b61, 24'hb68a6d, 24'hc39a79, 24'hcfa986, 24'hdbb892, 24'he6c89e, 24'hf3d7aa, 24'hffe7b7,
24'h550000, 24'h600e10, 24'h6b1c21, 24'h772a32, 24'h823843, 24'h8d4654, 24'h995465, 24'ha46276, 24'haf7187, 24'hbb7f98, 24'hc68da9, 24'hd19bba, 24'hdda9cb, 24'he8b7dc, 24'hf3c5ed, 24'hffd4fe,
24'h4c0047, 24'h570d53, 24'h631b5f, 24'h6f286b, 24'h7b3678, 24'h874384, 24'h935190, 24'h9f5e9c, 24'hab6ca9, 24'hb779b5, 24'hc387c1, 24'hcf94cd, 24'hdba2da, 24'he7afe6, 24'hf3bdf2, 24'hffcbff,
24'h30007e, 24'h3b0b85, 24'h49198d, 24'h572796, 24'h65349f, 24'h7242a7, 24'h8050b0, 24'h8e5db8, 24'h9c6bc1, 24'ha979c9, 24'hb786d2, 24'hc594db, 24'hd3a2e3, 24'he0afec, 24'heebdf4, 24'hfccbfd,
24'h0a0097, 24'h1a0e9d, 24'h2a1da4, 24'h3b2cab, 24'h4b3ab2, 24'h5b49b9, 24'h6c58c0, 24'h7c67c7, 24'h8c75ce, 24'h9c84d5, 24'had93dc, 24'hbda2e3, 24'hceb0ea, 24'hdebff1, 24'heecef8, 24'hffddff,
24'h00008e, 24'h0c0d94, 24'h1b1e9c, 24'h2a2ea3, 24'h393eab, 24'h484eb2, 24'h575eba, 24'h666ec1, 24'h747ec9, 24'h838fd0, 24'h929fd8, 24'ha1afdf, 24'hb0bfe6, 24'hbfcfee, 24'hcedff5, 24'hddeffd,
24'h000e64, 24'h0c1e6e, 24'h192e78, 24'h263e83, 24'h324e8d, 24'h3f5e97, 24'h4c6ea2, 24'h587eac, 24'h658eb6, 24'h729ec1, 24'h7eaecb, 24'h8bbed5, 24'h98cee0, 24'ha4deea, 24'hb1eef4, 24'hbeffff,
24'h002422, 24'h09302e, 24'h153f3d, 24'h204d4c, 24'h2c5c5a, 24'h376a69, 24'h427978, 24'h4e8786, 24'h599695, 24'h65a4a4, 24'h70b3b2, 24'h7cc1c1, 24'h87d0d0, 24'h92dfde, 24'h9eeded, 24'ha9fcfc,
24'h003200, 24'h0b3f0e, 24'h164d1c, 24'h225b2b, 24'h2d6839, 24'h397648, 24'h448456, 24'h509164, 24'h5b9f73, 24'h67ad81, 24'h72ba90, 24'h7ec89e, 24'h89d6ac, 24'h95e3bb, 24'ha0f1c9, 24'hacffd8,
24'h003400, 24'h0c410a, 24'h194f14, 24'h265c1e, 24'h336a28, 24'h407732, 24'h4c853c, 24'h599246, 24'h66a050, 24'h73ad5a, 24'h80bb64, 24'h8cc86e, 24'h99d678, 24'ha6e382, 24'hb3f18c, 24'hc0ff97,
24'h002a00, 24'h0f3807, 24'h1e460e, 24'h2d5416, 24'h3c621d, 24'h4b7124, 24'h5a7f2c, 24'h698d33, 24'h799b3b, 24'h88a942, 24'h97b849, 24'ha6c651, 24'hb5d458, 24'hc4e260, 24'hd3f067, 24'he3ff6f,
24'h0d1700, 24'h1d2606, 24'h2d350d, 24'h3d4514, 24'h4d541b, 24'h5d6422, 24'h6d7329, 24'h7d8330, 24'h8e9237, 24'h9ea23e, 24'haeb145, 24'hbec14c, 24'hced053, 24'hdee05a, 24'heeef61, 24'hffff68,
24'h330000, 24'h401008, 24'h4e2111, 24'h5b321a, 24'h694323, 24'h77542c, 24'h846535, 24'h92763e, 24'h9f8646, 24'had974f, 24'hbba858, 24'hc8b961, 24'hd6ca6a, 24'he3db73, 24'hf1ec7c, 24'hfffd85};

// Lumacode palette Atari CTIA/TIA
wire [23:0] lumacode_data_ctia[0:127] = '{
24'h000000, 24'h404040, 24'h6C6C6C, 24'h909090, 24'hB0B0B0, 24'hC8C8C8, 24'hDCDCDC, 24'hECECEC,
24'h444400, 24'h646410, 24'h848424, 24'hA0A034, 24'hB8B840, 24'hD0D050, 24'hE8E85C, 24'hFCFC68,
24'h702800, 24'h844414, 24'h985C28, 24'hAC783C, 24'hBC8C4C, 24'hCCA05C, 24'hDCB468, 24'hECC878,
24'h841800, 24'h983418, 24'hAC5030, 24'hC06848, 24'hD0805C, 24'hE09470, 24'hECA880, 24'hFCBC94,
24'h880000, 24'h9C2020, 24'hB03C3C, 24'hC05858, 24'hD07070, 24'hE08888, 24'hECA0A0, 24'hFCB4B4,
24'h78005C, 24'h8C2074, 24'hA03C88, 24'hB0589C, 24'hC070B0, 24'hD084C0, 24'hDC9CD0, 24'hECB0E0,
24'h480078, 24'h602090, 24'h783CA4, 24'h8C58B8, 24'hA070CC, 24'hB484DC, 24'hC49CEC, 24'hD4B0FC,
24'h140084, 24'h302098, 24'h4C3CAC, 24'h6858C0, 24'h7C70D0, 24'h9488E0, 24'hA8A0EC, 24'hBCB4FC,
24'h000088, 24'h1C209C, 24'h3840B0, 24'h505CC0, 24'h6874D0, 24'h7C8CE0, 24'h90A4EC, 24'hA4B8FC,
24'h00187C, 24'h1C3890, 24'h3854A8, 24'h5070BC, 24'h6888CC, 24'h7C9CDC, 24'h90B4EC, 24'hA4C8FC,
24'h002C5C, 24'h1C4C78, 24'h386890, 24'h5084AC, 24'h689CC0, 24'h7CB4D4, 24'h90CCE8, 24'hA4E0FC,
24'h003C2C, 24'h1C5C48, 24'h387C64, 24'h509C80, 24'h68B494, 24'h7CD0AC, 24'h90E4C0, 24'hA4FCD4,
24'h003C00, 24'h205C20, 24'h407C40, 24'h5C9C5C, 24'h74B474, 24'h8CD08C, 24'hA4E4A4, 24'hB8FCB8,
24'h143800, 24'h345C1C, 24'h507C38, 24'h6C9850, 24'h84B468, 24'h9CCC7C, 24'hB4E490, 24'hC8FCA4,
24'h2C3000, 24'h4C501C, 24'h687034, 24'h848C4C, 24'h9CA864, 24'hB4C078, 24'hCCD488, 24'hE0EC9C,
24'h442800, 24'h644818, 24'h846830, 24'hA08444, 24'hB89C58, 24'hD0B46C, 24'hE8CC7C, 24'hFCE08C};

// SOF position for scaler
wire [10:0] V_SOF_LINE = hv_in_config3[23:13];

function [7:0] apply_reverse_lpf;
    input [7:0] data_prev;
    input signed [14:0] diff;
    reg signed [10:0] result;

    begin
        result = {3'b0,data_prev} + ~diff[14:4]; // allow for a small error to reduce adder length
        apply_reverse_lpf = result[10] ? 8'h00 : |result[9:8] ? 8'hFF : result[7:0];
    end
endfunction


// Pipeline stage 1
always @(posedge PCLK_i) begin
    R_pp[1] <= R_i;
    G_pp[1] <= G_i;
    B_pp[1] <= B_i;

    HS_i_prev <= HS_i;
    VS_i_np_prev <= VS_i_np;

    if (HS_i_prev & ~HS_i) begin
        h_cnt <= 0;
        h_ctr <= 0;
        HSYNC_pp[1] <= 1'b0;

        if (fid_next_ctr > 0)
            fid_next_ctr <= fid_next_ctr - 1'b1;

        if (fid_next_ctr == 2'h1) begin
            // regenerated output timings start lagging by one scanline due to vsync detection,
            // compensate by starting v_cnt from 1 (effectively reduces V_SYNCLEN by 1)
            v_cnt <= 1;
            if (~(interlace_flag & (fid_next == FID_EVEN))) begin
                vmax_cnt <= 0;
                //vtotal <= vmax_cnt + 1'b1;
                frame_change <= 1'b1;
            end else begin
                vmax_cnt <= vmax_cnt + 1'b1;
            end
        end else begin
            v_cnt <= v_cnt + 1'b1;
            vmax_cnt <= vmax_cnt + 1'b1;
            frame_change <= 1'b0;
        end

        sof_scaler <= (vmax_cnt == V_SOF_LINE);
    end else begin
        if (h_ctr == H_SKIP) begin
            h_cnt <= h_cnt + 1'b1;
            h_ctr <= 0;
            if (h_cnt == H_SYNCLEN-1)
                HSYNC_pp[1] <= 1'b1;
        end else begin
            h_ctr <= h_ctr + 1'b1;
        end
    end

    // vsync leading edge processing per quadrant
    if (VS_i_np_prev & ~VS_i_np) begin
        if ((HS_i_prev & ~HS_i) | (h_cnt_ref < even_min_thold)) begin
            fid_next <= FID_ODD;
            fid_next_ctr <= 2'h1;
        end else if ((h_cnt_ref > even_max_thold) | ~interlace_flag) begin
            fid_next <= FID_ODD;
            fid_next_ctr <= 2'h2;
        end else begin
            fid_next <= FID_EVEN;
            fid_next_ctr <= 2'h2;
        end
    end

    // record starting position of csync leading edge for later FID detection
    if (sogref_update_i) begin
        h_cnt_sogref <= (h_cnt > even_max_thold) ? 0 : h_cnt;
    end

    if (((fid_next == FID_ODD) & (HS_i_prev & ~HS_i)) | ((fid_next == FID_EVEN) & (h_cnt == (H_TOTAL/2)-1'b1))) begin
        if (fid_next_ctr == 2'h1) begin
            VSYNC_pp[1] <= 1'b0;
            FID_pp[1] <= fid_next;
            //interlace_flag <= FID_pp[1] ^ fid_next;
        end else begin
            if (v_cnt == V_SYNCLEN-1)
                VSYNC_pp[1] <= 1'b1;
        end
    end
end

// Pipeline stage 2
always @(posedge PCLK_i) begin
    // Lumacode sample aggregation
    if (h_ctr == H_SAMPLE_SEL) begin
        lc_code[1] <= G_pp[1][7:6];
        lc_cnt <= 0;
        lc_ctr <= 0;
        lc_atari_ctr <= (h_cnt == 0) ? 0 : lc_atari_ctr ^ 1'b1;
    end else if (lc_ctr == LC_H_SKIP) begin
        lc_code[2+lc_cnt] <= G_pp[1][7:6];
        lc_cnt <= lc_cnt + 1;
        lc_ctr <= 0; 
    end else begin
        lc_ctr <= lc_ctr + 1;
    end

    // Standard output
    if (MISC_LUMACODE_MODE == '0) begin
        {R_pp[2], G_pp[2], B_pp[2]} <= {R_pp[1], G_pp[1], B_pp[1]};
    // Lumacode C64, C128, VIC20, Spectrum, TMS99xxA
    end else if (MISC_LUMACODE_MODE <= 3) begin
        {R_pp[2], G_pp[2], B_pp[2]} <= lumacode_data_2s[MISC_LUMACODE_MODE-1'b1][{lc_code[1], lc_code[2]}];
    // Lumacode NES
    end else if (MISC_LUMACODE_MODE == 4) begin
        reg [7:0] nes_r, nes_g, nes_b;
        case(MISC_LUMACODE_NES_PALETTE)
            2'b00: begin // Default
                nes_r = lumacode_data_3s_default[{lc_code[1], lc_code[2], lc_code[3]}][23:16];
                nes_g = lumacode_data_3s_default[{lc_code[1], lc_code[2], lc_code[3]}][15:8];
                nes_b = lumacode_data_3s_default[{lc_code[1], lc_code[2], lc_code[3]}][7:0];
            end
            2'b01: begin // Firebrand X smooth
                nes_r = lumacode_data_3s_palette2[{lc_code[1], lc_code[2], lc_code[3]}][23:16];
                nes_g = lumacode_data_3s_palette2[{lc_code[1], lc_code[2], lc_code[3]}][15:8];
                nes_b = lumacode_data_3s_palette2[{lc_code[1], lc_code[2], lc_code[3]}][7:0];
            end
            2'b10: begin // Kitrinx34
                nes_r = lumacode_data_3s_palette3[{lc_code[1], lc_code[2], lc_code[3]}][23:16];
                nes_g = lumacode_data_3s_palette3[{lc_code[1], lc_code[2], lc_code[3]}][15:8];
                nes_b = lumacode_data_3s_palette3[{lc_code[1], lc_code[2], lc_code[3]}][7:0];
            end
            2'b11: begin // FCEUX
                nes_r = lumacode_data_3s_palette4[{lc_code[1], lc_code[2], lc_code[3]}][23:16];
                nes_g = lumacode_data_3s_palette4[{lc_code[1], lc_code[2], lc_code[3]}][15:8];
                nes_b = lumacode_data_3s_palette4[{lc_code[1], lc_code[2], lc_code[3]}][7:0];
            end
        endcase
    
        if (lc_emp_nes[1] & lc_emp_nes[0])
            R_pp[2] <= nes_r/2;
        else if (lc_emp_nes[1] | lc_emp_nes[0])
            R_pp[2] <= nes_r - nes_r/4;
        else
            R_pp[2] <= nes_r;

        if (lc_emp_nes[2] & lc_emp_nes[0])
            G_pp[2] <= nes_g/2;
        else if (lc_emp_nes[2] | lc_emp_nes[0])
            G_pp[2] <= nes_g - nes_g/4;
        else
            G_pp[2] <= nes_g;

        if (lc_emp_nes[2] & lc_emp_nes[1])
            B_pp[2] <= nes_b/2;
        else if (lc_emp_nes[2] | lc_emp_nes[1])
            B_pp[2] <= nes_b - nes_b/4;
        else
            B_pp[2] <= nes_b;        

        if ((h_ctr == H_SAMPLE_SEL) & ({lc_code[1], lc_code[2], lc_code[3]} < 8))
            lc_emp_nes <= {lc_code[2][0], lc_code[3]};
    // Lumacode Atari GTIA
    end else if (MISC_LUMACODE_MODE == 5) begin
        if (h_ctr == H_SAMPLE_SEL) begin
            if (lc_atari_ctr) begin
                // Store hue and luma (high bits) for 1st pixel, and display last pixel of previous pair
                lc_atari_hue <= {lc_code[1], lc_code[2]};
                lc_atari_luma[3:2] <= lc_code[3];
                {R_pp[2], G_pp[2], B_pp[2]} <= lumacode_data_gtia[{lc_atari_hue, lc_atari_luma}];
            end else begin
                // Store luma for 2nd pixel, and display first pixel of current pair
                lc_atari_luma <= {lc_code[2], lc_code[3]};
                {R_pp[2], G_pp[2], B_pp[2]} <= lumacode_data_gtia[{lc_atari_hue, lc_atari_luma[3:2], lc_code[1]}];
            end
        end
    // Lumacode Atari VCS
    end else begin
        if (h_ctr == H_SAMPLE_SEL) begin
            if (lc_atari_ctr) begin
                // Store first 2 lumacode samples (hue) from double-sampled input (160col->320col)
                lc_atari_hue <= {lc_code[1], lc_code[2]};
            end else begin
                // Display pixel after receiving remaining 2 lumacode samples (luma)
                {R_pp[2], G_pp[2], B_pp[2]} <= lumacode_data_ctia[{lc_atari_hue, lc_code[1], lc_code[2][1]}];
            end
        end
    end

    HSYNC_pp[2] <= HSYNC_pp[1];
    VSYNC_pp[2] <= VSYNC_pp[1];
    FID_pp[2] <= FID_pp[1];
    DE_pp[2] <= (h_cnt >= H_SYNCLEN+H_BACKPORCH) & (h_cnt < H_SYNCLEN+H_BACKPORCH+H_ACTIVE) & (v_cnt >= V_SYNCLEN+V_BACKPORCH) & (v_cnt < V_SYNCLEN+V_BACKPORCH+V_ACTIVE);
    datavalid_pp[2] <= (h_ctr == H_SAMPLE_SEL);
    xpos_pp[2] <= (h_cnt-H_SYNCLEN-H_BACKPORCH);
    ypos_pp[2] <= (v_cnt-V_SYNCLEN-V_BACKPORCH);
end

// Pipeline stages 3-
integer pp_idx;
always @(posedge PCLK_i) begin
    for(pp_idx = PP_RLPF_START+1; pp_idx <= PP_PL_END; pp_idx = pp_idx+1) begin
        R_pp[pp_idx] <= R_pp[pp_idx-1];
        G_pp[pp_idx] <= G_pp[pp_idx-1];
        B_pp[pp_idx] <= B_pp[pp_idx-1];
        HSYNC_pp[pp_idx] <= HSYNC_pp[pp_idx-1];
        VSYNC_pp[pp_idx] <= VSYNC_pp[pp_idx-1];
        FID_pp[pp_idx] <= FID_pp[pp_idx-1];
        DE_pp[pp_idx] <= DE_pp[pp_idx-1];
        datavalid_pp[pp_idx] <= datavalid_pp[pp_idx-1];
        xpos_pp[pp_idx] <= xpos_pp[pp_idx-1];
        ypos_pp[pp_idx] <= ypos_pp[pp_idx-1];
    end

    /* ---------- Reverse LPF (3 cycles) ---------- */
    // Store a copy of valid sample data
    if (datavalid_pp[PP_RLPF_START]) begin
        R_pp_prev_rlpf <= R_pp[PP_RLPF_START];
        G_pp_prev_rlpf <= G_pp[PP_RLPF_START];
        B_pp_prev_rlpf <= B_pp[PP_RLPF_START];
    end
    // Push previous valid data into pipeline when RLPF enabled
    if (MISC_REV_LPF_ENABLE) begin
        R_pp[PP_RLPF_START+1] <= R_pp_prev_rlpf;
        G_pp[PP_RLPF_START+1] <= G_pp_prev_rlpf;
        B_pp[PP_RLPF_START+1] <= B_pp_prev_rlpf;
    end
    // Calculate diff to previous valid data
    R_diff_s15_pre <= (R_pp_prev_rlpf - R_pp[PP_RLPF_START]);
    G_diff_s15_pre <= (G_pp_prev_rlpf - G_pp[PP_RLPF_START]);
    B_diff_s15_pre <= (B_pp_prev_rlpf - B_pp[PP_RLPF_START]);

    // Cycle 2
    R_diff_s15 <= (R_diff_s15_pre * MISC_REV_LPF_STR);
    G_diff_s15 <= (G_diff_s15_pre * MISC_REV_LPF_STR);
    B_diff_s15 <= (B_diff_s15_pre * MISC_REV_LPF_STR);

    // Cycle 3
    if (MISC_REV_LPF_ENABLE) begin
        R_pp[PP_RLPF_END] <= apply_reverse_lpf(R_pp[PP_RLPF_START+2], R_diff_s15);
        G_pp[PP_RLPF_END] <= apply_reverse_lpf(G_pp[PP_RLPF_START+2], G_diff_s15);
        B_pp[PP_RLPF_END] <= apply_reverse_lpf(B_pp[PP_RLPF_START+2], B_diff_s15);
    end
end

// Output
assign R_o = R_pp[PP_PL_END];
assign G_o = G_pp[PP_PL_END];
assign B_o = B_pp[PP_PL_END];
assign HSYNC_o = HSYNC_pp[PP_PL_END];
assign VSYNC_o = VSYNC_pp[PP_PL_END];
assign FID_o = FID_pp[PP_PL_END];
assign DE_o = DE_pp[PP_PL_END];
assign datavalid_o = datavalid_pp[PP_PL_END];
assign xpos_o = xpos_pp[PP_PL_END];
assign ypos_o = ypos_pp[PP_PL_END];


// Calculate horizontal and vertical counts
always @(posedge CLK_MEAS_i) begin
    if ((VSYNC_i_np_prev & ~VSYNC_i_np) & (~interlace_flag | (meas_fid == FID_EVEN))) begin
        pcnt_frame_ctr <= 1;
        pcnt_line_stored <= 1'b0;
        if (sync_active & (pcnt_frame_ctr != '1))
            pcnt_field <= interlace_flag ? (pcnt_frame_ctr>>1) : pcnt_frame_ctr[19:0];
    end else if (pcnt_frame_ctr < '1) begin
        pcnt_frame_ctr <= pcnt_frame_ctr + 1'b1;
    end else begin
        pcnt_field <= 0;
    end

    if (HSYNC_i_np_prev & ~HSYNC_i_np) begin
        pcnt_line_ctr <= 1;
        hs_ctr <= 1;

        // store count 1ms after vsync
        if (~pcnt_line_stored & (pcnt_frame_ctr > 21'd27000)) begin
            pcnt_line <= pcnt_line_ctr;
            hsync_width <= hs_ctr;
            pcnt_line_stored <= 1'b1;
        end
    end else begin
        pcnt_line_ctr <= pcnt_line_ctr + 1'b1;
        if (~HSYNC_i_np)
            hs_ctr <= hs_ctr + 1'b1;
    end

    HSYNC_i_np_prev <= HSYNC_i_np;
    VSYNC_i_np_prev <= VSYNC_i_np;
end

// Detect sync polarities and activity during ~10ms interval
always @(posedge CLK_MEAS_i) begin
    if (syncpol_det_ctr == 0) begin
        hsync_i_pol <= (hsync_hpol_ctr > 18'h1ffff);
        vsync_i_pol <= (vsync_hpol_ctr > 18'h1ffff);

        hsync_hpol_ctr <= 0;
        vsync_hpol_ctr <= 0;

        if ((vsync_hpol_ctr == '0) | (vsync_hpol_ctr == '1)) begin
            // If vsync has been stale for ~100ms, clear activity flag
            if (sync_inactive_ctr == '1)
                sync_active <= 1'b0;
            else
                sync_inactive_ctr <= sync_inactive_ctr + 1'b1;
        end else begin
            sync_inactive_ctr <= 0;
            sync_active <= 1'b1;
        end
    end else begin
        if (HSYNC_i)
            hsync_hpol_ctr <= hsync_hpol_ctr + 1'b1;
        if (VSYNC_i)
            vsync_hpol_ctr <= vsync_hpol_ctr + 1'b1;
    end

    syncpol_det_ctr <= syncpol_det_ctr + 1'b1;
end

// Detect interlace and line count
always @(posedge CLK_MEAS_i) begin
    if ((HSYNC_i_np_prev & ~HSYNC_i_np) & (meas_h_cnt > glitch_filt_thold)) begin
        // detect half-line equalization pulses
        if ((meas_h_cnt > ((pcnt_line/2) - (pcnt_line/4))) && (meas_h_cnt < ((pcnt_line/2) + (pcnt_line/4)))) begin
            /*if (meas_hl_det) begin
                meas_hl_det <= 1'b0;
                meas_h_cnt <= 0;
                meas_v_cnt <= meas_v_cnt + 1'b1;
            end else begin*/
                meas_hl_det <= 1'b1;
                meas_h_cnt <= meas_h_cnt + 1'b1;
            //end
        end else begin
            meas_hl_det <= 1'b0;
            meas_h_cnt <= 0;
            meas_v_cnt <= meas_v_cnt + 1'b1;
        end
        meas_h_cnt_sogref <= meas_h_cnt;
    end else if (meas_vblank_region & (meas_h_cnt >= pcnt_line+4)) begin
        // hsync may be missing or irregular during vblank, force line change detect if pcnt_line is exceeded
        meas_hl_det <= 1'b0;
        meas_h_cnt <= 0;
        meas_v_cnt <= meas_v_cnt + 1'b1;
    end else begin
        meas_h_cnt <= meas_h_cnt + 1'b1;
    end

    if (VSYNC_i_np_prev & ~VSYNC_i_np) begin
        if ((meas_h_cnt_ref < meas_even_min_thold) | (meas_h_cnt_ref > meas_even_max_thold)) begin
            meas_fid <= FID_ODD;
            interlace_flag <= (meas_fid == FID_EVEN);

            if (vsync_i_type == VSYNC_RAW) begin
                // vsync leading edge may occur at hsync edge or either side of it
                if ((HSYNC_i_np_prev & ~HSYNC_i_np) | (meas_h_cnt >= pcnt_line)) begin
                    meas_v_cnt <= 1;
                    vtotal <= meas_v_cnt;
                end else if (meas_h_cnt < meas_even_min_thold) begin
                    meas_v_cnt <= 1;
                    vtotal <= meas_v_cnt - 1'b1;
                end else begin
                    meas_v_cnt <= 0;
                    vtotal <= meas_v_cnt;
                end
            end else begin
                meas_v_cnt <= 0;
                vtotal <= meas_v_cnt;
            end
        end else begin
            meas_fid <= FID_EVEN;
            interlace_flag <= (meas_fid == FID_ODD);
            if (meas_fid == FID_EVEN) begin
                meas_v_cnt <= 0;
                vtotal <= meas_v_cnt;
            end
        end
    end
end

endmodule
