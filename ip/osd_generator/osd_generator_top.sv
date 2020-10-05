//
// Copyright (C) 2019-2020  Markus Hiienkari <mhiienka@niksula.hut.fi>
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

module osd_generator_top (
    // common
    input clk_i,
    input rst_i,
    // avalon slave
    input [31:0] avalon_s_writedata,
    output [31:0] avalon_s_readdata,
    input [7:0] avalon_s_address,
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
    output reg [1:0] osd_color
);

localparam CHAR_ROWS = 30;
localparam CHAR_COLS = 16;
localparam CHAR_SECTIONS = 2;
localparam CHAR_SEC_SEPARATOR = 2;

localparam BG_BLACK =   2'h0;
localparam BG_BLUE =    2'h1;
localparam BG_YELLOW =  2'h2;
localparam BG_WHITE =   2'h3;

localparam OSD_CONFIG_REGNUM =          8'hf0;
localparam OSD_ROW_LSEC_ENABLE_REGNUM = 8'hf1;
localparam OSD_ROW_RSEC_ENABLE_REGNUM = 8'hf2;
localparam OSD_ROW_COLOR_REGNUM =       8'hf3;

reg [31:0] osd_config;
reg [31:0] config_reg[OSD_ROW_LSEC_ENABLE_REGNUM:OSD_ROW_COLOR_REGNUM] /* synthesis ramstyle = "logic" */;

reg [10:0] xpos_osd_area_scaled, xpos_text_scaled;
reg [10:0] ypos_osd_area_scaled, ypos_text_scaled;
reg [7:0] x_ptr[2:5], y_ptr[2:5] /* synthesis ramstyle = "logic" */;
reg osd_text_act_pp[2:6], osd_act_pp[3:6];
reg [14:0] to_ctr, to_ctr_ms;
reg char_px;

wire render_enable = osd_config[0];
wire status_refresh = osd_config[1];
wire menu_active = osd_config[2];
wire [1:0] status_timeout = osd_config[4:3];
wire [2:0] x_offset = osd_config[7:5];
wire [2:0] y_offset = osd_config[10:8];
wire [1:0] x_size = osd_config[12:11];
wire [1:0] y_size = osd_config[14:13];
wire [1:0] border_color = osd_config[16:15];

wire [10:0] xpos_scaled_w = (xpos >> x_size)-({3'h0, x_offset} << 3);
wire [10:0] ypos_scaled_w = (ypos >> y_size)-({3'h0, y_offset} << 3);
wire [7:0] rom_rdaddr;
wire [0:7] char_data[7:0];
wire [4:0] char_row = (ypos_text_scaled >> 3);
wire [5:0] char_col = (xpos_text_scaled >> 3) - (((xpos_text_scaled >> 3) >= CHAR_COLS) ? CHAR_SEC_SEPARATOR : 0);
wire [9:0] char_idx = 32*char_row + char_col;

assign avalon_s_waitrequest_n = 1'b1;

char_array char_array_inst (
    .byteena_a(avalon_s_byteenable),
    .data(avalon_s_writedata),
    .rdaddress(char_idx),
    .rdclock(vclk),
    .wraddress(avalon_s_address),
    .wrclock(clk_i),
    .wren(avalon_s_chipselect && avalon_s_write && (avalon_s_address < CHAR_ROWS*CHAR_COLS*CHAR_SECTIONS)),
    .q(rom_rdaddr)
);

char_rom char_rom_inst (
    .clock(vclk),
    .address(rom_rdaddr),
    .q({char_data[7],char_data[6],char_data[5],char_data[4],char_data[3],char_data[2],char_data[1],char_data[0]})
);

// Pipeline structure
// |    0     |    1     |    2    |    3    |    4    |    5    |   6    |
// |----------|----------|---------|---------|---------|---------|--------|
// > POS_TEXT | POS_AREA |         |         |         |         |        |
// >          |   PTR    |   PTR   |   PTR   |   PTR   |         |        |
// >          |  ENABLE  | ENABLE  | ENABLE  | ENABLE  | ENABLE  | ENABLE |
// >          |  INDEX   |  INDEX  |         |         |         |        |
// >          |          |         | CHARROM | CHARROM | CHAR_PX | COLOR  |
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

    osd_text_act_pp[2] <= render_enable &
                          (menu_active || (to_ctr_ms > 0)) &
                          (((xpos_text_scaled < 8*CHAR_COLS) & config_reg[OSD_ROW_LSEC_ENABLE_REGNUM][ypos_text_scaled/8]) |
                           ((xpos_text_scaled >= 8*(CHAR_COLS+CHAR_SEC_SEPARATOR)) & (xpos_text_scaled < 8*(2*CHAR_COLS+CHAR_SEC_SEPARATOR)) & config_reg[OSD_ROW_RSEC_ENABLE_REGNUM][ypos_text_scaled/8])) &
                          (ypos_text_scaled < 8*CHAR_ROWS);
    for(pp_idx = 3; pp_idx <= 6; pp_idx = pp_idx+1) begin
        osd_text_act_pp[pp_idx] <= osd_text_act_pp[pp_idx-1];
    end

    osd_act_pp[3] <= render_enable &
                     (menu_active || (to_ctr_ms > 0)) &
                     (((xpos_osd_area_scaled/8 < (CHAR_COLS+1)) & config_reg[OSD_ROW_LSEC_ENABLE_REGNUM][(ypos_osd_area_scaled/8) ? ((ypos_osd_area_scaled/8)-1) : 0]) |
                      ((xpos_osd_area_scaled/8 >= (CHAR_COLS+1)) & (xpos_osd_area_scaled/8 < (2*CHAR_COLS+CHAR_SEC_SEPARATOR+1)) & (config_reg[OSD_ROW_RSEC_ENABLE_REGNUM][(ypos_osd_area_scaled/8)-1] | config_reg[OSD_ROW_RSEC_ENABLE_REGNUM][ypos_osd_area_scaled/8]))) &
                     (ypos_osd_area_scaled < 8*(CHAR_ROWS+1));
    for(pp_idx = 4; pp_idx <= 6; pp_idx = pp_idx+1) begin
        osd_act_pp[pp_idx] <= osd_act_pp[pp_idx-1];
    end

    char_px <= char_data[y_ptr[5]][x_ptr[5]];

    osd_enable <= osd_act_pp[6];

    if (osd_text_act_pp[6]) begin
        if (char_px) begin
            osd_color <= config_reg[OSD_ROW_COLOR_REGNUM][char_row] ? BG_YELLOW : BG_WHITE;
        end else begin
            osd_color <= BG_BLUE;
        end
    end else begin // border
        osd_color <= border_color;
    end
end

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
    for (i=OSD_ROW_LSEC_ENABLE_REGNUM; i <= OSD_ROW_COLOR_REGNUM; i++) begin : gen_reg
        always @(posedge clk_i or posedge rst_i) begin
            if (rst_i) begin
                config_reg[i] <= 0;
            end else begin
                if (avalon_s_chipselect && avalon_s_write && (avalon_s_address==i)) begin
                    if (avalon_s_byteenable[3])
                        config_reg[i][31:24] <= avalon_s_writedata[31:24];
                    if (avalon_s_byteenable[2])
                        config_reg[i][23:16] <= avalon_s_writedata[23:16];
                    if (avalon_s_byteenable[1])
                        config_reg[i][15:8] <= avalon_s_writedata[15:8];
                    if (avalon_s_byteenable[0])
                        config_reg[i][7:0] <= avalon_s_writedata[7:0];
                end
            end
        end
    end
endgenerate


always @(*) begin
    if (avalon_s_chipselect && avalon_s_read) begin
        case (avalon_s_address)
            OSD_CONFIG_REGNUM:              avalon_s_readdata = osd_config;
            OSD_ROW_LSEC_ENABLE_REGNUM:     avalon_s_readdata = config_reg[OSD_ROW_LSEC_ENABLE_REGNUM];
            OSD_ROW_RSEC_ENABLE_REGNUM:     avalon_s_readdata = config_reg[OSD_ROW_RSEC_ENABLE_REGNUM];
            OSD_ROW_COLOR_REGNUM:           avalon_s_readdata = config_reg[OSD_ROW_COLOR_REGNUM];
            default:                        avalon_s_readdata = 32'h00000000;
        endcase
    end else begin
        avalon_s_readdata = 32'h00000000;
    end
end

endmodule
