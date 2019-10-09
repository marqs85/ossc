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

module osd_generator_top #(
    parameter USE_MEMORY_BLOCKS = 0
) (
    // common
    input clk_i,
    input rst_i,
    // avalon slave
    input [31:0] avalon_s_writedata,
    output [31:0] avalon_s_readdata,
    input [3:0] avalon_s_address,
    input [3:0] avalon_s_byteenable,
    input avalon_s_write,
    input avalon_s_read,
    input avalon_s_chipselect,
    output avalon_s_waitrequest_n,
    // OSD interface
    input vclk,
    input [10:0] xpos,
    input [10:0] ypos,
    output reg osd_enable,
    output reg osd_color
);

localparam CHAR_ROWS = 2;
localparam CHAR_COLS = 16;

localparam OSD_CONFIG_REGNUM =   4'h0;

reg [31:0] osd_config;

reg [10:0] xpos_osd_area_scaled, xpos_text_scaled;
reg [10:0] ypos_osd_area_scaled, ypos_text_scaled;
reg [7:0] x_ptr[2:5], y_ptr[2:5] /* synthesis ramstyle = "logic" */;
reg osd_text_act_pp[2:5], osd_act_pp[3:5];
reg [14:0] to_ctr, to_ctr_ms;

wire render_enable = osd_config[0];
wire status_refresh = osd_config[1];
wire menu_active = osd_config[2];
wire [1:0] status_timeout = osd_config[4:3];
wire [2:0] x_offset = osd_config[7:5];
wire [2:0] y_offset = osd_config[10:8];
wire [1:0] x_size = osd_config[12:11];
wire [1:0] y_size = osd_config[14:13];

wire [10:0] xpos_scaled_w = (xpos >> x_size)-({3'h0, x_offset} << 3);
wire [10:0] ypos_scaled_w = (ypos >> y_size)-({3'h0, y_offset} << 3);
wire [7:0] rom_rdaddr;
wire [0:7] char_data[7:0];
wire [4:0] char_idx = CHAR_COLS*(ypos_text_scaled >> 3) + (xpos_text_scaled >> 3);

assign avalon_s_waitrequest_n = 1'b1;

generate
    if (USE_MEMORY_BLOCKS == 1) begin
        char_array char_array_inst (
            .byteena_a(avalon_s_byteenable),
            .data(avalon_s_writedata),
            .rdaddress(char_idx),
            .rdclock(vclk),
            .wraddress(avalon_s_address-1'b1),
            .wrclock(clk_i),
            .wren(avalon_s_chipselect && avalon_s_write && (avalon_s_address > 4'h0)),
            .q(rom_rdaddr)
        );
    end else begin
        reg [7:0] char_ptr[CHAR_ROWS*CHAR_COLS-1:0], char_ptr_pp3[7:0] /* synthesis ramstyle = "logic" */;
        reg [4:0] char_idx_pp[2:3];
        
        assign rom_rdaddr = char_ptr_pp3[char_idx_pp[3][2:0]];
    end
endgenerate

char_rom char_rom_inst (
    .clock(vclk),
    .address(rom_rdaddr),
    .q({char_data[7],char_data[6],char_data[5],char_data[4],char_data[3],char_data[2],char_data[1],char_data[0]})
);

// Pipeline structure
// |    0     |    1     |    2    |    3    |    4    |   5    |
// |----------|----------|---------|---------|---------|--------|
// > POS_TEXT | POS_AREA |         |         |         |        |
// >          |   PTR    |   PTR   |   PTR   |   PTR   |        |
// >          |  ENABLE  | ENABLE  | ENABLE  | ENABLE  | ENABLE |
// >          |  INDEX   |  INDEX  |         |         |        |
// >          |          |         | CHARROM | CHARROM | COLOR  |
integer idx, pp_idx;
always @(posedge vclk) begin
    xpos_text_scaled <= xpos_scaled_w;
    ypos_text_scaled <= ypos_scaled_w;

    xpos_osd_area_scaled <= xpos_text_scaled + 3'h4;
    ypos_osd_area_scaled <= ypos_text_scaled + 3'h4;

    x_ptr[2] <= xpos_text_scaled[7:0];
    y_ptr[2] <= ypos_text_scaled[7:0];
    for(pp_idx = 3; pp_idx <= 5; pp_idx = pp_idx+1) begin
        x_ptr[pp_idx] <= x_ptr[pp_idx-1];
        y_ptr[pp_idx] <= y_ptr[pp_idx-1];
    end

    osd_text_act_pp[2] <= render_enable & (menu_active || (to_ctr_ms > 0)) & ((xpos_text_scaled < 8*CHAR_COLS) && (ypos_text_scaled < 8*CHAR_ROWS));
    for(pp_idx = 3; pp_idx <= 5; pp_idx = pp_idx+1) begin
        osd_text_act_pp[pp_idx] <= osd_text_act_pp[pp_idx-1];
    end

    osd_act_pp[3] <= render_enable & (menu_active || (to_ctr_ms > 0)) & ((xpos_osd_area_scaled < 8*(CHAR_COLS+1)) && (ypos_osd_area_scaled < 8*(CHAR_ROWS+1)));
    for(pp_idx = 4; pp_idx <= 5; pp_idx = pp_idx+1) begin
        osd_act_pp[pp_idx] <= osd_act_pp[pp_idx-1];
    end

    osd_enable <= osd_act_pp[5];
    osd_color = osd_text_act_pp[5] ? char_data[y_ptr[5]][x_ptr[5]] : 1'b0;
end

generate
    if (USE_MEMORY_BLOCKS == 0) begin
        always @(posedge vclk) begin
            char_idx_pp[2] <= char_idx;
            char_idx_pp[3] <= char_idx_pp[2];

            for(idx = 0; idx <= 7; idx = idx+1) begin
                char_ptr_pp3[idx] <= char_ptr[{char_idx_pp[2][4:3], 3'(idx)}];
            end
        end
    end
endgenerate

// OSD status timeout counters
always @(posedge clk_i)
begin
    if (status_refresh) begin
        to_ctr <= 15'd0;
        case (status_timeout)
            default: to_ctr_ms <= 2000; // 2s
            2'b01:   to_ctr_ms <= 5000; // 5s
            2'b10:   to_ctr_ms <= 10000; // 10s
            2'b11:   to_ctr_ms <= 0;    // off
        endcase
    end else begin
        if (to_ctr == 27000-1) begin
            to_ctr <= 0;
            if (to_ctr_ms != 15'h0)
                to_ctr_ms <= to_ctr_ms - 1'b1;
        end else begin
            to_ctr <= to_ctr + 1'b1;
        end
    end
end

// Avalon register interface
always @(posedge clk_i or posedge rst_i) begin
    if (rst_i) begin
        osd_config <= 32'h0;
    end else begin
        if (avalon_s_chipselect && avalon_s_write && (avalon_s_address==OSD_CONFIG_REGNUM)) begin
            if (avalon_s_byteenable[3])
                osd_config[31:24] <= avalon_s_writedata[31:24];
            if (avalon_s_byteenable[2])
                osd_config[23:16] <= avalon_s_writedata[23:16];
            if (avalon_s_byteenable[1])
                osd_config[15:8] <= avalon_s_writedata[15:8];
            if (avalon_s_byteenable[0])
                osd_config[7:0] <= avalon_s_writedata[7:0];
        end else begin
            osd_config[1] <= 1'b0; // reset timer refresh bit
        end
    end
end


genvar i;
generate
    if (USE_MEMORY_BLOCKS == 0) begin
        for (i = 0; i < (CHAR_ROWS*CHAR_COLS); i = i + 4) begin : genreg
            always @(posedge clk_i or posedge rst_i) begin
                if (rst_i) begin
                    char_ptr[i] <= 0;
                    char_ptr[i+1] <= 0;
                    char_ptr[i+2] <= 0;
                    char_ptr[i+3] <= 0;
                end else begin
                    if (avalon_s_chipselect && avalon_s_write && (avalon_s_address==1+(i/4))) begin
                        if (avalon_s_byteenable[3])
                            char_ptr[i+3] <= avalon_s_writedata[31:24];
                        if (avalon_s_byteenable[2])
                            char_ptr[i+2] <= avalon_s_writedata[23:16];
                        if (avalon_s_byteenable[1])
                            char_ptr[i+1] <= avalon_s_writedata[15:8];
                        if (avalon_s_byteenable[0])
                            char_ptr[i] <= avalon_s_writedata[7:0];
                    end
                end
            end
        end
    end
endgenerate

always @(*) begin
    if (avalon_s_chipselect && avalon_s_read) begin
        case (avalon_s_address)
            OSD_CONFIG_REGNUM: avalon_s_readdata = osd_config;
            default: avalon_s_readdata = 32'h00000000;
        endcase
    end else begin
        avalon_s_readdata = 32'h00000000;
    end
end

endmodule
