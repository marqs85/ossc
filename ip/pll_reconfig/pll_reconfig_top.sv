//
// Copyright (C) 2019  Markus Hiienkari <mhiienka@niksula.hut.fi>
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

module pll_reconfig_top (
    // common
    input clk_i,
    input rst_i,
    // avalon slave
    input [31:0] avalon_s_writedata,
    output [31:0] avalon_s_readdata,
    input [2:0] avalon_s_address,
    input [3:0] avalon_s_byteenable,
    input avalon_s_write,
    input avalon_s_read,
    input avalon_s_chipselect,
    output avalon_s_waitrequest_n,
    // reconfig interface
    output areset,
    output scanclk,
    output reg scanclkena,
    output reg configupdate,
    output scandata,
    input scandone
);

localparam PLL_CONFIG_DATA_BITS = 8'd144;
localparam PLL_CONFIG_DATA_REGS = 5;

localparam PLL_CONFIG_STATUS_REGNUM =   3'h0;
localparam PLL_CONFIG_DATA_STARTREG =   3'h1;

localparam STATE_IDLE       = 2'h0;
localparam STATE_SHIFT      = 2'h1;
localparam STATE_WAITRESP   = 2'h2;

reg [31:0] pll_config_status;
reg [31:0] config_data[0:(PLL_CONFIG_DATA_REGS-1)] /* synthesis ramstyle = "logic" */;
reg areset_strobe;
reg [1:0] state;
reg scan_shift;
reg scandone_prev;
reg configupdate_pre;
reg [7:0] ctr;

wire pll_reset = pll_config_status[0];
wire start_update = pll_config_status[1];
wire config_busy = pll_config_status[31];

assign areset = pll_reset | areset_strobe;
assign scanclk = clk_i;
assign scandata = config_data[0][16];

assign avalon_s_waitrequest_n = 1'b1;


// Avalon register interface
always @(posedge clk_i or posedge rst_i) begin
    if (rst_i) begin
        pll_config_status[7:0] <= 8'h0;
    end else begin
        if (avalon_s_chipselect && avalon_s_write && (avalon_s_address==PLL_CONFIG_STATUS_REGNUM)) begin
            /*if (avalon_s_byteenable[3])
                pll_config_status[31:24] <= avalon_s_writedata[31:24];
            if (avalon_s_byteenable[2])
                pll_config_status[23:16] <= avalon_s_writedata[23:16];
            if (avalon_s_byteenable[1])
                pll_config_status[15:8] <= avalon_s_writedata[15:8];*/
            if (avalon_s_byteenable[0])
                pll_config_status[7:0] <= avalon_s_writedata[7:0];
        end else begin
            pll_config_status[1] <= 1'b0; // reset start_update bit
        end
    end
end


genvar i;
generate
    for (i = 0; i < PLL_CONFIG_DATA_REGS; i = i + 1) begin : genreg
        always @(posedge clk_i or posedge rst_i) begin
            if (rst_i) begin
                config_data[i] <= 32'h0;
            end else begin
                if (!scan_shift) begin
                    if (avalon_s_chipselect && avalon_s_write && (avalon_s_address==(PLL_CONFIG_DATA_STARTREG+PLL_CONFIG_DATA_REGS-1-i))) begin
                        if (avalon_s_byteenable[3])
                            config_data[i][31:24] <= avalon_s_writedata[31:24];
                        if (avalon_s_byteenable[2])
                            config_data[i][23:16] <= avalon_s_writedata[23:16];
                        if (avalon_s_byteenable[1])
                            config_data[i][15:8] <= avalon_s_writedata[15:8];
                        if (avalon_s_byteenable[0])
                            config_data[i][7:0] <= avalon_s_writedata[7:0];
                    end
                end else begin
                    if (i==(PLL_CONFIG_DATA_REGS-1)) begin
                        config_data[i] <= {1'b0, config_data[i][31:1]};
                    end else begin
                        config_data[i] <= {config_data[i+1][0], config_data[i][31:1]};
                    end
                end
            end
        end
    end
endgenerate

// Main FSM
always @(posedge clk_i or posedge rst_i)
begin
    if (rst_i) begin
        state <= STATE_IDLE;
        scanclkena <= 1'b0;
        configupdate_pre <= 1'b0;
        configupdate <= 1'b0;
        areset_strobe <= 1'b0;
        scan_shift <= 1'b0;
        scandone_prev <= 1'b0;
        pll_config_status[31] <= 1'b0;
    end else begin
        case (state)
            STATE_IDLE:
                begin
                    areset_strobe <= 1'b0;

                    if (start_update) begin
                        pll_config_status[31] <= 1'b1;
                        scanclkena <= 1'b1;
                        ctr <= PLL_CONFIG_DATA_BITS;
                        state <= STATE_SHIFT;
                    end else begin
                        pll_config_status[31] <= 1'b0;
                    end
                end
            STATE_SHIFT:
                begin
                    scan_shift <= 1'b1;
                    if (ctr > 0) begin
                        ctr <= ctr - 1'b1;
                    end else begin
                        scan_shift <= 1'b0;
                        scanclkena <= 1'b0;
                        configupdate_pre <= 1'b1;
                        ctr <= 8'hff;
                        state <= STATE_WAITRESP;
                    end
                end    
            STATE_WAITRESP:
                begin
                    configupdate_pre <= 1'b0;
                    ctr <= ctr - 1'b1;
                    if (scandone_prev | (ctr == 8'h0)) begin
                        areset_strobe <= 1'b1;
                        state <= STATE_IDLE;
                    end
                end
            default:
                state <= STATE_IDLE;
        endcase

        scandone_prev <= scandone;
        configupdate <= configupdate_pre;
    end
end

always @(*) begin
    if (avalon_s_chipselect && avalon_s_read) begin
        case (avalon_s_address)
            PLL_CONFIG_STATUS_REGNUM: avalon_s_readdata = pll_config_status;
            default: avalon_s_readdata = 32'h00000000;
        endcase
    end else begin
        avalon_s_readdata = 32'h00000000;
    end
end

endmodule
