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



`timescale 1ps / 1ps

module altera_epcq_controller #(
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
	localparam LOCAL_ADDR_WIDTH = ADDR_WIDTH+2;
	localparam CSR_DATA_WIDTH   = 32;
	localparam LAST_ADDR_BIT    = (ASMI_ADDR_WIDTH == 24) ? 15 :
									(ASMI_ADDR_WIDTH == 32) ? 23 : 15;
	
	reg  [8:0] wr_burstcount_cnt, rd_burstcount_cnt;
	reg	 [8:0] rd_mem_burstcount, wr_mem_burstcount;
	
	wire last_wr_byte;
	wire access_csr_status, access_csr_sid, access_csr_rdid, access_csr_mem_op, access_isr, access_imr, access_sce;
	wire read_status_combi, read_sid_combi, read_rdid_combi, read_isr_combi, read_imr_combi, write_isr_combi, write_imr_combi, write_sce_combi;
	wire bulk_erase_combi, sector_erase_combi, sector_protect_combi;
	wire wren_combi, illegal_write_combi, illegal_erase_combi;
	wire m_illegal_write_combi, m_illegal_erase_combi;
	wire read_mem_combi, write_mem_combi;
	wire data_valid_combi, pending_wr_data;
	wire detect_addroffset;
	wire [8:0] wfifo_data_in_0, wfifo_data_in_1, wfifo_data_in_2, wfifo_data_in_3;
	wire [ADDR_WIDTH-1:0] temp_mem_addr;
	
	reg reset_n_reg;
	reg wr_mem_waitrequest, local_waitrequest;
	reg illegal_write_reg, illegal_erase_reg, m_illegal_write_reg, m_illegal_erase_reg;
	reg read_status_valid, read_sid_valid, read_rdid_valid, read_isr_valid, read_imr_valid;
	reg read_status_en, read_sid_en, read_rdid_en;
	reg wren_internal;
	reg [LOCAL_ADDR_WIDTH-1:0] wr_mem_addr;
	reg [7:0] rd_data_reg [4];
	reg [3:0][8:0] wr_data_reg;
	reg [1:0] rd_cnt;
	reg [1:0] wr_cnt;
	reg [3:0] wr_data_reg_full;
	reg detect_addroffset_reg, asmi_busy_reg;
	reg [2:0] temp_sce;
	
	// Direct connection
	assign asmi_clkin 		= clk;
	assign asmi_reset 		= ~reset_n;
	assign ddasi_dataout 	= epcq_dataout;
	assign epcq_dclk 		= ddasi_dclk;
	assign epcq_scein		= ddasi_scein;
	assign epcq_sdoin		= ddasi_sdoin;
	assign epcq_dataoe		= ddasi_dataoe;
	
	// chip select
	generate if (DEVICE_FAMILY == "Arria 10") begin
		always @(posedge clk or negedge reset_n) begin
			if (~reset_n) begin
				asmi_sce			<= {CS_WIDTH{1'b0}};
			end 
// to pack the address space this is needed
			else if (write_mem_combi || read_mem_combi) begin
				if (CHIP_SELS == 1 ) 					
					asmi_sce			<= 3'b001;
				else if (CHIP_SELS == 2 && avl_mem_addr[ADDR_WIDTH-1] == 0) 
					asmi_sce			<= 3'b001;
				else if (CHIP_SELS == 2 && avl_mem_addr[ADDR_WIDTH-1] == 1) 
					asmi_sce			<= 3'b010;
				else if (CHIP_SELS == 3 && avl_mem_addr[ADDR_WIDTH-1] == 1)	
					asmi_sce			<= 3'b100;
				else if (CHIP_SELS == 3 && avl_mem_addr[ADDR_WIDTH-1:ADDR_WIDTH-2] == 0) 	
					asmi_sce			<= 3'b001;
				else if (CHIP_SELS == 3 && avl_mem_addr[ADDR_WIDTH-1:ADDR_WIDTH-2] == 1) 	
					asmi_sce			<= 3'b010;
				else
					asmi_sce			<= {CS_WIDTH{1'b0}};
			end
			else if (write_sce_combi) begin
				asmi_sce			<= avl_csr_wrdata[2:0];
			end
			else if (asmi_en4b_addr) begin
				asmi_sce			<= temp_sce;
			end
		end
// decoder ring  if the CHIP_SEL is only 1 then avalon address is the temp address
//		if the chipsele is 2 then need to remove top address bit
//		if the chipelect is 3 then remove the top 2 address bits.  
		assign temp_mem_addr	= CHIP_SELS == 1 ? avl_mem_addr:( CHIP_SELS == 2 ? {1'b0,avl_mem_addr[ADDR_WIDTH-2:0]}:{2'b00,avl_mem_addr[ADDR_WIDTH-3:0]});
	end
	else begin
		always @(posedge clk) begin
			asmi_sce				<= {CS_WIDTH{1'b0}};
		end
		assign temp_mem_addr	= avl_mem_addr;
	end
	endgenerate
	
	// wait_request generation logic
	assign avl_mem_waitrequest = (asmi_busy || asmi_busy_reg) ? 1'b1 : (local_waitrequest || wr_mem_waitrequest);
	assign avl_csr_waitrequest = (asmi_busy || asmi_busy_reg) ? 1'b1 : (local_waitrequest || wr_mem_waitrequest);
	
	// access CSR decoding logic
	assign access_csr_status	= (avl_csr_addr == 3'b000);
	assign access_csr_sid		= (avl_csr_addr == 3'b001);
	assign access_csr_rdid		= (avl_csr_addr == 3'b010);
	assign access_csr_mem_op	= (avl_csr_addr == 3'b011);
	assign access_isr			= (avl_csr_addr == 3'b100);
	assign access_imr			= (avl_csr_addr == 3'b101);
	assign access_sce			= (avl_csr_addr == 3'b110);
	
	// read/write memory combi logic
	assign read_mem_combi		= (avl_mem_read && ~avl_mem_waitrequest);
	assign write_mem_combi		= (avl_mem_write && ~avl_mem_waitrequest);

	// read csr logic
	assign read_status_combi	= (avl_csr_read && access_csr_status && ~avl_csr_waitrequest);
	assign read_sid_combi		= (avl_csr_read && access_csr_sid && ~avl_csr_waitrequest);
	assign read_rdid_combi		= (avl_csr_read && access_csr_rdid && ~avl_csr_waitrequest);
	assign read_isr_combi		= (avl_csr_read && access_isr && ~avl_csr_waitrequest);
	assign read_imr_combi		= (avl_csr_read && access_imr && ~avl_csr_waitrequest);
	assign write_isr_combi		= (avl_csr_write && access_isr && ~avl_csr_waitrequest);
	assign write_imr_combi		= (avl_csr_write && access_imr && ~avl_csr_waitrequest);
	assign write_sce_combi		= (avl_csr_write && access_sce && ~avl_csr_waitrequest);
	
	// write csr logic
	assign bulk_erase_combi		= (avl_csr_write && access_csr_mem_op && ~avl_csr_waitrequest && avl_csr_wrdata[1:0] == 2'b01);
	assign sector_erase_combi	= (avl_csr_write && access_csr_mem_op && ~avl_csr_waitrequest && avl_csr_wrdata[1:0] == 2'b10);
	assign sector_protect_combi	= (avl_csr_write && access_csr_mem_op && ~avl_csr_waitrequest && avl_csr_wrdata[1:0] == 2'b11);
	assign illegal_write_combi	= (asmi_illegal_write) ? 1'b1 :
									(write_isr_combi && avl_csr_wrdata[1]) ? 1'b0 : 
										illegal_write_reg;
	assign illegal_erase_combi	= (asmi_illegal_erase) ? 1'b1 :
									(write_isr_combi && avl_csr_wrdata[0]) ? 1'b0 : 
										illegal_erase_reg;
	assign m_illegal_write_combi= (write_imr_combi) ? avl_csr_wrdata[1] : m_illegal_write_reg;
	assign m_illegal_erase_combi= (write_imr_combi) ? avl_csr_wrdata[0] : m_illegal_erase_reg;
	assign wren_combi			= (sector_protect_combi || sector_erase_combi || bulk_erase_combi);
										
	assign asmi_rden			= (rd_burstcount_cnt > 9'd0);		// deasserted at the last 2 byte - refer to ASMI_PARALLEL UG

	// interrupt signal
	assign irq = (illegal_write_reg && m_illegal_write_reg) || (illegal_erase_reg && m_illegal_erase_reg);
	
	assign last_wr_byte = (wr_burstcount_cnt == wr_mem_burstcount - 9'd1) ? 1'b1 : 1'b0;
	
	assign asmi_wren = wren_internal || asmi_en4b_addr || asmi_shift_bytes || asmi_write;
	
	assign data_valid_combi = (rd_burstcount_cnt[1:0] == 2'b00) ? asmi_data_valid : 1'b0;
	
	assign wfifo_data_in_0 = {avl_mem_byteenable[0], avl_mem_wrdata[7:0]   };
	assign wfifo_data_in_1 = {avl_mem_byteenable[1], avl_mem_wrdata[15:8]  };
	assign wfifo_data_in_2 = {avl_mem_byteenable[2], avl_mem_wrdata[23:16] };
	assign wfifo_data_in_3 = {avl_mem_byteenable[3], avl_mem_wrdata[31:24] };
	
	assign avl_mem_rddata  = {rd_data_reg[3], rd_data_reg[2], rd_data_reg[1], rd_data_reg[0]};
	assign pending_wr_data = (|wr_data_reg_full) ? 1'b1 : 1'b0;
	assign detect_addroffset = (pending_wr_data && wr_data_reg[wr_cnt][8]) ? 1'b1 : 
								(wr_burstcount_cnt == {9{1'b0}}) ? 1'b0 : detect_addroffset_reg;

	//-------------------------------- array to store write data -------------------------------------
	always @(posedge clk or negedge reset_n) begin
		if (~reset_n) begin
			wr_data_reg			<= '{{9{1'b0}}, {9{1'b0}}, {9{1'b0}}, {9{1'b0}}};
			wr_data_reg_full	<= {4{1'b0}};
		end 
		else if (write_mem_combi) begin
			wr_data_reg			<= {wfifo_data_in_3, wfifo_data_in_2, wfifo_data_in_1, wfifo_data_in_0};
			wr_data_reg_full	<= {4{1'b1}};
		end
		else if (wr_data_reg_full > 4'b0000) begin
			wr_data_reg_full	<= wr_data_reg_full << 1;
		end
	end
	
	//-------------------------------- array to store read data -------------------------------------
	always @(posedge clk or negedge reset_n) begin
		if (~reset_n) begin
			rd_data_reg			<= '{{8{1'b0}}, {8{1'b0}}, {8{1'b0}}, {8{1'b0}}};
			rd_cnt				<= {2{1'b0}};
		end 
		else if (asmi_data_valid) begin
			rd_data_reg[rd_cnt]	<= asmi_dataout;
			rd_cnt				<= rd_cnt + 2'b01;
		end
	end
	
	//------------------------------- Enable 4-byte addressing out of reset ----------------------
	generate 
	if (ENABLE_4BYTE_ADDR) begin
		typedef enum logic[1:0] {EN4B_CHIP1, EN4B_CHIP2, EN4B_CHIP3, IDLE} state_t;
		state_t state;
	
		always @(posedge clk or negedge reset_n_reg) begin	// use reset_n_reg because user is allow to send cmd to ASMI_PARALLEL 2 clock cycles after reset
			if (~reset_n_reg) begin	
				state					<= EN4B_CHIP1;
				asmi_en4b_addr			<= 1'b1;
				temp_sce				<= 3'b001;
			end
			else begin
				case (state)
					EN4B_CHIP1 : begin
									asmi_en4b_addr			<= 1'b1;
									if (~asmi_busy) begin
										if (CHIP_SELS > 1) begin
											state					<= EN4B_CHIP2;
											temp_sce				<= 3'b010;
										end
										else begin
											state					<= IDLE;
											temp_sce				<= 3'b000;
										end
									end
								 end
					EN4B_CHIP2 : begin
									asmi_en4b_addr			<= 1'b1;
									if (~asmi_busy) begin
										if (CHIP_SELS > 2) begin
											state					<= EN4B_CHIP3;
											temp_sce				<= 3'b100;
										end
										else begin
											state					<= IDLE;
											temp_sce				<= 3'b000;
										end
									end
								 end
					EN4B_CHIP3 : begin
									asmi_en4b_addr			<= 1'b1;
									if (~asmi_busy) begin
										state					<= IDLE;
										temp_sce				<= 3'b000;
									end
								 end
					IDLE	   : begin
									asmi_en4b_addr			<= 1'b0;
									state					<= IDLE;
									temp_sce				<= 3'b000;
								 end
					default : 	 begin
									asmi_en4b_addr			<= 1'b0;
									state					<= IDLE;
									temp_sce				<= 3'b000;
								 end
				endcase
			end
		end
	end
	else begin
		always @(posedge clk) begin
			asmi_en4b_addr				<= 1'b0;
			temp_sce					<= 3'b000;
		end
	end
	endgenerate
	
	//--------------------------------------- Waitrequest logic ----------------------------------
	always @(posedge clk or negedge reset_n) begin
		if (~reset_n) begin
			wr_mem_waitrequest			<= 1'b0;
			local_waitrequest			<= 1'b0;
		end
		else begin
			if (read_mem_combi || read_status_combi || read_sid_combi || read_rdid_combi || bulk_erase_combi || sector_erase_combi || sector_protect_combi || asmi_en4b_addr) begin		// no back pressure during imr & isr access
				local_waitrequest		<= 1'b1;
			end
			else if (asmi_busy_reg && ~asmi_busy) begin
				local_waitrequest		<= 1'b0;
			end
			
			if (write_mem_combi) begin
				wr_mem_waitrequest	<= 1'b1;
			end
			else if ((~pending_wr_data && ~asmi_write) || asmi_busy_reg && ~asmi_busy) begin
				wr_mem_waitrequest	<= 1'b0;
			end
		end
	end
	
	// -------------------------------------- MEM ACCESS -----------------------------------------
	always @(posedge clk or negedge reset_n) begin
		if (~reset_n) begin
			rd_mem_burstcount			<= {9{1'b0}};
			wr_mem_burstcount			<= {9{1'b0}};
			wr_mem_addr					<= {LOCAL_ADDR_WIDTH{1'b0}};
		end
		else begin
			if (read_mem_combi) begin
				rd_mem_burstcount		<= {avl_mem_burstcount, 2'b00}; 
			end
			if (write_mem_combi && (wr_burstcount_cnt == {9{1'b0}})) begin
				wr_mem_addr				<= {temp_mem_addr, 2'b00};
				wr_mem_burstcount		<= {avl_mem_burstcount, 2'b00}; 
			end
		end
	end
	
	always @(posedge clk or negedge reset_n) begin
		if (~reset_n) begin
			wr_burstcount_cnt				<= {9{1'b0}};
		end
		else begin
			if (pending_wr_data) begin
				wr_burstcount_cnt			<= wr_burstcount_cnt + 9'd1;
			end
			else if (wr_burstcount_cnt == wr_mem_burstcount) begin
				wr_burstcount_cnt			<= {9{1'b0}};
			end
		end
	end
	
	always @(posedge clk or negedge reset_n) begin
		if (~reset_n) begin
			rd_burstcount_cnt				<= {9{1'b0}};
		end
		else begin
			if (read_mem_combi) begin
				rd_burstcount_cnt			<= 9'd1;
			end
			else if (rd_burstcount_cnt == rd_mem_burstcount) begin		// each rd 4 burst
				rd_burstcount_cnt			<= {9{1'b0}};
			end
			else if (asmi_data_valid && rd_burstcount_cnt > 0) begin
				rd_burstcount_cnt			<= rd_burstcount_cnt + 9'd1;
			end
		end
	end
	
	always @(posedge clk or negedge reset_n) begin
		if (~reset_n) begin
			asmi_addr					<= {ASMI_ADDR_WIDTH{1'b0}};
		end
		else begin
			if (sector_erase_combi) begin // set lower 16 bits to zero so that erase at starting address of each sector
				asmi_addr				<= {avl_csr_wrdata[LAST_ADDR_BIT : 8], {16{1'b0}}};	
			end
			if (read_mem_combi) begin
				asmi_addr				<= {temp_mem_addr, 2'b00};
			end
			
			if (detect_addroffset && ~detect_addroffset_reg) begin
				asmi_addr			<= wr_mem_addr + {{LOCAL_ADDR_WIDTH-9{1'b0}}, wr_burstcount_cnt};
			end

		end
	end
	
	always @(posedge clk or negedge reset_n) begin
		if (~reset_n) begin
			asmi_datain					<= {8{1'b0}};
			wr_cnt						<= {2{1'b0}};
			asmi_shift_bytes			<= 1'b0;
		end
		else begin
			if (sector_protect_combi) begin
				asmi_datain				<= {{1{1'b0}}, avl_csr_wrdata[11], avl_csr_wrdata[12], avl_csr_wrdata[10:8], {2{1'b0}}};	// BP3, TB, BP2, BP1, BP0
			end
			if (pending_wr_data) begin
				asmi_datain				<= wr_data_reg[wr_cnt][7:0];
				wr_cnt					<= wr_cnt + 2'd1;
			end
			if (pending_wr_data && wr_data_reg[wr_cnt][8]) begin
				asmi_shift_bytes		<= 1'b1;
			end
			else begin
				asmi_shift_bytes		<= 1'b0;
			end
		end
	end
	
	always @(posedge clk or negedge reset_n) begin
		if (~reset_n) begin
			asmi_read_status			<= 1'b0;
			asmi_read_sid				<= 1'b0;
			asmi_read_rdid				<= 1'b0;
			asmi_bulk_erase				<= 1'b0;
			asmi_sector_erase			<= 1'b0;
			asmi_sector_protect			<= 1'b0;
			wren_internal				<= 1'b0;
			asmi_write					<= 1'b0;
			asmi_fast_read				<= 1'b0;
			asmi_busy_reg				<= 1'b0;
			avl_mem_rddata_valid		<= 1'b0;
			detect_addroffset_reg		<= 1'b0;
			reset_n_reg					<= 1'b0;
		end
		else begin
			asmi_read_status			<= read_status_combi;
			asmi_read_sid				<= read_sid_combi;
			asmi_read_rdid				<= read_rdid_combi;
			asmi_bulk_erase				<= bulk_erase_combi;
			asmi_sector_erase			<= sector_erase_combi;				
			asmi_sector_protect			<= sector_protect_combi;
			wren_internal				<= wren_combi;
			asmi_write					<= last_wr_byte;
			asmi_fast_read				<= read_mem_combi;
			asmi_busy_reg				<= asmi_busy;
			avl_mem_rddata_valid		<= data_valid_combi;
			detect_addroffset_reg		<= detect_addroffset;
			reset_n_reg					<= 1'b1;
		end
	end
	
	// --------------------------------------------- CSR ACCESS -------------------------------------
	always @(posedge clk or negedge reset_n) begin
		if (~reset_n) begin
			illegal_write_reg			<= 1'b0;
			illegal_erase_reg			<= 1'b0;
			m_illegal_write_reg			<= 1'b0;
			m_illegal_erase_reg			<= 1'b0;
		end
		else begin
			illegal_write_reg			<= illegal_write_combi;
			illegal_erase_reg			<= illegal_erase_combi;
			m_illegal_write_reg			<= m_illegal_write_combi;
			m_illegal_erase_reg			<= m_illegal_erase_combi;
		end
	end
	
	// csr read only registers enable logic
	always @(posedge clk or negedge reset_n) begin
		if (~reset_n) begin
			read_status_en				<= 1'b0;
			read_sid_en 				<= 1'b0;
			read_rdid_en				<= 1'b0;
		end
		else if (asmi_read_status) begin
			read_status_en				<= 1'b1;
		end
		else if (asmi_read_sid) begin
			read_sid_en					<= 1'b1;
		end
		else if (asmi_read_rdid) begin
			read_rdid_en				<= 1'b1;
		end
		else if (asmi_busy == 0) begin
			read_status_en				<= 1'b0;
			read_sid_en 				<= 1'b0;
			read_rdid_en				<= 1'b0;
		end
	end
	
	// generation logic for avl csr read data valid
	assign avl_csr_rddata_valid	= read_status_valid || read_sid_valid || read_rdid_valid || read_isr_valid || read_imr_valid;
	
	always @(posedge clk or negedge reset_n) begin
		if (~reset_n) begin
			read_status_valid			<= 1'b0;
			read_sid_valid				<= 1'b0;
			read_rdid_valid				<= 1'b0;
			read_isr_valid				<= 1'b0;
			read_imr_valid				<= 1'b0;
		end
		else begin
			if (read_status_en && asmi_busy == 0) begin
				read_status_valid			<= 1'b1;
			end
			else begin
				read_status_valid			<= 1'b0;
			end
			
			if (read_sid_en && asmi_busy == 0) begin
				read_sid_valid				<= 1'b1;
			end
			else begin
				read_sid_valid				<= 1'b0;
			end
			
			if (read_rdid_en && asmi_busy == 0) begin
				read_rdid_valid				<= 1'b1;
			end
			else begin
				read_rdid_valid				<= 1'b0;
			end
			
			if (read_isr_combi) begin
				read_isr_valid				<= 1'b1;
			end
			else begin
				read_isr_valid				<= 1'b0;
			end
			
			if (read_imr_combi) begin
				read_imr_valid				<= 1'b1;
			end
			else begin
				read_imr_valid				<= 1'b0;
			end
		end
	end
	
	// generation logic for avl csr read data
	always @(posedge clk or negedge reset_n) begin
		if (~reset_n) begin
			avl_csr_rddata				<= {CSR_DATA_WIDTH{1'b0}};
		end
		else begin
			if (read_status_en && asmi_busy == 0) begin
				avl_csr_rddata				<= {{CSR_DATA_WIDTH-8{1'b0}}, asmi_status_out};
			end
			if (read_sid_en && asmi_busy == 0) begin
				avl_csr_rddata				<= {{CSR_DATA_WIDTH-8{1'b0}}, asmi_epcs_id};
			end
			if (read_rdid_en && asmi_busy == 0) begin
				avl_csr_rddata				<= {{CSR_DATA_WIDTH-8{1'b0}}, asmi_rdid_out};
			end
			if (read_isr_combi) begin
				avl_csr_rddata				<= {{CSR_DATA_WIDTH-2{1'b0}}, illegal_write_reg, illegal_erase_reg};
			end
			if (read_imr_combi) begin
				avl_csr_rddata				<= {{CSR_DATA_WIDTH-2{1'b0}}, m_illegal_write_reg, m_illegal_erase_reg};
			end
		end
	end
	

endmodule
