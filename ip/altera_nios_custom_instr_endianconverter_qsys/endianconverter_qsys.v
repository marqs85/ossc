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


//Legal Notice: (C)2010 Altera Corporation. All rights reserved.  Your
//use of Altera Corporation's design tools, logic functions and other
//software and tools, and its AMPP partner logic functions, and any
//output files any of the foregoing (including device programming or
//simulation files), and any associated documentation or information are
//expressly subject to the terms and conditions of the Altera Program
//License Subscription Agreement or other applicable license agreement,
//including, without limitation, that your use is for the sole purpose
//of programming logic devices manufactured by Altera and sold by Altera
//or its authorized distributors.  Please refer to the applicable
//agreement for further details.

// synthesis translate_off
`timescale 1ns / 1ps
// synthesis translate_on

// turn off superfluous verilog processor warnings 
// altera message_level Level1 
// altera message_off 10034 10035 10036 10037 10230 10240 10030 

module endianconverter_qsys (
                        // inputs:
                         dataa,
                         datab,

                        // outputs:
                         result
                      )
;

  output  [ 31: 0] result;
  input   [ 31: 0] dataa;
  input   [ 31: 0] datab;

  wire    [ 31: 0] result;
  //s1, which is an e_custom_instruction_slave
  assign result[7 : 0] = dataa[31 : 24];
  assign result[15 : 8] = dataa[23 : 16];
  assign result[23 : 16] = dataa[15 : 8];
  assign result[31 : 24] = dataa[7 : 0];

endmodule

