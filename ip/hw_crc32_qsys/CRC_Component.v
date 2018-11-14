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
  This component supports 8, 16, 24, and 32 bit little endian data
  and any CRC standard between 1 to 128 bits.  Through parameterization
  you can change the CRC standard which will take effect after you
  regenerate your system in SOPC Builder.

  Register Map:
  000 -> reset the CRC peripheral to the inital value (data and byte enables ignored)
  001 -> data write between 1-32 bits
  010 -> reserved
  011 -> reserved
  100 -> read bits 1-32 of the crc result
  101 -> read bits 33-64 of the crc result (where applicable)
  110 -> read bits 65-96 of the crc result (where applicable)
  111 -> read bits 97-128 of the crc result (where applicable)

  Write latency = 0
  Read latency = 1

  Note:  This component uses four blocks of eight bits of data in cascade.
         To improve the timing of logic you can create seperate cascades
         for 8, 16, 24, and 32 bit data which will allow for smaller area
         and a shorter combinational depth.  Since CRC logic consumes power
         even when not in use you can also add a logic disable feature using
         the chipselect signal.  Even though the registered CRC value is
         held constant when the circuit is not in use the input data will
         change during this time and cause the CRC cascade logic to react. 
*/


module CRC_Component (clk,
                      reset,
                      address,
                      writedata,
                      byteenable,
                      write,
                      read,
                      chipselect,
                      readdata);

/* 
  Using these parameters you can create any CRC ranging from one bit (parity checker)
  up to 128 bits.  The following list describes the function of each parameter:

  crc_width:
    The width of the registered CRC result, this value is typically apart of
    the name of the standard (CRC32 is 32 bits wide).  Adjusting this value
    will impact the logic resource usage.

  polynomial_initial:
    The initial value set for the CRC result register.  By writing any data to address 0
    this value will be stored in the result register thereby clearing any previously existing
    value.  This value must be the same width as 'crc_width'

  polynomial:
    This is the divisor value used on the input data.  Typically shown in polynomial format
    the value symbolizes the placement of xor operation on the input data.  In synthesis, the
    polynomial bits that are '1' will create a not gate, whereas the bits that are '0' will
    simply create a wire.  Even with 32 stages of these operations cascaded, the simple logic
    will not become a significant factor on logic utilization or fmax.  This value must be the
    same width as 'crc_width'

  reflected_input:
    Some CRC standards require that all the input bits be reflected around the center point.
    This option is enabled with '1' and disabled with '0'.  Typically this option is enabled
    or disabled with 'reflected_output'.

  reflected_output:
    Some CRC standards require that all the output bits be reflected around the center point.
    This operation occurs before the final optional xor output step.  This option is enabled
    with '1' and disabled with '0'.  Typically this option is enabled or disabled with
    'reflected_input' (to undo the input reversal typically).

  xor_output:
    This is the value used to bitwise xor the CRC result.  Most standards use either all zeros
    or all ones for this value.  When zeros are used the CRC result is passed directly and when
    ones are used the CRC result is inverted.  Since it's no mandatory that this value be all
    ones or zeros, this operation occurs before the output reflection when applicable.
*/
       
  parameter crc_width = 32;
  parameter polynomial_inital = 32'hFFFFFFFF;
  parameter polynomial = 32'h04C11DB7;
  parameter reflected_input = 1;
  parameter reflected_output = 1;
  parameter xor_output = 32'hFFFFFFFF;


  input clk;
  input reset;
  input [2:0] address;
  input [31:0] writedata;
  input [3:0] byteenable;
  input write;
  input read;
  input chipselect;
  output [31:0] readdata;

  reg [crc_width-1:0] crc_value;
  wire [crc_width-1:0] poly = polynomial;
  wire [crc_width-1:0] cascade [3:0];
  wire [7:0] block0_data, block1_data, block2_data, block3_data;
  wire [crc_width-1:0] result, result_xored;
  wire [31:0] mux_result;
  reg [31:0] readdata;


  /* 
    Some standards like CRC16 and CRC32 require this bitreversal for serial
    devices like ethernet, uarts, usb, etc...
  */
  genvar index;
  generate if (reflected_input == 1)
  begin
    for(index = 0; index < 8; index = index + 1)
    begin: input_reflection
      assign block0_data[index] = writedata[7-index];
      assign block1_data[index] = writedata[15-index];
      assign block2_data[index] = writedata[23-index];
      assign block3_data[index] = writedata[31-index];
    end
  end
  else
  begin
    assign block0_data = writedata[7:0];
    assign block1_data = writedata[15:8];
    assign block2_data = writedata[23:16];
    assign block3_data = writedata[31:24];
  end
  endgenerate


  /* 
    Control for the registered events.  It assumes that either 8, 16, 24, or 32
    bit data is being written using byte enables.  It is important that the data
    be in little endian format and no gaps of byte enables be present (like
    1011 or 1101 for example)
  */
  always @ (posedge clk or posedge reset)
  begin
    if(reset == 1)
    begin
      crc_value <= 0;
    end
    else 
    begin
      if(write && chipselect && (address == 3'b000))
      begin
        crc_value <= polynomial_inital;  // reset the crc to the initial value
      end
      else if(write && chipselect && (address == 3'b001))
      begin
		if(byteenable == 4'b0001) // 8 bit data input
        begin
          crc_value <= cascade[0];
        end
        else if(byteenable == 4'b0011) // 16 bit data input
        begin
          crc_value <= cascade[1];
        end
        else if(byteenable == 4'b0111) // 24 bit data input
        begin
          crc_value <= cascade[2];
        end
        else if(byteenable == 4'b1111) // 32 bit data input
        begin
          crc_value <= cascade[3];
        end
      end
    end
  end


  /* four stages of cascade blocks (each block is crc_width x 8 bits) */
  XOR_Shift_Block cascade_block0(.block_input(crc_value), .poly(poly), .data_input(block0_data), .block_output(cascade[0]));
    defparam cascade_block0.crc_width = crc_width;
  XOR_Shift_Block cascade_block1(.block_input(cascade[0]), .poly(poly), .data_input(block1_data), .block_output(cascade[1]));
    defparam cascade_block1.crc_width = crc_width;
  XOR_Shift_Block cascade_block2(.block_input(cascade[1]), .poly(poly), .data_input(block2_data), .block_output(cascade[2]));
    defparam cascade_block2.crc_width = crc_width;
  XOR_Shift_Block cascade_block3(.block_input(cascade[2]), .poly(poly), .data_input(block3_data), .block_output(cascade[3]));
    defparam cascade_block3.crc_width = crc_width;



  /* 
    Some standards like CRC16 and CRC32 require this bitreversal.
    This is to better support serial devices like uarts, ethernet, usb, etc...)
  */
  generate if (reflected_output == 1)
  begin
    for(index = 0; index < crc_width; index = index + 1)
    begin: output_reflection32
      assign result[index] = crc_value[(crc_width-1)-index];
    end
  end
  else
  begin
    assign result = crc_value;
  end
  endgenerate


  /* This final xor operation occurs after the bit swap */
  assign result_xored = result ^ xor_output;


  /* Generates the appropriate MUX logic depending on the CRC width */
  generate if((crc_width > 32) && (crc_width < 65))
  begin
    assign mux_result = (address == 3'b100)? result_xored[31:0] : result_xored[crc_width-1:32];
  end
  else if((crc_width > 64) && (crc_width < 97))
  begin
    assign mux_result = (address == 3'b100)? result_xored[31:0] :
                     ((address == 3'b101)? result_xored[63:32] : result_xored[crc_width-1:64]);
  end
  else if((crc_width > 96) && (crc_width < 129))
  begin
    assign mux_result = (address == 3'b100)? result_xored[31:0] :
                     ((address == 3'b101)? result_xored[63:32] :
                     ((address == 3'b110)? result_xored[95:64] : result_xored[crc_width-1:96]));
  end
  else
  begin
    assign mux_result = result_xored; 
  end
  endgenerate


  /* Registering the return path of the CRC data (32 bits of it) */
  always @ (posedge clk or posedge reset)
  begin
    if(reset == 1)
    begin
      readdata <= 0;
    end
    else if((read == 1) && (chipselect == 1))
    begin
      readdata <= mux_result;
    end
  end

endmodule



/* a single cascade block of width: crc_width and a length of eight input bits */
module XOR_Shift_Block(block_input,
                       poly,
                       data_input,
                       block_output);
  parameter crc_width = 32;

  input [(crc_width-1):0] block_input;
  input [(crc_width-1):0] poly;
  input [7:0] data_input;
  output [(crc_width-1):0] block_output;

  wire [(crc_width-1):0] cascade [7:0];

  XOR_Shift bit_0(.stage_input(block_input), .poly(poly), .new_bit(data_input[7]), .stage_output(cascade[0]));
    defparam bit_0.crc_width = crc_width;
  XOR_Shift bit_1(.stage_input(cascade[0]), .poly(poly), .new_bit(data_input[6]), .stage_output(cascade[1]));
    defparam bit_1.crc_width = crc_width;
  XOR_Shift bit_2(.stage_input(cascade[1]), .poly(poly), .new_bit(data_input[5]), .stage_output(cascade[2]));
    defparam bit_2.crc_width = crc_width;
  XOR_Shift bit_3(.stage_input(cascade[2]), .poly(poly), .new_bit(data_input[4]), .stage_output(cascade[3]));
    defparam bit_3.crc_width = crc_width;
  XOR_Shift bit_4(.stage_input(cascade[3]), .poly(poly), .new_bit(data_input[3]), .stage_output(cascade[4]));
    defparam bit_4.crc_width = crc_width;
  XOR_Shift bit_5(.stage_input(cascade[4]), .poly(poly), .new_bit(data_input[2]), .stage_output(cascade[5]));
    defparam bit_5.crc_width = crc_width;
  XOR_Shift bit_6(.stage_input(cascade[5]), .poly(poly), .new_bit(data_input[1]), .stage_output(cascade[6]));
    defparam bit_6.crc_width = crc_width;
  XOR_Shift bit_7(.stage_input(cascade[6]), .poly(poly), .new_bit(data_input[0]), .stage_output(cascade[7]));
    defparam bit_7.crc_width = crc_width;

  assign block_output = cascade[7];

endmodule


/* performs the 'new_bit' stuffing, shifting, and XOR operations for a single input bit */
module XOR_Shift (stage_input,
                  poly,
                  new_bit,
                  stage_output);

  parameter crc_width = 32;

  input [crc_width-1:0] stage_input;
  input [crc_width-1:0] poly;
  input new_bit;
  output [crc_width-1:0] stage_output;

  assign stage_output[0] = new_bit ^ stage_input[crc_width-1];
  assign stage_output[crc_width-1:1] = stage_input[crc_width-2:0] ^ ({crc_width-1{stage_output[0]}} & poly[crc_width-1:1]);

endmodule
