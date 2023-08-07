//
// Copyright (C) 2019-2023  Markus Hiienkari <mhiienka@niksula.hut.fi>
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

module scanconverter (
    input PCLK_CAP_i,
    input PCLK_OUT_i,
    input reset_n,
    input [7:0] R_i,
    input [7:0] G_i,
    input [7:0] B_i,
    input HSYNC_i,
    input VSYNC_i,
    input DE_i,
    input FID_i,
    input datavalid_i,
    input interlaced_in_i,
    input frame_change_i,
    input [10:0] xpos_i,
    input [10:0] ypos_i,
    input [11:0] h_in_active,
    input [31:0] hv_out_config,
    input [31:0] hv_out_config2,
    input [31:0] hv_out_config3,
    input [31:0] xy_out_config,
    input [31:0] xy_out_config2,
    input [31:0] misc_config,
    input [31:0] sl_config,
    input [31:0] sl_config2,
    input [31:0] sl_config3,
    input testpattern_enable,
    input lb_enable,
    input ext_sync_mode,
    input ext_frame_change_i,
    input [7:0] ext_R_i,
    input [7:0] ext_G_i,
    input [7:0] ext_B_i,
    output PCLK_o,
    output [7:0] R_o,
    output [7:0] G_o,
    output [7:0] B_o,
    output HSYNC_o,
    output VSYNC_o,
    output DE_o,
    output [11:0] xpos_o,
    output [10:0] ypos_o,
    output reg resync_strobe,
    input emif_br_clk,
    input emif_br_reset,
    output [27:0] emif_rd_addr,
    output emif_rd_read,
    input [255:0] emif_rd_rdata,
    input emif_rd_waitrequest,
    input emif_rd_readdatavalid,
    output [5:0] emif_rd_burstcount,
    output [27:0] emif_wr_addr,
    output emif_wr_write,
    output [255:0] emif_wr_wdata,
    input emif_wr_waitrequest,
    output [5:0] emif_wr_burstcount
);

parameter EMIF_ENABLE = 0;
parameter NUM_LINE_BUFFERS = 32;


localparam FID_EVEN = 1'b0;
localparam FID_ODD = 1'b1;

localparam PP_PL_START          = 1;
localparam PP_LINEBUF_START     = PP_PL_START + 1;
localparam PP_LINEBUF_LENGTH    = 1;
localparam PP_LINEBUF_END       = PP_LINEBUF_START + PP_LINEBUF_LENGTH;
localparam PP_SRCSEL_START      = PP_LINEBUF_END;
localparam PP_SRCSEL_LENGTH     = 1;
localparam PP_SRCSEL_END        = PP_SRCSEL_START + PP_SRCSEL_LENGTH;
localparam PP_Y_CALC_START      = PP_SRCSEL_END;
localparam PP_Y_CALC_LENGTH     = 2;
localparam PP_Y_CALC_END        = PP_Y_CALC_START + PP_Y_CALC_LENGTH;
localparam PP_SLGEN_START       = PP_Y_CALC_END;
localparam PP_SLGEN_LENGTH      = 5;
localparam PP_SLGEN_END         = PP_SLGEN_START + PP_SLGEN_LENGTH;
localparam PP_TP_START          = PP_SLGEN_END;
localparam PP_TP_LENGTH         = 1;
localparam PP_TP_END            = PP_TP_START + PP_TP_LENGTH;
localparam PP_PL_END            = PP_TP_END;

wire [11:0] H_TOTAL = hv_out_config[11:0];
wire [11:0] H_ACTIVE = hv_out_config[23:12];
wire [7:0] H_SYNCLEN = hv_out_config[31:24];
wire [8:0] H_BACKPORCH = hv_out_config2[8:0];

wire V_INTERLACED = hv_out_config2[31];
wire [10:0] V_TOTAL = hv_out_config2[19:9] >> V_INTERLACED;
wire [10:0] V_ACTIVE = hv_out_config2[30:20];
wire [3:0] V_SYNCLEN = hv_out_config3[3:0];
wire [8:0] V_BACKPORCH = hv_out_config3[12:4];

wire [10:0] V_STARTLINE = hv_out_config3[23:13];

wire [10:0] V_STARTLINE_PREV = (V_STARTLINE == 0) ? (V_TOTAL-1) : (V_STARTLINE-1);

wire [11:0] X_SIZE = xy_out_config[11:0];
wire [10:0] Y_SIZE = xy_out_config[22:12];
wire signed [9:0] X_OFFSET = xy_out_config2[9:0];
wire signed [8:0] Y_OFFSET = xy_out_config[31:23];

wire [7:0] X_START_LB = xy_out_config2[17:10];
wire signed [5:0] Y_START_LB = xy_out_config2[23:18];

wire signed [3:0] X_RPT = xy_out_config2[27:24];
wire signed [3:0] Y_RPT = xy_out_config2[31:28];

wire Y_SKIP = (Y_RPT == 4'(-1));
wire [1:0] Y_STEP = Y_SKIP+1'b1;

wire [3:0] SL_L_STR[5:0] = '{sl_config[23:20], sl_config[19:16], sl_config[15:12], sl_config[11:8], sl_config[7:4], sl_config[3:0]};
wire [3:0] SL_C_STR[9:0] = '{sl_config3[7:4], sl_config3[3:0], sl_config2[31:28], sl_config2[27:24], sl_config2[23:20], sl_config2[19:16], sl_config2[15:12], sl_config2[11:8], sl_config2[7:4], sl_config2[3:0]};
wire [5:0] SL_L_OVERLAY = sl_config[29:24];
wire SL_METHOD_PRE = sl_config[30];
wire SL_BOB_ALTERN = sl_config[31];
wire [9:0] SL_C_OVERLAY = sl_config3[17:8];
wire [2:0] SL_IV_Y = sl_config3[20:18];
wire [3:0] SL_IV_X = sl_config3[24:21];
wire [4:0] SL_HYBRSTR = sl_config3[29:25];

wire [3:0] MISC_MASK_BR = misc_config[3:0];
wire [2:0] MISC_MASK_COLOR = misc_config[6:4];
wire MISC_LM_DEINT_MODE = misc_config[12];
wire MISC_NIR_EVEN_OFFSET = misc_config[13];
wire [3:0] MISC_BFI_STR = misc_config[19:16];
wire MISC_BFI_ENABLE = misc_config[20];

wire [7:0] MASK_R = MISC_MASK_COLOR[2] ? {2{MISC_MASK_BR}} : 8'h00;
wire [7:0] MASK_G = MISC_MASK_COLOR[1] ? {2{MISC_MASK_BR}} : 8'h00;
wire [7:0] MASK_B = MISC_MASK_COLOR[0] ? {2{MISC_MASK_BR}} : 8'h00;


reg frame_change_sync1_reg, frame_change_sync2_reg, frame_change_prev, frame_change_resync;
wire frame_change = frame_change_sync2_reg;

reg [11:0] h_cnt;
reg [10:0] v_cnt;
reg h_avidstart, v_avidstart;
reg src_fid, dst_fid;

reg [10:0] xpos_lb;
wire [10:0] xpos_lb_start = (X_OFFSET < 10'sd0) ? 11'd0 : {1'b0, X_OFFSET};
reg [10:0] ypos_lb, ypos_lb_next;
reg [3:0] x_ctr;
reg [3:0] y_ctr;
reg line_id;
reg ypos_pp_init;

reg sl_method;
reg [7:0] Y_sl_str, R_sl_str, G_sl_str, B_sl_str;
wire [7:0] R_sl_mult, G_sl_mult, B_sl_mult;
wire bfi_frame;

reg [8:0] Y_rb_tmp;
reg [9:0] Y;
wire [8:0] Y_sl_hybr_ref_pre, R_sl_hybr_ref_pre, G_sl_hybr_ref_pre, B_sl_hybr_ref_pre;
wire [8:0] Y_sl_hybr_ref, R_sl_hybr_ref, G_sl_hybr_ref, B_sl_hybr_ref;

wire [7:0] R_linebuf, G_linebuf, B_linebuf;

// Pipeline registers
reg [7:0] R_pp[PP_SRCSEL_END:PP_PL_END] /* synthesis ramstyle = "logic" */;
reg [7:0] G_pp[PP_SRCSEL_END:PP_PL_END] /* synthesis ramstyle = "logic" */;
reg [7:0] B_pp[PP_SRCSEL_END:PP_PL_END] /* synthesis ramstyle = "logic" */;
reg HSYNC_pp[PP_PL_START:PP_PL_END] /* synthesis ramstyle = "logic" */;
reg VSYNC_pp[PP_PL_START:PP_PL_END] /* synthesis ramstyle = "logic" */;
reg DE_pp[PP_PL_START:PP_PL_END] /* synthesis ramstyle = "logic" */;
reg [11:0] xpos_pp[PP_PL_START:PP_PL_END] /* synthesis ramstyle = "logic" */;
reg [10:0] ypos_pp[PP_PL_START:PP_PL_END] /* synthesis ramstyle = "logic" */;
reg mask_enable_pp[PP_LINEBUF_START:PP_TP_START] /* synthesis ramstyle = "logic" */;
reg draw_sl_pp[(PP_SLGEN_START+1):(PP_SLGEN_END-1)] /* synthesis ramstyle = "logic" */;
reg [7:0] sl_str_pp[(PP_SLGEN_START+1):(PP_SLGEN_START+2)] /* synthesis ramstyle = "logic" */;
reg [3:0] x_ctr_sl_pp[PP_PL_START:PP_SLGEN_START] /* synthesis ramstyle = "logic" */;
reg [2:0] y_ctr_sl_pp[PP_PL_START:PP_SLGEN_START] /* synthesis ramstyle = "logic" */;

assign PCLK_o = PCLK_OUT_i;


lpm_mult_hybr_ref_pre Y_sl_hybr_ref_pre_u
(
    .clock(PCLK_OUT_i),
    .dataa(Y[9:2]),
    .datab(SL_HYBRSTR),
    .result(Y_sl_hybr_ref_pre)
);
lpm_mult_hybr_ref_pre R_sl_hybr_ref_pre_u
(
    .clock(PCLK_OUT_i),
    .dataa(R_pp[PP_SLGEN_START]),
    .datab(SL_HYBRSTR),
    .result(R_sl_hybr_ref_pre)
);
lpm_mult_hybr_ref_pre G_sl_hybr_ref_pre_u
(
    .clock(PCLK_OUT_i),
    .dataa(G_pp[PP_SLGEN_START]),
    .datab(SL_HYBRSTR),
    .result(G_sl_hybr_ref_pre)
);
lpm_mult_hybr_ref_pre B_sl_hybr_ref_pre_u
(
    .clock(PCLK_OUT_i),
    .dataa(B_pp[PP_SLGEN_START]),
    .datab(SL_HYBRSTR),
    .result(B_sl_hybr_ref_pre)
);

lpm_mult_hybr_ref Y_sl_hybr_ref_u
(
    .clock(PCLK_OUT_i),
    .dataa(Y_sl_hybr_ref_pre),
    .datab(sl_str_pp[PP_SLGEN_START+1]),
    .result(Y_sl_hybr_ref)
);
lpm_mult_hybr_ref R_sl_hybr_ref_u
(
    .clock(PCLK_OUT_i),
    .dataa(R_sl_hybr_ref_pre),
    .datab(sl_str_pp[PP_SLGEN_START+1]),
    .result(R_sl_hybr_ref)
);
lpm_mult_hybr_ref G_sl_hybr_ref_u
(
    .clock(PCLK_OUT_i),
    .dataa(G_sl_hybr_ref_pre),
    .datab(sl_str_pp[PP_SLGEN_START+1]),
    .result(G_sl_hybr_ref)
);
lpm_mult_hybr_ref B_sl_hybr_ref_u
(
    .clock(PCLK_OUT_i),
    .dataa(B_sl_hybr_ref_pre),
    .datab(sl_str_pp[PP_SLGEN_START+1]),
    .result(B_sl_hybr_ref)
);

lpm_mult_sl R_sl_mult_u
(
    .clock(PCLK_OUT_i),
    .dataa(R_pp[PP_SLGEN_START+3]),
    .datab(~Y_sl_str),
    .result(R_sl_mult)
);
lpm_mult_sl G_sl_mult_u
(
    .clock(PCLK_OUT_i),
    .dataa(G_pp[PP_SLGEN_START+3]),
    .datab(~Y_sl_str),
    .result(G_sl_mult)
);
lpm_mult_sl B_sl_mult_u
(
    .clock(PCLK_OUT_i),
    .dataa(B_pp[PP_SLGEN_START+3]),
    .datab(~Y_sl_str),
    .result(B_sl_mult)
);

linebuf_top #(
    .EMIF_ENABLE(EMIF_ENABLE),
    .NUM_LINE_BUFFERS(NUM_LINE_BUFFERS)
  ) linebuf_top_inst (
    .PCLK_CAP_i(PCLK_CAP_i),
    .PCLK_OUT_i(PCLK_OUT_i),
    .R_i(R_i),
    .G_i(G_i),
    .B_i(B_i),
    .DE_i(DE_i),
    .datavalid_i(datavalid_i),
    .h_in_active(h_in_active),
    .xpos_i(xpos_i),
    .ypos_i(ypos_i),
    .xpos_lb(xpos_lb),
    .ypos_lb(ypos_lb),
    .ypos_lb_next(ypos_lb_next),
    .line_id(line_id),
    .lb_enable(lb_enable),
    .R_linebuf(R_linebuf),
    .G_linebuf(G_linebuf),
    .B_linebuf(B_linebuf),
    .emif_br_clk(emif_br_clk),
    .emif_br_reset(emif_br_reset),
    .emif_rd_addr(emif_rd_addr),
    .emif_rd_read(emif_rd_read),
    .emif_rd_rdata(emif_rd_rdata),
    .emif_rd_waitrequest(emif_rd_waitrequest),
    .emif_rd_readdatavalid(emif_rd_readdatavalid),
    .emif_rd_burstcount(emif_rd_burstcount),
    .emif_wr_addr(emif_wr_addr),
    .emif_wr_write(emif_wr_write),
    .emif_wr_wdata(emif_wr_wdata),
    .emif_wr_waitrequest(emif_wr_waitrequest),
    .emif_wr_burstcount(emif_wr_burstcount)
);


// Frame change strobe synchronization
always @(posedge PCLK_OUT_i) begin
    frame_change_sync1_reg <= frame_change_i;
    frame_change_sync2_reg <= frame_change_sync1_reg;
    frame_change_prev <= frame_change_sync2_reg;

    frame_change_resync <= ~frame_change_prev & frame_change & ~(((v_cnt == V_STARTLINE_PREV) & (h_cnt > H_TOTAL-128)) | ((v_cnt == V_STARTLINE) & (h_cnt < 128)));
end

// H/V counters
always @(posedge PCLK_OUT_i) begin
    if (ext_sync_mode & ext_frame_change_i) begin
        h_cnt <= PP_SRCSEL_START; // compensate pipeline delays
        v_cnt <= 0;
        bfi_frame <= bfi_frame ^ 1'b1;
    end else if (~ext_sync_mode & frame_change_resync) begin
        h_cnt <= 0;
        v_cnt <= V_STARTLINE;
        bfi_frame <= 0;
        src_fid <= (~interlaced_in_i | (V_STARTLINE < (V_TOTAL/2))) ? FID_ODD : FID_EVEN;
        dst_fid <= (~V_INTERLACED | (V_STARTLINE < (V_TOTAL/2))) ? FID_ODD : FID_EVEN;
        resync_strobe <= 1'b1;
    end else begin
        if (h_cnt == H_TOTAL-1) begin
            if ((~V_INTERLACED & (v_cnt == V_TOTAL-1)) |
                (V_INTERLACED & (dst_fid == FID_ODD) & (v_cnt == V_TOTAL)) |
                (V_INTERLACED & (dst_fid == FID_EVEN) & (v_cnt == V_TOTAL-1)))
            begin
                v_cnt <= 0;
                v_avidstart <= (V_SYNCLEN+V_BACKPORCH-1'b1 == 0);
                src_fid <= interlaced_in_i ? (src_fid ^ 1'b1) : FID_ODD;
                dst_fid <= V_INTERLACED ? (dst_fid ^ 1'b1) : FID_ODD;
                resync_strobe <= 1'b0;
            end else begin
                v_cnt <= v_cnt + 1'b1;
                v_avidstart <= (v_cnt == V_SYNCLEN+V_BACKPORCH-2'h2);
            end
            h_cnt <= 0;
            h_avidstart <= 1'b0;
        end else begin
            h_cnt <= h_cnt + 1'b1;
            h_avidstart <= (h_cnt == H_SYNCLEN+H_BACKPORCH-1'b1);
        end
    end
end

// Postprocess pipeline structure
//            1          2         3         4         5         6         7         8         9         10        11        12
// |----------|----------|---------|---------|---------|---------|---------|---------|---------|---------|---------|---------|
// | SYNC/DE  |          |         |         |         |         |         |         |         |         |         |         |
// | X/Y POS  |          |         |         |         |         |         |         |         |         |         |         |
// |          |   MASK   |         |         |         |         |         |         |         |         |         |         |
// |          | LB_SETUP | LINEBUF |         |         |         |         |         |         |         |         |         |
// |          |          |         | SRCSEL  |         |         |         |         |         |         |         |         |
// |          |          |         |         |    Y    |    Y    |         |         |         |         |         |         |
// |          |          |         |         |         |         |  SLGEN  |  SLGEN  |  SLGEN  |  SLGEN  |  SLGEN  |         |
// |          |          |         |         |         |         |         |         |         |         |         |    TP   |


// Pipeline stage 1
always @(posedge PCLK_OUT_i) begin
    HSYNC_pp[1] <= (h_cnt < H_SYNCLEN) ? 1'b0 : 1'b1;
    if (dst_fid == FID_ODD)
        VSYNC_pp[1] <= ((v_cnt < V_SYNCLEN) | ((v_cnt == V_TOTAL) & (h_cnt >= (H_TOTAL/2)))) ? 1'b0 : 1'b1;
    else
        VSYNC_pp[1] <= ((v_cnt < V_SYNCLEN-1) | ((v_cnt == V_SYNCLEN-1) & (h_cnt < (H_TOTAL/2)))) ? 1'b0 : 1'b1;
    DE_pp[1] <= (h_cnt >= H_SYNCLEN+H_BACKPORCH) & (h_cnt < H_SYNCLEN+H_BACKPORCH+H_ACTIVE) & (v_cnt >= V_SYNCLEN+V_BACKPORCH) & (v_cnt < V_SYNCLEN+V_BACKPORCH+V_ACTIVE);

    if (h_avidstart) begin
        // Start 1 line before active so that linebuffer can be filled from DRAM in time
        if (v_avidstart) begin
            ypos_pp[1] <= 11'(-1);
            ypos_pp_init <= 1'b1;
            // Bob deinterlace adjusts linebuf start position and y_ctr for even source fields if
            // output is progressive mode. Noninterlace restore as raw output mode is an exception
            // which ignores LM deinterlace mode setting.
            if (~ext_sync_mode & ~MISC_LM_DEINT_MODE & (Y_RPT > 0) & ~V_INTERLACED & (src_fid == FID_EVEN)) begin
                ypos_lb_next <= 11'(Y_START_LB) - 1'b1;
                y_ctr <= ((Y_RPT+1'b1) >> 1);
                y_ctr_sl_pp[1] <= SL_BOB_ALTERN ? ((Y_RPT+1'b1) >> 1) : 0;
            end else begin
                if (Y_SKIP & (dst_fid == FID_EVEN)) begin
                    // Linedrop mode and output interlaced
                    ypos_lb_next <= 11'(Y_START_LB) + 1'b1;
                end else if ((((Y_RPT == 0) & ~V_INTERLACED) | ((Y_RPT > 0) & MISC_LM_DEINT_MODE)) & (src_fid == FID_EVEN)) begin
                    // Adjust even field Y-offset for noninterlace restore
                    ypos_lb_next <= 11'(Y_START_LB) - MISC_NIR_EVEN_OFFSET;
                end else begin
                    ypos_lb_next <= 11'(Y_START_LB);
                end
                y_ctr <= 0;
                y_ctr_sl_pp[1] <= 0;
            end
            line_id <= ~line_id;
        end else begin
            if (ypos_pp[1] != V_ACTIVE) begin
                ypos_pp[1] <= ypos_pp[1] + 1'b1;

                if (ypos_pp_init | (y_ctr == Y_RPT) | Y_SKIP) begin
                    if ((ypos_lb_next >= NUM_LINE_BUFFERS-Y_STEP) & (ypos_lb_next < NUM_LINE_BUFFERS))
                        ypos_lb_next <= ypos_lb_next + Y_STEP - NUM_LINE_BUFFERS;
                    else
                        ypos_lb_next <= ypos_lb_next + Y_STEP;
                    ypos_lb <= ypos_lb_next;
                    line_id <= ~line_id;
                    ypos_pp_init <= 1'b0;
                    if (!ypos_pp_init)
                        y_ctr <= 0;
                end else begin
                    y_ctr <= y_ctr + 1'b1;
                end
                if (!ypos_pp_init)
                    y_ctr_sl_pp[1] <= (y_ctr_sl_pp[1] == SL_IV_Y) ? 0 : y_ctr_sl_pp[1] + 1'b1;
            end
        end
        xpos_pp[1] <= 0;
        xpos_lb <= X_START_LB;
        x_ctr <= 0;
        x_ctr_sl_pp[1] <= 0;
    end else begin
        if (xpos_pp[1] != H_ACTIVE) begin
            xpos_pp[1] <= xpos_pp[1] + 1'b1;
        end

        if (xpos_pp[1] >= xpos_lb_start) begin
            if (x_ctr == X_RPT) begin
                xpos_lb <= xpos_lb + 1'b1;
                x_ctr <= 0;
            end else begin
                x_ctr <= x_ctr + 1'b1;
            end
            x_ctr_sl_pp[1] <= (x_ctr_sl_pp[1] == SL_IV_X) ? 0 : x_ctr_sl_pp[1] + 1'b1;
        end
    end
end

// Pipeline stages 2-
integer pp_idx;
always @(posedge PCLK_OUT_i) begin

    for(pp_idx = PP_LINEBUF_START; pp_idx <= PP_PL_END; pp_idx = pp_idx+1) begin
        HSYNC_pp[pp_idx] <= HSYNC_pp[pp_idx-1];
        VSYNC_pp[pp_idx] <= VSYNC_pp[pp_idx-1];
        DE_pp[pp_idx] <= DE_pp[pp_idx-1];
        xpos_pp[pp_idx] <= xpos_pp[pp_idx-1];
        ypos_pp[pp_idx] <= ypos_pp[pp_idx-1];
    end
    for(pp_idx = PP_LINEBUF_START; pp_idx <= PP_SLGEN_START; pp_idx = pp_idx+1) begin
        x_ctr_sl_pp[pp_idx] <= x_ctr_sl_pp[pp_idx-1];
        y_ctr_sl_pp[pp_idx] <= y_ctr_sl_pp[pp_idx-1];
    end
    // Overridden later where necessary
    for (pp_idx = PP_SRCSEL_END+1; pp_idx <= PP_PL_END; pp_idx = pp_idx+1) begin
        R_pp[pp_idx] <= R_pp[pp_idx-1];
        G_pp[pp_idx] <= G_pp[pp_idx-1];
        B_pp[pp_idx] <= B_pp[pp_idx-1];
    end

    if (($signed({1'b0, xpos_pp[PP_LINEBUF_START-1]}) >= X_OFFSET) &
        ($signed({1'b0, xpos_pp[PP_LINEBUF_START-1]}) < X_OFFSET+X_SIZE) &
        ($signed({1'b0, ypos_pp[PP_LINEBUF_START-1]}) >= Y_OFFSET) &
        ($signed({1'b0, ypos_pp[PP_LINEBUF_START-1]}) < Y_OFFSET+Y_SIZE))
    begin
        mask_enable_pp[PP_LINEBUF_START] <= 1'b0;
    end else begin
        mask_enable_pp[PP_LINEBUF_START] <= 1'b1;
    end
    for(pp_idx = PP_LINEBUF_START+1; pp_idx <= PP_TP_START; pp_idx = pp_idx+1) begin
        mask_enable_pp[pp_idx] <= mask_enable_pp[pp_idx-1];
    end

    /* ---------- Source selection (1 cycle) ---------- */
    R_pp[PP_SRCSEL_END] <= ext_sync_mode ? ext_R_i : R_linebuf;
    G_pp[PP_SRCSEL_END] <= ext_sync_mode ? ext_G_i : G_linebuf;
    B_pp[PP_SRCSEL_END] <= ext_sync_mode ? ext_B_i : B_linebuf;

    /* ---------- Calculate Y from RGB for hybrid scanlines (2 cycles) ---------- */
    Y_rb_tmp <=  {1'b0, R_pp[PP_Y_CALC_START]} + {1'b0, B_pp[PP_Y_CALC_START]};
    Y <= {1'b0, Y_rb_tmp} + {1'b0, G_pp[PP_Y_CALC_START+1], 1'b0};

    /* ---------- Scanline generation (5 cycles) ---------- */
    if (MISC_BFI_ENABLE & bfi_frame) begin
        sl_str_pp[PP_SLGEN_START+1] <= ((MISC_BFI_STR+8'h01)<<4)-1'b1;
        sl_method <= 1'b1;
        draw_sl_pp[PP_SLGEN_START+1] <= 1'b1;
    end else if (|(SL_L_OVERLAY & (6'h1<<y_ctr_sl_pp[PP_SLGEN_START]))) begin
        sl_str_pp[PP_SLGEN_START+1] <= ((SL_L_STR[y_ctr_sl_pp[PP_SLGEN_START]]+8'h01)<<4)-1'b1;
        sl_method <= ~SL_METHOD_PRE;
        draw_sl_pp[PP_SLGEN_START+1] <= 1'b1;
    end else if (|(SL_C_OVERLAY & (10'h1<<x_ctr_sl_pp[PP_SLGEN_START]))) begin
        sl_str_pp[PP_SLGEN_START+1] <= ((SL_C_STR[x_ctr_sl_pp[PP_SLGEN_START]]+8'h01)<<4)-1'b1;
        sl_method <= ~SL_METHOD_PRE;
        draw_sl_pp[PP_SLGEN_START+1] <= 1'b1;
    end else begin
        draw_sl_pp[PP_SLGEN_START+1] <= 1'b0;
    end
    for (pp_idx = PP_SLGEN_START+2; pp_idx <= PP_SLGEN_END-1; pp_idx = pp_idx+1) begin
        draw_sl_pp[pp_idx] <= draw_sl_pp[pp_idx-1];
    end

    // Cycle 2
    sl_str_pp[PP_SLGEN_START+2] <= sl_str_pp[PP_SLGEN_START+1];

    // Cycle 3
    Y_sl_str <= {1'b0, sl_str_pp[PP_SLGEN_START+2]} < Y_sl_hybr_ref ? 8'h0 : sl_str_pp[PP_SLGEN_START+2] - Y_sl_hybr_ref[7:0];
    R_sl_str <= {1'b0, sl_str_pp[PP_SLGEN_START+2]} < R_sl_hybr_ref ? 8'h0 : sl_str_pp[PP_SLGEN_START+2] - R_sl_hybr_ref[7:0];
    G_sl_str <= {1'b0, sl_str_pp[PP_SLGEN_START+2]} < G_sl_hybr_ref ? 8'h0 : sl_str_pp[PP_SLGEN_START+2] - G_sl_hybr_ref[7:0];
    B_sl_str <= {1'b0, sl_str_pp[PP_SLGEN_START+2]} < B_sl_hybr_ref ? 8'h0 : sl_str_pp[PP_SLGEN_START+2] - B_sl_hybr_ref[7:0];

    // Cycle 4
    // store subtraction based scanlined RGB into pipeline registers
    R_pp[PP_SLGEN_START+4] <= draw_sl_pp[PP_SLGEN_START+3] ? ((R_pp[PP_SLGEN_START+3] > R_sl_str) ? (R_pp[PP_SLGEN_START+3] - R_sl_str) : 8'h00) : R_pp[PP_SLGEN_START+3];
    G_pp[PP_SLGEN_START+4] <= draw_sl_pp[PP_SLGEN_START+3] ? ((G_pp[PP_SLGEN_START+3] > G_sl_str) ? (G_pp[PP_SLGEN_START+3] - G_sl_str) : 8'h00) : G_pp[PP_SLGEN_START+3];
    B_pp[PP_SLGEN_START+4] <= draw_sl_pp[PP_SLGEN_START+3] ? ((B_pp[PP_SLGEN_START+3] > B_sl_str) ? (B_pp[PP_SLGEN_START+3] - B_sl_str) : 8'h00) : B_pp[PP_SLGEN_START+3];

    // Cycle 5
    R_pp[PP_SLGEN_END] <= (draw_sl_pp[PP_SLGEN_START+4] & sl_method) ? R_sl_mult : R_pp[PP_SLGEN_START+4];
    G_pp[PP_SLGEN_END] <= (draw_sl_pp[PP_SLGEN_START+4] & sl_method) ? G_sl_mult : G_pp[PP_SLGEN_START+4];
    B_pp[PP_SLGEN_END] <= (draw_sl_pp[PP_SLGEN_START+4] & sl_method) ? B_sl_mult : B_pp[PP_SLGEN_START+4];

    /* ---------- Testpattern / mask generation ---------- */
    R_pp[PP_TP_END] <= testpattern_enable ? (xpos_pp[PP_TP_START] ^ ypos_pp[PP_TP_START]) : (mask_enable_pp[PP_TP_START] ? MASK_R : R_pp[PP_TP_START]);
    G_pp[PP_TP_END] <= testpattern_enable ? (xpos_pp[PP_TP_START] ^ ypos_pp[PP_TP_START]) : (mask_enable_pp[PP_TP_START] ? MASK_G : G_pp[PP_TP_START]);
    B_pp[PP_TP_END] <= testpattern_enable ? (xpos_pp[PP_TP_START] ^ ypos_pp[PP_TP_START]) : (mask_enable_pp[PP_TP_START] ? MASK_B : B_pp[PP_TP_START]);
end

// Output
assign R_o = R_pp[PP_PL_END];
assign G_o = G_pp[PP_PL_END];
assign B_o = B_pp[PP_PL_END];
assign HSYNC_o = HSYNC_pp[PP_PL_END];
assign VSYNC_o = VSYNC_pp[PP_PL_END];
assign DE_o = DE_pp[PP_PL_END];
assign xpos_o = xpos_pp[PP_PL_END];
assign ypos_o = ypos_pp[PP_PL_END];

endmodule
