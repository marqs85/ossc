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


----------------------------------------------------------------------------------------------------------------
-- This is an FSM that allows access to the SD Card IP core via the Avalon Interconnect.
--
-- This module takes a range of addresses on the Avalon Interconnect. Specifically:
-- - 0x00000000 to 0x000001ff
--				word addressable buffer space. The data to be written to the SD card as well
--				as data read from the SD card can be accessed here.
--
-- - 0x00000200 to 0x0000020f
--				128-bit containing the Card Identification Number. The meaning of each bit is described in the
--				SD Card Physical Layer Specification Document.
--
-- - 0x00000210 to 0x0000021f
--				128-bit register containing Card Specific Data. The meaning of each bit is described in the
--				SD Card Physical Layer Specification Document.
--
-- - 0x00000220 to 0x00000223
--				32-bit register containing Operating Conditions Register. The meaning of each bit is described
--				in the SD Card Physical Layer Specification Document.
--
-- - 0x00000224 to 0x00000227
--				32-bit register containing the Status Register. The meaning of each bit is described
--				in the SD Card Physical Layer Specification Document. However, if the card is not connected or the
--				status register could not be read from the SD card, this register will contain invalid data. In such
--				a case, wait for a card to be connected by checking the Auxiliary Status Register (UP Core Specific), and
--				a command 13 (SEND_STATUS) to update the contents of this register when possible. If a card is connected then
--				the Auxiliary Status Register can be polled until such a time that Status Register is valid, as the SD Card
--				interface circuit updates the status register approximately every 0.1 of a second, and after every command
--				is executed.
--
-- - 0x00000228 to 0x000000229
--				16-bit register containing the Relative Card Address. This address uniquely identifies a card
--				connected to the SD Card slot.
--
-- - 0x0000022C to 0x00000022F
--				32-bit register used to set the argument for a command to be sent to the SD Card.
--
-- - 0x00000230 to 0x000000231
--				16-bit register used to send a command to an SD card. Once written, the interface will issue the
--				specified command. The meaning of each bit in this register is as follows:
--				- 0-5 - command index. This is a command index as per SD Card Physical Layer specification document.
--				- 6	  - use most recent RCA. If this bit is set, the command argument will be replaced with the contents of
--						the Relative Card Address register, followed by 16 0s. For commands that require RCA to be sent as
--						an argument, this bit should be set and users will not need to specify RCA themselves.
--				- 7-15 - currently unused bits. They will be ignored.
--				NOTE: 	If a specified command is determined to be invalid, or the card is not connected to the SD Card socket,
--						then the SD Card interface circuit will not issue the command.
--
-- - 0x00000234 to 0x00000235
--				16-bit register with Auxiliary Status Register. This is the Altera UP SD Card Interface status. The meaning of
--				the bits is as follows:
--				- 0 - last command valid - Set to '1' if the most recently user issued command was valid.
--				- 1 - card connected - Set to '1' if at present an SD card
--				- 2 - execution in progress - 	Set to '1' if the command recently issued is currently being executed. If true,
--												then the current state of SD Card registers should be ignored.
--				- 3 - status register valid -	Set to '1' if the status register is valid.
--				- 4 - command timed out  	-	Set to '1' if the last command timed out.
--				- 5 - crc failed  			-	Set to '1' if the last command failed a CRC check.
--				- 6-15 - unused.
--
-- - 0x00000238 to 0x0000023B
--				32-bit register containing the 32-bit R1 response message. Use it to test validity of the response. This register
--				will not store the response to SEND_STATUS command. Insteand, read the SD_status register at location 0x00000224.
--
-- Date: December 8, 2008
-- NOTES/REVISIONS:
-- December 17, 2008 - added R1 response register to the core. It is now available at 0x00000238.
----------------------------------------------------------------------------------------------------------------

library ieee;
use ieee.std_logic_1164.all;
use ieee.std_logic_arith.all;
use ieee.std_logic_unsigned.all;

entity Altera_UP_SD_Card_Avalon_Interface is
	generic (
		ADDRESS_BUFFER 	: std_logic_vector(7 downto 0) := "00000000";
		ADDRESS_CID 		: std_logic_vector(7 downto 0) := "10000000";
		ADDRESS_CSD 		: std_logic_vector(7 downto 0) := "10000100";
		ADDRESS_OCR 		: std_logic_vector(7 downto 0) := "10001000";
		ADDRESS_SR 			: std_logic_vector(7 downto 0) := "10001001";
		ADDRESS_RCA 		: std_logic_vector(7 downto 0) := "10001010";
		ADDRESS_ARGUMENT 	: std_logic_vector(7 downto 0) := "10001011";
		ADDRESS_COMMAND 	: std_logic_vector(7 downto 0) := "10001100";
		ADDRESS_ASR 		: std_logic_vector(7 downto 0) := "10001101";
		ADDRESS_R1 			: std_logic_vector(7 downto 0) := "10001110"
	);
	port
	(
		-- Clock and Reset signals 
		i_clock	 	: in STD_LOGIC;
		i_reset_n	: in STD_LOGIC; -- Asynchronous reset
				
		-- Avalon Interconnect Signals
		i_avalon_address		: in STD_LOGIC_VECTOR(7 downto 0);
		i_avalon_chip_select	: in STD_LOGIC;
		i_avalon_read			: in STD_LOGIC;
		i_avalon_write			: in STD_LOGIC;
		i_avalon_byteenable 	: in STD_LOGIC_VECTOR(3 downto 0);
		i_avalon_writedata	: in STD_LOGIC_VECTOR(31 downto 0);
		o_avalon_readdata		: out STD_LOGIC_VECTOR(31 downto 0);
		o_avalon_waitrequest	: out STD_LOGIC;
		
		-- SD Card interface ports
		b_SD_cmd			: inout STD_LOGIC;
		b_SD_dat			: inout STD_LOGIC;
		b_SD_dat3		: inout STD_LOGIC;
		o_SD_clock		: out STD_LOGIC
	);

end entity;

architecture rtl of Altera_UP_SD_Card_Avalon_Interface is
	
	component Altera_UP_SD_Card_Interface is
	port
	(
		i_clock		  			: in std_logic;
		i_reset_n	  			: in std_logic;

		-- Command interface
		b_SD_cmd 				: inout std_logic;
		b_SD_dat 				: inout std_logic;
		b_SD_dat3				: inout std_logic;
		i_command_ID			: in std_logic_vector(5 downto 0);
		i_argument				: in std_logic_vector(31 downto 0);		
		i_user_command_ready	: in std_logic;
		
		o_SD_clock				: out std_logic;
		o_card_connected 		: out std_logic;
		o_command_completed	: out std_logic;
		o_command_valid 		: out std_logic;
		o_command_timed_out	: out std_logic;
		o_command_crc_failed	: out std_logic;			
		
		-- Buffer access
		i_buffer_enable		: in std_logic;
		i_buffer_address		: in std_logic_vector(7 downto 0);
		i_buffer_write			: in std_logic;
		i_buffer_data_in		: in std_logic_vector(15 downto 0);
		o_buffer_data_out 	: out std_logic_vector(15 downto 0);
		
		-- Show SD Card registers as outputs
		o_SD_REG_card_identification_number 	: out std_logic_vector(127 downto 0);
		o_SD_REG_relative_card_address 			: out std_logic_vector(15 downto 0);
		o_SD_REG_operating_conditions_register : out std_logic_vector(31 downto 0);
		o_SD_REG_card_specific_data 				: out std_logic_vector(127 downto 0);
		o_SD_REG_status_register 					: out std_logic_vector(31 downto 0);
		o_SD_REG_response_R1							: out std_logic_vector(31 downto 0);		
		o_SD_REG_status_register_valid 			: out std_logic		
	);
	end component;	
	
	-- Build an enumerated type for the state machine. On reset always reset the DE2 and read the state
	-- of the switches.
	type buffer_state_type is (	s_RESET, s_WAIT_REQUEST, s_READ_FIRST_WORD, s_READ_SECOND_WORD, s_RECEIVE_FIRST_WORD, 
								s_RECEIVE_SECOND_WORD, s_WR_READ_FIRST_WORD, s_WR_READ_FIRST_WORD_DELAY, s_WRITE_FIRST_BYTE, s_WRITE_FIRST_WORD,
								s_WR_READ_SECOND_WORD, s_WR_READ_SECOND_WORD_DELAY, s_WRITE_SECOND_BYTE, s_WRITE_SECOND_WORD, s_WAIT_RELEASE);
	type command_state_type is (s_RESET_CMD, s_WAIT_COMMAND, s_WAIT_RESPONSE, s_UPDATE_AUX_SR);

	-- Register to hold the current state
	signal current_state 		: buffer_state_type;
	signal next_state 			: buffer_state_type;
	signal current_cmd_state 	: command_state_type;
	signal next_cmd_state 		: command_state_type;	
	
	-------------------
	-- Local signals
	-------------------
	-- REGISTERED
	signal auxiliary_status_reg 	: std_logic_vector(5 downto 0);
	signal buffer_data_out_reg 	: std_logic_vector(31 downto 0);
	signal buffer_data_in_reg 		: std_logic_vector(31 downto 0);
	signal buffer_data_out			: std_logic_vector(15 downto 0);
	signal command_ID_reg			: std_logic_vector( 5 downto 0);
	signal argument_reg				: std_logic_vector(31 downto 0);
	signal avalon_address			: std_logic_vector(7 downto 0);
	signal avalon_byteenable		: std_logic_vector(3 downto 0);
	-- UNREGISTERED
	signal buffer_address								: std_logic_vector(7 downto 0);
	signal buffer_data_in								: std_logic_vector(15 downto 0);
	signal SD_REG_card_identification_number 		: std_logic_vector(127 downto 0);
	signal SD_REG_relative_card_address 			: std_logic_vector(15 downto 0);
	signal SD_REG_operating_conditions_register 	: std_logic_vector(31 downto 0);
	signal SD_REG_card_specific_data 				: std_logic_vector(127 downto 0);
	signal SD_REG_status_register 					: std_logic_vector(31 downto 0);
	signal SD_REG_response_R1							: std_logic_vector(31 downto 0);
	signal command_ready, send_command_ready,
		   command_valid, command_completed, card_connected 	: std_logic;
	signal status_reg_valid, argument_write 						: std_logic;
	signal read_buffer_request, write_buffer_request, buffer_enable, buffer_write : std_logic;
	signal command_timed_out, command_crc_failed 				: std_logic;

begin
	-- Define state transitions for buffer interface.
	state_transitions_buffer: process (current_state, read_buffer_request, write_buffer_request, i_avalon_byteenable, avalon_byteenable)
	begin
		case current_state is
			when s_RESET =>
				-- Reset local registers. 
				next_state <= s_WAIT_REQUEST;
			
			when s_WAIT_REQUEST =>
				-- Wait for a user command.
				if (read_buffer_request = '1') then
					next_state <= s_READ_FIRST_WORD;
				elsif (write_buffer_request = '1') then
					if ((i_avalon_byteenable(1) = '1') and (i_avalon_byteenable(0) = '1')) then
						next_state <= s_WRITE_FIRST_WORD;
					elsif ((i_avalon_byteenable(3) = '1') and (i_avalon_byteenable(2) = '1')) then
						next_state <= s_WRITE_SECOND_WORD;
					elsif ((i_avalon_byteenable(1) = '1') or (i_avalon_byteenable(0) = '1')) then
						next_state <= s_WR_READ_FIRST_WORD;
					elsif ((i_avalon_byteenable(3) = '1') or (i_avalon_byteenable(2) = '1')) then
						next_state <= s_WR_READ_SECOND_WORD;
					else
						next_state <= s_WAIT_REQUEST;
					end if;
				else
					next_state <= s_WAIT_REQUEST;				
				end if;
				
			when s_READ_FIRST_WORD =>
				-- Read first 16-bit word from the buffer
				next_state <= s_READ_SECOND_WORD;
				
			when s_READ_SECOND_WORD =>
				-- Read second 16-bit word from the buffer
				next_state <= s_RECEIVE_FIRST_WORD;
				
			when s_RECEIVE_FIRST_WORD =>
				-- Store first word read
				next_state <= s_RECEIVE_SECOND_WORD;

			when s_RECEIVE_SECOND_WORD =>
				-- Store second word read
				next_state <= s_WAIT_RELEASE;
			
			-- The following states control writing to the buffer. To write a single byte it is necessary to read a
			-- word and then write it back, changing only on of its bytes.
			when s_WR_READ_FIRST_WORD =>
				-- Read first 16-bit word from the buffer
				next_state <= s_WR_READ_FIRST_WORD_DELAY;

			when s_WR_READ_FIRST_WORD_DELAY =>
				-- Wait a cycle
				next_state <= s_WRITE_FIRST_BYTE;
								
			when s_WRITE_FIRST_BYTE =>
				-- Write one of the bytes in the given word into the memory.
				if ((avalon_byteenable(3) = '1') and (avalon_byteenable(2) = '1')) then
					next_state <= s_WRITE_SECOND_WORD;
				elsif ((avalon_byteenable(3) = '1') or (avalon_byteenable(2) = '1')) then
					next_state <= s_WR_READ_SECOND_WORD;
				else
					next_state <= s_WAIT_RELEASE;
				end if;
								
			when s_WR_READ_SECOND_WORD =>
				-- Read second 16-bit word from the buffer
				next_state <= s_WR_READ_SECOND_WORD_DELAY;

			when s_WR_READ_SECOND_WORD_DELAY =>
				-- Wait a cycle
				next_state <= s_WRITE_SECOND_BYTE;
								
			when s_WRITE_SECOND_BYTE =>
				-- Write one of the bytes in the given word into the memory.
				next_state <= s_WAIT_RELEASE;				
							
			-- Full word writing can be done without reading the word in the first place.
			when s_WRITE_FIRST_WORD =>
				-- Write the first word into memory
				if ((avalon_byteenable(3) = '1') and (avalon_byteenable(2) = '1')) then
					next_state <= s_WRITE_SECOND_WORD;
				elsif ((avalon_byteenable(3) = '1') or (avalon_byteenable(2) = '1')) then
					next_state <= s_WR_READ_SECOND_WORD;
				else
					next_state <= s_WAIT_RELEASE;
				end if;
				
			when s_WRITE_SECOND_WORD =>
				-- Write the second word into memory
				next_state <= s_WAIT_RELEASE;
				
			when s_WAIT_RELEASE =>
				-- if ((read_buffer_request = '1') or (write_buffer_request = '1')) then
					-- next_state <= s_WAIT_RELEASE;
				-- else
					next_state <= s_WAIT_REQUEST;
				-- end if;
							
			when others =>
				-- Make sure to start in the reset state if the circuit powers up in an odd state.
				next_state <= s_RESET;
		end case;
	end process;
	
	-- State Registers
	buffer_state_regs: process(i_clock, i_reset_n)
	begin
		if (i_reset_n = '0') then
			current_state <= s_RESET;
		elsif(rising_edge(i_clock)) then
			current_state <= next_state;
		end if;
	end process;	

	helper_regs: process(i_clock, i_reset_n)
	begin
		if (i_reset_n = '0') then
			avalon_address 			<= (OTHERS => '0');
			buffer_data_out_reg 		<= (OTHERS => '0');
			buffer_data_in_reg 		<= (OTHERS => '0');
			avalon_byteenable 		<= (OTHERS => '0');
		elsif(rising_edge(i_clock)) then
			if (current_state = s_WAIT_REQUEST) then
				avalon_address 		<= i_avalon_address;
				buffer_data_in_reg 	<= i_avalon_writedata;
				avalon_byteenable 	<= i_avalon_byteenable;
			end if;
			if (current_state = s_RECEIVE_FIRST_WORD) then
				buffer_data_out_reg(15 downto 0) 	<= buffer_data_out;
			end if;
			if (current_state = s_RECEIVE_SECOND_WORD) then
				buffer_data_out_reg(31 downto 16) 	<= buffer_data_out;
			end if;
		end if;
	end process;

	-- FSM outputs
	o_avalon_waitrequest <= (read_buffer_request or write_buffer_request) when (not (current_state = s_WAIT_RELEASE)) else '0';
	buffer_address(7 downto 1) <= avalon_address(6 downto 0);
	buffer_address(0) <= '1' when ( (current_state = s_READ_SECOND_WORD) or (current_state = s_WRITE_SECOND_WORD) or
									(current_state = s_WR_READ_SECOND_WORD) or (current_state = s_WRITE_SECOND_BYTE)) else
						 '0';
	buffer_enable <= '1' when (	(current_state = s_READ_FIRST_WORD) or (current_state = s_WR_READ_FIRST_WORD) or
								(current_state = s_READ_SECOND_WORD) or (current_state = s_WR_READ_SECOND_WORD) or
								(current_state = s_WRITE_FIRST_WORD) or (current_state = s_WRITE_FIRST_BYTE) or
								(current_state = s_WRITE_SECOND_WORD) or (current_state = s_WRITE_SECOND_BYTE)) else
					  '0';
	buffer_write <= '1' when (	(current_state = s_WRITE_FIRST_WORD) or (current_state = s_WRITE_FIRST_BYTE) or
								(current_state = s_WRITE_SECOND_WORD) or (current_state = s_WRITE_SECOND_BYTE)) else
					  '0';
	buffer_data_in <= 	(buffer_data_out(15 downto 8) & buffer_data_in_reg(7 downto 0)) when ((current_state = s_WRITE_FIRST_BYTE) and (avalon_byteenable(1 downto 0) = "01")) else
						(buffer_data_in_reg(15 downto 8) & buffer_data_out(7 downto 0)) when ((current_state = s_WRITE_FIRST_BYTE) and (avalon_byteenable(1 downto 0) = "10")) else
						(buffer_data_out(15 downto 8) & buffer_data_in_reg(23 downto 16)) when ((current_state = s_WRITE_SECOND_BYTE) and (avalon_byteenable(3 downto 2) = "01")) else
						(buffer_data_in_reg(31 downto 24) & buffer_data_out(7 downto 0)) when ((current_state = s_WRITE_SECOND_BYTE) and (avalon_byteenable(3 downto 2) = "10")) else
						buffer_data_in_reg(15 downto 0) when (current_state = s_WRITE_FIRST_WORD) else
						buffer_data_in_reg(31 downto 16);
	-- Glue Logic
	read_buffer_request <= (not i_avalon_address(7)) and (i_avalon_chip_select) and (i_avalon_read);
	write_buffer_request <= (not i_avalon_address(7)) and (i_avalon_chip_select) and (i_avalon_write);

	-- Define state transitions for command interface.
	state_transitions_cmd: process (current_cmd_state, command_completed, command_valid, command_ready)
	begin
		case current_cmd_state is
			when s_RESET_CMD =>
				-- Reset local registers. 
				next_cmd_state <= s_WAIT_COMMAND;
			
			when s_WAIT_COMMAND =>
				-- Wait for a user command.
				if (command_ready = '1') then
					next_cmd_state <= s_WAIT_RESPONSE;
				else
					next_cmd_state <= s_WAIT_COMMAND;
				end if;
				
			when s_WAIT_RESPONSE =>
				-- Generate a predefined command to the SD card. This is the identification process for the SD card.
				if ((command_completed = '1') or (command_valid = '0')) then
					next_cmd_state <= s_UPDATE_AUX_SR;
				else
					next_cmd_state <= s_WAIT_RESPONSE;
				end if;
				
			when s_UPDATE_AUX_SR =>
				-- Update the Auxiliary status register.
				if (command_ready = '1') then
					next_cmd_state <= s_UPDATE_AUX_SR;
				else
					next_cmd_state <= s_WAIT_COMMAND;
				end if;
			
			when others =>
				-- Make sure to start in the reset state if the circuit powers up in an odd state.
				next_cmd_state <= s_RESET_CMD;
		end case;
	end process;
	
	-- State registers
	cmd_state_regs: process(i_clock, i_reset_n)
	begin
		if (i_reset_n = '0') then
			current_cmd_state <= s_RESET_CMD;
		elsif(rising_edge(i_clock)) then
			current_cmd_state <= next_cmd_state;
		end if;
	end process;
	
	-- FSM outputs
	send_command_ready <= '1' when ((current_cmd_state = s_WAIT_RESPONSE) or (current_cmd_state = s_UPDATE_AUX_SR)) else '0';

	-- Glue logic
	command_ready <= '1' when (	(i_avalon_chip_select = '1') and (i_avalon_write = '1') and
								(i_avalon_address = ADDRESS_COMMAND)) else '0';
	argument_write <= '1' when ((i_avalon_chip_select = '1') and (i_avalon_write = '1') and
								(i_avalon_address = ADDRESS_ARGUMENT)) else '0';
	-- Local Registers
	local_regs: process(i_clock, i_reset_n, current_cmd_state, card_connected, command_valid, i_avalon_writedata, command_completed, command_ready)
	begin
		if (i_reset_n = '0') then
			auxiliary_status_reg <= "000000";
			command_ID_reg <= (OTHERS => '0');
		elsif(rising_edge(i_clock)) then
			-- AUX Status Register
			if ((current_cmd_state = s_WAIT_RESPONSE) or (current_cmd_state = s_UPDATE_AUX_SR)) then
				auxiliary_status_reg(2) <= not command_completed;
				auxiliary_status_reg(4) <= command_timed_out;
				auxiliary_status_reg(5) <= command_crc_failed;
			end if;
			auxiliary_status_reg(0) <= command_valid;
			auxiliary_status_reg(1) <= card_connected;
			auxiliary_status_reg(3) <= status_reg_valid;
			-- Command
			if (command_ready = '1') then
				command_ID_reg <= i_avalon_writedata(5 downto 0);
			end if;
		end if;
	end process;
	
	argument_regs_processing: process(i_clock, i_reset_n, current_cmd_state, i_avalon_writedata, command_ready)
	begin
		if (i_reset_n = '0') then
			argument_reg <= (OTHERS => '0');
		elsif(rising_edge(i_clock)) then
			-- Argument register
			if ((command_ready = '1') and ( i_avalon_writedata(6) = '1')) then
				argument_reg <= SD_REG_relative_card_address & "0000000000000000";
			elsif (argument_write = '1') then
				argument_reg <= i_avalon_writedata;
			end if;
		end if;
	end process;
	
	o_avalon_readdata <= buffer_data_out_reg 									when (not (current_state = s_WAIT_REQUEST)) else
						 SD_REG_card_identification_number(31 downto 0)		when (i_avalon_address = ADDRESS_CID) else
						 SD_REG_card_identification_number(63 downto 32)	when (i_avalon_address = ADDRESS_CID(7 downto 2) & "01") else
						 SD_REG_card_identification_number(95 downto 64)	when (i_avalon_address = ADDRESS_CID(7 downto 2) & "10") else
						 SD_REG_card_identification_number(127 downto 96)	when (i_avalon_address = ADDRESS_CID(7 downto 2) & "11") else
						 SD_REG_card_specific_data(31 downto 0)				when (i_avalon_address = ADDRESS_CSD) else
						 SD_REG_card_specific_data(63 downto 32)				when (i_avalon_address = ADDRESS_CSD(7 downto 2) & "01") else
						 SD_REG_card_specific_data(95 downto 64)				when (i_avalon_address = ADDRESS_CSD(7 downto 2) & "10") else
						 SD_REG_card_specific_data(127 downto 96)				when (i_avalon_address = ADDRESS_CSD(7 downto 2) & "11") else
						 SD_REG_operating_conditions_register					when (i_avalon_address = ADDRESS_OCR) else
						 SD_REG_status_register										when (i_avalon_address = ADDRESS_SR) else
						 ("0000000000000000" & SD_REG_relative_card_address)when (i_avalon_address = ADDRESS_RCA) else
						 argument_reg													when (i_avalon_address = ADDRESS_ARGUMENT) else
						 ("00000000000000000000000000" & command_ID_reg)	when (i_avalon_address = ADDRESS_COMMAND) else
						 SD_REG_response_R1											when (i_avalon_address = ADDRESS_R1) else
						 ("00000000000000000000000000" & auxiliary_status_reg);
		
	-- Instantiated Components
	SD_Card_Port: Altera_UP_SD_Card_Interface
	port map
	(
		i_clock					=> i_clock,
		i_reset_n 				=> i_reset_n,

		-- Command interface
		b_SD_cmd 				=> b_SD_cmd,
		b_SD_dat 				=> b_SD_dat,
		b_SD_dat3 				=> b_SD_dat3,
		i_command_ID 			=> command_ID_reg,
		i_argument 				=> argument_reg,	
		i_user_command_ready => send_command_ready,
		
		o_SD_clock => o_SD_clock,
		o_card_connected 		=> card_connected,
		o_command_completed	=> command_completed,
		o_command_valid 		=> command_valid,
		o_command_timed_out	=> command_timed_out,
		o_command_crc_failed => command_crc_failed,
				
		-- Buffer access
		i_buffer_enable 		=> buffer_enable,
		i_buffer_address 		=> buffer_address,
		i_buffer_write 		=> buffer_write,
		i_buffer_data_in 		=> buffer_data_in,
		o_buffer_data_out 	=> buffer_data_out,
		
		-- Show SD Card registers as outputs
		o_SD_REG_card_identification_number 	=> SD_REG_card_identification_number,
		o_SD_REG_relative_card_address			=> SD_REG_relative_card_address,
		o_SD_REG_operating_conditions_register => SD_REG_operating_conditions_register,
		o_SD_REG_card_specific_data 				=> SD_REG_card_specific_data,
		o_SD_REG_status_register 					=> SD_REG_status_register,
		o_SD_REG_response_R1 						=> SD_REG_response_R1,
		o_SD_REG_status_register_valid 			=> status_reg_valid
	);

end rtl;

