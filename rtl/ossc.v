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

    inout scl,
    inout sda,

    input ir_rx,
    input [1:0] btn,

    input TVP_PCLK_i,
    input [7:0] TVP_R_i,
    input [7:0] TVP_G_i,
    input [7:0] TVP_B_i,
    input TVP_HS_i,
    input TVP_HSYNC_i,
    input TVP_VSYNC_i,
    input TVP_FID_i,

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
    //output LED_R,

    output LCD_RS,
    output LCD_CS_N,
    output LCD_BL,

    output SD_CLK,
    inout SD_CMD,
    inout [3:0] SD_DAT
);


wire [31:0] sys_ctrl;
wire tvp_hsync_pol = sys_ctrl[16];
wire tvp_vsync_pol = sys_ctrl[17];
wire tvp_vsync_type = sys_ctrl[18];

wire h_unstable, pll_lock_lost;
wire [31:0] hv_in_config, hv_in_config2, hv_in_config3, misc_config, sl_config, sl_config2;
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

reg [7:0] TVP_R, TVP_G, TVP_B;
reg TVP_HS, TVP_VS, TVP_FID;
reg TVP_VS_sync1_reg, TVP_VS_sync2_reg;
reg TVP_HSYNC_sync1_reg, TVP_HSYNC_sync2_reg;
reg TVP_VSYNC_sync1_reg, TVP_VSYNC_sync2_reg;

reg [1:0] btn_L, btn_LL;
reg ir_rx_L, ir_rx_LL, HDMI_TX_INT_N_L, HDMI_TX_INT_N_LL, HDMI_TX_MODE_L, HDMI_TX_MODE_LL;

wire lt_sensor = btn_LL[1];
wire lt_active = sys_ctrl[15];
wire lt_armed = sys_ctrl[14];
wire lt_trigger = HDMI_TX_DE & HDMI_TX_GD[0];
wire [1:0] lt_mode = sys_ctrl[13:12];
wire [1:0] lt_mode_synced;
wire [15:0] lt_lat_result;
wire [11:0] lt_stb_result;
wire lt_trig_waiting;
wire lt_finished;

wire remote_event = sys_ctrl[8];
reg remove_event_prev;
reg [14:0] to_ctr, to_ctr_ms;
wire lcd_bl_timeout;

wire [1:0] osd_color;
wire osd_enable_pre;
wire osd_enable = osd_enable_pre & ~lt_active;
wire [10:0] xpos, xpos_sc, xpos_vg;
wire [10:0] ypos, ypos_sc, ypos_vg;

wire pll_areset, pll_scanclk, pll_scanclkena, pll_configupdate, pll_scandata, pll_scandone, pll_activeclock;


// TVP7002 RGB digitizer
always @(posedge TVP_PCLK_i) begin
    TVP_R <= TVP_R_i;
    TVP_G <= TVP_G_i;
    TVP_B <= TVP_B_i;
    TVP_HS <= TVP_HS_i;
    TVP_VS <= TVP_VSYNC_i;
    TVP_FID <= TVP_FID_i;

    // sync to pclk
    TVP_VS_sync1_reg <= TVP_VSYNC_i;
    TVP_VS_sync2_reg <= TVP_VS_sync1_reg;
end
always @(posedge clk27) begin
    // sync to always-running fixed meas clk
    TVP_HSYNC_sync1_reg <= TVP_HSYNC_i;
    TVP_HSYNC_sync2_reg <= TVP_HSYNC_sync1_reg;
    TVP_VSYNC_sync1_reg <= TVP_VSYNC_i;
    TVP_VSYNC_sync2_reg <= TVP_VSYNC_sync1_reg;
end

wire [7:0] TVP_R_post, TVP_G_post, TVP_B_post;
wire TVP_HSYNC_post, TVP_VSYNC_post, TVP_DE_post, TVP_FID_post, TVP_datavalid_post;
wire TVP_fe_interlace, TVP_fe_frame_change, TVP_sof_scaler;
wire [19:0] TVP_fe_pcnt_frame;
wire [10:0] TVP_fe_vtotal, TVP_fe_xpos, TVP_fe_ypos;
tvp7002_frontend u_tvp_frontend ( 
    .PCLK_i(TVP_PCLK_i),
    .CLK_MEAS_i(clk27),
    .reset_n(sys_reset_n),
    .R_i(TVP_R),
    .G_i(TVP_G),
    .B_i(TVP_B),
    .HS_i(TVP_HS),
    .VS_i(TVP_VS_sync2_reg),
    .HSYNC_i(TVP_HSYNC_sync2_reg),
    .VSYNC_i(TVP_VSYNC_sync2_reg),
    .DE_i(1'b0),
    .FID_i(1'b0),
    .hsync_i_polarity(tvp_hsync_pol),
    .vsync_i_polarity(tvp_vsync_pol),
    .vsync_i_type(tvp_vsync_type),
    .hv_in_config(hv_in_config),
    .hv_in_config2(hv_in_config2),
    .hv_in_config3(hv_in_config3),
    .R_o(TVP_R_post),
    .G_o(TVP_G_post),
    .B_o(TVP_B_post),
    .HSYNC_o(TVP_HSYNC_post),
    .VSYNC_o(TVP_VSYNC_post),
    .DE_o(TVP_DE_post),
    .FID_o(TVP_FID_post),
    .interlace_flag(TVP_fe_interlace),
    .datavalid_o(TVP_datavalid_post),
    .xpos_o(TVP_fe_xpos),
    .ypos_o(TVP_fe_ypos),
    .vtotal(TVP_fe_vtotal),
    .frame_change(TVP_fe_frame_change),
    .sof_scaler(TVP_sof_scaler),
    .pcnt_frame(TVP_fe_pcnt_frame)
);

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
//assign LED_R = lt_active ? lt_trig_waiting : (pll_lock_lost|h_unstable);
assign LED_G = lt_active ? ~lt_sensor : (ir_code == 0) & ~(pll_lock_lost|h_unstable);
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
    if (osd_enable) begin
        if (osd_color == 2'h0) begin
            {HDMI_TX_RD, HDMI_TX_GD, HDMI_TX_BD} <= 24'h000000;
        end else if (osd_color == 2'h1) begin
            {HDMI_TX_RD, HDMI_TX_GD, HDMI_TX_BD} <= 24'h0000ff;
        end else if (osd_color == 2'h2) begin
            {HDMI_TX_RD, HDMI_TX_GD, HDMI_TX_BD} <= 24'hffff00;
        end else begin
            {HDMI_TX_RD, HDMI_TX_GD, HDMI_TX_BD} <= 24'hffffff;
        end
    end else if (enable_sc) begin
        {HDMI_TX_RD, HDMI_TX_GD, HDMI_TX_BD} <= {R_out_sc, G_out_sc, B_out_sc};
    end else begin
        {HDMI_TX_RD, HDMI_TX_GD, HDMI_TX_BD} <= {R_out_vg, G_out_vg, B_out_vg};
    end

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
    .sc_config_0_sc_if_sc_status_i          ({vsync_flag, 2'b00, vmax_tvp, fpga_vsyncgen, 4'h0, TVP_fe_interlace, TVP_fe_vtotal}),
    .sc_config_0_sc_if_sc_status2_i         ({12'h000, TVP_fe_pcnt_frame}),
    .sc_config_0_sc_if_lt_status_i          ({lt_finished, 3'h0, lt_stb_result, lt_lat_result}),
    .sc_config_0_sc_if_h_config_o           (hv_in_config),
    .sc_config_0_sc_if_h_config2_o          (hv_in_config2),
    .sc_config_0_sc_if_v_config_o           (hv_in_config3),
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
    .PCLK_in        (TVP_PCLK_i),
    .clk27          (clk27),
    .enable_sc      (enable_sc),
    .HSYNC_in       (TVP_HSYNC_post),
    .VSYNC_in       (TVP_VSYNC_post),
    .FID_in         (~TVP_FID_post),
    .R_in           (TVP_R_post),
    .G_in           (TVP_G_post),
    .B_in           (TVP_B_post),
    .hv_in_config   (hv_in_config),
    .hv_in_config2  (hv_in_config2),
    .hv_in_config3  (hv_in_config3),
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
    .sensor         (lt_sensor),
    .trigger        (lt_trigger),
    .VSYNC_in       (HDMI_TX_VS),
    .mode_in        (lt_mode),
    .mode_synced    (lt_mode_synced),
    .lat_result     (lt_lat_result),
    .stb_result     (lt_stb_result),
    .trig_waiting   (lt_trig_waiting),
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
