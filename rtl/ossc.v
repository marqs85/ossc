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

//`define DEBUG
`define VIDEOGEN

module ossc (
    input clk27,
    input ir_rx,
    inout scl,
    inout sda,
    input [1:0] btn,
    input [7:0] R_in,
    input [7:0] G_in,
    input [7:0] B_in,
    input FID_in,
    input VSYNC_in,
    input HSYNC_in,
    input PCLK_in,
    output [7:0] HDMI_TX_RD,
    output [7:0] HDMI_TX_GD,
    output [7:0] HDMI_TX_BD,
    output HDMI_TX_DE,
    output HDMI_TX_HS,
    output HDMI_TX_VS,
    output HDMI_TX_PCLK,
    input HDMI_TX_INT_N,
    input HDMI_TX_MODE,
    output reset_n,
    output LED_G,
    output LED_R,
    output LCD_RS,
    output LCD_CS_N,
    output LCD_BL,
    output SD_CLK,
    inout SD_CMD,
    inout [3:0] SD_DAT
);

wire [7:0] sys_ctrl;
wire h_unstable;
wire [2:0] pclk_lock;
wire [2:0] pll_lock_lost;
wire [31:0] h_info;
wire [31:0] hscale_info;
wire [31:0] v_info;
wire [10:0] lines_out;
wire [1:0] fpga_vsyncgen;

wire [15:0] ir_code;
wire [7:0] ir_code_cnt;

wire [7:0] R_out, G_out, B_out;
wire HSYNC_out;
wire VSYNC_out;
wire PCLK_out;
wire DATA_enable;

wire [7:0] R_out_videogen, G_out_videogen, B_out_videogen;
wire HSYNC_out_videogen;
wire VSYNC_out_videogen;
wire PCLK_out_videogen;
wire DATA_enable_videogen;


reg [3:0] cpu_reset_ctr;
reg cpu_reset_n = 1'b1;

reg [7:0] R_in_L, G_in_L, B_in_L;
reg HSYNC_in_L, VSYNC_in_L, FID_in_L;

reg [1:0] btn_L, btn_LL;
reg ir_rx_L, ir_rx_LL, HDMI_TX_INT_N_L, HDMI_TX_INT_N_LL, HDMI_TX_MODE_L, HDMI_TX_MODE_LL;

// Latch inputs from TVP7002 (synchronized to PCLK_in)
always @(posedge PCLK_in or negedge reset_n)
begin
    if (!reset_n)
        begin
            R_in_L <= 8'h00;
            G_in_L <= 8'h00;
            B_in_L <= 8'h00;
            HSYNC_in_L <= 1'b0;
            VSYNC_in_L <= 1'b0;
            FID_in_L <= 1'b0;
        end
    else
        begin
            R_in_L <= R_in;
            G_in_L <= G_in;
            B_in_L <= B_in;
            HSYNC_in_L <= HSYNC_in;
            VSYNC_in_L <= VSYNC_in;
            FID_in_L <= FID_in;
        end
end

// Insert synchronizers to async inputs (synchronize to CPU clock)
always @(posedge clk27 or negedge cpu_reset_n)
begin
    if (!cpu_reset_n)
        begin
            btn_L <= 2'b00;
            btn_LL <= 2'b00;
            ir_rx_L <= 1'b0;
            ir_rx_LL <= 1'b0;
            HDMI_TX_INT_N_L <= 1'b0;
            HDMI_TX_INT_N_LL <= 1'b0;
            HDMI_TX_MODE_L <= 1'b0;
            HDMI_TX_MODE_LL <= 1'b0;
        end
    else
        begin
            btn_L <= btn;
            btn_LL <= btn_L;
            ir_rx_L <= ir_rx;
            ir_rx_LL <= ir_rx_L;
            HDMI_TX_INT_N_L <= HDMI_TX_INT_N;
            HDMI_TX_INT_N_LL <= HDMI_TX_INT_N_L;
            HDMI_TX_MODE_L <= HDMI_TX_MODE;
            HDMI_TX_MODE_LL <= HDMI_TX_MODE_L;
        end
end

// CPU reset pulse generation (is this really necessary?)
always @(posedge clk27)
begin
    if (cpu_reset_ctr == 4'b1000)
        cpu_reset_n <= 1'b1;
    else
        begin
            cpu_reset_ctr <= cpu_reset_ctr + 1'b1;
            cpu_reset_n <= 1'b0;
        end
end

assign reset_n = sys_ctrl[0];   //HDMI_TX_RST_N in v1.2 PCB


`ifdef DEBUG
assign LED_R = HSYNC_in_L;
assign LED_G = VSYNC_in_L;
`else
assign LED_R = videogen_sel ? 1'b0 : ((pll_lock_lost != 3'b000)|h_unstable);
assign LED_G = (ir_code == 0);
`endif

assign SD_DAT[3] = sys_ctrl[7]; //SD_SPI_SS_N
assign LCD_CS_N = sys_ctrl[6];
assign LCD_RS = sys_ctrl[5];
assign LCD_BL = sys_ctrl[4];    //reset_n in v1.2 PCB

`ifdef VIDEOGEN
wire videogen_sel;
assign videogen_sel = ~sys_ctrl[1];
assign HDMI_TX_RD = videogen_sel ? R_out_videogen : R_out;
assign HDMI_TX_GD = videogen_sel ? G_out_videogen : G_out;
assign HDMI_TX_BD = videogen_sel ? B_out_videogen : B_out;
assign HDMI_TX_HS = videogen_sel ? HSYNC_out_videogen : HSYNC_out;
assign HDMI_TX_VS = videogen_sel ? VSYNC_out_videogen : VSYNC_out;
assign HDMI_TX_PCLK = videogen_sel ? PCLK_out_videogen : PCLK_out;
assign HDMI_TX_DE = videogen_sel ? DATA_enable_videogen : DATA_enable;
`else
assign HDMI_TX_RD = R_out;
assign HDMI_TX_GD = G_out;
assign HDMI_TX_BD = B_out;
assign HDMI_TX_HS = HSYNC_out;
assign HDMI_TX_VS = VSYNC_out;
assign HDMI_TX_PCLK = PCLK_out;
assign HDMI_TX_DE = DATA_enable;
`endif

sys sys_inst(
    .clk_clk                                (clk27),
    .reset_reset_n                          (cpu_reset_n),
    .i2c_opencores_0_export_scl_pad_io      (scl),
    .i2c_opencores_0_export_sda_pad_io      (sda),
    .i2c_opencores_0_export_spi_miso_pad_i  (1'b0),
    .i2c_opencores_1_export_scl_pad_io      (SD_CLK),
    .i2c_opencores_1_export_sda_pad_io      (SD_CMD),
    .i2c_opencores_1_export_spi_miso_pad_i  (SD_DAT[0]),
    .pio_0_sys_ctrl_out_export              (sys_ctrl),
    .pio_1_controls_in_export               ({ir_code_cnt, 5'b00000, HDMI_TX_MODE_LL, btn_LL, ir_code}),
    .pio_2_horizontal_info_out_export       (h_info),
    .pio_3_vertical_info_out_export         (v_info),
    .pio_4_linecount_in_export              ({VSYNC_out, 13'h0000, fpga_vsyncgen, 5'h00, lines_out}),
    .pio_5_hscale_info_out_export           (hscale_info),
);

scanconverter scanconverter_inst (
    .reset_n        (reset_n),
    .PCLK_in        (PCLK_in),
    .HSYNC_in       (HSYNC_in_L),
    .VSYNC_in       (VSYNC_in_L),
    .FID_in         (FID_in_L),
    .R_in           (R_in_L),
    .G_in           (G_in_L),
    .B_in           (B_in_L),
    .h_info         (h_info),
    .v_info         (v_info),
    .hscale_info    (hscale_info),
    .R_out          (R_out),
    .G_out          (G_out),
    .B_out          (B_out),
    .HSYNC_out      (HSYNC_out),
    .VSYNC_out      (VSYNC_out),
    .PCLK_out       (PCLK_out),
    .DATA_enable    (DATA_enable),
    .h_unstable     (h_unstable),
    .fpga_vsyncgen  (fpga_vsyncgen),
    .pclk_lock      (pclk_lock),
    .pll_lock_lost  (pll_lock_lost),
    .lines_out      (lines_out)
);

ir_rcv ir0 (
    .clk27          (clk27),
    .reset_n        (cpu_reset_n),
    .ir_rx          (ir_rx_LL),
    .ir_code        (ir_code),
    .ir_code_ack    (),
    .ir_code_cnt    (ir_code_cnt)
);

`ifdef VIDEOGEN
videogen vg0 (
    .clk27          (clk27),
    .reset_n        (cpu_reset_n & videogen_sel),
    .R_out          (R_out_videogen),
    .G_out          (G_out_videogen),
    .B_out          (B_out_videogen),
    .HSYNC_out      (HSYNC_out_videogen),
    .VSYNC_out      (VSYNC_out_videogen),
    .PCLK_out       (PCLK_out_videogen),
    .ENABLE_out     (DATA_enable_videogen)
);
`endif

endmodule
