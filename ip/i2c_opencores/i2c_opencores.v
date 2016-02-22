//
// fixed for 9.1 jan 21 2010 cruben
//
`include "timescale.v"
`include "i2c_master_defines.v"

module i2c_opencores
(
	wb_clk_i, wb_rst_i, wb_adr_i, wb_dat_i, wb_dat_o,
	wb_we_i, wb_stb_i, /*wb_cyc_i,*/ wb_ack_o, wb_inta_o,
	scl_pad_io, sda_pad_io
);


// Common bus signals
input        wb_clk_i;		// WISHBONE clock
input        wb_rst_i;		// WISHBONE reset

// Slave signals
input  [2:0] wb_adr_i;		// WISHBONE address input
input  [7:0] wb_dat_i;		// WISHBONE data input
output [7:0] wb_dat_o;		// WISHBONE data output
input        wb_we_i;		// WISHBONE write enable input
input        wb_stb_i;		// WISHBONE strobe input
//input        wb_cyc_i;		// WISHBONE cycle input
output       wb_ack_o;		// WISHBONE acknowledge output
output       wb_inta_o; 	// WISHBONE interrupt output

// I2C signals
inout        scl_pad_io;	// I2C clock io
inout        sda_pad_io;	// I2C data io

wire        wb_cyc_i;		// WISHBONE cycle input
// Wire tri-state scl/sda
wire scl_pad_i;
wire scl_pad_o;
wire scl_pad_io;
wire scl_padoen_o;

assign wb_cyc_i = wb_stb_i;
assign scl_pad_i = scl_pad_io;
assign scl_pad_io = scl_padoen_o ? 1'bZ : scl_pad_o;

wire sda_pad_i;
wire sda_pad_o;
wire sda_pad_io;
wire sda_padoen_o;

assign sda_pad_i = sda_pad_io;
assign sda_pad_io = sda_padoen_o ? 1'bZ : sda_pad_o;

// Avalon doesn't have an asynchronous reset
//  set it to be inactive and just use synchronous reset
//  reset level is a parameter, 0 is the default (active-low reset)
wire arst_i;

assign arst_i = 1'b1;

// Connect the top level I2C core
i2c_master_top i2c_master_top_inst
(
	.wb_clk_i(wb_clk_i), .wb_rst_i(wb_rst_i), .arst_i(arst_i),
	
	.wb_adr_i(wb_adr_i), .wb_dat_i(wb_dat_i), .wb_dat_o(wb_dat_o),
	.wb_we_i(wb_we_i), .wb_stb_i(wb_stb_i), .wb_cyc_i(wb_cyc_i),
	.wb_ack_o(wb_ack_o), .wb_inta_o(wb_inta_o),
	
	.scl_pad_i(scl_pad_i), .scl_pad_o(scl_pad_o), .scl_padoen_o(scl_padoen_o),
	.sda_pad_i(sda_pad_i), .sda_pad_o(sda_pad_o), .sda_padoen_o(sda_padoen_o)
);

endmodule
