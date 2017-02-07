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

`define TRUE                    1'b1
`define FALSE                   1'b0
`define HI                      1'b1
`define LO                      1'b0

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
`define FALSE_FIELD             (fpga_vsyncgen[`VSYNCGEN_CHOPMID_BIT] & (FID_in == `FID_ODD))

`define VSYNC_LEADING_EDGE      ((prev_vs == `HI) & (VSYNC_in == `LO))
`define VSYNC_TRAILING_EDGE     ((prev_vs == `LO) & (VSYNC_in == `HI))

`define HSYNC_LEADING_EDGE      ((prev_hs == `HI) & (HSYNC_in == `LO))
`define HSYNC_TRAILING_EDGE     ((prev_hs == `LO) & (HSYNC_in == `HI))

module scanconverter (
    input reset_n,
    input [7:0] R_in,
    input [7:0] G_in,
    input [7:0] B_in,
    input FID_in,
    input VSYNC_in,
    input HSYNC_in,
    input PCLK_in,
    input [31:0] h_info,
    input [31:0] h_info2,
    input [31:0] v_info,
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
    output [10:0] lines_out
);

wire pclk_1x, pclk_2x, pclk_3x, pclk_4x, pclk_5x;
wire linebuf_rdclock;

wire pclk_act;
wire [2:0] line_id_act;
wire [2:0] col_id_act;

wire pclk_2x_lock, pclk_3x_lock;

wire HSYNC_act, VSYNC_act;
reg HSYNC_1x, HSYNC_2x, HSYNC_3x, HSYNC_4x, HSYNC_5x, HSYNC_pp1, HSYNC_pp2;
reg VSYNC_1x, VSYNC_2x, VSYNC_pp1, VSYNC_pp2;

reg [11:0] HSYNC_start;

reg FID_1x, FID_prev;

wire DE_act;
reg DE_pp1, DE_pp2;

wire [11:0] linebuf_hoffset; //Offset for line (max. 2047 pixels), MSB indicates which line is read/written
wire [11:0] hcnt_act;
reg [11:0] hcnt_1x, hcnt_2x, hcnt_3x, hcnt_4x, hcnt_5x, hcnt_4x_aspfix, hcnt_2x_opt, hcnt_3x_opt, hcnt_4x_opt, hcnt_5x_opt, hcnt_5x_hscomp;

reg [2:0] hcnt_2x_opt_ctr, hcnt_3x_opt_ctr, hcnt_4x_opt_ctr, hcnt_5x_opt_ctr;

wire [10:0] vcnt_act;
reg [10:0] vcnt_1x, vcnt_1x_tvp, vcnt_2x, vcnt_4x, vcnt_5x, vcnt_2x_ref, vcnt_3x_ref, vcnt_4x_ref, vcnt_5x_ref, lines_1x, lines_2x, lines_4x, lines_5x;       //max. 2047
reg [9:0] vcnt_3x, lines_3x; //max. 1023

reg DE_3x_prev4x;

reg pclk_1x_prev3x;
reg [1:0] pclk_3x_cnt;

reg pclk_1x_prev4x;
reg [1:0] pclk_4x_cnt;

reg pclk_1x_prev5x;
reg pclk_1x_prevprev5x;
reg [2:0] pclk_5x_cnt;

// Data enable
reg DE_1x, DE_2x, DE_3x, DE_4x, DE_5x;

reg prev_hs, prev_vs;
reg [11:0] hmax[0:1];
reg line_idx;
reg [1:0] line_out_idx_2x, line_out_idx_3x, line_out_idx_4x;
reg [2:0] line_out_idx_5x;

reg [23:0] warn_h_unstable, warn_pll_lock_lost, warn_pll_lock_lost_3x;

reg [10:0] H_ACTIVE;    //max. 2047
reg [8:0] H_BACKPORCH;  //max. 511
reg [10:0] V_ACTIVE;    //max. 2047
reg [5:0] V_BACKPORCH;  //max. 63
reg [1:0] V_SCANLINEMODE;
reg [4:0] V_SCANLINEID;
reg [7:0] H_SCANLINESTR;
reg [5:0] V_MASK;
reg [2:0] V_MULTMODE;
reg [1:0] H_MULTMODE;
reg [9:0] H_MASK;
reg [3:0] H_MASK_BR;
reg [9:0] H_OPT_STARTOFF;
reg [2:0] H_OPT_SCALE;
reg [2:0] H_OPT_SAMPLE_MULT;
reg [2:0] H_OPT_SAMPLE_SEL;
reg [9:0] H_L5BORDER;

//8 bits per component -> 16.7M colors
reg [7:0] R_1x, G_1x, B_1x, R_2x, G_2x, B_2x, R_3x, G_3x, B_3x, R_4x, G_4x, B_4x, R_5x, G_5x, B_5x, R_pp1, G_pp1, B_pp1;
wire [7:0] R_lbuf, G_lbuf, B_lbuf;
wire [7:0] R_act, G_act, B_act;

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
    input [11:0] hoffset;
    input [11:0] hstart;
    input [11:0] hend;
    input [10:0] voffset;
    input [10:0] vstart;
    input [10:0] vend;
    begin
        if (enable & ((hoffset < hstart) | (hoffset >= hend) | (voffset < vstart) | (voffset >= vend)))
            apply_mask = {2'h0, brightness, 2'h0};
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
begin
    case (V_MULTMODE)
    `V_MULTMODE_1X: begin
        R_act = R_1x;
        G_act = G_1x;
        B_act = B_1x;
        DE_act = DE_1x;
        HSYNC_act = HSYNC_1x;
        VSYNC_act = VSYNC_1x;
        lines_out = lines_1x;
        linebuf_rdclock = 0;
        linebuf_hoffset = 0;
        pclk_act = pclk_1x;
        line_id_act = {2'b00, vcnt_1x[0]};
        col_id_act = {2'b00, hcnt_1x[0]};
        hcnt_act = hcnt_1x;
        vcnt_act = vcnt_1x;
    end
    `V_MULTMODE_2X: begin
        R_act = R_2x;
        G_act = G_2x;
        B_act = B_2x;
        DE_act = DE_2x;
        HSYNC_act = HSYNC_2x;
        VSYNC_act = VSYNC_2x;
        lines_out = lines_2x;
        linebuf_rdclock = pclk_2x;
        case (H_MULTMODE)
        `H_MULTMODE_FULLWIDTH: begin
            linebuf_hoffset = hcnt_2x;
            pclk_act = pclk_2x;
            col_id_act = {2'b00, hcnt_2x[0]};
        end
        `H_MULTMODE_OPTIMIZED: begin
            linebuf_hoffset = hcnt_2x_opt;
            pclk_act = pclk_1x;
            col_id_act = {2'b00, hcnt_2x[1]};;
        end
        default: begin
            linebuf_hoffset = hcnt_2x;
            pclk_act = pclk_2x;
            col_id_act = {2'b00, hcnt_2x[0]};
        end
        endcase
        line_id_act = {1'b0, line_out_idx_2x[1], line_out_idx_2x[0]^FID_1x};
        hcnt_act = hcnt_2x;
        vcnt_act = vcnt_2x_ref;
    end
    `V_MULTMODE_3X: begin
        R_act = R_3x;
        G_act = G_3x;
        B_act = B_3x;
        HSYNC_act = HSYNC_3x;
        VSYNC_act = VSYNC_1x;
        DE_act = DE_3x;
        lines_out = {1'b0, lines_3x};
        line_id_act = {1'b0, line_out_idx_3x};
        vcnt_act = vcnt_3x_ref;
        case (H_MULTMODE)
        `H_MULTMODE_FULLWIDTH: begin
            linebuf_rdclock = pclk_3x;
            linebuf_hoffset = hcnt_3x;
            pclk_act = pclk_3x;
            hcnt_act = hcnt_3x;
            col_id_act = {2'b00, hcnt_3x[0]};
        end
        `H_MULTMODE_ASPECTFIX: begin
            linebuf_rdclock = pclk_4x;
            linebuf_hoffset = hcnt_4x_aspfix;
            pclk_act = pclk_4x;
            hcnt_act = hcnt_4x_aspfix;
            col_id_act = {2'b00, hcnt_4x[0]};
        end
        `H_MULTMODE_OPTIMIZED: begin
            linebuf_rdclock = pclk_3x;
            linebuf_hoffset = hcnt_3x_opt;
            pclk_act = pclk_3x;
            hcnt_act = hcnt_3x;
            col_id_act = hcnt_3x_opt_ctr;
        end
        default: begin
            linebuf_rdclock = pclk_3x;
            linebuf_hoffset = hcnt_3x;
            pclk_act = pclk_3x;
            hcnt_act = hcnt_3x;
            col_id_act = {2'b00, hcnt_3x[0]};
        end
        endcase
    end
    `V_MULTMODE_4X: begin
        R_act = R_4x;
        G_act = G_4x;
        B_act = B_4x;
        HSYNC_act = HSYNC_4x;
        VSYNC_act = VSYNC_1x;
        DE_act = DE_4x;
        lines_out = lines_4x;
        line_id_act = {1'b0, line_out_idx_4x};
        vcnt_act = vcnt_4x_ref;
        linebuf_rdclock = pclk_4x;
        pclk_act = pclk_4x;
        hcnt_act = hcnt_4x;
        case (H_MULTMODE)
        `H_MULTMODE_FULLWIDTH: begin
            linebuf_hoffset = hcnt_4x;
            col_id_act = {2'b00, hcnt_4x[0]};
        end
        `H_MULTMODE_OPTIMIZED: begin
            linebuf_hoffset = hcnt_4x_opt;
            col_id_act = hcnt_4x_opt_ctr;
        end
        default: begin
            linebuf_hoffset = hcnt_4x;
            col_id_act = {2'b00, hcnt_4x[0]};
        end
        endcase
    end
    `V_MULTMODE_5X: begin
        R_act = R_5x;
        G_act = G_5x;
        B_act = B_5x;
        HSYNC_act = HSYNC_5x;
        VSYNC_act = VSYNC_1x;
        DE_act = DE_5x;
        lines_out = lines_5x;
        line_id_act = {2'b00, line_out_idx_5x};
        vcnt_act = vcnt_5x_ref;
        linebuf_rdclock = pclk_5x;
        pclk_act = pclk_5x;
        hcnt_act = hcnt_5x;
        case (H_MULTMODE)
        `H_MULTMODE_FULLWIDTH: begin
            linebuf_hoffset = hcnt_5x_hscomp;
            col_id_act = {2'b00, hcnt_5x[0]};
        end
        `H_MULTMODE_OPTIMIZED: begin
            linebuf_hoffset = hcnt_5x_opt;
            col_id_act = hcnt_5x_opt_ctr;
        end
        default: begin
            linebuf_hoffset = hcnt_5x_hscomp;
            col_id_act = {2'b00, hcnt_5x[0]};
        end
        endcase
    end
    default: begin
        R_act = R_1x;
        G_act = G_1x;
        B_act = B_1x;
        DE_act = DE_1x;
        HSYNC_act = HSYNC_1x;
        VSYNC_act = VSYNC_1x;
        lines_out = lines_1x;
        linebuf_rdclock = 0;
        linebuf_hoffset = 0;
        pclk_act = pclk_1x;
        line_id_act = {2'b00, vcnt_1x[0]};
        col_id_act = {2'b00, hcnt_1x[0]};
        hcnt_act = hcnt_1x;
        vcnt_act = vcnt_1x;
    end
    endcase
end

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

//TODO: add secondary buffers for interlaced signals with alternative field order
linebuf linebuf_rgb (
    .data ( {R_1x, G_1x, B_1x} ),
    .rdaddress ( linebuf_hoffset + (~line_idx << 11) ),
    .rdclock ( linebuf_rdclock ),
    .wraddress ( hcnt_1x + (line_idx << 11) ),
    .wrclock ( pclk_1x ),
    .wren ( 1'b1 ),
    .q ( {R_lbuf, G_lbuf, B_lbuf} )
);

//Postprocess pipeline
always @(posedge pclk_act or negedge reset_n)
begin
    if (!reset_n)
        begin
            R_pp1 <= 8'h00;
            G_pp1 <= 8'h00;
            B_pp1 <= 8'h00;
            HSYNC_pp1 <= 1'b0;
            VSYNC_pp1 <= 1'b0;
            DE_pp1 <= 1'b0;
            R_out <= 8'h00;
            G_out <= 8'h00;
            B_out <= 8'h00;
            HSYNC_out <= 1'b0;
            VSYNC_out <= 1'b0;
            DE_out <= 1'b0;
        end
    else
        begin
            R_pp1 <= apply_scanlines(V_SCANLINEMODE, R_act, H_SCANLINESTR, V_SCANLINEID, line_id_act, col_id_act, FID_1x);
            G_pp1 <= apply_scanlines(V_SCANLINEMODE, G_act, H_SCANLINESTR, V_SCANLINEID, line_id_act, col_id_act, FID_1x);
            B_pp1 <= apply_scanlines(V_SCANLINEMODE, B_act, H_SCANLINESTR, V_SCANLINEID, line_id_act, col_id_act, FID_1x);
            HSYNC_pp1 <= HSYNC_act;
            VSYNC_pp1 <= VSYNC_act;
            DE_pp1 <= DE_act;
            
            R_out <= apply_mask(1, R_pp1, H_MASK_BR, hcnt_act, H_BACKPORCH+H_MASK+2'h2, H_BACKPORCH+H_ACTIVE-H_MASK+2'h2, vcnt_act, V_BACKPORCH+V_MASK, V_BACKPORCH+V_ACTIVE-V_MASK);
            G_out <= apply_mask(1, G_pp1, H_MASK_BR, hcnt_act, H_BACKPORCH+H_MASK+2'h2, H_BACKPORCH+H_ACTIVE-H_MASK+2'h2, vcnt_act, V_BACKPORCH+V_MASK, V_BACKPORCH+V_ACTIVE-V_MASK);
            B_out <= apply_mask(1, B_pp1, H_MASK_BR, hcnt_act, H_BACKPORCH+H_MASK+2'h2, H_BACKPORCH+H_ACTIVE-H_MASK+2'h2, vcnt_act, V_BACKPORCH+V_MASK, V_BACKPORCH+V_ACTIVE-V_MASK);
            HSYNC_out <= HSYNC_pp1;
            VSYNC_out <= VSYNC_pp1;
            DE_out <= DE_pp1;
        end
end

//Generate a warning signal from horizontal instability or PLL sync loss
always @(posedge pclk_1x or negedge reset_n)
begin
    if (!reset_n)
        begin
            warn_h_unstable <= 1'b0;
            warn_pll_lock_lost <= 1'b0;
            warn_pll_lock_lost_3x <= 1'b0;
        end
    else
        begin
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

//Buffer the inputs using input pixel clock and generate 1x signals
always @(posedge pclk_1x or negedge reset_n)
begin
    if (!reset_n)
        begin
            hcnt_1x <= 0;
            hmax[0] <= 0;
            hmax[1] <= 0;
            line_idx <= 0;
            vcnt_1x <= 0;
            vcnt_1x_tvp <= 0;
            FID_prev <= 0;
            fpga_vsyncgen <= 0;
            lines_1x <= 0;
            H_MULTMODE <= 0;
            V_MULTMODE <= 0;
            H_ACTIVE <= 0;
            H_BACKPORCH <= 0;
            H_MASK <= 0;
            V_ACTIVE <= 0;
            V_BACKPORCH <= 0;
            V_MASK <= 0;
            V_SCANLINEMODE <= 0;
            V_SCANLINEID <= 0;
            H_SCANLINESTR <= 0;
            H_OPT_STARTOFF <= 0;
            H_OPT_SAMPLE_MULT <= 0;
            H_OPT_SAMPLE_SEL <= 0;
            H_OPT_SCALE <= 0;
            H_L5BORDER <= 0;
            H_MASK_BR <= 0;
            prev_hs <= 0;
            prev_vs <= 0;
            HSYNC_start <= 0;
            R_1x <= 8'h00;
            G_1x <= 8'h00;
            B_1x <= 8'h00;
            HSYNC_1x <= 0;
            VSYNC_1x <= 0;
            DE_1x <= 0;
            FID_1x <= 0;
        end
    else
        begin
            if (`HSYNC_TRAILING_EDGE)
                begin
                    hcnt_1x <= 0;
                    hmax[line_idx] <= hcnt_1x;
                    line_idx <= line_idx ^ 1'b1;
                    vcnt_1x <= vcnt_1x + 1'b1;
                    vcnt_1x_tvp <= vcnt_1x_tvp + 1'b1;
                    FID_1x <= fpga_vsyncgen[`VSYNCGEN_CHOPMID_BIT] ? 1'b0 : (fpga_vsyncgen[`VSYNCGEN_GENMID_BIT] ? (vcnt_1x > (V_BACKPORCH + V_ACTIVE)) : FID_in);
                end
            else
                begin
                    hcnt_1x <= hcnt_1x + 1'b1;
                end

            if (`VSYNC_TRAILING_EDGE) //should be checked at every pclk_1x?
                begin
                    vcnt_1x_tvp <= 0;
                    FID_prev <= FID_in;
                    
                    // detect non-interlaced signal with odd-odd field signaling (TVP7002 detects it as interlaced with analog sync inputs).
                    // FID is updated at leading edge of VSYNC, so trailing edge has the status of current field.
                    if (FID_in == FID_prev)
                        fpga_vsyncgen[`VSYNCGEN_CHOPMID_BIT] <= `FALSE;
                    else if (FID_in == `FID_ODD)   // TVP7002 falsely indicates field change with (vcnt < active_lines)
                        fpga_vsyncgen[`VSYNCGEN_CHOPMID_BIT] <= (vcnt_1x_tvp < `MIN_VALID_LINES);

                    if (!(`FALSE_FIELD))
                        begin
                            vcnt_1x <= 0;
                            lines_1x <= vcnt_1x;
                        end
                    
                    //Read configuration data from CPU
                    H_MULTMODE <= h_info[31:30];    // Horizontal scaling mode
                    V_MULTMODE <= v_info[26:24];    // Line multiply mode

                    H_ACTIVE <= h_info[19:9];       // Horizontal active length from by the CPU - 11bits (0...2047)
                    H_BACKPORCH <= h_info[7:0];     // Horizontal backporch length from by the CPU - 8bits (0...255)
                    H_MASK <= h_info[29:20];
                    V_ACTIVE <= v_info[17:7];       // Vertical active length from by the CPU, 11bits (0...2047)
                    V_BACKPORCH <= v_info[5:0];     // Vertical backporch length from by the CPU, 6bits (0...64)
                    V_MASK <= v_info[23:18];
                    
                    H_SCANLINESTR <= ((h_info2[22:19]+8'h01)<<4)-1'b1;
                    V_SCANLINEMODE <= v_info[31:30];
                    case (v_info[26:24])
                        `V_MULTMODE_1X, `V_MULTMODE_2X: V_SCANLINEID <= (5'b00001 << v_info[28]);
                        `V_MULTMODE_3X: V_SCANLINEID <= (5'b00001 << {v_info[28], 1'b0});
                        `V_MULTMODE_4X: V_SCANLINEID <= (5'b00011 << {v_info[28], 1'b0});
                        `V_MULTMODE_5X: V_SCANLINEID <= (5'b00011 << {2{v_info[28]}});
                    endcase
                    
                    H_OPT_STARTOFF <= h_info2[9:0];
                    H_OPT_SAMPLE_MULT <= h_info2[12:10];
                    H_OPT_SAMPLE_SEL <= h_info2[15:13];
                    H_OPT_SCALE <= h_info2[18:16];
                    H_MASK_BR <= h_info2[26:23];
                    H_L5BORDER <= h_info2[27] ? (1920-h_info[19:9])/2 : (1600-h_info[19:9])/2;
                end
                
            prev_hs <= HSYNC_in;
            prev_vs <= VSYNC_in;

            // record start position of HSYNC
            if (`HSYNC_LEADING_EDGE)
                HSYNC_start <= hcnt_1x;
                
            // Check if extra vsync needed
            fpga_vsyncgen[`VSYNCGEN_GENMID_BIT] <= (lines_1x > ({1'b0, V_ACTIVE} << 1)) ? 1'b1 : 1'b0;
                
                
            R_1x <= R_in;
            G_1x <= G_in;
            B_1x <= B_in;
            HSYNC_1x <= HSYNC_in;

            // Ignore possible invalid vsyncs generated by TVP7002
            if (vcnt_1x > V_ACTIVE)
                VSYNC_1x <= VSYNC_in;
            
            DE_1x <= ((hcnt_1x >= H_BACKPORCH) & (hcnt_1x < H_BACKPORCH + H_ACTIVE)) & ((vcnt_1x >= V_BACKPORCH) & (vcnt_1x < V_BACKPORCH + V_ACTIVE));
        end
end

//Generate 2x signals for linedouble
always @(posedge pclk_2x or negedge reset_n)
begin
    if (!reset_n)
        begin
            hcnt_2x <= 0;
            vcnt_2x <= 0;
            vcnt_2x_ref <= 0;
            lines_2x <= 0;
            R_2x <= 8'h00;
            G_2x <= 8'h00;
            B_2x <= 8'h00;
            HSYNC_2x <= 0;
            VSYNC_2x <= 0;
            DE_2x <= 0;
            line_out_idx_2x <= 0;
            hcnt_2x_opt <= 0;
            hcnt_2x_opt_ctr <= 0;
        end
    else
        begin
            if ((pclk_1x == 1'b0) & `HSYNC_TRAILING_EDGE)   //aligned with posedge of pclk_1x
                begin
                    hcnt_2x <= 0;
                    line_out_idx_2x <= 0;
                    hcnt_2x_opt <= H_OPT_SAMPLE_SEL;
                    hcnt_2x_opt_ctr <= 0;
                end
            else if (hcnt_2x == hmax[~line_idx])
                begin
                    hcnt_2x <= 0;
                    line_out_idx_2x <= line_out_idx_2x + 1'b1;
                    hcnt_2x_opt <= H_OPT_SAMPLE_SEL;
                    hcnt_2x_opt_ctr <= 0;
                end
            else
                begin
                    hcnt_2x <= hcnt_2x + 1'b1;
                    if (hcnt_2x >= H_OPT_STARTOFF)
                        begin
                            if (hcnt_2x_opt_ctr == H_OPT_SCALE-1'b1)
                                begin
                                    hcnt_2x_opt <= hcnt_2x_opt + H_OPT_SAMPLE_MULT;
                                    hcnt_2x_opt_ctr <= 0;
                                end
                            else
                                hcnt_2x_opt_ctr <= hcnt_2x_opt_ctr + 1'b1;
                        end
                end

            if ((pclk_1x == 1'b0) & `VSYNC_TRAILING_EDGE & !(`FALSE_FIELD)) //aligned with posedge of pclk_1x
                begin
                    vcnt_2x <= 0;
                    vcnt_2x_ref <= 0;
                    if (fpga_vsyncgen[`VSYNCGEN_GENMID_BIT] == 1'b0)
                        lines_2x <= vcnt_2x;
                end
            else if (hcnt_2x == hmax[~line_idx])
                begin
                    if ((fpga_vsyncgen[`VSYNCGEN_GENMID_BIT] == 1'b1) & (vcnt_2x == lines_1x-1))
                        begin
                            vcnt_2x <= 0;
                            vcnt_2x_ref <= 0;
                            lines_2x <= lines_1x;
                        end
                    else
                        begin
                            vcnt_2x <= vcnt_2x + 1'b1;
                            if (line_out_idx_2x == 1)
                                vcnt_2x_ref <= vcnt_2x_ref + 1'b1;
                        end
                end
                
            if (pclk_1x == 1'b0)
                begin
                    if (fpga_vsyncgen[`VSYNCGEN_GENMID_BIT] == 1'b1)
                        VSYNC_2x <= (vcnt_2x >= lines_1x - `VSYNCGEN_LEN) ? 1'b0 : 1'b1;
                    else if (vcnt_1x > V_ACTIVE)
                        VSYNC_2x <= VSYNC_in;
                end

                
            R_2x <= R_lbuf;
            G_2x <= G_lbuf;
            B_2x <= B_lbuf;
            HSYNC_2x <= ~(hcnt_2x >= HSYNC_start);
            DE_2x <= ((hcnt_2x >= H_BACKPORCH) & (hcnt_2x < H_BACKPORCH + H_ACTIVE)) & ((vcnt_2x >= (V_BACKPORCH<<1)) & (vcnt_2x < ((V_BACKPORCH + V_ACTIVE)<<1)));
        end
end

always @(posedge pclk_3x or negedge reset_n)
begin
    if (!reset_n)
        begin
            hcnt_3x <= 0;
            vcnt_3x <= 0;
            vcnt_3x_ref <= 0;
            lines_3x <= 0;
            R_3x <= 8'h00;
            G_3x <= 8'h00;
            B_3x <= 8'h00;
            HSYNC_3x <= 0;
            DE_3x <= 0;
            pclk_3x_cnt <= 0;
            pclk_1x_prev3x <= 0;
            line_out_idx_3x <= 0;
            hcnt_3x_opt <= 0;
            hcnt_3x_opt_ctr <= 0;
        end
    else
        begin
            if ((pclk_3x_cnt == 0) & `HSYNC_TRAILING_EDGE)  //aligned with posedge of pclk_1x
                begin
                    hcnt_3x <= 0;
                    line_out_idx_3x <= 0;
                    hcnt_3x_opt <= H_OPT_SAMPLE_SEL;
                    hcnt_3x_opt_ctr <= 0;
                end
            else if (hcnt_3x == hmax[~line_idx])
                begin
                    hcnt_3x <= 0;
                    line_out_idx_3x <= line_out_idx_3x + 1'b1;
                    hcnt_3x_opt <= H_OPT_SAMPLE_SEL;
                    hcnt_3x_opt_ctr <= 0;
                end
            else
                begin
                    hcnt_3x <= hcnt_3x + 1'b1;
                    if (hcnt_3x >= H_OPT_STARTOFF)
                        begin
                            if (hcnt_3x_opt_ctr == H_OPT_SCALE-1'b1)
                                begin
                                    hcnt_3x_opt <= hcnt_3x_opt + H_OPT_SAMPLE_MULT;
                                    hcnt_3x_opt_ctr <= 0;
                                end
                            else
                                hcnt_3x_opt_ctr <= hcnt_3x_opt_ctr + 1'b1;
                        end
                end

            if ((pclk_3x_cnt == 0) & `VSYNC_TRAILING_EDGE & !(`FALSE_FIELD)) //aligned with posedge of pclk_1x
                begin
                    vcnt_3x <= 0;
                    vcnt_3x_ref <= 0;
                    lines_3x <= vcnt_3x;
                end
            else if (hcnt_3x == hmax[~line_idx])
                begin
                    vcnt_3x <= vcnt_3x + 1'b1;
                    if (line_out_idx_3x == 2)
                        vcnt_3x_ref <= vcnt_3x_ref + 1'b1;
                end
            
            //track pclk_3x alignment to pclk_1x rising edge (pclk_1x=1 @ 120deg & pclk_1x=0 @ 240deg)
            if (((pclk_1x_prev3x == 1'b1) & (pclk_1x == 1'b0)) | (pclk_3x_cnt == 2'h2))
                pclk_3x_cnt <= 0;
            else
                pclk_3x_cnt <= pclk_3x_cnt + 1'b1;
                
            pclk_1x_prev3x <= pclk_1x;

            
            R_3x <= R_lbuf;
            G_3x <= G_lbuf;
            B_3x <= B_lbuf;
            HSYNC_3x <= ~(hcnt_3x >= HSYNC_start);
            //TODO: VSYNC_3x
            DE_3x <= ((hcnt_3x >= H_BACKPORCH) & (hcnt_3x < H_BACKPORCH + H_ACTIVE)) & ((vcnt_3x_ref >= V_BACKPORCH) & (vcnt_3x_ref < V_BACKPORCH + V_ACTIVE));
        end
end

always @(posedge pclk_4x or negedge reset_n)
begin
    if (!reset_n)
        begin
            hcnt_4x_aspfix <= 0;
            DE_3x_prev4x <= 0;
            hcnt_4x <= 0;
            vcnt_4x <= 0;
            vcnt_4x_ref <= 0;
            lines_4x <= 0;
            R_4x <= 8'h00;
            G_4x <= 8'h00;
            B_4x <= 8'h00;
            HSYNC_4x <= 0;
            DE_4x <= 0;
            pclk_4x_cnt <= 0;
            pclk_1x_prev4x <= 0;
            line_out_idx_4x <= 0;
            hcnt_4x_opt <= 0;
            hcnt_4x_opt_ctr <= 0;
        end
    else
        begin
            // TODO: better implementation
            if ((DE_3x == 1) & (DE_3x_prev4x == 0))
                hcnt_4x_aspfix <= hcnt_3x - 160;
            else
                hcnt_4x_aspfix <= hcnt_4x_aspfix + 1'b1;

            DE_3x_prev4x <= DE_3x;


            if ((pclk_4x_cnt == 0) & `HSYNC_TRAILING_EDGE)  //aligned with posedge of pclk_1x
                begin
                    hcnt_4x <= 0;
                    line_out_idx_4x <= 0;
                    hcnt_4x_opt <= H_OPT_SAMPLE_SEL;
                    hcnt_4x_opt_ctr <= 0;
                end
            else if (hcnt_4x == hmax[~line_idx])
                begin
                    hcnt_4x <= 0;
                    line_out_idx_4x <= line_out_idx_4x + 1'b1;
                    hcnt_4x_opt <= H_OPT_SAMPLE_SEL;
                    hcnt_4x_opt_ctr <= 0;
                end
            else
                begin
                    hcnt_4x <= hcnt_4x + 1'b1;
                    if (hcnt_4x >= H_OPT_STARTOFF)
                        begin
                            if (hcnt_4x_opt_ctr == H_OPT_SCALE-1'b1)
                                begin
                                    hcnt_4x_opt <= hcnt_4x_opt + H_OPT_SAMPLE_MULT;
                                    hcnt_4x_opt_ctr <= 0;
                                end
                            else
                                hcnt_4x_opt_ctr <= hcnt_4x_opt_ctr + 1'b1;
                        end
                end
                
            if ((pclk_4x_cnt == 0) & `VSYNC_TRAILING_EDGE & !(`FALSE_FIELD)) //aligned with posedge of pclk_1x
                begin
                    vcnt_4x <= 0;
                    vcnt_4x_ref <= 0;
                    lines_4x <= vcnt_4x;
                end
            else if (hcnt_4x == hmax[~line_idx])
                begin
                    vcnt_4x <= vcnt_4x + 1'b1;
                    if (line_out_idx_4x == 3)
                        vcnt_4x_ref <= vcnt_4x_ref + 1'b1;
                end
            
            //track pclk_4x alignment to pclk_1x rising edge (pclk_1x=1 @ 180deg & pclk_1x=0 @ 270deg)
            if (((pclk_1x_prev4x == 1'b1) & (pclk_1x == 1'b0)) | (pclk_4x_cnt == 2'h3))
                pclk_4x_cnt <= 0;
            else
                pclk_4x_cnt <= pclk_4x_cnt + 1'b1;
                
            pclk_1x_prev4x <= pclk_1x;

            
            R_4x <= R_lbuf;
            G_4x <= G_lbuf;
            B_4x <= B_lbuf;
            HSYNC_4x <= ~(hcnt_4x >= HSYNC_start);
            //TODO: VSYNC_4x
            DE_4x <= ((hcnt_4x >= H_BACKPORCH) & (hcnt_4x < H_BACKPORCH + H_ACTIVE)) & ((vcnt_4x_ref >= V_BACKPORCH) & (vcnt_4x_ref < V_BACKPORCH + V_ACTIVE));
        end
end

always @(posedge pclk_5x or negedge reset_n)
begin
    if (!reset_n)
        begin
            hcnt_5x <= 0;
            vcnt_5x <= 0;
            vcnt_5x_ref <= 0;
            lines_5x <= 0;
            R_5x <= 8'h00;
            G_5x <= 8'h00;
            B_5x <= 8'h00;
            HSYNC_5x <= 0;
            DE_5x <= 0;
            pclk_5x_cnt <= 0;
            pclk_1x_prev5x <= 0;
            pclk_1x_prevprev5x <= 0;
            line_out_idx_5x <= 0;
            hcnt_5x_opt <= 0;
            hcnt_5x_opt_ctr <= 0;
            hcnt_5x_hscomp <= 0;
        end
    else
        begin
            if ((pclk_5x_cnt == 0) & `HSYNC_TRAILING_EDGE)  //aligned with posedge of pclk_1x
                begin
                    hcnt_5x <= 0;
                    line_out_idx_5x <= 0;
                    hcnt_5x_opt <= 120 + H_OPT_SAMPLE_SEL;
                    hcnt_5x_opt_ctr <= 0;
                end
            else if (hcnt_5x == hmax[~line_idx])
                begin
                    hcnt_5x <= 0;
                    line_out_idx_5x <= line_out_idx_5x + 1'b1;
                    hcnt_5x_opt <= 120 + H_OPT_SAMPLE_SEL;
                    hcnt_5x_opt_ctr <= 0;
                end
            else
                begin
                    hcnt_5x <= hcnt_5x + 1'b1;
                    if (hcnt_5x >= H_OPT_STARTOFF)
                        begin
                            if (hcnt_5x_opt_ctr == H_OPT_SCALE-1'b1)
                                begin
                                    hcnt_5x_opt <= hcnt_5x_opt + H_OPT_SAMPLE_MULT;
                                    hcnt_5x_opt_ctr <= 0;
                                end
                            else
                                hcnt_5x_opt_ctr <= hcnt_5x_opt_ctr + 1'b1;
                        end
                end
                
            if ((pclk_5x_cnt == 0) & `VSYNC_TRAILING_EDGE & !(`FALSE_FIELD)) //aligned with posedge of pclk_1x
                begin
                    vcnt_5x <= 0;
                    vcnt_5x_ref <= 0;
                    lines_5x <= vcnt_5x;
                end
            else if (hcnt_5x == hmax[~line_idx])
                begin
                    vcnt_5x <= vcnt_5x + 1'b1;
                    if (line_out_idx_5x == 4)
                        vcnt_5x_ref <= vcnt_5x_ref + 1'b1;
                end
            
            //track pclk_5x alignment to pclk_1x rising edge (pclk_1x=1 @ 144deg & pclk_1x=0 @ 216deg & pclk_1x=0 @ 288deg)
            if (((pclk_1x_prevprev5x == 1'b1) & (pclk_1x_prev5x == 1'b0)) | (pclk_5x_cnt == 3'h4))
                pclk_5x_cnt <= 0;
            else
                pclk_5x_cnt <= pclk_5x_cnt + 1'b1;
                
            pclk_1x_prev5x <= pclk_1x;
            pclk_1x_prevprev5x <= pclk_1x_prev5x;
            
            hcnt_5x_hscomp <= hcnt_5x + 121;
            
            
            R_5x <= R_lbuf;
            G_5x <= G_lbuf;
            B_5x <= B_lbuf;
            HSYNC_5x <= ~(hcnt_5x >= HSYNC_start);
            //TODO: VSYNC_5x
            DE_5x <= ((hcnt_5x >= H_BACKPORCH - H_L5BORDER) & (hcnt_5x < H_BACKPORCH + H_ACTIVE + H_L5BORDER)) & ((vcnt_5x_ref >= V_BACKPORCH) & (vcnt_5x_ref < V_BACKPORCH + V_ACTIVE));
        end
end

endmodule
