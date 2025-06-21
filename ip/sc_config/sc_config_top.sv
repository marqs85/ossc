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

module sc_config_top(
    // common
    input clk_i,
    input rst_i,
    // avalon slave
    input [31:0] avalon_s_writedata,
    output reg [31:0] avalon_s_readdata,
    input [9:0] avalon_s_address,
    input [3:0] avalon_s_byteenable,
    input avalon_s_write,
    input avalon_s_read,
    input avalon_s_chipselect,
    output avalon_s_waitrequest_n,
    // SC interface
    input [31:0] fe_status_i,
    input [31:0] fe_status2_i,
    input [31:0] lt_status_i,
    output [31:0] hv_in_config_o,
    output [31:0] hv_in_config2_o,
    output [31:0] hv_in_config3_o,
    output [31:0] hv_out_config_o,
    output [31:0] hv_out_config2_o,
    output [31:0] hv_out_config3_o,
    output [31:0] xy_out_config_o,
    output [31:0] xy_out_config2_o,
    output [31:0] misc_config_o,
    output [31:0] sl_config_o,
    output [31:0] sl_config2_o,
    output [31:0] sl_config3_o,
    // Shadow mask read interface
    input vclk,
    input [3:0] shmask_xpos,
    input [3:0] shmask_ypos,
    output [10:0] shmask_data,
    // Lumacode interface
    input lumacode_clk_i,
    input [8:0] lumacode_addr_i,
    input lumacode_rden_i,
    output [31:0] lumacode_data_o
);

localparam FE_STATUS_REGNUM =      10'h0;
localparam FE_STATUS2_REGNUM =     10'h1;
localparam LT_STATUS_REGNUM =      10'h2;
localparam HV_IN_CONFIG_REGNUM =   10'h3;
localparam HV_IN_CONFIG2_REGNUM =  10'h4;
localparam HV_IN_CONFIG3_REGNUM =  10'h5;
localparam HV_OUT_CONFIG_REGNUM =  10'h6;
localparam HV_OUT_CONFIG2_REGNUM = 10'h7;
localparam HV_OUT_CONFIG3_REGNUM = 10'h8;
localparam XY_OUT_CONFIG_REGNUM =  10'h9;
localparam XY_OUT_CONFIG2_REGNUM = 10'ha;
localparam MISC_CONFIG_REGNUM =    10'hb;
localparam SL_CONFIG_REGNUM =      10'hc;
localparam SL_CONFIG2_REGNUM =     10'hd;
localparam SL_CONFIG3_REGNUM =     10'he;

localparam SHMASK_DATA_OFFSET =    10'h100;
localparam LUMACODE_RAM_OFFSET =   10'h200;

reg [31:0] config_reg[HV_IN_CONFIG_REGNUM:SL_CONFIG3_REGNUM] /* synthesis ramstyle = "logic" */;

assign avalon_s_waitrequest_n = 1'b1;

// Shadow mask array
altsyncram shmask_array_inst (
    .address_a (avalon_s_address[7:0]),
    .address_b ({shmask_ypos, shmask_xpos}),
    .clock0 (clk_i),
    .clock1 (vclk),
    .data_a (avalon_s_writedata[10:0]),
    .wren_a (avalon_s_chipselect && avalon_s_write && (avalon_s_address >= SHMASK_DATA_OFFSET) && (avalon_s_address < LUMACODE_RAM_OFFSET)),
    .q_b (shmask_data),
    .aclr0 (1'b0),
    .aclr1 (1'b0),
    .addressstall_a (1'b0),
    .addressstall_b (1'b0),
    .byteena_a (1'b1),
    .byteena_b (1'b1),
    .clocken0 (1'b1),
    .clocken1 (1'b1),
    .clocken2 (1'b1),
    .clocken3 (1'b1),
    .data_b ({11{1'b1}}),
    .eccstatus (),
    .q_a (),
    .rden_a (1'b1),
    .rden_b (1'b1),
    .wren_b (1'b0));
defparam
    shmask_array_inst.address_aclr_b = "NONE",
    shmask_array_inst.address_reg_b = "CLOCK1",
    shmask_array_inst.clock_enable_input_a = "BYPASS",
    shmask_array_inst.clock_enable_input_b = "BYPASS",
    shmask_array_inst.clock_enable_output_b = "BYPASS",
    shmask_array_inst.intended_device_family = "Cyclone IV E",
    shmask_array_inst.lpm_type = "altsyncram",
    shmask_array_inst.numwords_a = 256,
    shmask_array_inst.numwords_b = 256,
    shmask_array_inst.operation_mode = "DUAL_PORT",
    shmask_array_inst.outdata_aclr_b = "NONE",
    shmask_array_inst.outdata_reg_b = "CLOCK1",
    shmask_array_inst.power_up_uninitialized = "FALSE",
    shmask_array_inst.widthad_a = 8,
    shmask_array_inst.widthad_b = 8,
    shmask_array_inst.width_a = 11,
    shmask_array_inst.width_b = 11,
    shmask_array_inst.width_byteena_a = 1;

// Lumacode palette RAM
altsyncram lumacode_pal_ram (
            .address_a (avalon_s_address[8:0]),
            .address_b (lumacode_addr_i),
            .clock0 (clk_i),
            .clock1 (lumacode_clk_i),
            .data_a (avalon_s_writedata),
            .rden_b (lumacode_rden_i),
            .wren_a (avalon_s_chipselect && avalon_s_write && (avalon_s_address >= LUMACODE_RAM_OFFSET)),
            .q_b (lumacode_data_o),
            .aclr0 (1'b0),
            .aclr1 (1'b0),
            .addressstall_a (1'b0),
            .addressstall_b (1'b0),
            .byteena_a (1'b1),
            .byteena_b (1'b1),
            .clocken0 (1'b1),
            .clocken1 (1'b1),
            .clocken2 (1'b1),
            .clocken3 (1'b1),
            .data_b ({32{1'b1}}),
            .eccstatus (),
            .q_a (),
            .rden_a (1'b1),
            .wren_b (1'b0));
defparam
    lumacode_pal_ram.address_aclr_b = "NONE",
    lumacode_pal_ram.address_reg_b = "CLOCK1",
    lumacode_pal_ram.clock_enable_input_a = "BYPASS",
    lumacode_pal_ram.clock_enable_input_b = "BYPASS",
    lumacode_pal_ram.clock_enable_output_b = "BYPASS",
    lumacode_pal_ram.intended_device_family = "Cyclone IV E",
    lumacode_pal_ram.lpm_type = "altsyncram",
    lumacode_pal_ram.numwords_a = 512,
    lumacode_pal_ram.numwords_b = 512,
    lumacode_pal_ram.operation_mode = "DUAL_PORT",
    lumacode_pal_ram.outdata_aclr_b = "NONE",
    lumacode_pal_ram.outdata_reg_b = "UNREGISTERED",
    lumacode_pal_ram.power_up_uninitialized = "FALSE",
    lumacode_pal_ram.rdcontrol_reg_b = "CLOCK1",
    lumacode_pal_ram.widthad_a = 9,
    lumacode_pal_ram.widthad_b = 9,
    lumacode_pal_ram.width_a = 32,
    lumacode_pal_ram.width_b = 32,
    lumacode_pal_ram.width_byteena_a = 1;


genvar i;
generate
    for (i=HV_IN_CONFIG_REGNUM; i <= SL_CONFIG3_REGNUM; i++) begin : gen_reg
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


// no readback for config regs -> unused bits optimized out
always @(*) begin
    if (avalon_s_chipselect && avalon_s_read) begin
        case (avalon_s_address)
            FE_STATUS_REGNUM: avalon_s_readdata = fe_status_i;
            FE_STATUS2_REGNUM: avalon_s_readdata = fe_status2_i;
            LT_STATUS_REGNUM: avalon_s_readdata = lt_status_i;
            default: avalon_s_readdata = 32'h00000000;
        endcase
    end else begin
        avalon_s_readdata = 32'h00000000;
    end
end

assign hv_in_config_o = config_reg[HV_IN_CONFIG_REGNUM];
assign hv_in_config2_o = config_reg[HV_IN_CONFIG2_REGNUM];
assign hv_in_config3_o = config_reg[HV_IN_CONFIG3_REGNUM];
assign hv_out_config_o = config_reg[HV_OUT_CONFIG_REGNUM];
assign hv_out_config2_o = config_reg[HV_OUT_CONFIG2_REGNUM];
assign hv_out_config3_o = config_reg[HV_OUT_CONFIG3_REGNUM];
assign xy_out_config_o = config_reg[XY_OUT_CONFIG_REGNUM];
assign xy_out_config2_o = config_reg[XY_OUT_CONFIG2_REGNUM];
assign misc_config_o = config_reg[MISC_CONFIG_REGNUM];
assign sl_config_o = config_reg[SL_CONFIG_REGNUM];
assign sl_config2_o = config_reg[SL_CONFIG2_REGNUM];
assign sl_config3_o = config_reg[SL_CONFIG3_REGNUM];

endmodule
