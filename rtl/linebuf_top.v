//
// Copyright (C) 2022  Markus Hiienkari <mhiienka@niksula.hut.fi>
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

//`define DEBUG

module linebuf_top (
    input PCLK_CAP_i,
    input PCLK_OUT_i,
    input [7:0] R_i,
    input [7:0] G_i,
    input [7:0] B_i,
    input DE_i,
    input datavalid_i,
    input [11:0] h_in_active,
    input [10:0] xpos_i,
    input [10:0] ypos_i,
    input [10:0] xpos_lb,
    input [10:0] ypos_lb,
    input [10:0] ypos_lb_next,
    input line_id,
    input lb_enable,
    output [7:0] R_linebuf,
    output [7:0] G_linebuf,
    output [7:0] B_linebuf,
    // optional EMIF if
    input emif_br_clk,
    input emif_br_reset,
    output reg [27:0] emif_rd_addr,
    output reg emif_rd_read,
    input [255:0] emif_rd_rdata,
    input emif_rd_waitrequest,
    input emif_rd_readdatavalid,
    output reg [5:0] emif_rd_burstcount,
    output [27:0] emif_wr_addr,
    output reg emif_wr_write,
    output [255:0] emif_wr_wdata,
    input emif_wr_waitrequest,
    output reg [5:0] emif_wr_burstcount
);

parameter EMIF_ENABLE = 0;
parameter NUM_LINE_BUFFERS = 32;


localparam EMIF_WR_MAXBURST = 32;
localparam EMIF_RD_MAXBURST = 32;

generate
if (EMIF_ENABLE) begin

`ifdef DEBUG
reg [10:0] emif_wr_stall_ctr /* synthesis noprune */;
reg [10:0] emif_rd_stall_ctr /* synthesis noprune */;
reg emif_rd_line_missed /* synthesis noprune */;

always @(posedge emif_br_clk) begin
    if (emif_wr_write & emif_wr_waitrequest & (emif_wr_stall_ctr != 2047))
        emif_wr_stall_ctr <= emif_wr_stall_ctr + 1'b1;
    else
        emif_wr_stall_ctr <= 0;

    if (emif_rd_burstcount > 0) begin
        if (emif_rd_readdatavalid)
            emif_rd_stall_ctr <= 0;
        else
            emif_rd_stall_ctr <= emif_rd_stall_ctr + 1'b1;
    end
end
`endif

// Number of 8-pixel blocks per line
wire [8:0] blocks_per_line = (h_in_active / 8) + (h_in_active[2:0] != 3'h0);

/* ------------------------------ EMIF WR interface ------------------------------ */

// PCLK_CAP_i related signals
reg [8:0] emif_wr_fifo_blksleft;
reg [19:0] emif_wr_fifo_addr; // [19:9] = y_pos; [8:0] = x_pos_x8
reg [23:0] emif_wr_fifo_pixs[7:0] /* synthesis ramstyle = "logic" */;
reg emif_wr_fifo_wrreq;
reg [8:0] blocks_inserted;
reg DE_prev;
wire emif_wr_fifo_wrfull;

// emif_br_clk related signals
wire [8:0] emif_wr_fifo_blksleft_q;
reg [8:0] emif_wr_blksleft;
wire [19:0] emif_wr_fifo_addr_q;
wire [23:0] emif_wr_fifo_pixs_q[7:0];
reg emif_wr_fifo_iniread_prev;
wire emif_wr_fifo_rdempty;
wire [8:0] emif_wr_fifo_rdusedw;
wire emif_wr_fifo_iniread = (lb_enable & (emif_wr_blksleft == 0) & !emif_wr_fifo_iniread_prev & !emif_wr_fifo_rdempty);
wire emif_wr_fifo_rdreq = emif_wr_fifo_iniread | ((emif_wr_burstcount > 1) & !emif_wr_waitrequest);

assign emif_wr_addr = {3'b001, emif_wr_fifo_addr_q, 5'h0};
assign emif_wr_wdata = {8'h0, emif_wr_fifo_pixs_q[0], 8'h0, emif_wr_fifo_pixs_q[1], 8'h0, emif_wr_fifo_pixs_q[2], 8'h0, emif_wr_fifo_pixs_q[3],
                        8'h0, emif_wr_fifo_pixs_q[4], 8'h0, emif_wr_fifo_pixs_q[5], 8'h0, emif_wr_fifo_pixs_q[6], 8'h0, emif_wr_fifo_pixs_q[7]};

dc_fifo_emif_wr dc_fifo_emif_wr_inst (
    .data({emif_wr_fifo_blksleft, emif_wr_fifo_addr,
           emif_wr_fifo_pixs[0], emif_wr_fifo_pixs[1], emif_wr_fifo_pixs[2], emif_wr_fifo_pixs[3],
           emif_wr_fifo_pixs[4], emif_wr_fifo_pixs[5], emif_wr_fifo_pixs[6], emif_wr_fifo_pixs[7]}),
    .rdclk(emif_br_clk),
    .rdreq(emif_wr_fifo_rdreq),
    .rdempty(emif_wr_fifo_rdempty),
    .rdusedw(emif_wr_fifo_rdusedw),
    .wrclk(PCLK_CAP_i),
    .wrreq(emif_wr_fifo_wrreq),
    .wrfull(emif_wr_fifo_wrfull),
    .q({emif_wr_fifo_blksleft_q, emif_wr_fifo_addr_q,
        emif_wr_fifo_pixs_q[0], emif_wr_fifo_pixs_q[1], emif_wr_fifo_pixs_q[2], emif_wr_fifo_pixs_q[3],
        emif_wr_fifo_pixs_q[4], emif_wr_fifo_pixs_q[5], emif_wr_fifo_pixs_q[6], emif_wr_fifo_pixs_q[7]})
);

always @(posedge PCLK_CAP_i) begin
    emif_wr_fifo_wrreq <= 1'b0;

    if (datavalid_i) begin
        emif_wr_fifo_pixs[xpos_i[2:0]] <= {R_i, G_i, B_i};
        DE_prev <= DE_i;

        if (~DE_prev & DE_i)
            blocks_inserted <= 0;

        if ((blocks_inserted < blocks_per_line) & ~emif_wr_fifo_wrfull & (xpos_i[2:0] == 3'h7)) begin
            emif_wr_fifo_blksleft <= blocks_per_line - blocks_inserted;
            emif_wr_fifo_addr[19:9] <= ypos_i;
            emif_wr_fifo_addr[7:0] <= xpos_i[10:3];
            emif_wr_fifo_wrreq <= 1'b1;
            blocks_inserted <= blocks_inserted + 1'b1;
        end
    end
end

always @(posedge emif_br_clk or posedge emif_br_reset) begin
    if (emif_br_reset) begin
        emif_wr_write <= 1'b0;
        emif_wr_burstcount <= 1'b0;
    end else begin
        if (emif_wr_burstcount > 0) begin
            if (!emif_wr_waitrequest) begin
                emif_wr_burstcount <= emif_wr_burstcount - 1'b1;
                if (emif_wr_burstcount == 1) begin
                    emif_wr_blksleft <= 0;
                    emif_wr_write <= 1'b0;
                end
            end
        end else if (lb_enable & (emif_wr_blksleft > 0) & ((emif_wr_fifo_rdusedw >= emif_wr_blksleft-1) | (emif_wr_fifo_rdusedw >= 31))) begin
            emif_wr_write <= 1'b1;
            emif_wr_burstcount <= (emif_wr_blksleft > EMIF_WR_MAXBURST) ? EMIF_WR_MAXBURST : emif_wr_blksleft;
        end else if (emif_wr_fifo_iniread_prev) begin
            emif_wr_blksleft <= emif_wr_fifo_blksleft_q;
        end

        emif_wr_fifo_iniread_prev <= emif_wr_fifo_iniread;
    end
end

/* ------------------------------ EMIF RD interface ------------------------------ */

// PCLK_OUT_i related signals
wire [12:0] linebuf_rdaddr = {xpos_lb + (line_id ? 2560 : 0)};

// emif_br_clk related signals
reg [8:0] blocks_copied;
wire linebuf_wren = emif_rd_readdatavalid;
wire [191:0] linebuf_wrdata = {emif_rd_rdata[23:0], emif_rd_rdata[55:32], emif_rd_rdata[87:64], emif_rd_rdata[119:96],
                               emif_rd_rdata[151:128], emif_rd_rdata[183:160], emif_rd_rdata[215:192], emif_rd_rdata[247:224]};
wire [9:0] linebuf_wraddr = {blocks_copied + (line_id ? 0 : 320)};
wire [19:0] emif_rd_block_addr = {ypos_lb_next, blocks_copied};
reg line_id_brclk_sync1_reg, line_id_brclk_sync2_reg, line_id_brclk_sync3_reg;


linebuf_double linebuf_rgb (
    .data(linebuf_wrdata),
    .rdaddress(linebuf_rdaddr),
    .rdclock(PCLK_OUT_i),
    .rdclocken(lb_enable),
    .wraddress(linebuf_wraddr),
    .wrclock(emif_br_clk),
    .wren(linebuf_wren),
    .wrclocken(lb_enable),
    .q({R_linebuf, G_linebuf, B_linebuf})
);

// BRAM linebuffer operation
always @(posedge emif_br_clk or posedge emif_br_reset) begin
    if (emif_br_reset) begin
        emif_rd_read <= 1'b0;
        emif_rd_burstcount <= 1'b0;
    end else begin
        if (emif_rd_burstcount > 0) begin // always finish read first, make sure doesn't last longer than line length
            if (emif_rd_readdatavalid) begin
                blocks_copied <= blocks_copied + 1'b1;
                emif_rd_burstcount <= emif_rd_burstcount - 1'b1;
            end
            if (!emif_rd_waitrequest)
                emif_rd_read <= 1'b0;
`ifdef DEBUG
            emif_rd_line_missed <= (line_id_brclk_sync2_reg != line_id_brclk_sync3_reg);
`endif
        end else if (line_id_brclk_sync2_reg != line_id_brclk_sync3_reg) begin
            blocks_copied <= 0;
        end else if (lb_enable & (blocks_copied < blocks_per_line)) begin
            emif_rd_read <= 1'b1;
            emif_rd_burstcount <= ((blocks_per_line-blocks_copied) > EMIF_RD_MAXBURST) ? EMIF_RD_MAXBURST : (blocks_per_line-blocks_copied);
            emif_rd_addr <= {3'b001, emif_rd_block_addr, 5'h0};
        end

        line_id_brclk_sync1_reg <= line_id;
        line_id_brclk_sync2_reg <= line_id_brclk_sync1_reg;
        line_id_brclk_sync3_reg <= line_id_brclk_sync2_reg;
    end
end

end else begin // EMIF_ENABLE

/* ------------------------------ BRAM lb interface ------------------------------ */

reg [5:0] ypos_wraddr;
reg [10:0] ypos_prev;
reg [10:0] xpos_wraddr;
reg [23:0] linebuf_wrdata;
reg linebuf_wren;

wire [16:0] linebuf_wraddr = {ypos_wraddr[($clog2(NUM_LINE_BUFFERS)-1):0], xpos_wraddr};
wire [16:0] linebuf_rdaddr = {ypos_lb[($clog2(NUM_LINE_BUFFERS)-1):0], xpos_lb[10:0]};


linebuf linebuf_rgb (
    .data(linebuf_wrdata),
    .rdaddress(linebuf_rdaddr),
    .rdclock(PCLK_OUT_i),
    .rdclocken(lb_enable),
    .wraddress(linebuf_wraddr),
    .wrclock(PCLK_CAP_i),
    .wren(linebuf_wren),
    .wrclocken(lb_enable),
    .q({R_linebuf, G_linebuf, B_linebuf})
);

// Linebuffer write address calculation
always @(posedge PCLK_CAP_i) begin
    if (ypos_i == 0) begin
        ypos_wraddr <= 0;
    end else if (ypos_i != ypos_prev) begin
        if (ypos_wraddr == NUM_LINE_BUFFERS-1)
            ypos_wraddr <= 0;
        else
            ypos_wraddr <= ypos_wraddr + 1'b1;
    end

    xpos_wraddr <= xpos_i;
    ypos_prev <= ypos_i;
    linebuf_wrdata <= {R_i, G_i, B_i};
    linebuf_wren <= DE_i & datavalid_i;
end

end
endgenerate

endmodule