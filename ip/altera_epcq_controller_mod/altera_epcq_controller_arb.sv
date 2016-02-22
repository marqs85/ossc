// (C) 2001-2015 Altera Corporation. All rights reserved.
// Your use of Altera Corporation's design tools, logic functions and other 
// software and tools, and its AMPP partner logic functions, and any output 
// files any of the foregoing (including device programming or simulation 
// files), and any associated documentation or information are expressly subject 
// to the terms and conditions of the Altera Program License Subscription 
// Agreement, Altera MegaCore Function License Agreement, or other applicable 
// license agreement, including, without limitation, that your use is for the 
// sole purpose of programming logic devices manufactured by Altera and sold by 
// Altera or its authorized distributors.  Please refer to the applicable 
// agreement for further details.


// (C) 2001-2014 Altera Corporation. All rights reserved.
// Your use of Altera Corporation's design tools, logic functions and other 
// software and tools, and its AMPP partner logic functions, and any output 
// files any of the foregoing (including device programming or simulation 
// files), and any associated documentation or information are expressly subject 
// to the terms and conditions of the Altera Program License Subscription 
// Agreement, Altera MegaCore Function License Agreement, or other applicable 
// license agreement, including, without limitation, that your use is for the 
// sole purpose of programming logic devices manufactured by Altera and sold by 
// Altera or its authorized distributors.  Please refer to the applicable 
// agreement for further details.

`timescale 1ps / 1ps

module altera_epcq_controller_arb #(
	parameter CS_WIDTH		= 1,
	parameter ENABLE_4BYTE_ADDR = 1,
	parameter ADDR_WIDTH	= 22,
	parameter ASI_WIDTH		= 1,
	parameter DEVICE_FAMILY = "CYCLONE V",
	parameter ASMI_ADDR_WIDTH = 22,
	parameter CHIP_SELS = 1
)(
	input 	wire		  								clk,
	input 	wire		  								reset_n,
	                                        			
	// ports to access csr                        			
	input 	wire										avl_csr_write,
	input 	wire		  								avl_csr_read,
	input 	wire 	[2:0] 								avl_csr_addr,
	input 	wire 	[31:0] 								avl_csr_wrdata,
	output 	reg 	[31:0] 								avl_csr_rddata,
	output 	reg 		 								avl_csr_rddata_valid,
	output 	reg 			 							avl_csr_waitrequest,
	                                        			
	// ports to access memory        			
	input	wire		  								avl_mem_write,
	input	wire										avl_mem_read,
	input	wire  	[ADDR_WIDTH-1:0]					avl_mem_addr,			
	input	wire	[31:0]								avl_mem_wrdata,
	input	wire 	[3:0]								avl_mem_byteenable,
	input	wire	[6:0]								avl_mem_burstcount,
	output	wire	[31:0]								avl_mem_rddata,
	output	reg											avl_mem_rddata_valid,
	output 	reg 			 							avl_mem_waitrequest,
	
	// interrupt signal
	output	reg 										irq,
	
	// Disable dedicated active serial interface
	input  	wire	[ASI_WIDTH-1:0]						epcq_dataout,
	output  reg											epcq_dclk,
	output  reg		[CS_WIDTH-1:0] 						epcq_scein,
	output  reg		[ASI_WIDTH-1:0]						epcq_sdoin,
	output  reg		[ASI_WIDTH-1:0]						epcq_dataoe,
	
	// ASMI PARALLEL interface
	input   wire	[ASI_WIDTH-1:0]						ddasi_dataoe,
	output  reg 	[ASI_WIDTH-1:0]						ddasi_dataout,
	input   wire 										ddasi_dclk,
	input   wire 	[CS_WIDTH-1:0] 						ddasi_scein,
	input   reg		[ASI_WIDTH-1:0]  					ddasi_sdoin,
	
	input   wire										asmi_busy,
	input   wire										asmi_data_valid,
	input   wire	[7:0]  								asmi_dataout,
	output  reg											asmi_clkin,
	output  reg											asmi_reset,
	output  reg		[CS_WIDTH-1:0] 						asmi_sce,
	output	reg	    [ASMI_ADDR_WIDTH-1:0]  				asmi_addr,
	output  reg		[7:0]  								asmi_datain,
	output  reg											asmi_fast_read,
	output  wire										asmi_rden,
	output  reg											asmi_shift_bytes,
	output  reg 										asmi_en4b_addr,
	output  wire										asmi_wren,
	output  reg											asmi_write,
	
	input   wire										asmi_illegal_erase,
	input   wire										asmi_illegal_write,
	input   wire	[7:0]  								asmi_rdid_out,
	input   wire	[7:0]  								asmi_status_out,
	input   wire	[7:0]  								asmi_epcs_id,
	output  reg											asmi_read_rdid,
	output  reg											asmi_read_status,
	output	reg											asmi_read_sid,
	output  reg 										asmi_bulk_erase,
	output  reg											asmi_sector_erase,
	output  reg											asmi_sector_protect
);

	reg temp_mem_write, temp_mem_read, mem_write, mem_read, back_pressured_ctrl;
	reg [ADDR_WIDTH-1:0] temp_mem_addr, mem_addr;		
	reg [31:0] temp_mem_wrdata, mem_wrdata;
	reg [3:0] temp_mem_byteenable, mem_byteenable;
	reg [6:0] temp_mem_burstcount, mem_burstcount;
	
	wire back_pressured, temp_csr_waitrequest, temp_mem_waitrequest;

	//-------------------- Arbitration logic between avalon csr and mem interface -----------
	always @(posedge clk or negedge reset_n) begin
		if (~reset_n) begin
			back_pressured_ctrl		<= 1'b0;
		end 
		else if (back_pressured) begin
			back_pressured_ctrl		<= 1'b1;
		end
		else if (~temp_csr_waitrequest) begin
			back_pressured_ctrl		<= 1'b0;
		end
	end

	always @(posedge clk or negedge reset_n) begin
		if (~reset_n) begin
			mem_write 			<= 1'b0;
			mem_read 			<= 1'b0;
			mem_addr			<= {ADDR_WIDTH{1'b0}};
			mem_wrdata			<= {32{1'b0}};
			mem_byteenable		<= {4{1'b0}};
			mem_burstcount		<= {7{1'b0}};
		end 
		else if ((avl_csr_write || avl_csr_read) && ~avl_csr_waitrequest && (avl_mem_write || avl_mem_read) && ~avl_mem_waitrequest) begin
			// to back pressure master
			mem_write 			<= avl_mem_write;
			mem_read 			<= avl_mem_read;
			mem_addr			<= avl_mem_addr;
			mem_wrdata			<= avl_mem_wrdata;
			mem_byteenable		<= avl_mem_byteenable;
			mem_burstcount		<= avl_mem_burstcount;
		end
	end
	
	assign back_pressured	   = ((avl_csr_write || avl_csr_read) && ~temp_csr_waitrequest && (avl_mem_write || avl_mem_read)) ? 1'b1 : 1'b0; // to back pressure controller
	assign avl_csr_waitrequest = (~avl_csr_write && ~avl_csr_read && back_pressured_ctrl) ? 1'b1 : temp_csr_waitrequest;
	assign avl_mem_waitrequest = (back_pressured_ctrl) ? 1'b1 : temp_mem_waitrequest;
	assign temp_mem_write	   = (back_pressured) ? 1'b0 :
									(back_pressured_ctrl) ? mem_write : avl_mem_write;
	assign temp_mem_read	   = (back_pressured) ? 1'b0 :
									(back_pressured_ctrl) ? mem_read : avl_mem_read;
	assign temp_mem_addr	   = (back_pressured) ? {ADDR_WIDTH{1'b0}} :
									(back_pressured_ctrl) ? mem_addr : avl_mem_addr;
	assign temp_mem_wrdata	   = (back_pressured) ? {32{1'b0}} :
									(back_pressured_ctrl) ? mem_wrdata : avl_mem_wrdata;
	assign temp_mem_byteenable = (back_pressured) ? {4{1'b0}} :
									(back_pressured_ctrl) ? mem_byteenable : avl_mem_byteenable;
	assign temp_mem_burstcount = (back_pressured) ? {7{1'b0}} :
									(back_pressured_ctrl) ? mem_burstcount : avl_mem_burstcount;
	
	
	//---------------------------------------------------------------------------------------//
	
	altera_epcq_controller #(
		.CS_WIDTH		   (CS_WIDTH),
		.DEVICE_FAMILY     (DEVICE_FAMILY),
		.ADDR_WIDTH        (ADDR_WIDTH),
		.ASMI_ADDR_WIDTH   (ASMI_ADDR_WIDTH),
		.ASI_WIDTH         (ASI_WIDTH),
		.CHIP_SELS	 (CHIP_SELS),
		.ENABLE_4BYTE_ADDR (ENABLE_4BYTE_ADDR)
	) controller (
		.clk                  (clk),                  
		.reset_n              (reset_n),              
		.avl_csr_read         (avl_csr_read),         
		.avl_csr_waitrequest  (temp_csr_waitrequest),  
		.avl_csr_write        (avl_csr_write),        
		.avl_csr_addr         (avl_csr_addr),         
		.avl_csr_wrdata       (avl_csr_wrdata),       
		.avl_csr_rddata       (avl_csr_rddata),       
		.avl_csr_rddata_valid (avl_csr_rddata_valid), 
		.avl_mem_write        (temp_mem_write),        
		.avl_mem_burstcount   (temp_mem_burstcount),   
		.avl_mem_waitrequest  (temp_mem_waitrequest),  
		.avl_mem_read         (temp_mem_read),         
		.avl_mem_addr         (temp_mem_addr),         
		.avl_mem_wrdata       (temp_mem_wrdata),
		.avl_mem_byteenable   (temp_mem_byteenable),
		.avl_mem_rddata       (avl_mem_rddata),       
		.avl_mem_rddata_valid (avl_mem_rddata_valid), 
		.asmi_status_out      (asmi_status_out),      
		.asmi_epcs_id         (asmi_epcs_id),         
		.asmi_illegal_erase   (asmi_illegal_erase),   
		.asmi_illegal_write   (asmi_illegal_write),   
		.ddasi_dataoe         (ddasi_dataoe),         
		.ddasi_dclk           (ddasi_dclk),           
		.ddasi_scein          (ddasi_scein),          
		.ddasi_sdoin          (ddasi_sdoin),          
		.asmi_busy            (asmi_busy),            
		.asmi_data_valid      (asmi_data_valid),      
		.asmi_dataout         (asmi_dataout),         
		.epcq_dataout         (epcq_dataout),         
		.ddasi_dataout        (ddasi_dataout),        
		.asmi_read_rdid       (asmi_read_rdid),       
		.asmi_read_status     (asmi_read_status),     
		.asmi_read_sid        (asmi_read_sid),        
		.asmi_bulk_erase      (asmi_bulk_erase),      
		.asmi_sector_erase    (asmi_sector_erase),    
		.asmi_sector_protect  (asmi_sector_protect),  
		.epcq_dclk            (epcq_dclk),            
		.epcq_scein           (epcq_scein),           
		.epcq_sdoin           (epcq_sdoin),           
		.epcq_dataoe          (epcq_dataoe),          
		.asmi_clkin           (asmi_clkin),           
		.asmi_reset           (asmi_reset),           
		.asmi_sce			  (asmi_sce),
		.asmi_addr            (asmi_addr),            
		.asmi_datain          (asmi_datain),          
		.asmi_fast_read       (asmi_fast_read),       
		.asmi_rden            (asmi_rden),            
		.asmi_shift_bytes     (asmi_shift_bytes),     
		.asmi_wren            (asmi_wren),            
		.asmi_write           (asmi_write),           
		.asmi_rdid_out        (asmi_rdid_out),        
		.asmi_en4b_addr       (asmi_en4b_addr),       
		.irq                  (irq)                   
	);                                                

endmodule
