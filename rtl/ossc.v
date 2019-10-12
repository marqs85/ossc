//
// Copyright (C) 2015-2019  Markus Hiienkari <mhiienka@niksula.hut.fi>
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
`define PO_RESET_WIDTH 27  //1us

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
    output HDMI_TX_PCLK,
    output reg [7:0] HDMI_TX_RD,
    output reg [7:0] HDMI_TX_GD,
    output reg [7:0] HDMI_TX_BD,
    output reg HDMI_TX_DE,
    output reg HDMI_TX_HS,
    output reg HDMI_TX_VS,
    input HDMI_TX_INT_N,
    input HDMI_TX_MODE,
    output hw_reset_n,
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
wire h_unstable, pll_lock_lost;
wire [31:0] h_config, h_config2, v_config, misc_config, sl_config, sl_config2;
wire [10:0] vmax, vmax_tvp;
wire [1:0] fpga_vsyncgen;
wire ilace_flag, vsync_flag;
wire [19:0] pcnt_frame;

wire [15:0] ir_code;
wire [7:0] ir_code_cnt;

wire [7:0] R_out_sc, G_out_sc, B_out_sc;
wire HSYNC_out_sc;
wire VSYNC_out_sc;
wire PCLK_out;
wire DE_out_sc;

wire [7:0] R_out_vg, G_out_vg, B_out_vg;
wire HSYNC_out_vg;
wire VSYNC_out_vg;
wire DE_out_vg;


reg [7:0] po_reset_ctr = 0;
reg po_reset_n = 1'b0;
wire jtagm_reset_req;
wire sys_reset_n = (po_reset_n & ~jtagm_reset_req);

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

wire remote_event = sys_ctrl[8];
reg remove_event_prev;
reg [14:0] to_ctr, to_ctr_ms;
wire lcd_bl_timeout;

wire osd_color, osd_enable_pre;
wire osd_enable = osd_enable_pre & ~lt_active;
wire [10:0] xpos, xpos_sc, xpos_vg;
wire [10:0] ypos, ypos_sc, ypos_vg;

wire pll_areset, pll_scanclk, pll_scanclkena, pll_configupdate, pll_scandata, pll_scandone, pll_activeclock;


// Latch inputs from TVP7002 (synchronized to PCLK_in)
always @(posedge PCLK_in or negedge hw_reset_n)
begin
    if (!hw_reset_n) begin
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
always @(posedge clk27 or negedge po_reset_n)
begin
    if (!po_reset_n) begin
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

// Power-on reset pulse generation (not strictly necessary)
always @(posedge clk27)
begin
    if (po_reset_ctr == `PO_RESET_WIDTH)
        po_reset_n <= 1'b1;
    else
        po_reset_ctr <= po_reset_ctr + 1'b1;
end

assign hw_reset_n = sys_ctrl[0];   //HDMI_TX_RST_N in v1.2 PCB


`ifdef DEBUG
assign LED_R = HSYNC_in_L;
assign LED_G = VSYNC_in_L;
`else
assign LED_R = (pll_lock_lost|h_unstable);
assign LED_G = (ir_code == 0);
`endif

assign SD_DAT[3] = sys_ctrl[7]; //SD_SPI_SS_N
assign LCD_CS_N = sys_ctrl[6];
assign LCD_RS = sys_ctrl[5];
wire lcd_bl_on = sys_ctrl[4];    //hw_reset_n in v1.2 PCB
wire [1:0] lcd_bl_time = sys_ctrl[3:2];
assign LCD_BL = lcd_bl_on ? (~lcd_bl_timeout | lt_active) : 1'b0;

wire enable_sc = sys_ctrl[1];
assign xpos = enable_sc ? xpos_sc : xpos_vg;
assign ypos = enable_sc ? ypos_sc : ypos_vg;
assign HDMI_TX_PCLK = PCLK_out;

always @(posedge PCLK_out) begin
    HDMI_TX_RD <= osd_enable ? {8{osd_color}} : (enable_sc ? R_out_sc : R_out_vg);
    HDMI_TX_GD <= osd_enable ? {8{osd_color}} : (enable_sc ? G_out_sc : G_out_vg);
    HDMI_TX_BD <= osd_enable ? 8'hff : (enable_sc ? B_out_sc : B_out_vg);
    HDMI_TX_HS <= enable_sc ? HSYNC_out_sc : HSYNC_out_vg;
    HDMI_TX_VS <= enable_sc ? VSYNC_out_sc : VSYNC_out_vg;
    HDMI_TX_DE <= enable_sc ? DE_out_sc : DE_out_vg;
end

// LCD backlight timeout counters
always @(posedge clk27)
begin
    if (remote_event != remove_event_prev) begin
        to_ctr <= 15'd0;
        to_ctr_ms <= 15'd0;
    end else begin
        if (to_ctr == 27000-1) begin
            to_ctr <= 0;
            if (to_ctr_ms < 15'h7fff)
                to_ctr_ms <= to_ctr_ms + 1'b1;
        end else begin
            to_ctr <= to_ctr + 1'b1;
        end
    end

    case (lcd_bl_time)
        default: lcd_bl_timeout <= 0; //off
        2'b01:  lcd_bl_timeout <= (to_ctr_ms >= 3000);  //3s
        2'b10:  lcd_bl_timeout <= (to_ctr_ms >= 10000); //10s
        2'b11:  lcd_bl_timeout <= (to_ctr_ms >= 30000); //30s
    endcase

    remove_event_prev <= remote_event;
end


sys sys_inst(
    .clk_clk                                (clk27),
    .reset_reset_n                          (sys_reset_n),
    .pulpino_0_config_testmode_i            (1'b0),
    .pulpino_0_config_fetch_enable_i        (1'b1),
    .pulpino_0_config_clock_gating_i        (1'b0),
    .pulpino_0_config_boot_addr_i           (32'h00010000),
    .master_0_master_reset_reset            (jtagm_reset_req),
    .i2c_opencores_0_export_scl_pad_io      (scl),
    .i2c_opencores_0_export_sda_pad_io      (sda),
    .i2c_opencores_0_export_spi_miso_pad_i  (1'b0),
    .i2c_opencores_1_export_scl_pad_io      (SD_CLK),
    .i2c_opencores_1_export_sda_pad_io      (SD_CMD),
    .i2c_opencores_1_export_spi_miso_pad_i  (SD_DAT[0]),
    .pio_0_sys_ctrl_out_export              (sys_ctrl),
    .pio_1_controls_in_export               ({ir_code_cnt, 4'b0000, pll_activeclock, HDMI_TX_MODE_LL, btn_LL, ir_code}),
    .sc_config_0_sc_if_sc_status_i          ({vsync_flag, 2'b00, vmax_tvp, fpga_vsyncgen, 4'h0, ilace_flag, vmax}),
    .sc_config_0_sc_if_sc_status2_i         ({12'h000, pcnt_frame}),
    .sc_config_0_sc_if_lt_status_i          ({lt_finished, 3'h0, lt_stb_result, lt_lat_result}),
    .sc_config_0_sc_if_h_config_o           (h_config),
    .sc_config_0_sc_if_h_config2_o          (h_config2),
    .sc_config_0_sc_if_v_config_o           (v_config),
    .sc_config_0_sc_if_misc_config_o        (misc_config),
    .sc_config_0_sc_if_sl_config_o          (sl_config),
    .sc_config_0_sc_if_sl_config2_o         (sl_config2),
    .osd_generator_0_osd_if_vclk            (PCLK_out),
    .osd_generator_0_osd_if_xpos            (xpos),
    .osd_generator_0_osd_if_ypos            (ypos),
    .osd_generator_0_osd_if_osd_enable      (osd_enable_pre),
    .osd_generator_0_osd_if_osd_color       (osd_color),
    .pll_reconfig_0_pll_reconfig_if_areset       (pll_areset),
    .pll_reconfig_0_pll_reconfig_if_scanclk      (pll_scanclk),
    .pll_reconfig_0_pll_reconfig_if_scanclkena   (pll_scanclkena),
    .pll_reconfig_0_pll_reconfig_if_configupdate (pll_configupdate),
    .pll_reconfig_0_pll_reconfig_if_scandata     (pll_scandata),
    .pll_reconfig_0_pll_reconfig_if_scandone     (pll_scandone)
);

scanconverter scanconverter_inst (
    .reset_n        (hw_reset_n),
    .PCLK_in        (PCLK_in),
    .clk27          (clk27),
    .enable_sc      (enable_sc),
    .HSYNC_in       (HSYNC_in_L),
    .VSYNC_in       (VSYNC_in_L),
    .FID_in         (FID_in_L),
    .R_in           (R_in_L),
    .G_in           (G_in_L),
    .B_in           (B_in_L),
    .h_config       (h_config),
    .h_config2      (h_config2),
    .v_config       (v_config),
    .misc_config    (misc_config),
    .sl_config      (sl_config),
    .sl_config2     (sl_config2),
    .R_out          (R_out_sc),
    .G_out          (G_out_sc),
    .B_out          (B_out_sc),
    .PCLK_out       (PCLK_out),
    .HSYNC_out      (HSYNC_out_sc),
    .VSYNC_out      (VSYNC_out_sc),
    .DE_out         (DE_out_sc),
    .h_unstable     (h_unstable),
    .fpga_vsyncgen  (fpga_vsyncgen),
    .pll_lock_lost  (pll_lock_lost),
    .vmax           (vmax),
    .vmax_tvp       (vmax_tvp),
    .pcnt_frame     (pcnt_frame),
    .ilace_flag     (ilace_flag),
    .vsync_flag     (vsync_flag),
    .lt_active      (lt_active),
    .lt_mode        (lt_mode_synced),
    .xpos           (xpos_sc),
    .ypos           (ypos_sc),
    .pll_areset       (pll_areset),
    .pll_scanclk      (pll_scanclk),
    .pll_scanclkena   (pll_scanclkena),
    .pll_configupdate (pll_configupdate),
    .pll_scandata     (pll_scandata),
    .pll_scandone     (pll_scandone),
    .pll_activeclock  (pll_activeclock)
);

ir_rcv ir0 (
    .clk27          (clk27),
    .reset_n        (po_reset_n),
    .ir_rx          (ir_rx_LL),
    .ir_code        (ir_code),
    .ir_code_ack    (),
    .ir_code_cnt    (ir_code_cnt)
);

lat_tester lt0 (
    .clk27          (clk27),
    .pclk           (PCLK_out),
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

videogen vg0 (
    .clk27          (PCLK_out),
    .reset_n        (po_reset_n & ~enable_sc),
    .lt_active      (lt_active),
    .lt_mode        (lt_mode_synced),
    .R_out          (R_out_vg),
    .G_out          (G_out_vg),
    .B_out          (B_out_vg),
    .HSYNC_out      (HSYNC_out_vg),
    .VSYNC_out      (VSYNC_out_vg),
    .DE_out         (DE_out_vg),
    .xpos           (xpos_vg),
    .ypos           (ypos_vg)
);

endmodule
