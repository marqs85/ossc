//
// Copyright (C) 2015-2017  Markus Hiienkari <mhiienka@niksula.hut.fi>
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
`define CPU_RESET_WIDTH 27  //1us

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

wire [15:0] sys_ctrl;
wire h_unstable;
wire [1:0] pclk_lock;
wire [1:0] pll_lock_lost;
wire [31:0] h_info, h_info2, v_info, extra_info;
wire [10:0] vmax, vmax_tvp;
wire [1:0] fpga_vsyncgen;

wire [15:0] ir_code;
wire [7:0] ir_code_cnt;

wire [7:0] R_out, G_out, B_out;
wire HSYNC_out;
wire VSYNC_out;
wire PCLK_out;
wire DE_out;

wire [7:0] R_out_videogen, G_out_videogen, B_out_videogen;
wire HSYNC_out_videogen;
wire VSYNC_out_videogen;
wire PCLK_out_videogen;
wire DE_out_videogen;


reg [7:0] cpu_reset_ctr = 0;
reg cpu_reset_n = 1'b0;

reg [7:0] R_in_L, G_in_L, B_in_L;
reg HSYNC_in_L, VSYNC_in_L, FID_in_L;

reg [1:0] btn_L, btn_LL;
reg ir_rx_L, ir_rx_LL, HDMI_TX_INT_N_L, HDMI_TX_INT_N_LL, HDMI_TX_MODE_L, HDMI_TX_MODE_LL;

wire lt_active = sys_ctrl[15];
wire lt_armed = sys_ctrl[14];
wire [1:0] lt_mode = sys_ctrl[13:12];
wire [1:0] lt_mode_synced;
wire [15:0] lt_lat_result;
wire [11:0] lt_stb_result;
wire lt_finished;

// Latch inputs from TVP7002 (synchronized to PCLK_in)
always @(posedge PCLK_in or negedge reset_n)
begin
    if (!reset_n) begin
        R_in_L <= 8'h00;
        G_in_L <= 8'h00;
        B_in_L <= 8'h00;
        HSYNC_in_L <= 1'b0;
        VSYNC_in_L <= 1'b0;
        FID_in_L <= 1'b0;
    end else begin
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
    if (!cpu_reset_n) begin
        btn_L <= 2'b00;
        btn_LL <= 2'b00;
        ir_rx_L <= 1'b0;
        ir_rx_LL <= 1'b0;
        HDMI_TX_INT_N_L <= 1'b0;
        HDMI_TX_INT_N_LL <= 1'b0;
        HDMI_TX_MODE_L <= 1'b0;
        HDMI_TX_MODE_LL <= 1'b0;
    end else begin
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
    if (cpu_reset_ctr == `CPU_RESET_WIDTH)
        cpu_reset_n <= 1'b1;
    else
        cpu_reset_ctr <= cpu_reset_ctr + 1'b1;
end

assign reset_n = sys_ctrl[0];   //HDMI_TX_RST_N in v1.2 PCB


`ifdef DEBUG
assign LED_R = HSYNC_in_L;
assign LED_G = VSYNC_in_L;
`else
assign LED_R = videogen_sel ? 1'b0 : ((pll_lock_lost != 2'h0)|h_unstable);
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
assign HDMI_TX_DE = videogen_sel ? DE_out_videogen : DE_out;
`else
wire videogen_sel;
assign videogen_sel = 1'b0;
assign HDMI_TX_RD = R_out;
assign HDMI_TX_GD = G_out;
assign HDMI_TX_BD = B_out;
assign HDMI_TX_HS = HSYNC_out;
assign HDMI_TX_VS = VSYNC_out;
assign HDMI_TX_PCLK = PCLK_out;
assign HDMI_TX_DE = DE_out;
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
    .pio_2_status_in_export                 ({VSYNC_out, 2'b00, vmax_tvp, fpga_vsyncgen, 5'h0, vmax}),
    .pio_3_h_info_out_export                (h_info),
    .pio_4_h_info2_out_export               (h_info2),
    .pio_5_v_info_out_export                (v_info),
    .pio_6_extra_info_out_export            (extra_info),
    .pio_7_lt_results_in_export             ({lt_finished, 3'h0, lt_stb_result, lt_lat_result})
);

scanconverter scanconverter_inst (
    .reset_n        (reset_n),
    .PCLK_in        (PCLK_in),
    .clk27          (clk27),
    .HSYNC_in       (HSYNC_in_L),
    .VSYNC_in       (VSYNC_in_L),
    .FID_in         (FID_in_L),
    .R_in           (R_in_L),
    .G_in           (G_in_L),
    .B_in           (B_in_L),
    .h_info         (h_info),
    .h_info2        (h_info2),
    .v_info         (v_info),
    .extra_info     (extra_info),
    .R_out          (R_out),
    .G_out          (G_out),
    .B_out          (B_out),
    .HSYNC_out      (HSYNC_out),
    .VSYNC_out      (VSYNC_out),
    .PCLK_out       (PCLK_out),
    .DE_out         (DE_out),
    .h_unstable     (h_unstable),
    .fpga_vsyncgen  (fpga_vsyncgen),
    .pclk_lock      (pclk_lock),
    .pll_lock_lost  (pll_lock_lost),
    .vmax           (vmax),
    .vmax_tvp       (vmax_tvp),
    .lt_active      (lt_active),
    .lt_mode        (lt_mode_synced)
);

ir_rcv ir0 (
    .clk27          (clk27),
    .reset_n        (cpu_reset_n),
    .ir_rx          (ir_rx_LL),
    .ir_code        (ir_code),
    .ir_code_ack    (),
    .ir_code_cnt    (ir_code_cnt)
);

lat_tester lt0 (
    .clk27          (clk27),
    .pclk           (HDMI_TX_PCLK),
    .active         (lt_active),
    .armed          (lt_armed),
    .sensor         (btn_LL[1]),
    .trigger        (HDMI_TX_DE & HDMI_TX_GD[0]),
    .VSYNC_in       (HDMI_TX_VS),
    .mode_in        (lt_mode),
    .mode_synced    (lt_mode_synced),
    .lat_result     (lt_lat_result),
    .stb_result     (lt_stb_result),
    .finished       (lt_finished)
);

`ifdef VIDEOGEN
videogen vg0 (
    .clk27          (clk27),
    .reset_n        (cpu_reset_n & videogen_sel),
    .lt_active      (lt_active),
    .lt_mode        (lt_mode_synced),
    .R_out          (R_out_videogen),
    .G_out          (G_out_videogen),
    .B_out          (B_out_videogen),
    .HSYNC_out      (HSYNC_out_videogen),
    .VSYNC_out      (VSYNC_out_videogen),
    .PCLK_out       (PCLK_out_videogen),
    .ENABLE_out     (DE_out_videogen)
);
`endif

endmodule
