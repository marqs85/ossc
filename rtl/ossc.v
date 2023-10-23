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
    input [1:0] cfg,

    input TVP_PCLK_i,
    input [7:0] TVP_R_i,
    input [7:0] TVP_G_i,
    input [7:0] TVP_B_i,
    input TVP_HS_i,
    input TVP_SOG_i,
    input TVP_VSYNC_i,

    output HDMI_TX_PCLK,
    output reg [7:0] HDMI_TX_RD,
    output reg [7:0] HDMI_TX_GD,
    output reg [7:0] HDMI_TX_BD,
    output reg HDMI_TX_DE,
    output reg HDMI_TX_HS,
    output reg HDMI_TX_VS,
    input HDMI_TX_INT_N,

    output hw_reset_n,

    output LED_G,
    inout LED_R,

    output LCD_RS,
    output LCD_CS_N,
    output LCD_BL,

    output SD_CLK,
    inout SD_CMD,
    inout [3:0] SD_DAT
);


wire [31:0] sys_ctrl;
wire lt_active = sys_ctrl[15];
wire lt_armed = sys_ctrl[14];
wire [1:0] lt_mode = sys_ctrl[13:12];
wire tvp_vsync_type = sys_ctrl[10];
wire pll_bypass = sys_ctrl[9];
wire remote_event = sys_ctrl[8];
assign SD_DAT[3] = sys_ctrl[7]; //SD_SPI_SS_N
assign LCD_CS_N = sys_ctrl[6];
assign LCD_RS = sys_ctrl[5];
wire lcd_bl_on = sys_ctrl[4];    //hw_reset_n in v1.2 PCB
wire [1:0] lcd_bl_time = sys_ctrl[3:2];
wire enable_sc = sys_ctrl[1];
assign hw_reset_n = sys_ctrl[0];   //HDMI_TX_RST_N in v1.2 PCB

wire [31:0] hv_in_config, hv_in_config2, hv_in_config3, hv_out_config, hv_out_config2, hv_out_config3, xy_out_config, xy_out_config2;
wire [31:0] misc_config, sl_config, sl_config2, sl_config3;

wire pll_clkout, pll_clkswitch, pll_locked;
wire clkmux_clkout;

wire [15:0] ir_code;
wire [7:0] ir_code_cnt;

wire [7:0] R_sc, G_sc, B_sc;
wire HSYNC_sc, VSYNC_sc, DE_sc;
wire pll_areset, pll_scanclk, pll_scanclkena, pll_configupdate, pll_scandata, pll_scandone, pll_activeclock;
wire PCLK_sc;
wire pclk_out = PCLK_sc;


reg [7:0] po_reset_ctr = 0;
reg po_reset_n = 1'b0;
wire jtagm_reset_req;
wire sys_reset_n = (po_reset_n & ~jtagm_reset_req);

reg [7:0] TVP_R, TVP_G, TVP_B;
reg TVP_HS, TVP_VS, TVP_FID;
reg TVP_VS_sync1_reg, TVP_VS_sync2_reg;
reg TVP_SOG_sync1_reg, TVP_SOG_sync2_reg, TVP_SOG_prev;
reg TVP_HSYNC_sync1_reg, TVP_HSYNC_sync2_reg;
reg TVP_VSYNC_sync1_reg, TVP_VSYNC_sync2_reg;

reg [1:0] btn_L, btn_LL;
reg ir_rx_L, ir_rx_LL, HDMI_TX_INT_N_L, HDMI_TX_INT_N_LL;
reg vsync_flag_sync1_reg, vsync_flag_sync2_reg;
reg [1:0] cfg_reg;
reg cfg_stored;
wire TVP_SOG_ALT_i;
wire TVP_HSYNC_i = cfg_reg[1] ? TVP_SOG_i : TVP_SOG_ALT_i;

reg [23:0] resync_led_ctr, warn_pll_lock_lost;
reg resync_strobe_sync1_reg, resync_strobe_sync2_reg, resync_strobe_prev;
wire resync_strobe_i;
wire resync_strobe = resync_strobe_sync2_reg;

wire [31:0] controls = {ir_code_cnt, 3'b000, vsync_flag_sync2_reg, pll_activeclock, cfg_reg[0], btn_LL, ir_code};

wire lt_sensor = btn_LL[1];
wire lt_trigger = DE_sc & G_sc[0];
wire [1:0] lt_mode_synced;
wire [15:0] lt_lat_result;
wire [11:0] lt_stb_result;
wire lt_trig_waiting;
wire lt_finished;

reg remove_event_prev;
reg [14:0] to_ctr, to_ctr_ms;
wire lcd_bl_timeout;

wire [1:0] osd_color;
wire osd_enable_pre;
wire osd_enable = osd_enable_pre & ~lt_active;
wire [10:0] xpos_sc;
wire [10:0] ypos_sc;

wire resync_indicator = (warn_pll_lock_lost != 0)  | (resync_led_ctr != 0);
wire LED_R_i = lt_active ? lt_trig_waiting : resync_indicator;
assign LED_G = lt_active ? ~lt_sensor : (ir_code == 0) & (~resync_indicator | cfg_reg[1]);

assign LCD_BL = lcd_bl_on ? (~lcd_bl_timeout | lt_active) : 1'b0;
assign HDMI_TX_PCLK = pclk_out;


// TVP7002 RGB digitizer
always @(posedge TVP_PCLK_i) begin
    TVP_R <= TVP_R_i;
    TVP_G <= TVP_G_i;
    TVP_B <= TVP_B_i;
    TVP_HS <= TVP_HS_i;
    TVP_VS <= TVP_VSYNC_i;

    // sync to pclk
    TVP_SOG_sync1_reg <= TVP_HSYNC_i;
    TVP_SOG_sync2_reg <= TVP_SOG_sync1_reg;
    TVP_SOG_prev <= TVP_SOG_sync2_reg;
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
wire TVP_fe_interlace, TVP_fe_frame_change, TVP_sof_scaler, TVP_sync_active;
wire [19:0] TVP_fe_pcnt_frame;
wire [7:0] TVP_hsync_width;
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
    .sogref_update_i(TVP_SOG_prev & ~TVP_SOG_sync2_reg),
    .vsync_i_type(tvp_vsync_type),
    .hv_in_config(hv_in_config),
    .hv_in_config2(hv_in_config2),
    .hv_in_config3(hv_in_config3),
    .misc_config(misc_config),
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
    .pcnt_frame(TVP_fe_pcnt_frame),
    .hsync_width(TVP_hsync_width),
    .sync_active(TVP_sync_active)
);

// Insert synchronizers to async inputs (synchronize to CPU clock)
always @(posedge clk27 or negedge po_reset_n)
begin
    if (!po_reset_n) begin
        {btn_L, btn_LL} <= '0;
        {ir_rx_L, ir_rx_LL} <= '0;
        {HDMI_TX_INT_N_L, HDMI_TX_INT_N_LL} <= '0;
        {cfg_stored, cfg_reg} <= '0;
    end else begin
        {btn_L, btn_LL} <= {btn, btn_L};
        {ir_rx_L, ir_rx_LL} <=  {ir_rx, ir_rx_L};
        {HDMI_TX_INT_N_L, HDMI_TX_INT_N_LL} <= {HDMI_TX_INT_N, HDMI_TX_INT_N_L};
        if (!cfg_stored)
            cfg_reg <= cfg;
        cfg_stored <= 1'b1;
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

// Sync vsync flag to CPU clock
always @(posedge clk27) begin
    {vsync_flag_sync1_reg, vsync_flag_sync2_reg} <= {~VSYNC_sc, vsync_flag_sync1_reg};
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

// Generate a warning signal from sync lock loss
always @(posedge clk27) begin
    if (enable_sc) begin
        if (~resync_strobe_prev & resync_strobe) begin
            resync_led_ctr <= {24{1'b1}};
        end else if (resync_led_ctr > 0) begin
            resync_led_ctr <= resync_led_ctr - 1'b1;
        end
    end

    resync_strobe_sync1_reg <= resync_strobe_i;
    resync_strobe_sync2_reg <= resync_strobe_sync1_reg;
    resync_strobe_prev <= resync_strobe_sync2_reg;
end

// Generate a warning signal from PLL lock loss
always @(posedge clk27 or negedge sys_reset_n)
begin
    if (!sys_reset_n) begin
        warn_pll_lock_lost <= 1'b0;
    end else begin
        if (~pll_areset & ~pll_locked)
            warn_pll_lock_lost <= 1;
        else if (warn_pll_lock_lost != 0)
            warn_pll_lock_lost <= warn_pll_lock_lost + 1'b1;
    end
end

// Control PLL reference clock switchover
always @(posedge clk27)
begin
    pll_clkswitch <= (pll_activeclock != enable_sc);
end

// Output registers
always @(posedge pclk_out) begin
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
    end else begin
        {HDMI_TX_RD, HDMI_TX_GD, HDMI_TX_BD} <= {R_sc, G_sc, B_sc};
    end

    HDMI_TX_HS <= HSYNC_sc;
    HDMI_TX_VS <= VSYNC_sc;
    HDMI_TX_DE <= DE_sc;
end


// A modified <= v1.7 board uses LED_R for TVP_HSYNC input
ALT_IOBUF led_r_iobuf (.i(LED_R_i), .oe(cfg_reg[1]), .o(TVP_SOG_ALT_i), .io(LED_R));

pll_2x pll_pclk (
    .areset(pll_areset),
    .clkswitch(pll_clkswitch),
    .configupdate(pll_configupdate),
    .inclk0(clk27), // set videogen clock to primary (power-on default) since both reference clocks must be running during switchover
    .inclk1(TVP_PCLK_i), // is the secondary input clock fully compensated?
    .scanclk(pll_scanclk),
    .scanclkena(pll_scanclkena),
    .scandata(pll_scandata),
    .activeclock(pll_activeclock),
    .c0(pll_clkout),
    .locked(pll_locked),
    .scandataout(),
    .scandone(pll_scandone)
);

cycloneive_clkctrl clkctrl1 ( 
    .clkselect(pll_bypass ? 2'h0 : 2'h2),
    .ena(1'b1),
    .inclk({1'b0, pll_clkout, 1'b0, TVP_PCLK_i}), // fitter forbids using both clk27 and pclk_1x here since they're on opposite sides
    .outclk(clkmux_clkout)
// synopsys translate_off
    ,
    .devclrn(1'b1),
    .devpor(1'b1)
// synopsys translate_on
);
defparam
    clkctrl1.clock_type = "Global Clock",
    clkctrl1.ena_register_mode = "falling edge",
    clkctrl1.lpm_type = "cycloneive_clkctrl";

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
    .pio_1_controls_in_export               (controls),
    .sc_config_0_sc_if_fe_status_i          ({19'h0, TVP_sync_active, TVP_fe_interlace, TVP_fe_vtotal}),
    .sc_config_0_sc_if_fe_status2_i         ({4'h0, TVP_hsync_width, TVP_fe_pcnt_frame}),
    .sc_config_0_sc_if_lt_status_i          (32'h00000000),
    .sc_config_0_sc_if_hv_in_config_o       (hv_in_config),
    .sc_config_0_sc_if_hv_in_config2_o      (hv_in_config2),
    .sc_config_0_sc_if_hv_in_config3_o      (hv_in_config3),
    .sc_config_0_sc_if_hv_out_config_o      (hv_out_config),
    .sc_config_0_sc_if_hv_out_config2_o     (hv_out_config2),
    .sc_config_0_sc_if_hv_out_config3_o     (hv_out_config3),
    .sc_config_0_sc_if_xy_out_config_o      (xy_out_config),
    .sc_config_0_sc_if_xy_out_config2_o     (xy_out_config2),
    .sc_config_0_sc_if_misc_config_o        (misc_config),
    .sc_config_0_sc_if_sl_config_o          (sl_config),
    .sc_config_0_sc_if_sl_config2_o         (sl_config2),
    .sc_config_0_sc_if_sl_config3_o         (sl_config3),
    .osd_generator_0_osd_if_vclk            (PCLK_sc),
    .osd_generator_0_osd_if_xpos            (xpos_sc),
    .osd_generator_0_osd_if_ypos            (ypos_sc),
    .osd_generator_0_osd_if_osd_enable      (osd_enable_pre),
    .osd_generator_0_osd_if_osd_color       (osd_color),
    .pll_reconfig_0_pll_reconfig_if_areset       (pll_areset),
    .pll_reconfig_0_pll_reconfig_if_scanclk      (pll_scanclk),
    .pll_reconfig_0_pll_reconfig_if_scanclkena   (pll_scanclkena),
    .pll_reconfig_0_pll_reconfig_if_configupdate (pll_configupdate),
    .pll_reconfig_0_pll_reconfig_if_scandata     (pll_scandata),
    .pll_reconfig_0_pll_reconfig_if_scandone     (pll_scandone)
);

scanconverter #(
    .EMIF_ENABLE(0),
    .NUM_LINE_BUFFERS(2)
  ) scanconverter_inst (
    .PCLK_CAP_i(TVP_PCLK_i),
    .PCLK_OUT_i(clkmux_clkout),
    .reset_n(hw_reset_n),  //TODO: sync to pclk_capture
    .R_i(TVP_R_post),
    .G_i(TVP_G_post),
    .B_i(TVP_B_post),
    .HSYNC_i(TVP_HSYNC_post),
    .VSYNC_i(TVP_VSYNC_post),
    .DE_i(TVP_DE_post),
    .FID_i(TVP_FID_post),
    .datavalid_i(TVP_datavalid_post),
    .interlaced_in_i(TVP_fe_interlace),
    .frame_change_i(TVP_fe_frame_change),
    .xpos_i(TVP_fe_xpos),
    .ypos_i(TVP_fe_ypos),
    .h_in_active(hv_in_config[23:12]),
    .hv_out_config(hv_out_config),
    .hv_out_config2(hv_out_config2),
    .hv_out_config3(hv_out_config3),
    .xy_out_config(xy_out_config),
    .xy_out_config2(xy_out_config2),
    .misc_config(misc_config),
    .sl_config(sl_config),
    .sl_config2(sl_config2),
    .sl_config3(sl_config3),
    .testpattern_enable(~enable_sc),
    .lb_enable(enable_sc),
    .ext_sync_mode(1'b0),
    .ext_frame_change_i(1'b0),
    .ext_R_i(8'h00),
    .ext_G_i(8'h00),
    .ext_B_i(8'h00),
    .PCLK_o(PCLK_sc),
    .R_o(R_sc),
    .G_o(G_sc),
    .B_o(B_sc),
    .HSYNC_o(HSYNC_sc),
    .VSYNC_o(VSYNC_sc),
    .DE_o(DE_sc),
    .xpos_o(xpos_sc),
    .ypos_o(ypos_sc),
    .resync_strobe(resync_strobe_i),
    .emif_br_clk(1'b0),
    .emif_br_reset(1'b0),
    .emif_rd_addr(),
    .emif_rd_read(),
    .emif_rd_rdata(0),
    .emif_rd_waitrequest(0),
    .emif_rd_readdatavalid(0),
    .emif_rd_burstcount(),
    .emif_wr_addr(),
    .emif_wr_write(),
    .emif_wr_wdata(),
    .emif_wr_waitrequest(0),
    .emif_wr_burstcount()
);

ir_rcv ir0 (
    .clk27          (clk27),
    .reset_n        (po_reset_n),
    .ir_rx          (ir_rx_LL),
    .ir_code        (ir_code),
    .ir_code_ack    (),
    .ir_code_cnt    (ir_code_cnt)
);

/*lat_tester lt0 (
    .clk27          (clk27),
    .pclk           (PCLK_sc),
    .active         (lt_active),
    .armed          (lt_armed),
    .sensor         (lt_sensor),
    .trigger        (lt_trigger),
    .VSYNC_in       (VSYNC_sc),
    .mode_in        (lt_mode),
    .mode_synced    (lt_mode_synced),
    .lat_result     (lt_lat_result),
    .stb_result     (lt_stb_result),
    .trig_waiting   (lt_trig_waiting),
    .finished       (lt_finished)
);*/

endmodule
