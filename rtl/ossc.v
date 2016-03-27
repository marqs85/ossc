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

wire cpu_reset_n;
wire [7:0] sys_ctrl;
wire h_unstable;
wire [2:0] pclk_lock;
wire [2:0] pll_lock_lost;
wire [31:0] h_info;
wire [31:0] v_info;
wire [10:0] lines_out;
wire [1:0] fpga_vsyncgen;

wire [15:0] ir_code;

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

wire [7:0] lcd_ctrl;

reg [3:0] reset_n_ctr;

`ifdef DEBUG
assign LED_G = {clk27, PCLK_in};
assign LED_R = {HSYNC_in, VSYNC_in};
`else
//assign LED_G = {(pclk_lock != 3'b000), (ir_code != 0)};
//assign LED_R = {(pll_lock_lost != 3'b000), h_unstable};
assign LED_G = (ir_code == 0);
assign LED_R = (pll_lock_lost != 3'b000)|h_unstable;
`endif

assign LCD_CS_N = lcd_ctrl[0];
assign LCD_RS = lcd_ctrl[1];
assign LCD_BL = sys_ctrl[1];    //reset_n in v1.2 PCB

assign reset_n = sys_ctrl[0];   //HDMI_TX_RST_N in v1.2 PCB

`ifdef VIDEOGEN
wire videogen_sel;
assign videogen_sel = ~sys_ctrl[2];
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

always @(posedge clk27)
begin
    if (reset_n_ctr == 4'b1000)
        reset_n_reg <= 1'b1;
    else
        reset_n_ctr <= reset_n_ctr + 1'b1;
end

assign cpu_reset_n = reset_n_reg;

sys sys_inst(
    .clk_clk                            (clk27),
    .reset_reset_n                      (cpu_reset_n),
    .pio_0_sys_ctrl_out_export          (sys_ctrl),
    .pio_1_controls_in_export           ({13'b00000000000000, HDMI_TX_MODE, btn, ir_code}),
    .pio_2_horizontal_info_out_export   (h_info),
    .pio_3_vertical_info_out_export     (v_info),
`ifdef DEBUG
    .pio_4_linecount_in_export          ({8'h00, R_in, G_in, B_in}),
`else
    .pio_4_linecount_in_export          ({VSYNC_out, 13'h0000, fpga_vsyncgen, 5'h00, lines_out}),
`endif
    .pio_5_lcd_ctrl_out_export          (lcd_ctrl),
    .i2c_opencores_0_export_scl_pad_io  (scl),
    .i2c_opencores_0_export_sda_pad_io  (sda),
    .sdcard_0_b_SD_cmd                  (SD_CMD),
    .sdcard_0_b_SD_dat                  (SD_DAT[0]),
    .sdcard_0_b_SD_dat3                 (SD_DAT[3]),
    .sdcard_0_o_SD_clock                (SD_CLK)
);

scanconverter scanconverter_inst (
    .reset_n        (reset_n_reg),
    .HSYNC_in       (HSYNC_in),
    .VSYNC_in       (VSYNC_in),
    .PCLK_in        (PCLK_in),
    .FID_in         (FID_in),
    .R_in           (R_in),
    .G_in           (G_in),
    .B_in           (B_in),
    .h_info         (h_info),
    .v_info         (v_info),
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
    .reset_n        (reset_n_reg),
    .ir_rx          (ir_rx),
    .ir_code        (ir_code),
    .ir_code_ack    ()
);

`ifdef VIDEOGEN
videogen vg0 (
    .clk27          (clk27),
    .reset_n        (reset_n_reg & videogen_sel),
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
