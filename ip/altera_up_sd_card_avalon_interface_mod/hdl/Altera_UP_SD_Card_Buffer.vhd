-- (C) 2001-2015 Altera Corporation. All rights reserved.
-- Your use of Altera Corporation's design tools, logic functions and other 
-- software and tools, and its AMPP partner logic functions, and any output 
-- files any of the foregoing (including device programming or simulation 
-- files), and any associated documentation or information are expressly subject 
-- to the terms and conditions of the Altera Program License Subscription 
-- Agreement, Altera MegaCore Function License Agreement, or other applicable 
-- license agreement, including, without limitation, that your use is for the 
-- sole purpose of programming logic devices manufactured by Altera and sold by 
-- Altera or its authorized distributors.  Please refer to the applicable 
-- agreement for further details.


-------------------------------------------------------------------------------------
-- This module is a dual port memory block. It has a 16-bit port and a 1-bit port.
-- The 1-bit port is used to either send or receive data, while the 16-bit port is used
-- by Avalon interconnet to store and retrieve data.
--
-- NOTES/REVISIONS:
-------------------------------------------------------------------------------------
library ieee;
use ieee.std_logic_1164.all;
use ieee.std_logic_arith.all;
use ieee.std_logic_unsigned.all;

entity Altera_UP_SD_Card_Buffer is
	generic (
		TIMEOUT		: std_logic_vector(15 downto 0) := "1111111111111111";
		BUSY_WAIT	: std_logic_vector(15 downto 0) := "0000001111110000"
	);
	port
	(
		i_clock		  					: in std_logic;
		i_reset_n	  					: in std_logic;

		-- 1 bit port to transmit and receive data on the data line.
		i_begin							: in std_logic;
		i_sd_clock_pulse_trigger	: in std_logic;
		i_transmit						: in std_logic;
		i_1bit_data_in 				: in std_logic;
		o_1bit_data_out 				: out std_logic;
		o_operation_complete			: out std_logic;
		o_crc_passed					: out std_logic;
		o_timed_out						: out std_logic;
		o_dat_direction				: out std_logic; -- set to 1 to send data, set to 0 to receive it.
		
		-- 16 bit port to be accessed by a user circuit.
		i_enable_16bit_port 			: in std_logic;
		i_address_16bit_port			: in std_logic_vector(7 downto 0);
		i_write_16bit					: in std_logic;
		i_16bit_data_in 				: in std_logic_vector(15 downto 0);
		o_16bit_data_out 				: out std_logic_vector(15 downto 0)
	);

end entity;

architecture rtl of Altera_UP_SD_Card_Buffer is
				 
	component Altera_UP_SD_CRC16_Generator
		port
		(
			i_clock 			: in std_logic;
			i_enable			: in std_logic;
			i_reset_n		: in std_logic;
			i_sync_reset	: in std_logic;
			i_shift			: in std_logic;
			i_datain			: in std_logic;
			o_dataout		: out std_logic;
			o_crcout			: out std_logic_vector(15 downto 0)
		);
	end component;
	
	component Altera_UP_SD_Card_Memory_Block
	PORT
	(
		address_a			: IN STD_LOGIC_VECTOR (7 DOWNTO 0);
		address_b			: IN STD_LOGIC_VECTOR (11 DOWNTO 0);
		clock_a				: IN STD_LOGIC ;
		clock_b				: IN STD_LOGIC ;
		data_a				: IN STD_LOGIC_VECTOR (15 DOWNTO 0);
		data_b				: IN STD_LOGIC_VECTOR (0 DOWNTO 0);
		enable_a				: IN STD_LOGIC  := '1';
		enable_b				: IN STD_LOGIC  := '1';
		wren_a				: IN STD_LOGIC  := '1';
		wren_b				: IN STD_LOGIC  := '1';
		q_a					: OUT STD_LOGIC_VECTOR (15 DOWNTO 0);
		q_b					: OUT STD_LOGIC_VECTOR (0 DOWNTO 0)
	);
	END component;
	
	-- Build an enumerated type for the state machine. On reset always reset the DE2 and read the state
	-- of the switches.
	type state_type is (s_RESET, s_WAIT_REQUEST, s_SEND_START_BIT, s_SEND_DATA, s_SEND_CRC, s_SEND_STOP, s_WAIT_BUSY, s_WAIT_BUSY_END,
						s_WAIT_DATA_START, s_RECEIVING_LEADING_BITS, s_RECEIVING_DATA, s_RECEIVING_STOP_BIT, s_WAIT_DEASSERT);

	-- Register to hold the current state
	signal current_state : state_type;
	signal next_state 	: state_type;
	-- Local wires
	-- REGISTERED
	signal	crc_counter			: std_logic_vector(3 downto 0);
	signal  local_mode 			: std_logic;
	signal	dataout_1bit		: std_logic;
	signal	bit_counter 		: std_logic_vector(2 downto 0);
	signal	byte_counter 		: std_logic_vector(8 downto 0);
	signal	shift_register 	: std_logic_vector(16 downto 0);
	signal 	timeout_register 	: std_logic_vector(15 downto 0);
	signal	data_in_reg 		: std_logic;
	-- UNREGISTERED
	signal	crc_out				: std_logic_vector(15 downto 0);
	signal	single_bit_conversion, single_bit_out 	: std_logic_vector( 0 downto 0);
	signal	packet_mem_addr_b	: std_logic_vector(11 downto 0);
	signal	local_reset, to_crc_generator, from_crc_generator, from_mem_1_bit, shift_crc,
			recv_data, crc_generator_enable 				: std_logic;
begin
	-- State transitions
	state_transitions: process(	current_state, i_begin, i_sd_clock_pulse_trigger, i_transmit, byte_counter, 
								bit_counter, crc_counter, i_1bit_data_in, timeout_register, data_in_reg)
	begin
		case (current_state) is
			when s_RESET =>
				-- Reset local registers and begin waiting for user input.
				next_state <= s_WAIT_REQUEST;
				
			when s_WAIT_REQUEST =>
				-- Wait for i_begin to be high
				if ((i_begin = '1') and (i_sd_clock_pulse_trigger = '1')) then
					if (i_transmit = '1') then
						next_state <= s_SEND_START_BIT;
					else
						next_state <= s_WAIT_DATA_START;
					end if;
				else
					next_state <= s_WAIT_REQUEST;
				end if;
				
			when s_SEND_START_BIT => 
				-- Send a 0 first, followed by 4096 bits of data, 16 CRC bits, and stop bit.
				if (i_sd_clock_pulse_trigger = '1') then
					next_state <= s_SEND_DATA;
				else
					next_state <= s_SEND_START_BIT;
				end if;
				
			when s_SEND_DATA =>
				-- Send 4096 data bits
				if ((i_sd_clock_pulse_trigger = '1') and (bit_counter = "000") and (byte_counter = "111111111")) then
					next_state <= s_SEND_CRC;
				else
					next_state <= s_SEND_DATA;
				end if;		
	
			when s_SEND_CRC =>
				-- Send 16 CRC bits
				if ((i_sd_clock_pulse_trigger = '1') and (crc_counter = "1111")) then
					next_state <= s_SEND_STOP;
				else
					next_state <= s_SEND_CRC;
				end if;	
							
			when s_SEND_STOP =>
				-- Send stop bit.
				if (i_sd_clock_pulse_trigger = '1') then
					next_state <= s_WAIT_BUSY;
				else
					next_state <= s_SEND_STOP;
				end if;
							
			when s_WAIT_BUSY =>
				-- After a write, wait for the busy signal. Do not return a done signal until you receive a busy signal.
				-- If you do not and a long time expires, then the data must have been rejected (due to CRC error maybe).
				-- In such a case return failure.
				if ((i_sd_clock_pulse_trigger = '1') and (data_in_reg = '0') and (timeout_register = "0000000000010000")) then
					next_state 		<= s_WAIT_BUSY_END;
				else
					if (timeout_register = BUSY_WAIT) then
						next_state 	<= s_WAIT_DEASSERT;
					else
						next_state 	<= s_WAIT_BUSY;
					end if;				
				end if;
				
			when s_WAIT_BUSY_END =>
				if (i_sd_clock_pulse_trigger = '1') then
					if (data_in_reg = '1') then
						next_state 	<= s_WAIT_DEASSERT;
					else
						next_state 	<= s_WAIT_BUSY_END;
					end if;
				else
					next_state 		<= s_WAIT_BUSY_END;
				end if;
				
			when s_WAIT_DATA_START =>
				-- Wait for the start bit
				if ((i_sd_clock_pulse_trigger = '1') and (data_in_reg = '0')) then
					next_state 		<= s_RECEIVING_LEADING_BITS;
				else
					if (timeout_register = TIMEOUT) then
						next_state 	<= s_WAIT_DEASSERT;
					else
						next_state 	<= s_WAIT_DATA_START;
					end if;
				end if;
				
			when s_RECEIVING_LEADING_BITS =>
				-- shift the start bit in as well as the next 16 bits. Once they are all in you can start putting data into memory.
				if ((i_sd_clock_pulse_trigger = '1') and (crc_counter = "1111")) then
					next_state <= s_RECEIVING_DATA;
				else
					next_state <= s_RECEIVING_LEADING_BITS;
				end if;
				
			when s_RECEIVING_DATA =>
				-- Wait until all bits arrive. 
				if ((i_sd_clock_pulse_trigger = '1') and (bit_counter = "000") and (byte_counter = "111111111")) then
					next_state <= s_RECEIVING_STOP_BIT;
				else
					next_state <= s_RECEIVING_DATA;
				end if;		
						
			when s_RECEIVING_STOP_BIT => 
				-- Wait until all bits arrive. 
				if (i_sd_clock_pulse_trigger = '1')then
					next_state <= s_WAIT_DEASSERT;
				else
					next_state <= s_RECEIVING_STOP_BIT;
				end if;
							
			when s_WAIT_DEASSERT =>
				if (i_begin = '1') then
					next_state <= s_WAIT_DEASSERT;
				else
					next_state <= s_WAIT_REQUEST;
				end if;
				
			when others =>
				next_state <= s_RESET;
		end case;
	end process;
	
	-- State registers
	state_regs: process(i_clock, i_reset_n, local_reset)
	begin
		if (i_reset_n = '0') then
			current_state <= s_RESET;
		elsif (rising_edge(i_clock)) then
			current_state <= next_state;
		end if;
	end process;
	
	-- FSM outputs
	to_crc_generator <= shift_register(16) when (current_state = s_RECEIVING_DATA) else
						from_mem_1_bit when (current_state = s_SEND_DATA) else
						'0';
	shift_crc 	<= '1' when (current_state = s_SEND_CRC) else '0';
	local_reset <= '1' when ((current_state = s_RESET) or (current_state = s_WAIT_REQUEST)) else '0';
	recv_data 	<= '1' when (current_state = s_RECEIVING_DATA) else '0';
	single_bit_conversion(0) <= shift_register(15);
	crc_generator_enable <= '0' when (current_state = s_WAIT_DEASSERT) else i_sd_clock_pulse_trigger;
	o_operation_complete <= '1' when (current_state = s_WAIT_DEASSERT) else '0';
	o_dat_direction <= '1' when (	(current_state = s_SEND_START_BIT) or 
									(current_state = s_SEND_DATA) or 
									(current_state = s_SEND_CRC) or 
									(current_state = s_SEND_STOP))
						else '0';
	o_1bit_data_out 	<= dataout_1bit;
	o_crc_passed 		<= '1' when ((crc_out = shift_register(16 downto 1)) and (shift_register(0) = '1')) else '0';
	o_timed_out 		<= '1' when (timeout_register = TIMEOUT) else '0';
	
	-- Local components
	local_regs: process(i_clock, i_reset_n, local_reset)
	begin
		if (i_reset_n = '0') then
			bit_counter 	<= (OTHERS => '1');
			byte_counter 	<= (OTHERS => '0');
			dataout_1bit 	<= '1';
			crc_counter 	<= (OTHERS => '0');
			shift_register <= (OTHERS => '0');
		elsif (rising_edge(i_clock)) then
			-- counters and serial output
			if (local_reset = '1') then
				bit_counter 	<= (OTHERS => '1');
				byte_counter 	<= (OTHERS => '0');
				dataout_1bit 	<= '1';
				data_in_reg 	<= '1';
				crc_counter 	<= (OTHERS => '0');
				shift_register <= (OTHERS => '0');
			elsif (i_sd_clock_pulse_trigger = '1') then
				if ((not (current_state = s_RECEIVING_LEADING_BITS)) and (not (current_state = s_SEND_CRC))) then
					crc_counter <= (OTHERS => '0');
				else
					if (not (crc_counter = "1111")) then
						crc_counter <= crc_counter + '1';
					end if;
				end if;
				if ((current_state = s_RECEIVING_DATA) or (current_state = s_SEND_DATA)) then			
					if (not ((bit_counter = "000") and (byte_counter = "111111111"))) then
						if (bit_counter = "000") then
							byte_counter 	<= byte_counter + '1';
							bit_counter 	<= "111";
						else
							bit_counter 	<= bit_counter - '1';
						end if;
					end if;
				end if;
				-- Output data bit.
				if (current_state = s_SEND_START_BIT) then
					dataout_1bit <= '0';
				elsif (current_state = s_SEND_DATA) then
					dataout_1bit <= from_mem_1_bit;
				elsif (current_state = s_SEND_CRC) then
					dataout_1bit <= from_crc_generator;
				else
					dataout_1bit <= '1'; -- Stop bit.
				end if;

				-- Shift register to store the CRC bits once the message is received. 
				if ((current_state = s_RECEIVING_DATA) or
					(current_state = s_RECEIVING_LEADING_BITS) or
					(current_state = s_RECEIVING_STOP_BIT)) then
					shift_register(16 downto 1) <= shift_register(15 downto 0);
					shift_register(0) <= data_in_reg;
				end if;
				data_in_reg <= i_1bit_data_in;
			end if;
		end if;
	end process;
	
	-- Register holding the timeout value for data transmission.
	timeout_reg: process(i_clock, i_reset_n, current_state, i_sd_clock_pulse_trigger)
	begin
		if (i_reset_n = '0') then
			timeout_register <= (OTHERS => '0');
		elsif (rising_edge(i_clock)) then
			if ((current_state = s_SEND_STOP) or
				(current_state = s_WAIT_REQUEST)) then
				timeout_register <= (OTHERS => '0');
			elsif (i_sd_clock_pulse_trigger = '1') then
				-- Increment the timeout counter
				if (((current_state = s_WAIT_DATA_START) or
					(current_state = s_WAIT_BUSY)) and (not (timeout_register = TIMEOUT))) then
					timeout_register <= timeout_register  + '1';
				end if;
			end if;
		end if;
	end process;

	-- Instantiated components. 
	crc16_checker:	Altera_UP_SD_CRC16_Generator
	port map
	(
		i_clock 			=> i_clock,
		i_reset_n 		=> i_reset_n,
		i_sync_reset 	=> local_reset,
		i_enable 		=> crc_generator_enable,
		i_shift			=> shift_crc,
		i_datain 		=> to_crc_generator,
		o_dataout 		=> from_crc_generator,
		o_crcout 		=> crc_out
	);
	
	packet_memory: 	Altera_UP_SD_Card_Memory_Block
	PORT MAP
	(
		address_a 		=> i_address_16bit_port,
		address_b 		=> packet_mem_addr_b,
		clock_a			=> i_clock,
		clock_b			=> i_clock,
		data_a 			=> i_16bit_data_in,
		data_b 			=> single_bit_conversion,
		enable_a 		=> i_enable_16bit_port,
		enable_b 		=> '1',
		wren_a 			=> i_write_16bit,
		wren_b 			=> recv_data,
		q_a				=> o_16bit_data_out,
		q_b				=> single_bit_out
	);
	from_mem_1_bit 	<= single_bit_out(0);
	packet_mem_addr_b	<= (byte_counter & bit_counter);
	
end rtl;
