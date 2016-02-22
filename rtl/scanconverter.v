//
// Copyright (C) 2015  Markus Hiienkari <mhiienka@niksula.hut.fi>
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

`define LINEMULT_DISABLE		2'h0
`define LINEMULT_DOUBLE			2'h1
`define LINEMULT_TRIPLE			2'h2

`define LINETRIPLE_M0			2'h0
`define LINETRIPLE_M1			2'h1
`define LINETRIPLE_M2			2'h2
`define LINETRIPLE_M3			2'h3

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
	input [7:0] R_in,
	input [7:0] G_in,
	input [7:0] B_in,
	input FID_in,
	input VSYNC_in,
	input HSYNC_in,
	input PCLK_in,
	input [31:0] h_info,
	input [31:0] v_info,
	output reg [7:0] R_out,
	output reg [7:0] G_out,
	output reg [7:0] B_out,
	output reg HSYNC_out,
	output reg VSYNC_out,
	output PCLK_out,
	output reg DATA_enable,
	output h_unstable,
    output reg [1:0] fpga_vsyncgen,
	output [2:0] pclk_lock,
	output [2:0] pll_lock_lost,
	output [10:0] lines_out
);

wire pclk_1x, pclk_2x, pclk_3x, pclk_4x, pclk_3x_h1x, pclk_3x_h4x, pclk_3x_h5x;
wire pclk_out_1x, pclk_out_2x, pclk_out_3x, pclk_out_4x, pclk_out_3x_h4x, pclk_out_3x_h5x;
wire linebuf_rdclock;

wire pclk_act;
wire [1:0] slid_act;

wire pclk_2x_lock, pclk_3x_lock, pclk_3x_lowfreq_lock;

wire HSYNC_act, VSYNC_act;
reg HSYNC_1x, HSYNC_2x, HSYNC_3x, HSYNC_3x_h1x, HSYNC_pp1;
reg VSYNC_1x, VSYNC_2x, VSYNC_pp1;

reg [11:0] HSYNC_start;

reg FID_prev;

wire DATA_enable_act;
reg DATA_enable_pp1;

wire [11:0] linebuf_hoffset; //Offset for line (max. 2047 pixels), MSB indicates which line is read/written
wire [11:0] hcnt_act;
reg [11:0] hcnt_1x, hcnt_2x, hcnt_3x, hcnt_4x, hcnt_3x_h1x, hcnt_3x_h4x, hcnt_3x_h5x;

wire [10:0] vcnt_act;
reg [10:0] vcnt_1x, vcnt_1x_tvp, vcnt_2x, lines_1x, lines_2x; 		//max. 2047
reg [9:0] vcnt_3x, vcnt_3x_h1x, lines_3x, lines_3x_h1x; //max. 1023

reg h_enable_3x_prev4x, h_enable_3x_prev3x_h4x, h_enable_3x_prev3x_h5x;
reg [1:0] hcnt_3x_h4x_ctr;
reg [1:0] hcnt_3x_h5x_ctr;

reg pclk_1x_prev3x, pclk_1x_prev3x_h1x, pclk_1x_prev3x_h4x;
reg [1:0] pclk_3x_cnt, pclk_3x_h1x_cnt;
reg [3:0] pclk_3x_h4x_cnt;

// Data enable
reg h_enable_1x, v_enable_1x;
reg h_enable_2x, v_enable_2x;
reg h_enable_3x, h_enable_3x_h1x, v_enable_3x, v_enable_3x_h1x;

reg prev_hs, prev_vs;
reg [11:0] hmax[0:1];
reg line_idx;

reg [23:0] warn_h_unstable, warn_pll_lock_lost, warn_pll_lock_lost_3x, warn_pll_lock_lost_3x_lowfreq;

reg [10:0] H_ACTIVE;	//max. 2047
reg [7:0] H_BACKPORCH;	//max. 255
reg [10:0] V_ACTIVE;	//max. 2047
reg [5:0] V_BACKPORCH;	//max. 63
reg V_SCANLINES;
reg V_SCANLINEDIR;
reg V_SCANLINEID;
reg [7:0] V_SCANLINESTR;
reg [5:0] V_MASK;
reg [1:0] H_LINEMULT;
reg [1:0] H_L3MODE;
reg [5:0] H_MASK;

//8 bits per component -> 16.7M colors
reg [7:0] R_1x, G_1x, B_1x, R_pp1, G_pp1, B_pp1;
wire [7:0] R_lbuf, G_lbuf, B_lbuf;
wire [7:0] R_act, G_act, B_act;


assign pclk_1x = PCLK_in;
assign pclk_lock = {pclk_2x_lock, pclk_3x_lock, pclk_3x_lowfreq_lock};

//Output sampled at the rising edge of active pclk
assign pclk_out_1x = PCLK_in;
assign pclk_out_2x = pclk_2x;
assign pclk_out_3x = pclk_3x;
assign pclk_out_4x = pclk_4x;
assign pclk_out_3x_h4x = pclk_3x_h4x;
assign pclk_out_3x_h5x = pclk_3x_h5x;

//Scanline generation
function [8:0] apply_scanlines;
    input enable;
    input dir;
    input [8:0] data;
    input [8:0] str;
    input [1:0] actid;
    input [1:0] lineid;
    input pixid;
    begin
        if (enable & (dir == 1'b0) & (actid == lineid))
            apply_scanlines = (data > str) ? (data-str) : 8'h00;
        else if (enable & (dir == 1'b1) & (actid == pixid))
            apply_scanlines = (data > str) ? (data-str) : 8'h00;
        else
            apply_scanlines = data;
    end
    endfunction

//Border masking
function [8:0] apply_mask;
	input enable;
    input [8:0] data;
    input [11:0] hoffset;
	input [11:0] hstart;
	input [11:0] hend;
    input [10:0] voffset;
	input [10:0] vstart;
	input [10:0] vend;
    begin
        if (enable & ((hoffset < hstart) | (hoffset >= hend) | (voffset < vstart) | (voffset >= vend)))
            apply_mask = 8'h00;
        else
            apply_mask = data;
        //apply_mask = (hoffset[0] ^ voffset[0]) ? 8'b11111111 : 8'b00000000;
    end
    endfunction


//Mux for active data selection
//
//Possible clock transfers:
//
// L3_MODE1: pclk_3x -> pclk_4x
// L3_MODE2: pclk_3x_h1x -> pclk_3x_h4x
// L3_MODE3: pclk_3x_h1x -> pclk_3x_h5x
//
//List of critical signals:
// DATA_enable_act, HSYNC_act
//
//Non-critical signals and inactive clock combinations filtered out in SDC
always @(*)
begin
	case (H_LINEMULT)
	`LINEMULT_DISABLE: begin
		R_act = R_1x;
		G_act = G_1x;
		B_act = B_1x;
		DATA_enable_act = (h_enable_1x & v_enable_1x);
		PCLK_out = pclk_out_1x;
		HSYNC_act = HSYNC_1x;
        VSYNC_act = VSYNC_1x;
		lines_out = lines_1x;
		linebuf_rdclock = 0;
		linebuf_hoffset = 0;
		pclk_act = pclk_1x;
		slid_act = {1'b0, vcnt_1x[0]};
		hcnt_act = hcnt_1x;
		vcnt_act = vcnt_1x;
	end
	`LINEMULT_DOUBLE: begin
		R_act = R_lbuf;
		G_act = G_lbuf;
		B_act = B_lbuf;
		DATA_enable_act = (h_enable_2x & v_enable_2x);
		PCLK_out = pclk_out_2x;
		HSYNC_act = HSYNC_2x;
        VSYNC_act = VSYNC_2x;
		lines_out = lines_2x;
		linebuf_rdclock = pclk_2x;
		linebuf_hoffset = hcnt_2x;
		pclk_act = pclk_2x;
		slid_act = {1'b0, vcnt_2x[0]};
		hcnt_act = hcnt_2x;
		vcnt_act = vcnt_2x>>1;
	end
	`LINEMULT_TRIPLE: begin
		R_act = R_lbuf;
		G_act = G_lbuf;
		B_act = B_lbuf;
        VSYNC_act = VSYNC_1x;
		case (H_L3MODE)
		`LINETRIPLE_M0: begin
			DATA_enable_act = (h_enable_3x & v_enable_3x);
			PCLK_out = pclk_out_3x;
			HSYNC_act = HSYNC_3x;
			lines_out = {1'b0, lines_3x};
			linebuf_rdclock = pclk_3x;
			linebuf_hoffset = hcnt_3x;
			pclk_act = pclk_3x;
			hcnt_act = hcnt_3x;
			vcnt_act = vcnt_3x/2'h3; //divider generated
			slid_act = (vcnt_3x % 2'h3);
		end
		`LINETRIPLE_M1: begin
			DATA_enable_act = (h_enable_3x & v_enable_3x);
			PCLK_out = pclk_out_4x;
			HSYNC_act = HSYNC_3x;
			lines_out = {1'b0, lines_3x};
			linebuf_rdclock = pclk_4x;
			linebuf_hoffset = hcnt_4x;
			pclk_act = pclk_4x;
			hcnt_act = hcnt_4x;
			vcnt_act = vcnt_3x/2'h3; //divider generated
			slid_act = (vcnt_3x % 2'h3);
		end
		`LINETRIPLE_M2: begin
			DATA_enable_act = (h_enable_3x_h1x & v_enable_3x_h1x);
			PCLK_out = pclk_out_3x_h4x;
			HSYNC_act = HSYNC_3x_h1x;
			lines_out = {1'b0, lines_3x_h1x};
			linebuf_rdclock = pclk_3x_h4x;
			linebuf_hoffset = hcnt_3x_h4x;
			pclk_act = pclk_3x_h4x;
			hcnt_act = hcnt_3x_h4x;
			vcnt_act = vcnt_3x_h1x/2'h3; //divider generated
			slid_act = (vcnt_3x_h1x % 2'h3);
		end
		`LINETRIPLE_M3: begin
			DATA_enable_act = (h_enable_3x_h1x & v_enable_3x_h1x);
			PCLK_out = pclk_out_3x_h5x;
			HSYNC_act = HSYNC_3x_h1x;
			lines_out = {1'b0, lines_3x_h1x};
			linebuf_rdclock = pclk_3x_h5x;
			linebuf_hoffset = hcnt_3x_h5x;
			pclk_act = pclk_3x_h5x;
			hcnt_act = hcnt_3x_h5x;
			vcnt_act = vcnt_3x_h1x/2'h3; //divider generated
			slid_act = (vcnt_3x_h1x % 2'h3);
		end
		endcase
	end
	default: begin
		R_act = 0;
		G_act = 0;
		B_act = 0;
		DATA_enable_act = 0;
		PCLK_out = 0;
		HSYNC_act = 0;
        VSYNC_act = VSYNC_1x;
		lines_out = 0;
		linebuf_rdclock = 0;
		linebuf_hoffset = 0;
		pclk_act = 0;
		slid_act = 0;
		hcnt_act = 0;
		vcnt_act = 0;
	end
	endcase
end

pll_2x pll_linedouble (
	.areset ( (H_LINEMULT != `LINEMULT_DOUBLE) ),
	.inclk0 ( PCLK_in ),
	.c0 ( pclk_2x ),
	.locked ( pclk_2x_lock )
);

pll_3x pll_linetriple (
	.areset ( ((H_LINEMULT != `LINEMULT_TRIPLE) | H_L3MODE[1]) ),
	.inclk0 ( PCLK_in ),
	.c0 ( pclk_3x ),		// sampling clock for 240p: 1280 or 960 samples & MODE0: 1280 output pixels from 1280 input samples (16:9)
	.c1 ( pclk_4x ),		// MODE1: 1280 output pixels from 960 input samples (960 drawn -> 4:3 aspect)
	.locked ( pclk_3x_lock )
);

pll_3x_lowfreq pll_linetriple_lowfreq (
	.areset ( (H_LINEMULT != `LINEMULT_TRIPLE) | ~H_L3MODE[1]),
	.inclk0 ( PCLK_in ),
	.c0 ( pclk_3x_h1x ),	// sampling clock for 240p: 320 or 256 samples
	.c1 ( pclk_3x_h4x ),	// MODE2: 1280 output pixels from 320 input samples (960 drawn -> 4:3 aspect)
	.c2 ( pclk_3x_h5x ),	// MODE3: 1280 output pixels from 256 input samples (1024 drawn -> 5:4 aspect)
	.locked ( pclk_3x_lowfreq_lock )
);

//TODO: add secondary buffers for interlaced signals with alternative field order
linebuf	linebuf_rgb (
	.data ( {R_1x, G_1x, B_1x} ), //or *_in?
	.rdaddress ( linebuf_hoffset + (~line_idx << 11) ),
	.rdclock ( linebuf_rdclock ),
	.wraddress ( hcnt_1x + (line_idx << 11) ),
	.wrclock ( pclk_1x ),
	.wren ( 1'b1 ),
	.q ( {R_lbuf, G_lbuf, B_lbuf} )
);

//Postprocess pipeline
always @(posedge pclk_act /*or negedge reset_n*/)
begin
	/*if (!reset_n)
		begin
		end
	else*/
		begin
			R_pp1 <= apply_mask(1, R_act, hcnt_act, H_BACKPORCH+H_MASK, H_BACKPORCH+H_ACTIVE-H_MASK, vcnt_act, V_BACKPORCH+V_MASK, V_BACKPORCH+V_ACTIVE-V_MASK);
			G_pp1 <= apply_mask(1, G_act, hcnt_act, H_BACKPORCH+H_MASK, H_BACKPORCH+H_ACTIVE-H_MASK, vcnt_act, V_BACKPORCH+V_MASK, V_BACKPORCH+V_ACTIVE-V_MASK);
			B_pp1 <= apply_mask(1, B_act, hcnt_act, H_BACKPORCH+H_MASK, H_BACKPORCH+H_ACTIVE-H_MASK, vcnt_act, V_BACKPORCH+V_MASK, V_BACKPORCH+V_ACTIVE-V_MASK);
			HSYNC_pp1 <= HSYNC_act;
			VSYNC_pp1 <= VSYNC_act;
			DATA_enable_pp1 <= DATA_enable_act;
			
			R_out <= apply_scanlines(V_SCANLINES, V_SCANLINEDIR, R_pp1, V_SCANLINESTR, {1'b0, V_SCANLINEID}, slid_act, hcnt_act[0]);
			G_out <= apply_scanlines(V_SCANLINES, V_SCANLINEDIR, G_pp1, V_SCANLINESTR, {1'b0, V_SCANLINEID}, slid_act, hcnt_act[0]);
			B_out <= apply_scanlines(V_SCANLINES, V_SCANLINEDIR, B_pp1, V_SCANLINESTR, {1'b0, V_SCANLINEID}, slid_act, hcnt_act[0]);
			HSYNC_out <= HSYNC_pp1;
			VSYNC_out <= VSYNC_pp1;
			DATA_enable <= DATA_enable_pp1;
		end
end

//Generate a warning signal from horizontal instability or PLL sync loss
always @(posedge pclk_1x /*or negedge reset_n*/)
begin
	/*if (!reset_n)
		begin
		end
	else*/
		begin
			if (hmax[0] != hmax[1])
				warn_h_unstable <= 1;
			else if (warn_h_unstable != 0)
				warn_h_unstable <= warn_h_unstable + 1'b1;
		
			if ((H_LINEMULT == `LINEMULT_DOUBLE) & ~pclk_2x_lock)
				warn_pll_lock_lost <= 1;
			else if (warn_pll_lock_lost != 0)
				warn_pll_lock_lost <= warn_pll_lock_lost + 1'b1;
				
			if ((H_LINEMULT == `LINEMULT_TRIPLE) & ~H_L3MODE[1] & ~pclk_3x_lock)
				warn_pll_lock_lost_3x <= 1;
			else if (warn_pll_lock_lost_3x != 0)
				warn_pll_lock_lost_3x <= warn_pll_lock_lost_3x + 1'b1;

			if ((H_LINEMULT == `LINEMULT_TRIPLE) & H_L3MODE[1] & ~pclk_3x_lowfreq_lock)
				warn_pll_lock_lost_3x_lowfreq <= 1;
			else if (warn_pll_lock_lost_3x_lowfreq != 0)
				warn_pll_lock_lost_3x_lowfreq <= warn_pll_lock_lost_3x_lowfreq + 1'b1;
		end
end

assign h_unstable = (warn_h_unstable != 0);
assign pll_lock_lost = {(warn_pll_lock_lost != 0), (warn_pll_lock_lost_3x != 0), (warn_pll_lock_lost_3x_lowfreq != 0)};

//Buffer the inputs using input pixel clock and generate 1x signals
always @(posedge pclk_1x /*or negedge reset_n*/)
begin
	/*if (!reset_n)
		begin
		end
	else*/
		begin
			if (`HSYNC_TRAILING_EDGE)
				begin
					hcnt_1x <= 0;
					hmax[line_idx] <= hcnt_1x;
					line_idx <= line_idx ^ 1'b1;
					vcnt_1x <= vcnt_1x + 1'b1;
                    vcnt_1x_tvp <= vcnt_1x_tvp + 1'b1;
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
					H_ACTIVE <= h_info[26:16];		// Horizontal active length from by the CPU - 11bits (0...2047)
					H_BACKPORCH <= h_info[7:0];		// Horizontal backporch length from by the CPU - 8bits (0...255)
					H_LINEMULT <= h_info[31:30];	// Horizontal line multiply mode
					H_L3MODE <= h_info[29:28];		// Horizontal line triple mode
					H_MASK <= {h_info[11:8], 2'b00};
					V_ACTIVE <= v_info[23:13];		// Vertical active length from by the CPU, 11bits (0...2047)
					V_BACKPORCH <= v_info[5:0];		// Vertical backporch length from by the CPU, 6bits (0...64)
					V_SCANLINES <= v_info[29];
                    V_SCANLINEDIR <= v_info[28];
                    V_SCANLINEID <= v_info[27];
					V_SCANLINESTR <= ((v_info[26:24]+8'h01)<<5)-1'b1;
					V_MASK <= {v_info[9:6], 2'b00};
				end
				
			prev_hs <= HSYNC_in;
			prev_vs <= VSYNC_in;

			// record start position of HSYNC
			if (`HSYNC_LEADING_EDGE)
				HSYNC_start <= hcnt_1x;
			
			R_1x <= R_in;
			G_1x <= G_in;
			B_1x <= B_in;
			HSYNC_1x <= HSYNC_in;

			// Ignore possible invalid vsyncs generated by TVP7002
			if (vcnt_1x > V_ACTIVE)
				VSYNC_1x <= VSYNC_in;
                
            // Check if extra vsync needed
			fpga_vsyncgen[`VSYNCGEN_GENMID_BIT] <= (lines_1x > ({1'b0, V_ACTIVE} << 1)) ? 1'b1 : 1'b0;
			
			h_enable_1x <= ((hcnt_1x >= H_BACKPORCH) & (hcnt_1x < H_BACKPORCH + H_ACTIVE));
			v_enable_1x <= ((vcnt_1x >= V_BACKPORCH) & (vcnt_1x < V_BACKPORCH + V_ACTIVE));	//- FID_in ???
			
			/*HSYNC_out_debug <= HSYNC_in;
			VSYNC_out_debug <= VSYNC_in;*/
		end
end

//Generate 2x signals for linedouble
always @(posedge pclk_2x /*or negedge reset_n*/)
begin
	/*if (!reset_n)
		begin
		end
	else*/
		begin
			if ((pclk_1x == 1'b0) & `HSYNC_TRAILING_EDGE)	//sync with posedge of pclk_1x
				hcnt_2x <= 0;
			else if (hcnt_2x == hmax[~line_idx]) //line_idx_prev?
				hcnt_2x <= 0;
			else
				hcnt_2x <= hcnt_2x + 1'b1;

			if (hcnt_2x == 0)
				vcnt_2x <= vcnt_2x + 1'b1;
			
            if ((pclk_1x == 1'b0) & (fpga_vsyncgen[`VSYNCGEN_GENMID_BIT] == 1'b1))
                begin
                    if (`VSYNC_TRAILING_EDGE)
                        vcnt_2x <= 0;
                    else if (vcnt_2x == lines_1x)
                        begin
                            vcnt_2x <= 0;
                            lines_2x <= vcnt_2x;
                        end
                end
			else if ((pclk_1x == 1'b0) & `VSYNC_TRAILING_EDGE & !(`FALSE_FIELD)) //sync with posedge of pclk_1x
				begin
					vcnt_2x <= 0;
					lines_2x <= vcnt_2x;
				end
                
            if (pclk_1x == 1'b0)
            begin
                if (fpga_vsyncgen[`VSYNCGEN_GENMID_BIT] == 1'b1)
                    VSYNC_2x <= (vcnt_2x >= lines_1x - `VSYNCGEN_LEN) ? 1'b0 : 1'b1;
                else if (vcnt_1x > V_ACTIVE)
                    VSYNC_2x <= VSYNC_in;
            end

			HSYNC_2x <= ~(hcnt_2x >= HSYNC_start);
			//TODO: VSYNC_2x
			h_enable_2x <= ((hcnt_2x >= H_BACKPORCH) & (hcnt_2x < H_BACKPORCH + H_ACTIVE));
			v_enable_2x <= ((vcnt_2x >= (V_BACKPORCH<<1)) & (vcnt_2x < ((V_BACKPORCH + V_ACTIVE)<<1)));
		end
end

//Generate 3x signals for linetriple M0
always @(posedge pclk_3x /*or negedge reset_n*/)
begin
	/*if (!reset_n)
		begin
		end
	else*/
		begin
			if ((pclk_3x_cnt == 0) & `HSYNC_TRAILING_EDGE)	//sync with posedge of pclk_1x
				hcnt_3x <= 0;
			else if (hcnt_3x == hmax[~line_idx]) //line_idx_prev?
				hcnt_3x <= 0;
			else
				hcnt_3x <= hcnt_3x + 1'b1;

			if (hcnt_3x == 0)
				vcnt_3x <= vcnt_3x + 1'b1;
			
			if ((pclk_3x_cnt == 0) & `VSYNC_TRAILING_EDGE & !(`FALSE_FIELD)) //sync with posedge of pclk_1x
				begin
					vcnt_3x <= 0;
					lines_3x <= vcnt_3x;
				end
				
			HSYNC_3x <= ~(hcnt_3x >= HSYNC_start);
			//TODO: VSYNC_3x
			h_enable_3x <= ((hcnt_3x >= H_BACKPORCH) & (hcnt_3x < H_BACKPORCH + H_ACTIVE));
			v_enable_3x <= ((vcnt_3x >= (3*V_BACKPORCH)) & (vcnt_3x < (3*(V_BACKPORCH + V_ACTIVE))));	//multiplier generated!!!
			
			//read pclk_1x to examine when edges are synced (pclk_1x=1 @ 120deg & pclk_1x=0 @ 240deg)
			if (((pclk_1x_prev3x == 1'b1) & (pclk_1x == 1'b0)) | (pclk_3x_cnt == 2'h2))
				pclk_3x_cnt <= 0;
			else
				pclk_3x_cnt <= pclk_3x_cnt + 1'b1;
				
			pclk_1x_prev3x <= pclk_1x;
		end
end

//Generate 4x signals for linetriple M1
always @(posedge pclk_4x /*or negedge reset_n*/)
begin
	/*if (!reset_n)
		begin
		end
	else*/
		begin
			// Can we sync reliably to h_enable_3x???
			if ((h_enable_3x == 1) & (h_enable_3x_prev4x == 0))
				hcnt_4x <= hcnt_3x - 160;
			else
				hcnt_4x <= hcnt_4x + 1'b1;
				
			h_enable_3x_prev4x <= h_enable_3x;
		end
end



//Generate 3x_h1x signals for linetriple M2 and M3
always @(posedge pclk_3x_h1x /*or negedge reset_n*/)
begin
	/*if (!reset_n)
		begin
		end
	else*/
		begin
			if ((pclk_3x_h1x_cnt == 0) & `HSYNC_TRAILING_EDGE)	//sync with posedge of pclk_1x
				hcnt_3x_h1x <= 0;
			else if (hcnt_3x_h1x == hmax[~line_idx]) //line_idx_prev?
				hcnt_3x_h1x <= 0;
			else
				hcnt_3x_h1x <= hcnt_3x_h1x + 1'b1;

			if (hcnt_3x_h1x == 0)
				vcnt_3x_h1x <= vcnt_3x_h1x + 1'b1;
			
			if ((pclk_3x_h1x_cnt == 0) & `VSYNC_TRAILING_EDGE & !(`FALSE_FIELD)) //sync with posedge of pclk_1x
				begin
					vcnt_3x_h1x <= 0;
					lines_3x_h1x <= vcnt_3x_h1x;
				end
				
			HSYNC_3x_h1x <= ~(hcnt_3x_h1x >= HSYNC_start);
			//TODO: VSYNC_3x_h1x
			h_enable_3x_h1x <= ((hcnt_3x_h1x >= H_BACKPORCH) & (hcnt_3x_h1x < H_BACKPORCH + H_ACTIVE));
			v_enable_3x_h1x <= ((vcnt_3x_h1x >= (3*V_BACKPORCH)) & (vcnt_3x_h1x < (3*(V_BACKPORCH + V_ACTIVE))));	//multiplier generated!!!
			
			//read pclk_1x to examine when edges are synced (pclk_1x=1 @ 120deg & pclk_1x=0 @ 240deg)
			if (((pclk_1x_prev3x_h1x == 1'b1) & (pclk_1x == 1'b0)) | (pclk_3x_h1x_cnt == 2'h2))
				pclk_3x_h1x_cnt <= 0;
			else
				pclk_3x_h1x_cnt <= pclk_3x_h1x_cnt + 1'b1;
				
			pclk_1x_prev3x_h1x <= pclk_1x;
		end
end



//Generate 3x_h4x signals for for linetriple M2
always @(posedge pclk_3x_h4x /*or negedge reset_n*/)
begin
	/*if (!reset_n)
		begin
		end
	else*/
		begin
			// Can we sync reliably to h_enable_3x???
			if ((h_enable_3x_h1x == 1) & (h_enable_3x_prev3x_h4x == 0))
				hcnt_3x_h4x <= hcnt_3x_h1x - (160/3);
			else if (hcnt_3x_h4x_ctr == 2'h0)
				hcnt_3x_h4x <= hcnt_3x_h4x + 1'b1;

			if ((h_enable_3x_h1x == 1) & (h_enable_3x_prev3x_h4x == 0))
				hcnt_3x_h4x_ctr <= 2'h1;
			else if (hcnt_3x_h4x_ctr == 2'h2)
				hcnt_3x_h4x_ctr <= 2'h0;
			else
				hcnt_3x_h4x_ctr <= hcnt_3x_h4x_ctr + 1'b1;
				
			h_enable_3x_prev3x_h4x <= h_enable_3x_h1x;
		end
end

//Generate 3x_h5x signals for for linetriple M3
always @(posedge pclk_3x_h5x /*or negedge reset_n*/)
begin
	/*if (!reset_n)
		begin
		end
	else*/
		begin
			// Can we sync reliably to h_enable_3x???
			if ((h_enable_3x_h1x == 1) & (h_enable_3x_prev3x_h5x == 0))
				hcnt_3x_h5x <= hcnt_3x_h1x - (128/4);
			else if (hcnt_3x_h5x_ctr == 2'h0)
				hcnt_3x_h5x <= hcnt_3x_h5x + 1'b1;

			if ((h_enable_3x_h1x == 1) & (h_enable_3x_prev3x_h5x == 0))
				hcnt_3x_h5x_ctr <= 2'h2;
			else
				hcnt_3x_h5x_ctr <= hcnt_3x_h5x_ctr + 1'b1;
				
			h_enable_3x_prev3x_h5x <= h_enable_3x_h1x;
		end
end

endmodule
