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

`define H_MULTMODE_FULLWIDTH    2'h0
`define H_MULTMODE_ASPECTFIX    2'h1
`define H_MULTMODE_OPTIMIZED    2'h2

`define SCANLINES_OFF           2'h0
`define SCANLINES_H             2'h1
`define SCANLINES_V             2'h2
`define SCANLINES_ALT           2'h3

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
    input [31:0] h_info,
    input [31:0] h_info2,
    input [31:0] v_info,
    input [31:0] extra_info,
    output reg [7:0] R_out,
    output reg [7:0] G_out,
    output reg [7:0] B_out,
    output reg HSYNC_out,
    output reg VSYNC_out,
    output PCLK_out,
    output reg DE_out,
    output h_unstable,
    output reg [1:0] fpga_vsyncgen,
    output [1:0] pclk_lock,
    output [1:0] pll_lock_lost,
    output reg [10:0] vmax,
    output reg [10:0] vmax_tvp
);

//clock-related signals
wire pclk_act;
wire pclk_1x, pclk_2x, pclk_3x, pclk_4x, pclk_5x;
wire pclk_2x_lock, pclk_3x_lock;
wire linebuf_rdclock;

//RGB signals&registers: 8 bits per component -> 16.7M colors
wire [7:0] R_act, G_act, B_act;
wire [7:0] R_lbuf, G_lbuf, B_lbuf;
reg [7:0] R_in_L, G_in_L, B_in_L, R_in_LL, G_in_LL, B_in_LL, R_1x, G_1x, B_1x, R_pp3, G_pp3, B_pp3;

//H+V syncs + data enable signals&registers
wire HSYNC_act, VSYNC_act, DE_act;
reg HSYNC_in_L, VSYNC_in_L;
reg HSYNC_1x, HSYNC_2x, HSYNC_3x, HSYNC_4x, HSYNC_5x, HSYNC_pp1, HSYNC_pp2, HSYNC_pp3;
reg VSYNC_1x, VSYNC_2x, VSYNC_3x, VSYNC_4x, VSYNC_5x, VSYNC_pp1, VSYNC_pp2, VSYNC_pp3;
reg DE_1x, DE_2x, DE_3x, DE_4x, DE_5x, DE_pp1, DE_pp2, DE_pp3, DE_3x_prev4x;

//registers indicating line/frame change and field type
reg FID_cur, FID_prev, FID_1x;
reg frame_change, line_change;

//H+V counters
wire [11:0] linebuf_hoffset; //Offset for line (max. 2047 pixels), MSB indicates which line is read/written
wire [11:0] hcnt_act;
reg [11:0] hcnt_1x, hcnt_2x, hcnt_3x, hcnt_4x, hcnt_5x, hcnt_4x_aspfix, hcnt_2x_opt, hcnt_3x_opt, hcnt_4x_opt, hcnt_5x_opt, hcnt_5x_hscomp;
reg [2:0] hcnt_2x_opt_ctr, hcnt_3x_opt_ctr, hcnt_4x_opt_ctr, hcnt_5x_opt_ctr;
wire [10:0] vcnt_act;
reg [10:0] vcnt_tvp, vcnt_1x, vcnt_2x, vcnt_3x, vcnt_4x, vcnt_5x;       //max. 2047

//other counters
wire [2:0] line_id_act, col_id_act;
reg [2:0] line_id_pp1, line_id_pp2, col_id_pp1, col_id_pp2;
reg [11:0] hmax[0:1];
reg line_idx;
reg [1:0] line_out_idx_2x, line_out_idx_3x, line_out_idx_4x;
reg [2:0] line_out_idx_5x;
reg [23:0] warn_h_unstable, warn_pll_lock_lost, warn_pll_lock_lost_3x;
reg mask_enable_pp1, mask_enable_pp2, mask_enable_pp3;

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
reg [1:0] V_SCANLINEMODE;
reg [4:0] V_SCANLINEID;
reg [5:0] V_MASK;
reg [2:0] V_MULTMODE;
reg [1:0] H_MULTMODE;
reg [9:0] H_MASK;
reg [9:0] H_OPT_STARTOFF;
reg [2:0] H_OPT_SCALE;
reg [2:0] H_OPT_SAMPLE_MULT;
reg [2:0] H_OPT_SAMPLE_SEL;
reg [9:0] H_L5BORDER;
reg [3:0] X_MASK_BR;
reg [7:0] X_SCANLINESTR;

//clk27 related registers
reg VSYNC_in_cc_L, VSYNC_in_cc_LL, VSYNC_in_cc_LLL;
reg [21:0] clk27_ctr;   // min. 6.5Hz
reg [2:0] dbl_frame_ctr;


assign pclk_1x = PCLK_in;
assign PCLK_out = pclk_act;
assign pclk_lock = {pclk_2x_lock, pclk_3x_lock};

//Scanline generation
function [7:0] apply_scanlines;
    input [1:0] mode;
    input [7:0] data;
    input [7:0] str;
    input [4:0] mask;
    input [2:0] line_id;
    input [2:0] col_id;
    input fid;
    begin
        if ((mode == `SCANLINES_H) && (mask & (5'h1<<line_id)))
            apply_scanlines = (data > str) ? (data-str) : 8'h00;
        else if ((mode == `SCANLINES_V) && (5'h0 == col_id))
            apply_scanlines = (data > str) ? (data-str) : 8'h00;
        else if ((mode == `SCANLINES_ALT) && (mask & (5'h1<<(line_id^fid))))
            apply_scanlines = (data > str) ? (data-str) : 8'h00;
        else
            apply_scanlines = data;
    end
    endfunction

//Border masking
function [7:0] apply_mask;
    input enable;
    input [7:0] data;
    input [3:0] brightness;
    begin
        if (enable)
            apply_mask = {brightness, 4'h0};
        else
            apply_mask = data;
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
        pclk_act = pclk_1x;
        linebuf_rdclock = 0;
        linebuf_hoffset = 0;
        col_id_act = {2'b00, hcnt_1x[0]};
    end
    `V_MULTMODE_2X: begin
        R_act = R_lbuf;
        G_act = G_lbuf;
        B_act = B_lbuf;
        HSYNC_act = HSYNC_2x;
        VSYNC_act = VSYNC_2x;
        DE_act = DE_2x;
        line_id_act = {1'b0, line_out_idx_2x[1], line_out_idx_2x[0]^FID_1x};
        hcnt_act = hcnt_2x;
        vcnt_act = vcnt_2x;
        linebuf_rdclock = pclk_2x;
        case (H_MULTMODE)
            default: begin //`H_MULTMODE_FULLWIDTH
                pclk_act = pclk_2x;
                linebuf_hoffset = hcnt_2x;
                col_id_act = {2'b00, hcnt_2x[0]};
            end
            `H_MULTMODE_OPTIMIZED: begin
                pclk_act = pclk_1x;     //special case: pclk bypass to enable 2x native sampling
                linebuf_hoffset = hcnt_2x_opt;
                col_id_act = {2'b00, hcnt_2x[1]};
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
                pclk_act = pclk_3x;
                linebuf_rdclock = pclk_3x;
                linebuf_hoffset = hcnt_3x;
                hcnt_act = hcnt_3x;
                col_id_act = {2'b00, hcnt_3x[0]};
            end
            `H_MULTMODE_ASPECTFIX: begin
                pclk_act = pclk_4x;
                linebuf_rdclock = pclk_4x;
                linebuf_hoffset = hcnt_4x_aspfix;
                hcnt_act = hcnt_4x_aspfix;
                col_id_act = {2'b00, hcnt_4x[0]};
            end
            `H_MULTMODE_OPTIMIZED: begin
                pclk_act = pclk_3x;
                linebuf_rdclock = pclk_3x;
                linebuf_hoffset = hcnt_3x_opt;
                hcnt_act = hcnt_3x;
                col_id_act = hcnt_3x_opt_ctr;
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
        line_id_act = {1'b0, line_out_idx_4x};
        hcnt_act = hcnt_4x;
        vcnt_act = vcnt_4x;
        pclk_act = pclk_4x;
        linebuf_rdclock = pclk_4x;
        case (H_MULTMODE)
            default: begin //`H_MULTMODE_FULLWIDTH
                linebuf_hoffset = hcnt_4x;
                col_id_act = {2'b00, hcnt_4x[0]};
            end
            `H_MULTMODE_OPTIMIZED: begin
                linebuf_hoffset = hcnt_4x_opt;
                col_id_act = hcnt_4x_opt_ctr;
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
        line_id_act = {2'b00, line_out_idx_5x};
        hcnt_act = hcnt_5x;
        vcnt_act = vcnt_5x;
        pclk_act = pclk_5x;
        linebuf_rdclock = pclk_5x;
        case (H_MULTMODE)
            default: begin //`H_MULTMODE_FULLWIDTH
                linebuf_hoffset = hcnt_5x_hscomp;
                col_id_act = {2'b00, hcnt_5x[0]};
            end
            `H_MULTMODE_OPTIMIZED: begin
                linebuf_hoffset = hcnt_5x_opt;
                col_id_act = hcnt_5x_opt_ctr;
            end
        endcase
    end
endcase

//TODO: use single PLL and ALTPLL_RECONFIG
pll_2x pll_linedouble (
    .areset ( (V_MULTMODE != `V_MULTMODE_2X) & (V_MULTMODE != `V_MULTMODE_5X) ),
    .inclk0 ( PCLK_in ),
    .c0 ( pclk_2x ),
    .c1 ( pclk_5x ),
    .locked ( pclk_2x_lock )
);

pll_3x pll_linetriple (
    .areset ( (V_MULTMODE != `V_MULTMODE_3X) & (V_MULTMODE != `V_MULTMODE_4X) ),
    .inclk0 ( PCLK_in ),
    .c0 ( pclk_3x ),
    .c1 ( pclk_4x ),
    .locked ( pclk_3x_lock )
);

wire [11:0] linebuf_rdaddr = linebuf_hoffset-H_AVIDSTART;
wire [11:0] linebuf_wraddr = hcnt_1x-H_AVIDSTART;

//TODO: add secondary buffers for interlaced signals with alternative field order
linebuf linebuf_rgb (
    .data({R_in_L, G_in_L, B_in_L}),
    .rdaddress ( {~line_idx, linebuf_rdaddr[10:0]} ),
    .rdclock ( linebuf_rdclock ),
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
always @(posedge pclk_act)
begin
    line_id_pp1 <= line_id_act;
    col_id_pp1 <= col_id_act;
    mask_enable_pp1 <= ((hcnt_act < H_AVIDSTART+H_MASK) | (hcnt_act >= H_AVIDSTART+H_ACTIVE-H_MASK) | (vcnt_act < V_AVIDSTART+V_MASK) | (vcnt_act >= V_AVIDSTART+V_ACTIVE-V_MASK));

    HSYNC_pp2 <= HSYNC_act;
    VSYNC_pp2 <= VSYNC_act;
    DE_pp2 <= DE_act;
    line_id_pp2 <= line_id_pp1;
    col_id_pp2 <= col_id_pp1;
    mask_enable_pp2 <= mask_enable_pp1;
    
    R_pp3 <= apply_scanlines(V_SCANLINEMODE, R_act, X_SCANLINESTR, V_SCANLINEID, line_id_pp2, col_id_pp2, FID_1x);
    G_pp3 <= apply_scanlines(V_SCANLINEMODE, G_act, X_SCANLINESTR, V_SCANLINEID, line_id_pp2, col_id_pp2, FID_1x);
    B_pp3 <= apply_scanlines(V_SCANLINEMODE, B_act, X_SCANLINESTR, V_SCANLINEID, line_id_pp2, col_id_pp2, FID_1x);
    HSYNC_pp3 <= HSYNC_pp2;
    VSYNC_pp3 <= VSYNC_pp2;
    DE_pp3 <= DE_pp2;
    mask_enable_pp3 <= mask_enable_pp2;
    
    R_out <= apply_mask(mask_enable_pp3, R_pp3, X_MASK_BR);
    G_out <= apply_mask(mask_enable_pp3, G_pp3, X_MASK_BR);
    B_out <= apply_mask(mask_enable_pp3, B_pp3, X_MASK_BR);
    HSYNC_out <= HSYNC_pp3;
    VSYNC_out <= VSYNC_pp3;
    DE_out <= DE_pp3;
end

//Generate a warning signal from horizontal instability or PLL sync loss
always @(posedge pclk_1x or negedge reset_n)
begin
    if (!reset_n) begin
        warn_h_unstable <= 1'b0;
        warn_pll_lock_lost <= 1'b0;
        warn_pll_lock_lost_3x <= 1'b0;
    end else begin
        if (hmax[0] != hmax[1])
            warn_h_unstable <= 1;
        else if (warn_h_unstable != 0)
            warn_h_unstable <= warn_h_unstable + 1'b1;
    
        if (((V_MULTMODE == `V_MULTMODE_2X) | (V_MULTMODE == `V_MULTMODE_5X)) & ~pclk_2x_lock)
            warn_pll_lock_lost <= 1;
        else if (warn_pll_lock_lost != 0)
            warn_pll_lock_lost <= warn_pll_lock_lost + 1'b1;
            
        if (((V_MULTMODE == `V_MULTMODE_3X) | (V_MULTMODE == `V_MULTMODE_4X)) & ~pclk_3x_lock)
            warn_pll_lock_lost_3x <= 1;
        else if (warn_pll_lock_lost_3x != 0)
            warn_pll_lock_lost_3x <= warn_pll_lock_lost_3x + 1'b1;
    end
end

assign h_unstable = (warn_h_unstable != 0);
assign pll_lock_lost = {(warn_pll_lock_lost != 0), (warn_pll_lock_lost_3x != 0)};

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
        line_change <= 1'b0;
        frame_change <= 1'b0;
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
                vcnt_1x <= -1;
                frame_change <= 1'b1;
                vmax <= vcnt_1x;
            end
            vcnt_tvp <= 0;
            vmax_tvp <= vcnt_tvp;
        end else if ((fpga_vsyncgen[`VSYNCGEN_GENMID_BIT]) && (vcnt_tvp == (vmax_tvp>>1)) && (hcnt_1x == (hmax[~line_idx]>>1))) begin //VSM=1
            FID_cur <= 1'b1;
            vcnt_1x <= -1;
            frame_change <= 1'b1;
            vmax <= vcnt_1x;
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
            H_MULTMODE <= h_info[31:30];    // Horizontal scaling mode
            V_MULTMODE <= v_info[31:29];    // Line multiply mode

            H_SYNCLEN <= h_info[27:20];                     // Horizontal sync length (0...255)
            H_AVIDSTART <= h_info[19:11] + h_info[27:20];   // Horizontal sync+backporch length (0...1023)
            H_ACTIVE <= h_info[10:0];                       // Horizontal active length (0...2047)

            V_SYNCLEN <= v_info[19:17];                     // Vertical sync length (0...7)
            V_AVIDSTART <= v_info[16:11] + v_info[19:17];   // Vertical sync+backporch length (0...127)
            V_ACTIVE <= v_info[10:0];                       // Vertical active length (0...2047)

            H_MASK <= h_info2[28:19];
            V_MASK <= v_info[25:20];
            
            V_SCANLINEMODE <= v_info[28:27];
            case (v_info[31:29])
                `V_MULTMODE_1X, `V_MULTMODE_2X: V_SCANLINEID <= (5'b00001 << v_info[26]);
                `V_MULTMODE_3X: V_SCANLINEID <= (5'b00001 << {v_info[26], 1'b0});
                `V_MULTMODE_4X: V_SCANLINEID <= (5'b00011 << {v_info[26], 1'b0});
                `V_MULTMODE_5X: V_SCANLINEID <= (5'b00011 << {2{v_info[26]}});
            endcase

            H_L5BORDER <= h_info[29] ? (11'd1920-h_info[10:0])/2 : (11'd1600-h_info[10:0])/2;

            H_OPT_SCALE <= h_info2[18:16];
            H_OPT_SAMPLE_SEL <= h_info2[15:13];
            H_OPT_SAMPLE_MULT <= h_info2[12:10];
            H_OPT_STARTOFF <= h_info2[9:0];

            X_MASK_BR <= extra_info[7:4];
            X_SCANLINESTR <= ((extra_info[3:0]+8'h01)<<4)-1'b1;
        end
            
        R_in_L <= R_in;
        G_in_L <= G_in;
        B_in_L <= B_in;
        HSYNC_in_L <= HSYNC_in;
        VSYNC_in_L <= VSYNC_in;

        // Add one delay stage to match linebuf delay
        R_in_LL <= R_in_L;
        G_in_LL <= G_in_L;
        B_in_LL <= B_in_L;

        R_1x <= R_in_LL;
        G_1x <= G_in_LL;
        B_1x <= B_in_LL;
        HSYNC_1x <= (hcnt_1x < H_SYNCLEN) ? `HSYNC_POL : ~`HSYNC_POL;
        if (FID_cur == `FID_EVEN)
            VSYNC_1x <= (vcnt_1x < V_SYNCLEN) ? `VSYNC_POL : ~`VSYNC_POL;
        else
            VSYNC_1x <= (((vcnt_1x+1'b1) < V_SYNCLEN) | ((vcnt_1x+1'b1 == V_SYNCLEN) & (hcnt_1x <= (hmax[~line_idx]>>1)))) ? `VSYNC_POL : ~`VSYNC_POL;
        DE_1x <= ((hcnt_1x >= H_AVIDSTART) & (hcnt_1x < H_AVIDSTART+H_ACTIVE)) & ((vcnt_1x >= V_AVIDSTART) & (vcnt_1x < V_AVIDSTART+V_ACTIVE));
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
        if ((pclk_1x == 1'b0) & (line_change | frame_change)) begin  //aligned with posedge of pclk_1x
            hcnt_2x <= 0;
            hcnt_2x_opt <= H_OPT_SAMPLE_SEL;
            hcnt_2x_opt_ctr <= 0;
            line_out_idx_2x <= 0;
            if (frame_change)
                vcnt_2x <= -1;
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
        DE_2x <= ((hcnt_2x >= H_AVIDSTART) & (hcnt_2x < H_AVIDSTART+H_ACTIVE)) & ((vcnt_2x >= V_AVIDSTART) & (vcnt_2x < V_AVIDSTART+V_ACTIVE));
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
                hcnt_3x_opt <= H_OPT_SAMPLE_SEL;
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
            hcnt_3x_opt <= H_OPT_SAMPLE_SEL;
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
        
        DE_3x <= ((hcnt_3x >= H_AVIDSTART) & (hcnt_3x < H_AVIDSTART+H_ACTIVE)) & ((vcnt_3x >= V_AVIDSTART) & (vcnt_3x < V_AVIDSTART+V_ACTIVE));
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
            hcnt_4x_aspfix <= hcnt_3x - 160;
        else
            hcnt_4x_aspfix <= hcnt_4x_aspfix + 1'b1;

        DE_3x_prev4x <= DE_3x;

        if ((pclk_4x_cnt == 0) & (line_change | frame_change)) begin  //aligned with posedge of pclk_1x
            hcnt_4x <= 0;
            hcnt_4x_opt <= H_OPT_SAMPLE_SEL;
            hcnt_4x_opt_ctr <= 0;
            line_out_idx_4x <= 0;
            if (frame_change)
                vcnt_4x <= -1;
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
        DE_4x <= ((hcnt_4x >= H_AVIDSTART) & (hcnt_4x < H_AVIDSTART+H_ACTIVE)) & ((vcnt_4x >= V_AVIDSTART) & (vcnt_4x < V_AVIDSTART+V_ACTIVE));
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
                vcnt_5x <= -1;
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
        DE_5x <= ((hcnt_5x >= H_AVIDSTART-H_L5BORDER) & (hcnt_5x < H_AVIDSTART+H_ACTIVE+H_L5BORDER)) & ((vcnt_5x >= V_AVIDSTART) & (vcnt_5x < V_AVIDSTART+V_ACTIVE));
    end
end

endmodule
