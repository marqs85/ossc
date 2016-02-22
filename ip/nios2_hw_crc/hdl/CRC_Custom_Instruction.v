/*
  Legal Notice: (C)2006 Altera Corporation. All rights reserved.  Your
  use of Altera Corporation's design tools, logic functions and other
  software and tools, and its AMPP partner logic functions, and any
  output files any of the foregoing (including device programming or
  simulation files), and any associated documentation or information are
  expressly subject to the terms and conditions of the Altera Program
  License Subscription Agreement or other applicable license agreement,
  including, without limitation, that your use is for the sole purpose
  of programming logic devices manufactured by Altera and sold by Altera
  or its authorized distributors.  Please refer to the applicable
  agreement for further details.
*/

/* 
  This thin wrapper re-uses the CRC Avalon component as a Nios II
  custom instruction. The n port of custom instruction is used as
  control to the CRC Avalon component. Below are the values of n and
  the corresponding operations perform by the custom instruction:
  n = 0, Initialize the custom instruction to the initial remainder value
  n = 1, Write  8 bits data to custom instruction
  n = 2, Write 16 bits data to custom instruction
  n = 3, Write 32 bits data to custom instruction
  n = 4, Read  32 bits data from the custom instruction
  n = 5, Read  64 bits data from the custom instruction
  n = 6, Read  96 bits data from the custom instruction
  n = 7, Read 128 bits data from the custom instruction 
*/



module CRC_Custom_Instruction(clk,
                              reset,
                              dataa,
                              n,
                              clk_en,
                              start,
					done,
                              result);
  /*
    See the Avalon CRC component for details on the meaning of each
    parameter listed below.
  */
  parameter crc_width = 32;
  parameter polynomial_inital = 32'hFFFFFFFF;
  parameter polynomial = 32'h04C11DB7;
  parameter reflected_input = 1;
  parameter reflected_output = 1;
  parameter xor_output = 32'hFFFFFFFF;

  input clk;
  input reset;
  input [31:0] dataa;  
  input [2:0] n;
  input clk_en;
  input start;
  output done;
  output [31:0] result;

  wire [2:0] address;
  wire [3:0] byteenable;
  wire write;
  wire read;
  reg done_delay;

  assign write = (n<4);
  assign read = (n>3);
  assign byteenable = (n==1)?4'b0001 : (n==2)?4'b0011 : (n==3)?4'b1111 : 4'b0000;
  assign address = (n==0)?3'b000 : ((n==1)|(n==2)|(n==3))?3'b001 : (n==4)?3'b100 : (n==5)?3'b101 : (n==6)?3'b110 : 3'b111;
  assign done = (n>3)? done_delay : start;

  always @ (posedge clk or posedge reset)
  begin
  if (reset)
		done_delay <= 0;
  else
		done_delay <= start;
  end

  /* 
    Instantiating the Avalon CRC component and wiring it to be
    custom instruction compilant
  */
  CRC_Component wrapper_wiring(.clk(clk),
                               .reset(reset),
                               .address(address),
                               .writedata(dataa),
                               .byteenable(byteenable),
                               .write(write & start),
                               .read(read),
                               .chipselect(clk_en),
                               .readdata(result));

  defparam wrapper_wiring.crc_width = crc_width;
  defparam wrapper_wiring.polynomial_inital = polynomial_inital;
  defparam wrapper_wiring.polynomial = polynomial;
  defparam wrapper_wiring.reflected_input = reflected_input;
  defparam wrapper_wiring.reflected_output = reflected_output;
  defparam wrapper_wiring.xor_output = xor_output;

endmodule
