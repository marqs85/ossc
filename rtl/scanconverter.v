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

`include "lat_tester_includes.v"

`define TRUE                    1'b1
`define FALSE                   1'b0
`define HI                      1'b1
`define LO                      1'b0

`define HSYNC_POL               `LO
`define VSYNC_POL               `LO

`define V_MULTMODE_1X           3'd0
`define V_MULTMODE_2X           3'd1
`define V_MULTMODE_3X           3'd2
`define V_MULTMODE_4X           3'd3
`define V_MULTMODE_5X           3'd4

`define PCLK_MUX_1X             2'd0
`define PCLK_MUX_2X             2'd2
`define PCLK_MUX_3X             2'd2
`define PCLK_MUX_4X             2'd3
`define PCLK_MUX_5X             2'd3

`define H_MULTMODE_FULLWIDTH    2'h0
`define H_MULTMODE_ASPECTFIX    2'h1
`define H_MULTMODE_OPTIMIZED    2'h2
`define H_MULTMODE_OPTIMIZED_1X 2'h3

`define SCANLINES_HYBR_CONTR_LOW  2'h1
`define SCANLINES_HYBR_CONTR_MED  2'h2
`define SCANLINES_HYBR_CONTR_HIGH 2'h3

`define VSYNCGEN_LEN            6
`define VSYNCGEN_GENMID_BIT     0
`define VSYNCGEN_CHOPMID_BIT    1

`define FID_EVEN                1'b0
`define FID_ODD                 1'b1

`define MIN_VALID_LINES         256     //power of 2 optimization -> ignore lower bits with comparison
`define DBLFRAME_THOLD          5
`define FALSE_FIELD             (fpga_vsyncgen[`VSYNCGEN_CHOPMID_BIT] & (FID_in == `FID_ODD))

`define HSYNC_LEADING_EDGE      ((HSYNC_in_L == `HI) & (HSYNC_in == `LO))
`define VSYNC_LEADING_EDGE      ((VSYNC_in_L == `HI) & (VSYNC_in == `LO))

`define PP_PL_START             1
`define PP_HS_VS_DE_START       2
`define PP_ENABLES_START        2
`define PP_RGB_START            4

//`define PP_RLPF_PL_START_EARLY      // set if start with 2
`define PP_RLPF_PL_START        `PP_RGB_START   // minimum 2
`define PP_RLPF_PL_LENGTH       3   // counted from aquisition
`define PP_SLGEN_PL_LENGTH      5
`define PP_LT_BORDER_GEN_LENGTH 1   // lt_box / border_mask gen

`define PP_RLPF_PL_END          (`PP_RLPF_PL_START+`PP_RLPF_PL_LENGTH)
`define PP_SLGEN_PL_END         (`PP_RLPF_PL_END+`PP_SLGEN_PL_LENGTH)
`define PP_PIPELINE_LENGTH      (`PP_SLGEN_PL_END+`PP_LT_BORDER_GEN_LENGTH-1'b1)

module scanconverter (
    input reset_n,
    input [7:0] R_in,
    input [7:0] G_in,
    input [7:0] B_in,
    input FID_in,
    input VSYNC_in,
    input HSYNC_in,
    input PCLK_in,
    input clk27,
    input enable_sc,
    input [31:0] h_config,
    input [31:0] h_config2,
    input [31:0] v_config,
    input [31:0] misc_config,
    input [31:0] sl_config,
    input [31:0] sl_config2,
    output PCLK_out,
    output reg [7:0] R_out,
    output reg [7:0] G_out,
    output reg [7:0] B_out,
    output reg HSYNC_out,
    output reg VSYNC_out,
    output reg DE_out,
    output h_unstable,
    output reg [1:0] fpga_vsyncgen,
    output pll_lock_lost,
    output reg [10:0] vmax,
    output reg [10:0] vmax_tvp,
    output reg [19:0] pcnt_frame,
    output ilace_flag,
    output vsync_flag,
    input lt_active,
    input [1:0] lt_mode,
    output reg [10:0] xpos,
    output reg [10:0] ypos,
    input pll_areset,
    input pll_scanclk,
    input pll_scanclkena,
    input pll_configupdate,
    input pll_scandata,
    output pll_scandone
);

//clock-related signals
wire pclk_act;
wire pclk_1x, pclk_2x, pclk_3x, pclk_4x, pclk_5x;
wire [1:0] pclk_mux_sel;
wire pll_lock;

//RGB signals&registers: 8 bits per component -> 16.7M colors
wire [7:0] R_act, G_act, B_act;
wire [7:0] R_lbuf, G_lbuf, B_lbuf;
reg [7:0] R_in_L, G_in_L, B_in_L, R_in_LL, G_in_LL, B_in_LL, R_in_LLL, G_in_LLL, B_in_LLL, R_1x, G_1x, B_1x;

//H+V syncs + data enable signals&registers
wire HSYNC_act, VSYNC_act, DE_act;
reg HSYNC_in_L, VSYNC_in_L;
reg HSYNC_1x, HSYNC_2x, HSYNC_3x, HSYNC_4x, HSYNC_5x;
reg VSYNC_1x, VSYNC_2x, VSYNC_3x, VSYNC_4x, VSYNC_5x;
reg DE_1x, DE_2x, DE_3x, DE_4x, DE_5x, DE_3x_prev4x;

//registers indicating line/frame change and field type
reg FID_cur, FID_last, FID_prev, FID_1x;
reg frame_change, frame_change_longpulse, line_change;

//H+V counters
reg [11:0] linebuf_hoffset_pp; //Offset for line (max. 2047 pixels), MSB indicates which line is read/written
wire [11:0] linebuf_hoffset_act;
wire [11:0] hcnt_act;
reg [11:0] hcnt_1x, hcnt_2x, hcnt_3x, hcnt_4x, hcnt_5x, hcnt_4x_aspfix, hcnt_2x_opt, hcnt_3x_opt, hcnt_4x_opt, hcnt_5x_opt, hcnt_5x_hscomp;
reg [2:0] hcnt_2x_opt_ctr, hcnt_3x_opt_ctr, hcnt_4x_opt_ctr, hcnt_5x_opt_ctr;
wire [10:0] vcnt_act;
reg [10:0] vcnt_tvp, vcnt_1x, vcnt_2x, vcnt_3x, vcnt_4x, vcnt_5x; //max. 2047

//other counters
wire [2:0] line_id_act, col_id_act;
reg [11:0] hmax[0:1];
reg line_idx;
reg [1:0] line_out_idx_2x, line_out_idx_3x, line_out_idx_4x;
reg [2:0] line_out_idx_5x;
reg [23:0] warn_h_unstable, warn_pll_lock_lost;

// post-processing pipeline
reg HSYNC_pp[1:`PP_PIPELINE_LENGTH] /* synthesis ramstyle = "logic" */;
reg VSYNC_pp[1:`PP_PIPELINE_LENGTH] /* synthesis ramstyle = "logic" */;
reg DE_pp[1:`PP_PIPELINE_LENGTH] /* synthesis ramstyle = "logic" */;
reg [7:0] R_pp[3:`PP_PIPELINE_LENGTH], G_pp[3:`PP_PIPELINE_LENGTH], B_pp[3:`PP_PIPELINE_LENGTH] /* synthesis ramstyle = "logic" */;
reg [11:0] hcnt_pp /* synthesis ramstyle = "logic" */;
reg [10:0] vcnt_pp /* synthesis ramstyle = "logic" */;
reg  rlpf_trigger_r[1:`PP_RLPF_PL_START-1] /* synthesis ramstyle = "logic" */;
reg [7:0] R_prev_pp[`PP_RLPF_PL_START:`PP_RLPF_PL_END-1], G_prev_pp[`PP_RLPF_PL_START:`PP_RLPF_PL_END-1], B_prev_pp[`PP_RLPF_PL_START:`PP_RLPF_PL_END-1] /* synthesis ramstyle = "logic" */;
reg [2:0] line_id_pp[1:`PP_SLGEN_PL_END-5], col_id_pp[1:`PP_SLGEN_PL_END-5] /* synthesis ramstyle = "logic" */;
reg draw_sl_pp[`PP_SLGEN_PL_END-4:`PP_SLGEN_PL_END-1] /* synthesis ramstyle = "logic" */;
reg border_enable_pp[2:`PP_PIPELINE_LENGTH] /* synthesis ramstyle = "logic" */;
reg lt_box_enable_pp[2:`PP_PIPELINE_LENGTH] /* synthesis ramstyle = "logic" */;

//helper registers for sampling at synchronized clock edges
reg pclk_1x_prev3x;
reg [1:0] pclk_3x_cnt;
reg pclk_1x_prev4x;
reg [1:0] pclk_4x_cnt;
reg pclk_1x_prev5x;
reg pclk_1x_prevprev5x;
reg [2:0] pclk_5x_cnt;

//configuration registers
reg [10:0] H_ACTIVE;    //max. 2047
reg [9:0] H_AVIDSTART;  //max. 1023
reg [10:0] V_ACTIVE;    //max. 2047
reg [6:0] V_AVIDSTART;  //max. 127
reg [7:0] H_SYNCLEN;
reg [2:0] V_SYNCLEN;
reg [5:0] V_MASK;
reg [2:0] V_MULTMODE;
reg [1:0] H_MULTMODE;
reg [10:0] H_MASK;
reg [9:0] H_OPT_STARTOFF;
reg [2:0] H_OPT_SCALE;
reg [2:0] H_OPT_SAMPLE_MULT;
reg [2:0] H_OPT_SAMPLE_SEL;
reg [9:0] H_L5BORDER;
reg [9:0] H_L3BORDER;
reg [6:0] H_L3_OPT_SAMPLE_COMP;
reg [3:0] X_MASK_BR;
reg [2:0] X_MASK_COLOR;
reg [5:0] X_REV_LPF_STR;
reg [3:0] SL_L_STR[4:0] /* synthesis ramstyle = "logic" */;
reg [3:0] SL_C_STR[5:0] /* synthesis ramstyle = "logic" */;
reg [4:0] SL_HYBRSTR;
reg [4:0] SL_L_OVERLAY;
reg [5:0] SL_C_OVERLAY;
reg SL_METHOD;
reg SL_NO_ALTERN;
reg SL_ALTIV;
reg X_REV_LPF_ENABLE;
reg X_PANASONIC_HACK;

// constants for each frame to be calculated off config-registers
reg CALC_CONSTS;
reg [11:0] H_AVIDSTOP;
reg [10:0] V_AVIDSTOP;
reg [10:0] H_AVIDMASK_START;
reg [11:0] H_AVIDMASK_STOP;
reg [7:0] V_AVIDMASK_START;
reg [10:0] V_AVIDMASK_STOP;

reg [11:0] LT_POS_TOPLEFT_BOX_H_STOP;
reg [10:0] LT_POS_TOPLEFT_BOX_V_STOP;
reg [11:0] LT_POS_CENTER_BOX_H_START;
reg [11:0] LT_POS_CENTER_BOX_H_STOP;
reg [10:0] LT_POS_CENTER_BOX_V_START;
reg [10:0] LT_POS_CENTER_BOX_V_STOP;
reg [11:0] LT_POS_BOTTOMRIGHT_H_START;
reg [10:0] LT_POS_BOTTOMRIGHT_V_START;


//clk27 related registers
reg VSYNC_in_cc_L, VSYNC_in_cc_LL, VSYNC_in_cc_LLL;
reg [21:0] clk27_ctr;   // min. 6.5Hz
reg [2:0] dbl_frame_ctr;
reg frame_change_longpulse_cc_L, frame_change_longpulse_cc_LL, frame_change_longpulse_cc_LLL;
reg [19:0] pcnt_ctr;


assign pclk_1x = PCLK_in;
assign PCLK_out = pclk_act;
assign ilace_flag = (FID_cur != FID_last);

//Scanline generation
reg [8:0] Y_rb_tmp;
reg [9:0] Y;
wire [8:0] Y_sl_hybr_ref_pre, R_sl_hybr_ref_pre, G_sl_hybr_ref_pre, B_sl_hybr_ref_pre;
lpm_mult_4_hybr_ref_pre Y_sl_hybr_ref_pre_u
(
    .clock(pclk_act),
    .dataa(Y[9:2]),
    .datab(SL_HYBRSTR),
    .result(Y_sl_hybr_ref_pre)
);
lpm_mult_4_hybr_ref_pre R_sl_hybr_ref_pre_u
(
    .clock(pclk_act),
    .dataa(R_pp[`PP_RLPF_PL_END]),
    .datab(SL_HYBRSTR),
    .result(R_sl_hybr_ref_pre)
);
lpm_mult_4_hybr_ref_pre G_sl_hybr_ref_pre_u
(
    .clock(pclk_act),
    .dataa(G_pp[`PP_RLPF_PL_END]),
    .datab(SL_HYBRSTR),
    .result(G_sl_hybr_ref_pre)
);
lpm_mult_4_hybr_ref_pre B_sl_hybr_ref_pre_u
(
    .clock(pclk_act),
    .dataa(B_pp[`PP_RLPF_PL_END]),
    .datab(SL_HYBRSTR),
    .result(B_sl_hybr_ref_pre)
);

wire [8:0] Y_sl_hybr_ref, R_sl_hybr_ref, G_sl_hybr_ref, B_sl_hybr_ref;
lpm_mult_4_hybr_ref Y_sl_hybr_ref_u
(
    .clock(pclk_act),
    .dataa(Y_sl_hybr_ref_pre),
    .datab(sl_str_tmp),
    .result(Y_sl_hybr_ref)
);
lpm_mult_4_hybr_ref R_sl_hybr_ref_u
(
    .clock(pclk_act),
    .dataa(R_sl_hybr_ref_pre),
    .datab(sl_str_tmp),
    .result(R_sl_hybr_ref)
);
lpm_mult_4_hybr_ref G_sl_hybr_ref_u
(
    .clock(pclk_act),
    .dataa(G_sl_hybr_ref_pre),
    .datab(sl_str_tmp),
    .result(G_sl_hybr_ref)
);
lpm_mult_4_hybr_ref B_sl_hybr_ref_u
(
    .clock(pclk_act),
    .dataa(B_sl_hybr_ref_pre),
    .datab(sl_str_tmp),
    .result(B_sl_hybr_ref)
);

reg [7:0] sl_str, sl_str_tmp, Y_sl_str, R_sl_str, G_sl_str, B_sl_str;

reg [7:0] R_sl_sub, G_sl_sub, B_sl_sub;
wire [7:0] R_sl_mult, G_sl_mult, B_sl_mult;
lpm_mult_4_sl R_sl_mult_u
(
    .clock(pclk_act),
    .dataa(R_pp[`PP_SLGEN_PL_END-2]),
    .datab(~Y_sl_str),
    .result(R_sl_mult)
);
lpm_mult_4_sl G_sl_mult_u
(
    .clock(pclk_act),
    .dataa(G_pp[`PP_SLGEN_PL_END-2]),
    .datab(~Y_sl_str),
    .result(G_sl_mult)
);
lpm_mult_4_sl B_sl_mult_u
(
    .clock(pclk_act),
    .dataa(B_pp[`PP_SLGEN_PL_END-2]),
    .datab(~Y_sl_str),
    .result(B_sl_mult)
);

//Reverse LPF
wire rlpf_trigger_act;
reg signed [14:0] R_diff_s15_pre, G_diff_s15_pre, B_diff_s15_pre, R_diff_s15, G_diff_s15, B_diff_s15;
reg signed [10:0] R_rlpf_result, G_rlpf_result, B_rlpf_result;

function [7:0] apply_reverse_lpf;
    input [7:0] data_prev;
    input signed [14:0] diff;
    reg signed [10:0] result;

    begin
//        result = ({3'b0,data_prev,4'b0} - diff) >>> 4;
        result = {3'b0,data_prev} + ~diff[14:4]; // allow for a small error to reduce adder length
        apply_reverse_lpf = result[10] ? 8'h00 : |result[9:8] ? 8'hFF : result[7:0];
    end
endfunction


//Mux for active data selection
//
//List of critical signals:
// [RGB]_act, DE_act, HSYNC_act, VSYNC_act
//
//Non-critical signals and inactive clock combinations filtered out in SDC
always @(*)
case (V_MULTMODE)
    default: begin //`V_MULTMODE_1X
        R_act = R_1x;
        G_act = G_1x;
        B_act = B_1x;
        HSYNC_act = HSYNC_1x;
        VSYNC_act = VSYNC_1x;
        DE_act = DE_1x;
        line_id_act = {2'b00, vcnt_1x[0]};
        hcnt_act = hcnt_1x;
        vcnt_act = vcnt_1x;
        pclk_mux_sel = `PCLK_MUX_1X;
        linebuf_hoffset_act = 0;
        col_id_act = {2'b00, hcnt_1x[0]};
        rlpf_trigger_act = 1'b1;
    end
    `V_MULTMODE_2X: begin
        R_act = R_lbuf;
        G_act = G_lbuf;
        B_act = B_lbuf;
        HSYNC_act = HSYNC_2x;
        VSYNC_act = VSYNC_2x;
        DE_act = DE_2x;
        line_id_act = SL_NO_ALTERN ? {2'b0, {line_out_idx_2x[0]+FID_1x}} : {1'b0, line_out_idx_2x};
        hcnt_act = hcnt_2x;
        vcnt_act = vcnt_2x;
        case (H_MULTMODE)
            default: begin //`H_MULTMODE_FULLWIDTH
                pclk_mux_sel = `PCLK_MUX_2X;
                linebuf_hoffset_act = hcnt_2x;
                col_id_act = {2'b00, hcnt_2x[0]};
                rlpf_trigger_act = 1'b1;
            end
            `H_MULTMODE_OPTIMIZED_1X: begin
                pclk_mux_sel = `PCLK_MUX_1X;     //special case: pclk bypass to enable 2x native sampling
                linebuf_hoffset_act = hcnt_2x_opt;
                col_id_act = {2'b00, hcnt_2x[1]};
                rlpf_trigger_act = (hcnt_2x_opt_ctr == 0);
            end
            `H_MULTMODE_OPTIMIZED: begin
                pclk_mux_sel = `PCLK_MUX_2X;
                linebuf_hoffset_act = hcnt_2x_opt;
                col_id_act = hcnt_2x_opt_ctr;
                rlpf_trigger_act = (hcnt_2x_opt_ctr == 0);
             end
        endcase
    end
    `V_MULTMODE_3X: begin
        R_act = R_lbuf;
        G_act = G_lbuf;
        B_act = B_lbuf;
        HSYNC_act = HSYNC_3x;
        VSYNC_act = VSYNC_3x;
        DE_act = DE_3x;
        line_id_act = {1'b0, line_out_idx_3x};
        vcnt_act = vcnt_3x;
        case (H_MULTMODE)
            default: begin //`H_MULTMODE_FULLWIDTH
                pclk_mux_sel = `PCLK_MUX_3X;
                linebuf_hoffset_act = hcnt_3x;
                hcnt_act = hcnt_3x;
                col_id_act = {2'b00, hcnt_3x[0]};
                rlpf_trigger_act = 1'b1;
            end
            `H_MULTMODE_ASPECTFIX: begin
                pclk_mux_sel = `PCLK_MUX_4X;
                linebuf_hoffset_act = hcnt_4x_aspfix;
                hcnt_act = hcnt_4x_aspfix;
                col_id_act = {2'b00, hcnt_4x[0]};
                rlpf_trigger_act = 1'b1;
            end
            `H_MULTMODE_OPTIMIZED: begin
                pclk_mux_sel = `PCLK_MUX_3X;
                linebuf_hoffset_act = hcnt_3x_opt;
                hcnt_act = hcnt_3x;
                col_id_act = hcnt_3x_opt_ctr;
                rlpf_trigger_act = (hcnt_3x_opt_ctr == 0);
            end
        endcase
    end
    `V_MULTMODE_4X: begin
        R_act = R_lbuf;
        G_act = G_lbuf;
        B_act = B_lbuf;
        HSYNC_act = HSYNC_4x;
        VSYNC_act = VSYNC_4x;
        DE_act = DE_4x;
        line_id_act = SL_NO_ALTERN ? {1'b0, {line_out_idx_4x+{FID_1x, 1'b0}}} : {1'b0, line_out_idx_4x};
        hcnt_act = hcnt_4x;
        vcnt_act = vcnt_4x;
        pclk_mux_sel = `PCLK_MUX_4X;
        case (H_MULTMODE)
            default: begin //`H_MULTMODE_FULLWIDTH
                linebuf_hoffset_act = hcnt_4x;
                col_id_act = {2'b00, hcnt_4x[0]};
                rlpf_trigger_act = 1'b1;
            end
            `H_MULTMODE_OPTIMIZED: begin
                linebuf_hoffset_act = hcnt_4x_opt;
                col_id_act = hcnt_4x_opt_ctr;
                rlpf_trigger_act = (hcnt_4x_opt_ctr == 0);
            end
        endcase
    end
    `V_MULTMODE_5X: begin
        R_act = R_lbuf;
        G_act = G_lbuf;
        B_act = B_lbuf;
        HSYNC_act = HSYNC_5x;
        VSYNC_act = VSYNC_5x;
        DE_act = DE_5x;
        line_id_act = line_out_idx_5x;
        hcnt_act = hcnt_5x;
        vcnt_act = vcnt_5x;
        pclk_mux_sel = `PCLK_MUX_5X;
        case (H_MULTMODE)
            default: begin //`H_MULTMODE_FULLWIDTH
                linebuf_hoffset_act = hcnt_5x_hscomp;
                col_id_act = {2'b00, hcnt_5x[0]};
                rlpf_trigger_act = 1'b1;
            end
            `H_MULTMODE_OPTIMIZED: begin
                linebuf_hoffset_act = hcnt_5x_opt;
                col_id_act = hcnt_5x_opt_ctr;
                rlpf_trigger_act = (hcnt_5x_opt_ctr == 0);
            end
        endcase
    end
endcase

pll_2x pll_pclk (
    .areset(pll_areset),
    .clkswitch(enable_sc),
    .configupdate(pll_configupdate),
    .inclk0(clk27), // set videogen clock to primary (power-on default) since both reference clocks must be running during switchover
    .inclk1(PCLK_in), // is the secondary input clock fully compensated?
    .scanclk(pll_scanclk),
    .scanclkena(pll_scanclkena),
    .scandata(pll_scandata),
    .c0(pclk_2x), // pclk_3x in secondary config
    .c1(pclk_5x), // pclk_4x in secondary config
    .locked(pll_lock),
    .scandataout(),
    .scandone(pll_scandone)
);

assign pclk_3x = pclk_2x;
assign pclk_4x = pclk_5x;

cycloneive_clkctrl   clkctrl1 ( 
    .clkselect(enable_sc ? pclk_mux_sel : 2'h2),
    .ena(1'b1),
    .inclk({pclk_5x, pclk_2x, 1'b0, pclk_1x}), // fitter forbids using both clk27 and pclk_1x here since they're on opposite sides
    .outclk(pclk_act)
// synopsys translate_off
    ,
    .devclrn(1'b1),
    .devpor(1'b1)
// synopsys translate_on
);
defparam
    clkctrl1.clock_type = "Global Clock",
    clkctrl1.ena_register_mode = "falling edge",
    clkctrl1.lpm_type = "cycloneive_clkctrl";


wire [11:0] linebuf_rdaddr = linebuf_hoffset_pp-H_AVIDSTART;
wire [11:0] linebuf_wraddr = hcnt_1x-H_AVIDSTART;

//TODO: add secondary buffers for interlaced signals with alternative field order
linebuf linebuf_rgb (
    .data({R_in_L, G_in_L, B_in_L}),
    .rdaddress ( {~line_idx, linebuf_rdaddr[10:0]} ),
    .rdclock ( pclk_act ),
    .wraddress( {line_idx, linebuf_wraddr[10:0]} ),
    .wrclock ( pclk_1x ),
    .wren ( !linebuf_wraddr[11] ),
    .q ( {R_lbuf, G_lbuf, B_lbuf} )
);


//Postprocess pipeline
//
// Latency with respect to h_cnt/v_cnt before 1st stage:
// line_id, col_id:                 0 cycles
// HSYNC, VSYNC, DE:                1 cycle
// RGB:                             2 cycles
//
// Pipeline structure
// |   1   |   2   |   3   |   4   |   5   |   6   |   7   |   8   |   9   |  10   |  11   |  12   |
// |-------|-------|-------|-------|-------|-------|-------|-------|-------|-------|-------|-------|
// | RADDR |       |       |       |       |       |       |       |       |       |       |       |
// |       | LBUF  | LBUF  |       |       |       |       |       |       |       |       |       |
// |       |       |       | RLPF  | RLPF  | RLPF  |       |       |       |       |       |       |
// |       |       |       |       |   Y   |   Y   |       |       |       |       |       |       |
// |       |       |       |       |       |       |  SLG  |  SLG  |  SLG  |  SLG  |  SLG  |       |
// |       |       |       |       |       |       |       |       |       |       |       | MASK  |
// |       |       |       |       |       |       |       |       |       |       |       | LTBOX |
integer pp_idx;
always @(posedge pclk_act)
begin
    line_id_pp[`PP_PL_START] <= SL_ALTIV ? {2'b00, vcnt_act[0]} : line_id_act;
    col_id_pp[`PP_PL_START] <= col_id_act;
    for(pp_idx = `PP_PL_START+1; pp_idx <= `PP_SLGEN_PL_END-5; pp_idx = pp_idx+1) begin
        line_id_pp[pp_idx] <= line_id_pp[pp_idx-1];
        col_id_pp[pp_idx] <= col_id_pp[pp_idx-1];
    end

    hcnt_pp <= hcnt_act;
    vcnt_pp <= vcnt_act;
    linebuf_hoffset_pp <= linebuf_hoffset_act;
    xpos <= hcnt_pp - H_AVIDSTART;
    ypos <= vcnt_pp - V_AVIDSTART;

    border_enable_pp[`PP_ENABLES_START] <= ((hcnt_pp < H_AVIDMASK_START) | (hcnt_pp >= H_AVIDMASK_STOP) | (vcnt_pp < V_AVIDMASK_START) | (vcnt_pp >= V_AVIDMASK_STOP));
    case (lt_mode)
        default: begin
            lt_box_enable_pp[`PP_ENABLES_START] <= 0;
        end
        `LT_POS_TOPLEFT: begin
            lt_box_enable_pp[`PP_ENABLES_START] <= ((hcnt_pp < LT_POS_TOPLEFT_BOX_H_STOP) && (vcnt_pp < LT_POS_TOPLEFT_BOX_V_STOP)) ? 1'b1 : 1'b0;
        end
        `LT_POS_CENTER: begin
            lt_box_enable_pp[`PP_ENABLES_START] <= ((hcnt_pp >= LT_POS_CENTER_BOX_H_START) && (hcnt_pp < LT_POS_CENTER_BOX_H_STOP) && (vcnt_pp >= LT_POS_CENTER_BOX_V_START) && (vcnt_pp < LT_POS_CENTER_BOX_V_STOP)) ? 1'b1 : 1'b0;
        end
        `LT_POS_BOTTOMRIGHT: begin
            lt_box_enable_pp[`PP_ENABLES_START] <= ((hcnt_pp >= LT_POS_BOTTOMRIGHT_H_START) && (vcnt_pp >= LT_POS_BOTTOMRIGHT_V_START)) ? 1'b1 : 1'b0;
        end
    endcase
    for(pp_idx = `PP_ENABLES_START+1; pp_idx <= `PP_PIPELINE_LENGTH; pp_idx = pp_idx+1) begin
        lt_box_enable_pp[pp_idx] <= lt_box_enable_pp[pp_idx-1];
        border_enable_pp[pp_idx] <= border_enable_pp[pp_idx-1];
    end

    HSYNC_pp[`PP_HS_VS_DE_START] <= HSYNC_act;
    VSYNC_pp[`PP_HS_VS_DE_START] <= VSYNC_act;
    DE_pp[`PP_HS_VS_DE_START] <= DE_act;
    for(pp_idx = `PP_HS_VS_DE_START+1; pp_idx <= `PP_PIPELINE_LENGTH; pp_idx = pp_idx+1) begin
        HSYNC_pp[pp_idx] <= HSYNC_pp[pp_idx-1];
        VSYNC_pp[pp_idx] <= VSYNC_pp[pp_idx-1];
        DE_pp[pp_idx] <= DE_pp[pp_idx-1];
    end
    HSYNC_out <= HSYNC_pp[`PP_PIPELINE_LENGTH];
    VSYNC_out <= VSYNC_pp[`PP_PIPELINE_LENGTH];
    DE_out <= DE_pp[`PP_PIPELINE_LENGTH];

    // get RGB and delay it
    R_pp[`PP_RGB_START] <= R_act;
    G_pp[`PP_RGB_START] <= G_act;
    B_pp[`PP_RGB_START] <= B_act;
    for(pp_idx = `PP_RGB_START+1; pp_idx <= `PP_PIPELINE_LENGTH; pp_idx = pp_idx + 1) begin
        R_pp[pp_idx] <= R_pp[pp_idx-1];
        G_pp[pp_idx] <= G_pp[pp_idx-1];
        B_pp[pp_idx] <= B_pp[pp_idx-1];
    end
    R_out <= R_pp[`PP_PIPELINE_LENGTH];
    G_out <= G_pp[`PP_PIPELINE_LENGTH];
    B_out <= B_pp[`PP_PIPELINE_LENGTH];

    // reverse LPF ...
    rlpf_trigger_r[`PP_PL_START] <= rlpf_trigger_act;
    for(pp_idx = `PP_PL_START+1; pp_idx <= `PP_RLPF_PL_START-1; pp_idx = pp_idx + 1)
        rlpf_trigger_r[pp_idx] <= rlpf_trigger_r[pp_idx-1];

    // Optimized modes repeat pixels. Save previous pixel only when linebuffer offset changes.
    if (rlpf_trigger_r[`PP_RLPF_PL_START-1]) begin
`ifdef PP_RLPF_PL_START_EARLY
        R_prev_pp[`PP_RLPF_PL_START] <= R_act;
        G_prev_pp[`PP_RLPF_PL_START] <= G_act;
        B_prev_pp[`PP_RLPF_PL_START] <= B_act;
`else
        R_prev_pp[`PP_RLPF_PL_START] <= R_pp[`PP_RLPF_PL_START];
        G_prev_pp[`PP_RLPF_PL_START] <= G_pp[`PP_RLPF_PL_START];
        B_prev_pp[`PP_RLPF_PL_START] <= B_pp[`PP_RLPF_PL_START];
`endif
    end
    for(pp_idx = `PP_RLPF_PL_START+1; pp_idx <= `PP_RLPF_PL_END-1; pp_idx = pp_idx + 1) begin
        R_prev_pp[pp_idx] <= R_prev_pp[pp_idx-1];
        G_prev_pp[pp_idx] <= G_prev_pp[pp_idx-1];
        B_prev_pp[pp_idx] <= B_prev_pp[pp_idx-1];
    end

    // ... step 1
`ifdef PP_RLPF_PL_START_EARLY
    R_diff_s15_pre <= (R_prev_pp[`PP_RLPF_PL_START] - R_act);
    G_diff_s15_pre <= (G_prev_pp[`PP_RLPF_PL_START] - G_act);
    B_diff_s15_pre <= (B_prev_pp[`PP_RLPF_PL_START] - B_act);
`else
    R_diff_s15_pre <= (R_prev_pp[`PP_RLPF_PL_START] - R_pp[`PP_RLPF_PL_START]);
    G_diff_s15_pre <= (G_prev_pp[`PP_RLPF_PL_START] - G_pp[`PP_RLPF_PL_START]);
    B_diff_s15_pre <= (B_prev_pp[`PP_RLPF_PL_START] - B_pp[`PP_RLPF_PL_START]);
`endif


    // ... step 2
    // R_diff_s15, G_diff_s15, B_diff_s15 are outputs of multiplier IPs 12 pp-stage delay)
    R_diff_s15 <= (R_diff_s15_pre * X_REV_LPF_STR);
    G_diff_s15 <= (G_diff_s15_pre * X_REV_LPF_STR);
    B_diff_s15 <= (B_diff_s15_pre * X_REV_LPF_STR);

    // ... step 3
    if (X_REV_LPF_ENABLE) begin
        R_pp[`PP_RLPF_PL_END] <= apply_reverse_lpf(R_prev_pp[`PP_RLPF_PL_END-1], R_diff_s15);
        G_pp[`PP_RLPF_PL_END] <= apply_reverse_lpf(G_prev_pp[`PP_RLPF_PL_END-1], G_diff_s15);
        B_pp[`PP_RLPF_PL_END] <= apply_reverse_lpf(B_prev_pp[`PP_RLPF_PL_END-1], B_diff_s15);
    end

    // calculate Y (based on non-reverseLPF values to keep pipeline length a bit lower)
    Y_rb_tmp <=  {1'b0,R_pp[`PP_RLPF_PL_END-2]} + {1'b0,B_pp[`PP_RLPF_PL_END-2]};
    Y <= {1'b0,Y_rb_tmp} + {1'b0,G_pp[`PP_RLPF_PL_END-1],1'b0};

    // modify scanline strength (3 pp-stages)
    // ... step 1/3
    // Y_sl_hybr_ref_tmp, R_sl_hybr_ref_tmp, G_sl_hybr_ref_tmp, B_sl_hybr_ref_tmp are outputs of multiplier IPs (1 pp-stage delay)
    if (|(SL_L_OVERLAY & (5'h1<<line_id_pp[`PP_SLGEN_PL_END-5]))) begin
        sl_str_tmp <= ((SL_L_STR[line_id_pp[`PP_SLGEN_PL_END-5]]+8'h01)<<4)-1'b1;
        draw_sl_pp[`PP_SLGEN_PL_END-4] <= 1'b1;
    end else if (|(SL_C_OVERLAY & (6'h1<<col_id_pp[`PP_SLGEN_PL_END-5]))) begin
        sl_str_tmp <= ((SL_C_STR[col_id_pp[`PP_SLGEN_PL_END-5]]+8'h01)<<4)-1'b1;
        draw_sl_pp[`PP_SLGEN_PL_END-4] <= 1'b1;
    end else begin
        draw_sl_pp[`PP_SLGEN_PL_END-4] <= 1'b0;
    end
    for(pp_idx = `PP_SLGEN_PL_END-3; pp_idx <= `PP_SLGEN_PL_END-1; pp_idx = pp_idx + 1) begin
        draw_sl_pp[pp_idx] <= draw_sl_pp[pp_idx-1];
    end

    // ... step 2/3
    // Y_sl_hybr_ref,R_sl_hybr_ref,G_sl_hybr_ref,B_sl_hybr_ref are outputs of multiplier IPs (1 pp-stage delay)
    sl_str <= sl_str_tmp;

    // ... step 3/3
    Y_sl_str <= {1'b0,sl_str} < Y_sl_hybr_ref ? 8'h0 : sl_str - Y_sl_hybr_ref[7:0];
    R_sl_str <= {1'b0,sl_str} < R_sl_hybr_ref ? 8'h0 : sl_str - R_sl_hybr_ref[7:0];
    G_sl_str <= {1'b0,sl_str} < G_sl_hybr_ref ? 8'h0 : sl_str - G_sl_hybr_ref[7:0];
    B_sl_str <= {1'b0,sl_str} < B_sl_hybr_ref ? 8'h0 : sl_str - B_sl_hybr_ref[7:0];

    // perform scanline generation (1 pp-stage)
    // R_sl_mult, G_sl_mult and B_sl_mult are registered outputs of IP blocks (1 pp-stage delay)
    R_sl_sub <= (R_pp[`PP_SLGEN_PL_END-2] > R_sl_str) ? (R_pp[`PP_SLGEN_PL_END-2]-R_sl_str) : 8'h00;
    G_sl_sub <= (G_pp[`PP_SLGEN_PL_END-2] > G_sl_str) ? (G_pp[`PP_SLGEN_PL_END-2]-G_sl_str) : 8'h00;
    B_sl_sub <= (B_pp[`PP_SLGEN_PL_END-2] > B_sl_str) ? (B_pp[`PP_SLGEN_PL_END-2]-B_sl_str) : 8'h00;

    // draw scanline (1 pp-stage)
    if (draw_sl_pp[`PP_SLGEN_PL_END-1]) begin
        R_pp[`PP_SLGEN_PL_END] <= SL_METHOD ? R_sl_sub : R_sl_mult;
        G_pp[`PP_SLGEN_PL_END] <= SL_METHOD ? G_sl_sub : G_sl_mult;
        B_pp[`PP_SLGEN_PL_END] <= SL_METHOD ? B_sl_sub : B_sl_mult;
    end

    // apply LT box / mask
    if (lt_active) begin
        R_out <= {8{lt_box_enable_pp[`PP_PIPELINE_LENGTH]}};
        G_out <= {8{lt_box_enable_pp[`PP_PIPELINE_LENGTH]}};
        B_out <= {8{lt_box_enable_pp[`PP_PIPELINE_LENGTH]}};
    end else if (border_enable_pp[`PP_PIPELINE_LENGTH]) begin
        R_out <= X_MASK_COLOR[2] ? {2{X_MASK_BR}} : 8'h00;
        G_out <= X_MASK_COLOR[1] ? {2{X_MASK_BR}} : 8'h00;
        B_out <= X_MASK_COLOR[0] ? {2{X_MASK_BR}} : 8'h00;
    end
end

//Generate a warning signal from horizontal instability or PLL sync loss
always @(posedge pclk_1x or negedge reset_n)
begin
    if (!reset_n) begin
        warn_h_unstable <= 1'b0;
        warn_pll_lock_lost <= 1'b0;
    end else begin
        if (hmax[0] != hmax[1])
            warn_h_unstable <= 1;
        else if (warn_h_unstable != 0)
            warn_h_unstable <= warn_h_unstable + 1'b1;
    
        if ((V_MULTMODE > `V_MULTMODE_1X) & ~pll_lock)
            warn_pll_lock_lost <= 1;
        else if (warn_pll_lock_lost != 0)
            warn_pll_lock_lost <= warn_pll_lock_lost + 1'b1;
    end
end

assign h_unstable = (warn_h_unstable != 0);
assign pll_lock_lost = (warn_pll_lock_lost != 0);

//Detect if TVP7002 is skipping VSYNCs. This occurs for interlaced signals fed via digital sync inputs,
//causing TVP7002 not to regenerate VSYNC for field 1. Moreover, if leading edges of HSYNC and VSYNC are
//too far from each other for field 0, no VSYNC is regenerated at all. This can be avoided by disabling
//doubled sampling rates ("AV3 interlacefix") and/or minimizing VSYNC delay induced by RC filter on PCB.
//However, TVP7002 datasheet warns that HSYNC/VSYNC should not change simultaneously, so leaving out the
//filter may lead to stability issues and is not recommended. A combination of 220ohm resistor and 1nF
//capacitor seems to be optimal for 480i/576i, including doubled sampling rates.
always @(posedge clk27 or negedge reset_n)
begin
    if (!reset_n) begin
        fpga_vsyncgen[`VSYNCGEN_GENMID_BIT] <= 1'b0;
        VSYNC_in_cc_L <= 1'b0;
        VSYNC_in_cc_LL <= 1'b0;
        VSYNC_in_cc_LLL <= 1'b0;
        clk27_ctr <= 0;
        dbl_frame_ctr <= 0;
    end else begin
        if ((VSYNC_in_cc_LLL == `HI) && (VSYNC_in_cc_LL == `LO)) begin
            // If calculated refresh rate is between 22Hz and 44Hz, assume TVP7002 has skipped a vsync
            if ((clk27_ctr >= (27000000/44)) && (clk27_ctr <= (27000000/22)) && (dbl_frame_ctr < `DBLFRAME_THOLD))
                dbl_frame_ctr <= dbl_frame_ctr + 1'b1;
            else if ((clk27_ctr < (27000000/44)) && (dbl_frame_ctr > 0))
                dbl_frame_ctr <= dbl_frame_ctr - 1'b1;

            clk27_ctr <= 0;
        end else if (clk27_ctr < (27000000/10)) begin   //prevent overflow
            clk27_ctr <= clk27_ctr + 1'b1;
        end

        if (dbl_frame_ctr == 0)
            fpga_vsyncgen[`VSYNCGEN_GENMID_BIT] <= 1'b0;
        else if (dbl_frame_ctr == `DBLFRAME_THOLD)
            fpga_vsyncgen[`VSYNCGEN_GENMID_BIT] <= 1'b1;

        VSYNC_in_cc_L <= VSYNC_in;
        VSYNC_in_cc_LL <= VSYNC_in_cc_L;
        VSYNC_in_cc_LLL <= VSYNC_in_cc_LL;
    end
end

//Calculate exact vertical frequency
always @(posedge clk27 or negedge reset_n)
begin
    if (!reset_n) begin
        frame_change_longpulse_cc_L <= 1'b0;
        frame_change_longpulse_cc_LL <= 1'b0;
        frame_change_longpulse_cc_LLL <= 1'b0;
        pcnt_ctr <= 1;
        pcnt_frame <= 1;
    end else begin
        if (frame_change_longpulse_cc_LL & !frame_change_longpulse_cc_LLL) begin
            pcnt_ctr <= 1;
            pcnt_frame <= pcnt_ctr;
        end else if (pcnt_ctr < 20'hfffff) begin
            pcnt_ctr <= pcnt_ctr + 1'b1;
        end

        frame_change_longpulse_cc_L <= frame_change_longpulse;
        frame_change_longpulse_cc_LL <= frame_change_longpulse_cc_L;
        frame_change_longpulse_cc_LLL <= frame_change_longpulse_cc_LL;
    end
end

//Forward status flag to CPU
assign vsync_flag = ~VSYNC_in_cc_LL;


wire [11:0] H_L5BORDER_1920_tmp = (11'd1920-h_config[10:0]);
wire [11:0] H_L5BORDER_1600_tmp = (11'd1600-h_config[10:0]);

//Buffer the inputs using input pixel clock and generate 1x signals
always @(posedge pclk_1x or negedge reset_n)
begin
    if (!reset_n) begin
        hcnt_1x <= 0;
        vcnt_1x <= 0;
        vcnt_tvp <= 0;
        hmax[0] <= 0;
        hmax[1] <= 0;
        vmax <= 0;
        vmax_tvp <= 0;
        line_idx <= 0;
        FID_cur <= 1'b0;
        FID_last <= 1'b0;
        line_change <= 1'b0;
        frame_change <= 1'b0;
        frame_change_longpulse <= 1'b0;
        fpga_vsyncgen[`VSYNCGEN_CHOPMID_BIT] <= 1'b0;
        H_MULTMODE <= 0;
        V_MULTMODE <= 0;
    end else begin
        if (`HSYNC_LEADING_EDGE) begin
            hcnt_1x <= 0;
            hmax[line_idx] <= hcnt_1x;
            line_idx <= line_idx ^ 1'b1;
            line_change <= 1'b1;
        end else begin
            hcnt_1x <= hcnt_1x + 1'b1;
            line_change <= 1'b0;
        end

        if (`HSYNC_LEADING_EDGE) begin
            if (`VSYNC_LEADING_EDGE) begin // non-interlace frame or even field (interlace) start
                FID_cur <= 1'b0;
                FID_last <= FID_cur;
                vcnt_1x <= 0;
                frame_change <= 1'b1;
                vmax <= vcnt_1x;
                vcnt_tvp <= 0;
                vmax_tvp <= vcnt_tvp;
            end else begin
                vcnt_1x <= vcnt_1x + 1'b1;
                vcnt_tvp <= vcnt_tvp + 1'b1;
            end
        end else if (`VSYNC_LEADING_EDGE) begin // odd field (interlace) start
            if (!`FALSE_FIELD) begin
                FID_cur <= 1'b1;
                FID_last <= FID_cur;
                vcnt_1x <= 11'h7ff; // -1 for 11 bit word
                frame_change <= 1'b1;
                //vmax <= vcnt_1x;
            end
            vcnt_tvp <= 0;
            vmax_tvp <= vcnt_tvp;
        end else if ((fpga_vsyncgen[`VSYNCGEN_GENMID_BIT]) && (vcnt_tvp == (vmax_tvp>>1)) && (hcnt_1x == (hmax[~line_idx]>>1))) begin //VSM=1
            FID_cur <= 1'b1;
            FID_last <= FID_cur;
            vcnt_1x <= 11'h7ff; // -1 for 11 bit word
            frame_change <= 1'b1;
            //vmax <= vcnt_1x;
        end else
            frame_change <= 1'b0;

        if (`VSYNC_LEADING_EDGE) begin
            FID_prev <= FID_in;
            // detect non-interlaced signal with odd-odd field signaling (TVP7002 detects it as interlaced with analog sync inputs).
            // FID is updated at leading edge of VSYNC
            if (FID_in == FID_prev)
                fpga_vsyncgen[`VSYNCGEN_CHOPMID_BIT] <= `FALSE;
            else if (FID_in == `FID_ODD)   // TVP7002 falsely indicates field change with (vcnt < active_lines)
                fpga_vsyncgen[`VSYNCGEN_CHOPMID_BIT] <= (vcnt_tvp < `MIN_VALID_LINES);
        end

        if (frame_change) begin
            //Read configuration data from CPU
            H_MULTMODE <= h_config[31:30];    // Horizontal scaling mode
            V_MULTMODE <= v_config[31:29];    // Line multiply mode

            H_SYNCLEN <= h_config[27:20];                     // Horizontal sync length (0...255)
            H_AVIDSTART <= h_config[19:11] + h_config[27:20];   // Horizontal sync+backporch length (0...1023)
            H_ACTIVE <= h_config[10:0];                       // Horizontal active length (0...2047)

            V_SYNCLEN <= v_config[19:17];                     // Vertical sync length (0...7)
            V_AVIDSTART <= v_config[16:11] + v_config[19:17];   // Vertical sync+backporch length (0...127)
            V_ACTIVE <= v_config[10:0];                       // Vertical active length (0...2047)

            H_MASK <= h_config2[29:19];
            V_MASK <= v_config[25:20];

//            H_L5BORDER <= h_config[29] ? (11'd1920-h_config[10:0])/2 : (11'd1600-h_config[10:0])/2;
            H_L5BORDER <= h_config[29] ? H_L5BORDER_1920_tmp[10:1] : H_L5BORDER_1600_tmp[10:1];
            // For Line3x 240x360
            H_L3BORDER <= h_config[28] ? H_L5BORDER_1920_tmp[10:1] : 10'd0;

            H_L3_OPT_SAMPLE_COMP <= h_config[28] ? 7'd90 : 7'd0;

            H_OPT_SCALE <= h_config2[18:16];
            H_OPT_SAMPLE_SEL <= h_config2[15:13];
            H_OPT_SAMPLE_MULT <= h_config2[12:10];
            H_OPT_STARTOFF <= h_config2[9:0];

            X_PANASONIC_HACK <= misc_config[12];
            X_REV_LPF_ENABLE <= (misc_config[11:7] != 5'b00000);
            X_REV_LPF_STR <= (misc_config[11:7] + 6'd16);
            X_MASK_COLOR <= misc_config[6:4];
            X_MASK_BR <= misc_config[3:0];

            SL_NO_ALTERN <= sl_config[31];
            SL_METHOD  <= sl_config[30];
            SL_HYBRSTR <= sl_config[29:25];
            SL_L_OVERLAY <= sl_config[24:20];
            SL_L_STR[4] <= sl_config[19:16];
            SL_L_STR[3] <= sl_config[15:12];
            SL_L_STR[2] <= sl_config[11:8];
            SL_L_STR[1] <= sl_config[7:4];
            SL_L_STR[0] <= sl_config[3:0];
            SL_ALTIV <= sl_config2[31];
            SL_C_OVERLAY <= sl_config2[29:24];
            SL_C_STR[5] <= sl_config2[23:20];
            SL_C_STR[4] <= sl_config2[19:16];
            SL_C_STR[3] <= sl_config2[15:12];
            SL_C_STR[2] <= sl_config2[11:8];
            SL_C_STR[1] <= sl_config2[7:4];
            SL_C_STR[0] <= sl_config2[3:0];

            CALC_CONSTS <= 1'b1;
        end

        // generate long pulse for hz counter
        if (frame_change)
            frame_change_longpulse <= 1'b1;
        else if (vcnt_1x > 0)
            frame_change_longpulse <= 1'b0;

        if (CALC_CONSTS) begin
            H_AVIDSTOP <= H_AVIDSTART+H_ACTIVE;
            V_AVIDSTOP <= V_AVIDSTART+V_ACTIVE;

            H_AVIDMASK_START <= H_AVIDSTART+H_MASK;
            H_AVIDMASK_STOP <= H_AVIDSTART+H_ACTIVE-H_MASK;
            V_AVIDMASK_START <= V_AVIDSTART+V_MASK;
            V_AVIDMASK_STOP <= V_AVIDSTART+V_ACTIVE-V_MASK;

            LT_POS_TOPLEFT_BOX_H_STOP <= H_AVIDSTART+(H_ACTIVE/`LT_WIDTH_DIV);
            LT_POS_TOPLEFT_BOX_V_STOP <= V_AVIDSTART+(V_ACTIVE/`LT_HEIGHT_DIV);
            LT_POS_CENTER_BOX_H_START <= H_AVIDSTART+(H_ACTIVE/2'h2)-(H_ACTIVE/(`LT_WIDTH_DIV*2'h2));
            LT_POS_CENTER_BOX_H_STOP <= H_AVIDSTART+(H_ACTIVE/2'h2)+(H_ACTIVE/(`LT_WIDTH_DIV*2'h2));
            LT_POS_CENTER_BOX_V_START <= V_AVIDSTART+(V_ACTIVE/2'h2)-(V_ACTIVE/(`LT_HEIGHT_DIV*2'h2));
            LT_POS_CENTER_BOX_V_STOP <= V_AVIDSTART+(V_ACTIVE/2'h2)+(V_ACTIVE/(`LT_HEIGHT_DIV*2'h2));
            LT_POS_BOTTOMRIGHT_H_START <= H_AVIDSTART+H_ACTIVE-(H_ACTIVE/`LT_WIDTH_DIV);
            LT_POS_BOTTOMRIGHT_V_START <= V_AVIDSTART+V_ACTIVE-(V_ACTIVE/`LT_HEIGHT_DIV);

            CALC_CONSTS <= 1'b0;
        end

        R_in_L <= R_in;
        G_in_L <= G_in;
        B_in_L <= B_in;
        HSYNC_in_L <= HSYNC_in;
        VSYNC_in_L <= VSYNC_in;

        // Add two delay stages to match linebuf delay
        R_in_LL <= R_in_L;
        G_in_LL <= G_in_L;
        B_in_LL <= B_in_L;
        R_in_LLL <= R_in_LL;
        G_in_LLL <= G_in_LL;
        B_in_LLL <= B_in_LL;

        R_1x <= R_in_LLL;
        G_1x <= G_in_LLL;
        B_1x <= B_in_LLL;
        HSYNC_1x <= (hcnt_1x < H_SYNCLEN) ? `HSYNC_POL : ~`HSYNC_POL;
        if (FID_cur == `FID_EVEN)
            VSYNC_1x <= (vcnt_1x < V_SYNCLEN) ? `VSYNC_POL : ~`VSYNC_POL;
        else
            VSYNC_1x <= (((vcnt_1x+1'b1) < V_SYNCLEN) | ((vcnt_1x+1'b1 == V_SYNCLEN) & (hcnt_1x <= (hmax[~line_idx]>>1)))) ? `VSYNC_POL : ~`VSYNC_POL;
        DE_1x <= ((hcnt_1x >= H_AVIDSTART) & (hcnt_1x < H_AVIDSTOP)) & ((vcnt_1x >= V_AVIDSTART) & (vcnt_1x < V_AVIDSTOP));
        FID_1x <= FID_cur;
    end
end

//Generate 2x signals for linedouble
always @(posedge pclk_2x or negedge reset_n)
begin
    if (!reset_n) begin
        hcnt_2x <= 0;
        vcnt_2x <= 0;
        line_out_idx_2x <= 0;
    end else begin
        if ((pclk_1x == 1'b1) & (line_change | frame_change)) begin  //aligned with negedge of pclk_1x
            hcnt_2x <= 0;
            hcnt_2x_opt <= H_OPT_SAMPLE_SEL;
            hcnt_2x_opt_ctr <= 0;
            line_out_idx_2x <= 0;
            if (frame_change)
                vcnt_2x <= 11'h7ff; // -1 for 11 bit word
            else if (line_change & (FID_cur == `FID_EVEN))
                vcnt_2x <= vcnt_2x + 1'b1;
        end else if (hcnt_2x == hmax[~line_idx]) begin
            hcnt_2x <= 0;
            line_out_idx_2x <= line_out_idx_2x + 1'b1;
            hcnt_2x_opt <= H_OPT_SAMPLE_SEL;
            hcnt_2x_opt_ctr <= 0;
            if (FID_cur == `FID_ODD)
                vcnt_2x <= vcnt_2x + 1'b1;
        end else begin
            hcnt_2x <= hcnt_2x + 1'b1;
            if (hcnt_2x >= H_OPT_STARTOFF) begin
                if (hcnt_2x_opt_ctr == H_OPT_SCALE-1'b1) begin
                    hcnt_2x_opt <= hcnt_2x_opt + H_OPT_SAMPLE_MULT;
                    hcnt_2x_opt_ctr <= 0;
                end else
                    hcnt_2x_opt_ctr <= hcnt_2x_opt_ctr + 1'b1;
            end
        end

        HSYNC_2x <= (hcnt_2x < H_SYNCLEN) ? `HSYNC_POL : ~`HSYNC_POL;
        VSYNC_2x <= (vcnt_2x < V_SYNCLEN) ? `VSYNC_POL : ~`VSYNC_POL;
        DE_2x <= ((hcnt_2x >= H_AVIDSTART) & (hcnt_2x < ((X_PANASONIC_HACK & (vcnt_2x == V_AVIDSTOP-1'b1) & (line_out_idx_2x==2'h1)) ? (H_AVIDSTOP-12'd98) : H_AVIDSTOP))) & ((vcnt_2x >= V_AVIDSTART) & (vcnt_2x < V_AVIDSTOP));
    end
end

always @(posedge pclk_3x or negedge reset_n)
begin
    if (!reset_n) begin
        hcnt_3x <= 0;
        vcnt_3x <= 0;
        line_out_idx_3x <= 0;
    end else begin
        if ((pclk_3x_cnt == 0) & (line_change | frame_change)) begin  //aligned with posedge of pclk_1x
            if (!(frame_change & (FID_cur == `FID_ODD))) begin
                hcnt_3x <= 0;
                hcnt_3x_opt <= H_OPT_SAMPLE_SEL + H_L3_OPT_SAMPLE_COMP;
                hcnt_3x_opt_ctr <= 0;
                line_out_idx_3x <= 0;
            end
            if (frame_change)
                vcnt_3x <= -11'b1-FID_cur;
            else if (line_change)
                vcnt_3x <= vcnt_3x + 1'b1;
        end else if (hcnt_3x == hmax[~line_idx]) begin
            hcnt_3x <= 0;
            line_out_idx_3x <= line_out_idx_3x + 1'b1;
            hcnt_3x_opt <= H_OPT_SAMPLE_SEL + H_L3_OPT_SAMPLE_COMP;
            hcnt_3x_opt_ctr <= 0;
        end else begin
            hcnt_3x <= hcnt_3x + 1'b1;
            if (hcnt_3x >= H_OPT_STARTOFF) begin
                if (hcnt_3x_opt_ctr == H_OPT_SCALE-1'b1) begin
                    hcnt_3x_opt <= hcnt_3x_opt + H_OPT_SAMPLE_MULT;
                    hcnt_3x_opt_ctr <= 0;
                end else
                    hcnt_3x_opt_ctr <= hcnt_3x_opt_ctr + 1'b1;
            end
        end

        //track pclk_3x alignment to pclk_1x rising edge (pclk_1x=1 @ 120deg & pclk_1x=0 @ 240deg)
        if (((pclk_1x_prev3x == 1'b1) & (pclk_1x == 1'b0)) | (pclk_3x_cnt == 2'h2))
            pclk_3x_cnt <= 0;
        else
            pclk_3x_cnt <= pclk_3x_cnt + 1'b1;

        pclk_1x_prev3x <= pclk_1x;

        HSYNC_3x <= (hcnt_3x < H_SYNCLEN) ? `HSYNC_POL : ~`HSYNC_POL;
        if (FID_cur == `FID_EVEN)
            VSYNC_3x <= (vcnt_3x < V_SYNCLEN) ? `VSYNC_POL : ~`VSYNC_POL;
        else begin
            if ((vcnt_3x+1'b1 == 11'd0) & (line_out_idx_3x == 1) & (hcnt_3x == (hmax[~line_idx]>>1)+1'b1))
                VSYNC_3x <= `VSYNC_POL;
            else if ((vcnt_3x+1'b1 == V_SYNCLEN) & (line_out_idx_3x == 1) & (hcnt_3x == (hmax[~line_idx]>>1)+1'b1))
                VSYNC_3x <= ~`VSYNC_POL;
        end
        
        DE_3x <= ((hcnt_3x >= H_AVIDSTART-H_L3BORDER) & (hcnt_3x < H_AVIDSTOP+H_L3BORDER)) & ((vcnt_3x >= V_AVIDSTART) & (vcnt_3x < V_AVIDSTOP));
    end
end

always @(posedge pclk_4x or negedge reset_n)
begin
    if (!reset_n) begin
        hcnt_4x <= 0;
        vcnt_4x <= 0;
        line_out_idx_4x <= 0;
    end else begin

        // TODO: better implementation
        if ((DE_3x == 1) & (DE_3x_prev4x == 0))
            hcnt_4x_aspfix <= hcnt_3x - 12'd160;
        else
            hcnt_4x_aspfix <= hcnt_4x_aspfix + 1'b1;

        DE_3x_prev4x <= DE_3x;

        if ((pclk_4x_cnt == 0) & (line_change | frame_change)) begin  //aligned with posedge of pclk_1x
            hcnt_4x <= 0;
            hcnt_4x_opt <= H_OPT_SAMPLE_SEL;
            hcnt_4x_opt_ctr <= 0;
            line_out_idx_4x <= 0;
            if (frame_change)
                vcnt_4x <= 11'h7ff; // -1 for 11 bit word
            else if (line_change & (FID_cur == `FID_EVEN))
                vcnt_4x <= vcnt_4x + 1'b1;
        end else if (hcnt_4x == hmax[~line_idx]) begin
            hcnt_4x <= 0;
            line_out_idx_4x <= line_out_idx_4x + 1'b1;
            hcnt_4x_opt <= H_OPT_SAMPLE_SEL;
            hcnt_4x_opt_ctr <= 0;
            if ((FID_cur == `FID_ODD) && (line_out_idx_4x == 1))
                vcnt_4x <= vcnt_4x + 1'b1;
        end else begin
            hcnt_4x <= hcnt_4x + 1'b1;
            if (hcnt_4x >= H_OPT_STARTOFF) begin
                if (hcnt_4x_opt_ctr == H_OPT_SCALE-1'b1) begin
                    hcnt_4x_opt <= hcnt_4x_opt + H_OPT_SAMPLE_MULT;
                    hcnt_4x_opt_ctr <= 0;
                end else
                    hcnt_4x_opt_ctr <= hcnt_4x_opt_ctr + 1'b1;
            end
        end

        //track pclk_4x alignment to pclk_1x rising edge (pclk_1x=1 @ 180deg & pclk_1x=0 @ 270deg)
        if (((pclk_1x_prev4x == 1'b1) & (pclk_1x == 1'b0)) | (pclk_4x_cnt == 2'h3))
            pclk_4x_cnt <= 0;
        else
            pclk_4x_cnt <= pclk_4x_cnt + 1'b1;

        pclk_1x_prev4x <= pclk_1x;

        HSYNC_4x <= (hcnt_4x < H_SYNCLEN) ? `HSYNC_POL : ~`HSYNC_POL;
        VSYNC_4x <= (vcnt_4x < V_SYNCLEN) ? `VSYNC_POL : ~`VSYNC_POL;
        DE_4x <= ((hcnt_4x >= H_AVIDSTART) & (hcnt_4x < H_AVIDSTOP)) & ((vcnt_4x >= V_AVIDSTART) & (vcnt_4x < V_AVIDSTOP));
    end
end

always @(posedge pclk_5x or negedge reset_n)
begin
    if (!reset_n) begin
        hcnt_5x <= 0;
        vcnt_5x <= 0;
        line_out_idx_5x <= 0;
    end else begin
        if ((pclk_5x_cnt == 0) & (line_change | frame_change)) begin  //aligned with posedge of pclk_1x
            hcnt_5x <= 0;
            hcnt_5x_opt <= H_OPT_SAMPLE_SEL + 11'd120;
            hcnt_5x_opt_ctr <= 0;
            line_out_idx_5x <= 0;
            if (frame_change)
                vcnt_5x <= 11'h7ff; // -1 for 11 bit word
            else if (line_change)
                vcnt_5x <= vcnt_5x + 1'b1;
        end else if (hcnt_5x == hmax[~line_idx]) begin
            hcnt_5x <= 0;
            line_out_idx_5x <= line_out_idx_5x + 1'b1;
            hcnt_5x_opt <= H_OPT_SAMPLE_SEL + 11'd120;
            hcnt_5x_opt_ctr <= 0;
        end else begin
            hcnt_5x <= hcnt_5x + 1'b1;
            if (hcnt_5x >= H_OPT_STARTOFF) begin
                if (hcnt_5x_opt_ctr == H_OPT_SCALE-1'b1) begin
                    hcnt_5x_opt <= hcnt_5x_opt + H_OPT_SAMPLE_MULT;
                    hcnt_5x_opt_ctr <= 0;
                end else
                    hcnt_5x_opt_ctr <= hcnt_5x_opt_ctr + 1'b1;
            end
        end

        //track pclk_5x alignment to pclk_1x rising edge (pclk_1x=1 @ 144deg & pclk_1x=0 @ 216deg & pclk_1x=0 @ 288deg)
        if (((pclk_1x_prevprev5x == 1'b1) & (pclk_1x_prev5x == 1'b0)) | (pclk_5x_cnt == 3'h4))
            pclk_5x_cnt <= 0;
        else
            pclk_5x_cnt <= pclk_5x_cnt + 1'b1;

        pclk_1x_prev5x <= pclk_1x;
        pclk_1x_prevprev5x <= pclk_1x_prev5x;

        hcnt_5x_hscomp <= hcnt_5x + 11'd121;

        HSYNC_5x <= (hcnt_5x < H_SYNCLEN) ? `HSYNC_POL : ~`HSYNC_POL;
        VSYNC_5x <= (vcnt_5x < V_SYNCLEN) ? `VSYNC_POL : ~`VSYNC_POL;
        DE_5x <= ((hcnt_5x >= H_AVIDSTART-H_L5BORDER) & (hcnt_5x < H_AVIDSTOP+H_L5BORDER)) & ((vcnt_5x >= V_AVIDSTART) & (vcnt_5x < V_AVIDSTOP));
    end
end

endmodule
