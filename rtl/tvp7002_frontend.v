//
// Copyright (C) 2022 Markus Hiienkari <mhiienka@niksula.hut.fi>
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
    output reg [19:0] pcnt_frame,
    output reg sync_active
);

localparam FID_EVEN = 1'b0;
localparam FID_ODD = 1'b1;

localparam VSYNC_SEPARATED = 1'b0;
localparam VSYNC_RAW = 1'b1;

localparam PP_PL_START  = 1;
localparam PP_PL_END    = 4;

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
reg DE_pp[PP_PL_START:PP_PL_END] /* synthesis ramstyle = "logic" */;
reg datavalid_pp[PP_PL_START:PP_PL_END] /* synthesis ramstyle = "logic" */;
reg [10:0] xpos_pp[PP_PL_START:PP_PL_END] /* synthesis ramstyle = "logic" */;
reg [10:0] ypos_pp[PP_PL_START:PP_PL_END] /* synthesis ramstyle = "logic" */;

// Measurement registers
reg [20:0] pcnt_frame_ctr;
reg [17:0] syncpol_det_ctr, hsync_hpol_ctr, vsync_hpol_ctr;
reg [3:0] sync_inactive_ctr;
reg [11:0] pcnt_line, pcnt_line_ctr, meas_h_cnt, meas_h_cnt_sogref;
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

wire [11:0] h_cnt_ref = (vsync_i_type == VSYNC_SEPARATED) ? h_cnt_sogref : h_cnt;
wire [11:0] even_min_thold = (H_TOTAL / 12'd4);
wire [11:0] even_max_thold = (H_TOTAL / 12'd2) + (H_TOTAL / 12'd4);

wire [11:0] meas_h_cnt_ref = (vsync_i_type == VSYNC_SEPARATED) ? meas_h_cnt_sogref : meas_h_cnt;
wire [11:0] meas_even_min_thold = (pcnt_line / 12'd4);
wire [11:0] meas_even_max_thold = (pcnt_line / 12'd2) + (pcnt_line / 12'd4);
wire meas_vblank_region = ((pcnt_frame_ctr < (pcnt_frame/8)) | (pcnt_frame_ctr > (pcnt_frame - (pcnt_frame/8))));
wire [11:0] glitch_filt_thold = meas_vblank_region ? (pcnt_line/4) : (pcnt_line/8);

// TODO: calculate H/V polarity independently
wire VS_i_np = (VS_i ^ ~vsync_i_pol);
wire VSYNC_i_np = (VSYNC_i ^ ~vsync_i_pol);
wire HSYNC_i_np = (HSYNC_i ^ ~hsync_i_pol);

// Sample skip for low-res optimized modes
wire [3:0] H_SKIP = hv_in_config3[27:24];
wire [3:0] H_SAMPLE_SEL = hv_in_config3[31:28];

// SOF position for scaler
wire [10:0] V_SOF_LINE = hv_in_config3[23:13];


always @(posedge PCLK_i) begin
    R_pp[1] <= R_i;
    G_pp[1] <= G_i;
    B_pp[1] <= B_i;
    DE_pp[1] <= (h_cnt >= H_SYNCLEN+H_BACKPORCH) & (h_cnt < H_SYNCLEN+H_BACKPORCH+H_ACTIVE) & (v_cnt >= V_SYNCLEN+V_BACKPORCH) & (v_cnt < V_SYNCLEN+V_BACKPORCH+V_ACTIVE);
    datavalid_pp[1] <= (h_ctr == H_SAMPLE_SEL);
    xpos_pp[1] <= (h_cnt-H_SYNCLEN-H_BACKPORCH);
    ypos_pp[1] <= (v_cnt-V_SYNCLEN-V_BACKPORCH);

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
        if (h_cnt_ref < even_min_thold) begin
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

// Pipeline stages 1-
integer pp_idx;
always @(posedge PCLK_i) begin
    for(pp_idx = PP_PL_START+1; pp_idx <= PP_PL_END-1; pp_idx = pp_idx+1) begin
        R_pp[pp_idx] <= R_pp[pp_idx-1];
        G_pp[pp_idx] <= G_pp[pp_idx-1];
        B_pp[pp_idx] <= B_pp[pp_idx-1];
    end
    R_pp[PP_PL_END] <= R_pp[PP_PL_END-1];
    G_pp[PP_PL_END] <= G_pp[PP_PL_END-1];
    B_pp[PP_PL_END] <= B_pp[PP_PL_END-1];

    for(pp_idx = PP_PL_START+1; pp_idx <= PP_PL_END; pp_idx = pp_idx+1) begin
        HSYNC_pp[pp_idx] <= HSYNC_pp[pp_idx-1];
        VSYNC_pp[pp_idx] <= VSYNC_pp[pp_idx-1];
        FID_pp[pp_idx] <= FID_pp[pp_idx-1];
        DE_pp[pp_idx] <= DE_pp[pp_idx-1];
        datavalid_pp[pp_idx] <= datavalid_pp[pp_idx-1];
        xpos_pp[pp_idx] <= xpos_pp[pp_idx-1];
        ypos_pp[pp_idx] <= ypos_pp[pp_idx-1];
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
        pcnt_frame <= interlace_flag ? (pcnt_frame_ctr>>1) : pcnt_frame_ctr[19:0];
    end else if (pcnt_frame_ctr < 21'h1fffff) begin
        pcnt_frame_ctr <= pcnt_frame_ctr + 1'b1;
    end

    if (HSYNC_i_np_prev & ~HSYNC_i_np) begin
        pcnt_line_ctr <= 1;

        // store count 1ms after vsync
        if (~pcnt_line_stored & (pcnt_frame_ctr > 21'd27000)) begin
            pcnt_line <= pcnt_line_ctr;
            pcnt_line_stored <= 1'b1;
        end
    end else begin
        pcnt_line_ctr <= pcnt_line_ctr + 1'b1;
    end

    HSYNC_i_np_prev <= HSYNC_i_np;
    VSYNC_i_np_prev <= VSYNC_i_np;
end

// Detect sync polarities and activity
always @(posedge CLK_MEAS_i) begin
    if (syncpol_det_ctr == 0) begin
        hsync_i_pol <= (hsync_hpol_ctr > 18'h1ffff);
        vsync_i_pol <= (vsync_hpol_ctr > 18'h1ffff);

        hsync_hpol_ctr <= 0;
        vsync_hpol_ctr <= 0;

        if ((vsync_hpol_ctr == '0) | (vsync_hpol_ctr == '1)) begin
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
    end else if (meas_vblank_region & (meas_h_cnt > pcnt_line)) begin
        // hsync may be missing during vblank, force line change detect if pcnt_line is exceeded +-1/8 field around vsync edge
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
                if ((HSYNC_i_np_prev & ~HSYNC_i_np) | (meas_h_cnt > pcnt_line)) begin
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
