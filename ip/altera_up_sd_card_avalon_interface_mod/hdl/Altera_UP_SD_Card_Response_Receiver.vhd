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
-- This module looks at the data on the CMD line and waits to receive a response.
-- It begins examining the data lines when the i_begin signal is asserted. It then
-- waits for a first '0'. It then proceeds to store as many bits as are required by
-- the response packet. Each message bit passes through the CRC7 circuit so that
-- the CRC check sum can be verified at the end of transmission. The circuit then produces
-- the o_data and o_CRC_passed outputs to indicate the message received and if the CRC
-- check passed.
--
-- If for some reason the requested response does not arrive within 56 clock cycles
-- then the circuit will produce a '1' on the o_timeout output. In such a case the
-- o_data should be ignored.
--
-- In case of a response that is not 001, 010, 011 or 110, the circuit expects
-- no response.
--
-- A signal o_done is asserted when the circuit has completed response retrieval. In
-- a case when a response is not expected, just wait for the CD Card to process the
-- command. This is done by waiting 8 (=PROCESSING_DELAY) clock cycles.
--
-- NOTES/REVISIONS:
-------------------------------------------------------------------------------------
library ieee;
use ieee.std_logic_1164.all;
use ieee.std_logic_arith.all;
use ieee.std_logic_unsigned.all;

entity Altera_UP_SD_Card_Response_Receiver is
	generic (
		TIMEOUT				: std_logic_vector(7 downto 0) := "00111000";
		BUSY_WAIT			: std_logic_vector(7 downto 0) := "00110000";
		PROCESSING_DELAY	: std_logic_vector(7 downto 0) := "00001000"
	);
	port
	(
		i_clock		  		: in std_logic;
		i_reset_n	  		: in std_logic;
		i_begin				: in std_logic;
		i_scan_pulse		: in std_logic;
		i_datain				: in std_logic;
		i_wait_cmd_busy	: in std_logic;		
		i_response_type	: in std_logic_vector(2 downto 0);
		o_data				: out std_logic_vector(127 downto 0);
		o_CRC_passed		: out std_logic;
		o_timeout			: out std_logic;
		o_done				: out std_logic
	);
end entity;

architecture rtl of Altera_UP_SD_Card_Response_Receiver is

	component Altera_UP_SD_CRC7_Generator
	port
	(
		i_clock 				: in std_logic;
		i_enable				: in std_logic;
		i_reset_n			: in std_logic;
		i_shift				: in std_logic;
		i_datain				: in std_logic;
		o_dataout			: out std_logic;
		o_crcout				: out std_logic_vector(6 downto 0)
	);
	end component;
	-- Build an enumerated type for the state machine. On reset always reset the DE2 and read the state
	-- of the switches.
	type state_type is (s_WAIT_BEGIN, s_WAIT_END, s_WAIT_PROCESSING_DELAY, s_WAIT_BUSY, s_WAIT_BUSY_END, s_WAIT_BEGIN_DEASSERT);

	-- Register to hold the current state
	signal current_state : state_type;
	signal next_state : state_type;	
	
	-- Local wires
	-- REGISTERED
	signal registered_data_input 		: std_logic_vector(127 downto 0);
	signal response_incoming 			: std_logic;
	signal counter, timeout_counter 	: std_logic_vector(7 downto 0);
	signal crc_shift, keep_reading_bits, shift_crc_bits : std_logic;
	-- UNREGISTERED
	signal limit, limit_minus_1 		: std_logic_vector(7 downto 0);
	signal check_crc 						: std_logic;
	signal CRC_bits						: std_logic_vector(6 downto 0);
	signal start_reading_bits, operation_complete, enable_crc_unit : std_logic;
begin
	-- Control FSM. Begin operation when i_begin is raised, then wait for the operation to end and i_begin to be deasserted.
	state_regs: process(i_clock, i_reset_n)
	begin
		if (i_reset_n = '0') then
			current_state <= s_WAIT_BEGIN;
		elsif (rising_edge(i_clock)) then
			current_state <= next_state;
		end if;
	end process;
	
	state_transitions: process(current_state, i_begin, operation_complete, timeout_counter, i_wait_cmd_busy, i_scan_pulse, i_datain)
	begin
		case current_state is
			when s_WAIT_BEGIN =>
				if (i_begin = '1') then
					next_state <= s_WAIT_END;
				else
					next_state <= s_WAIT_BEGIN;
				end if;
				
			when s_WAIT_END =>
				if (operation_complete = '1') then
					if (timeout_counter = TIMEOUT) then
						next_state <= s_WAIT_BEGIN_DEASSERT;
					else
						next_state <= s_WAIT_PROCESSING_DELAY;
					end if;
						
				else
					next_state <= s_WAIT_END;
				end if;	
			
			when s_WAIT_PROCESSING_DELAY =>
				if (timeout_counter = PROCESSING_DELAY) then
					if (i_wait_cmd_busy = '1') then					
						next_state <= s_WAIT_BUSY;
					else
						next_state <= s_WAIT_BEGIN_DEASSERT;
					end if;
				else
					next_state <= s_WAIT_PROCESSING_DELAY;
				end if;
			
			when s_WAIT_BUSY =>
				if ((i_scan_pulse = '1') and (i_datain = '0')) then
					next_state <= s_WAIT_BUSY_END;
				else
					if (timeout_counter = BUSY_WAIT) then
						-- If the card did not become busy, then it would not have raised the optional busy signal.
						-- In such a case, proceeed further as the command has finished correctly.
						next_state <= s_WAIT_BEGIN_DEASSERT;
					else
						next_state <= s_WAIT_BUSY;
					end if;
				end if;
				
			when s_WAIT_BUSY_END =>
				if ((i_scan_pulse = '1') and (i_datain = '1')) then
					next_state <= s_WAIT_BEGIN_DEASSERT;
				else
					next_state <= s_WAIT_BUSY_END;
				end if;
								
			when s_WAIT_BEGIN_DEASSERT =>
				if (i_begin = '1') then
					next_state <= s_WAIT_BEGIN_DEASSERT;
				else
					next_state <= s_WAIT_BEGIN;
				end if;
			when others =>
				next_state <= s_WAIT_BEGIN;
		end case;
	end process;
	
	-- Store the response as it appears on the i_datain line.
	received_data_buffer: process (i_clock, i_reset_n)
	begin
		if (i_reset_n = '0') then
			registered_data_input <= (OTHERS=>'0');
		elsif (rising_edge(i_clock)) then
			-- Only read new data and update the counter value when the scan pulse is high.
			if (i_scan_pulse = '1') then
				if ((start_reading_bits = '1') or (keep_reading_bits = '1')) then
					registered_data_input(127 downto 1) <= registered_data_input(126 downto 0);
					registered_data_input(0) <= i_datain;
				end if;
			end if;
		end if;
	end process;

	-- Counter received bits
	data_read_counter: process (i_clock, i_reset_n)
	begin
		if (i_reset_n = '0') then
			counter <= (OTHERS=>'0');
		elsif (rising_edge(i_clock)) then
			-- Reset he counter every time you being reading the response.
			if (current_state = s_WAIT_BEGIN) then
				counter <= (OTHERS => '0');
			end if;
			-- Update the counter value when the scan pulse is high.
			if (i_scan_pulse = '1') then
				if ((start_reading_bits = '1') or (keep_reading_bits = '1')) then
					counter <= counter + '1';
				end if;
			end if;
		end if;
	end process;
	operation_complete <= '1' when (((counter = limit) and (not (limit = "00000000"))) or
									 (timeout_counter = TIMEOUT) or
									 ((timeout_counter = PROCESSING_DELAY) and (limit = "00000000"))) else '0';
	
	-- Count the number of scan pulses before the response is received. If the counter
	-- exceeds TIMEOUT value, then an error must have occured when the SD card received a message.
	timeout_counter_control: process (i_clock, i_reset_n)
	begin
		if (i_reset_n = '0') then
			timeout_counter <= (OTHERS=>'0');
		elsif (rising_edge(i_clock)) then
			-- Reset the counter every time you begin reading the response.
			if ((current_state = s_WAIT_BEGIN) or ((current_state = s_WAIT_END) and (operation_complete = '1') and (not (timeout_counter = TIMEOUT)))) then
				timeout_counter <= (OTHERS => '0');
			end if;
			-- Update the counter value when the scan pulse is high.
			if (i_scan_pulse = '1') then
				if (((start_reading_bits = '0') and (keep_reading_bits = '0') and (current_state = s_WAIT_END) and (not (timeout_counter = TIMEOUT))) or
					(current_state = s_WAIT_PROCESSING_DELAY) or (current_state = s_WAIT_BUSY)) then
					timeout_counter <= timeout_counter + '1';
				end if;
			end if;
		end if;
	end process;
		
	-- Enable data storing only after you see the first 0.
	read_enable_logic: process (i_clock, i_reset_n)
	begin
		if (i_reset_n = '0') then
			keep_reading_bits <= '0';
		elsif (rising_edge(i_clock)) then
			if (i_scan_pulse = '1') then
				if ((start_reading_bits = '1') or ((keep_reading_bits = '1') and (not (counter = limit_minus_1)))) then
					keep_reading_bits <= '1';
				else
					keep_reading_bits <= '0';				
				end if;
			end if;
		end if;
	end process;
	start_reading_bits <= '1' when ((current_state = s_WAIT_END) and (i_datain = '0') and
									(counter = "00000000") and (not (limit = "00000000"))) else '0';
		
	-- CRC7 checker.
	crc_checker: Altera_UP_SD_CRC7_Generator PORT MAP
	(
		i_clock 		=> i_clock,
		i_reset_n 	=> i_reset_n,
		i_enable 	=> enable_crc_unit,
		i_shift		=> shift_crc_bits,
		i_datain 	=> registered_data_input(7),
		o_crcout 	=> CRC_bits
	);
	enable_crc_unit <= '1' when ((i_scan_pulse = '1') and (current_state = s_WAIT_END)) else '0';
	
	-- Clear CRC7 registers before processing the response bits
	crc_control_register: process (i_clock, i_reset_n)
	begin
		if (i_reset_n = '0') then
			shift_crc_bits <= '1';
		elsif (rising_edge(i_clock)) then
			-- Reset he counter every time you being reading the response.
			if (current_state = s_WAIT_BEGIN) then
				-- clear the CRC7 contents before you process the next message.
				shift_crc_bits <= '1';
			end if;
			-- Only read new data and update the counter value when the scan pulse is high.
			if (i_scan_pulse = '1') then
				if ((start_reading_bits = '1') or (keep_reading_bits = '1')) then
					if (counter = "00000111") then
						-- Once the 7-bits of the CRC checker have been cleared you can process the message and
						-- compute its CRC bits to verify the validity of the transmission.
						shift_crc_bits <= '0';
					end if;
				end if;
			end if;
		end if;
	end process;
	
	-- Indicate the number of bits to expect in the response packet.
	limit <= "00110000" when 	((i_response_type = "001") or
								 (i_response_type = "011") or
								 (i_response_type = "110")) else
			 "10001000" when	(i_response_type = "010") else
			 "00000000";	-- No response
	limit_minus_1 <= 
			"00101111" when 	((i_response_type = "001") or
								 (i_response_type = "011") or
								 (i_response_type = "110")) else
			"10000111" when		 (i_response_type = "010") else
			"00000000";	-- No response

	check_crc <= '1' when ((i_response_type = "001") or (i_response_type = "110")) else '0';
	
	-- Generate Circuit outputs
	o_data <= (registered_data_input(127 downto 1) & '1') when (i_response_type = "010") else
			  (CONV_STD_LOGIC_VECTOR(0, 96) & registered_data_input(39 downto 8));
			  
	o_CRC_passed 	<= '1' when ((check_crc = '0') or 
							  ((registered_data_input(0) = '1') and (CRC_bits = registered_data_input(7 downto 1)))) else '0';
							  
	o_timeout 		<= '1' when (timeout_counter = TIMEOUT) else '0';
	o_done 			<= '1' when (current_state = s_WAIT_BEGIN_DEASSERT) else '0';
end rtl;