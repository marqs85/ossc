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
-- This module takes a command ID and data, and generates a 48-bit message for it.
-- It will first check if the command is a valid 48-bit command and produce the
-- following outputs:
-- 1. o_dataout -> 	a single bit output that produces the message to be sent to the 
--					SD card one bit at a time. Every time the i_message_bit_out input
--					is high and the i_clock has a positive edge, a new bit is produced.
-- 2. o_message_done -> a signal that is asserted high when the entire message has been
-- 						produced through the o_dataout output.
-- 3. o_valid	->	is a signal that is asserted high if the specified message is valid.
-- 4. o_response_type -> indicates the command response type.
-- 5. o_returning_ocr -> the response from the SD card will contain the OCR register
-- 6. o_returning_cid -> the response from the SD card will contain the CID register
-- 7. o_returning_rca -> the response from the SD card will contain the RCA register
-- 8. o_returning_csd -> the response from the SD card will contain the CSD register
-- 9. o_data_read -> asserted when the command being sent is a data read command.
-- 10. o_data_write -> asserted when the command being sent is a data write command.
-- 11. o_wait_cmd_busy -> 	is set high when the response to this command will be
--							followed by a busy signal.
--
-- NOTES/REVISIONS:
-------------------------------------------------------------------------------------
library ieee;
use ieee.std_logic_1164.all;
use ieee.std_logic_arith.all;
use ieee.std_logic_unsigned.all;

entity Altera_UP_SD_Card_48_bit_Command_Generator is
	generic (
		-- Basic commands
		COMMAND_0_GO_IDLE 					: STD_LOGIC_VECTOR(5 downto 0) := "000000";
		COMMAND_2_ALL_SEND_CID 				: STD_LOGIC_VECTOR(5 downto 0) := "000010";
		COMMAND_3_SEND_RCA 					: STD_LOGIC_VECTOR(5 downto 0) := "000011";
		COMMAND_4_SET_DSR 					: STD_LOGIC_VECTOR(5 downto 0) := "000100";
		COMMAND_6_SWITCH_FUNCTION			: STD_LOGIC_VECTOR(5 downto 0) := "000110";
		COMMAND_7_SELECT_CARD 				: STD_LOGIC_VECTOR(5 downto 0) := "000111";
		COMMAND_9_SEND_CSD 					: STD_LOGIC_VECTOR(5 downto 0) := "001001";
		COMMAND_10_SEND_CID 					: STD_LOGIC_VECTOR(5 downto 0) := "001010";
		COMMAND_12_STOP_TRANSMISSION 		: STD_LOGIC_VECTOR(5 downto 0) := "001100";
		COMMAND_13_SEND_STATUS				: STD_LOGIC_VECTOR(5 downto 0) := "001101";
		COMMAND_15_GO_INACTIVE				: STD_LOGIC_VECTOR(5 downto 0) := "001111";
		-- Block oriented read/write/lock commands
		COMMAND_16_SET_BLOCK_LENGTH 		: STD_LOGIC_VECTOR(5 downto 0) := "010000";
		-- Block oriented read commands
		COMMAND_17_READ_BLOCK 				: STD_LOGIC_VECTOR(5 downto 0) := "010001";
		COMMAND_18_READ_MULTIPLE_BLOCKS	: STD_LOGIC_VECTOR(5 downto 0) := "010010";
		-- Block oriented write commands
		COMMAND_24_WRITE_BLOCK				: STD_LOGIC_VECTOR(5 downto 0) := "011000";
		COMMAND_25_WRITE_MULTIPLE_BLOCKS	: STD_LOGIC_VECTOR(5 downto 0) := "011001";
		COMMAND_27_PROGRAM_CSD				: STD_LOGIC_VECTOR(5 downto 0) := "011011";
		-- Block oriented write-protection commands
		COMMAND_28_SET_WRITE_PROTECT		: STD_LOGIC_VECTOR(5 downto 0) := "011100";
		COMMAND_29_CLEAR_WRITE_PROTECT	: STD_LOGIC_VECTOR(5 downto 0) := "011101";
		COMMAND_30_SEND_PROTECTED_GROUPS	: STD_LOGIC_VECTOR(5 downto 0) := "011110";
		-- Erase commands
		COMMAND_32_ERASE_BLOCK_START		: STD_LOGIC_VECTOR(5 downto 0) := "100000";
		COMMAND_33_ERASE_BLOCK_END			: STD_LOGIC_VECTOR(5 downto 0) := "100001";
		COMMAND_38_ERASE_SELECTED_GROUPS: STD_LOGIC_VECTOR(5 downto 0) := "100110";
		-- Block lock commands
		COMMAND_42_LOCK_UNLOCK				: STD_LOGIC_VECTOR(5 downto 0) := "101010";
		-- Command Type Settings
		COMMAND_55_APP_CMD					: STD_LOGIC_VECTOR(5 downto 0) := "110111";
		COMMAND_56_GEN_CMD					: STD_LOGIC_VECTOR(5 downto 0) := "111000";
		-- Application Specific commands - must be preceeded with command 55.
		ACOMMAND_6_SET_BUS_WIDTH			: STD_LOGIC_VECTOR(5 downto 0) := "000110";
		ACOMMAND_13_SD_STATUS				: STD_LOGIC_VECTOR(5 downto 0) := "001101";
		ACOMMAND_22_SEND_NUM_WR_BLOCKS	: STD_LOGIC_VECTOR(5 downto 0) := "010100";
		ACOMMAND_23_SET_BLK_ERASE_COUNT	: STD_LOGIC_VECTOR(5 downto 0) := "010101";
		ACOMMAND_41_SEND_OP_CONDITION		: STD_LOGIC_VECTOR(5 downto 0) := "101001";
		ACOMMAND_42_SET_CLR_CARD_DETECT	: STD_LOGIC_VECTOR(5 downto 0) := "101010";
		ACOMMAND_51_SEND_SCR					: STD_LOGIC_VECTOR(5 downto 0) := "110011";
		-- First custom_command
		FIRST_NON_PREDEFINED_COMMAND		: STD_LOGIC_VECTOR(3 downto 0) := "1010"
	);
	port
	(
		i_clock		  			: in std_logic;
		i_reset_n	  			: in std_logic;
		i_message_bit_out		: in std_logic;
		i_command_ID			: in std_logic_vector(5 downto 0);
		i_argument				: in std_logic_vector(31 downto 0);
		i_predefined_message	: in std_logic_vector(3 downto 0);
		i_generate				: in std_logic;
		i_DSR						: in std_logic_vector(15 downto 0);
		i_OCR						: in std_logic_vector(31 downto 0);
		i_RCA						: in std_logic_vector(15 downto 0);
		o_dataout				: out std_logic;
		o_message_done			: out std_logic;
		o_valid					: out std_logic;
		o_returning_ocr		: out std_logic;
		o_returning_cid		: out std_logic;
		o_returning_rca		: out std_logic;
		o_returning_csd 		: out std_logic;
		o_returning_status	: out std_logic;
		o_data_read				: out std_logic;
		o_data_write			: out std_logic;
		o_wait_cmd_busy		: out std_logic;
		o_last_cmd_was_55		: out std_logic;		
		o_response_type		: out std_logic_vector(2 downto 0)
	);

end entity;

architecture rtl of Altera_UP_SD_Card_48_bit_Command_Generator is

	component Altera_UP_SD_CRC7_Generator
	port
	(
		i_clock 		: in std_logic;
		i_enable		: in std_logic;
		i_reset_n	: in std_logic;
		i_shift		: in std_logic;
		i_datain		: in std_logic;
		o_dataout	: out std_logic;
		o_crcout		: out std_logic_vector(6 downto 0)
	);
	end component;
	
	-- Local wires
	-- REGISTERED
	signal	counter 				: std_logic_vector(6 downto 0);
	signal	last_command_id 	: std_logic_vector(5 downto 0);	
	signal	message_bits 		: std_logic_vector(39 downto 0);
	signal	last_command_sent_was_CMD55, valid 			: std_logic;
	signal	bit_to_send, sending_CRC, command_valid 	: std_logic;
	signal	returning_cid_reg, returning_rca_reg, returning_csd_reg, returning_dsr_reg, returning_ocr_reg, returning_status_reg : std_logic;	
	-- UNREGISTERED
	signal	temp_4_bits 													: std_logic_vector(3 downto 0);
	signal	message_done, CRC_generator_out, produce_next_bit	: std_logic;
	signal	app_specific_valid, regular_command_valid  			: std_logic;
	signal 	response_type, response_type_reg 						: std_logic_vector(2 downto 0);
	signal	cmd_argument 													: std_logic_vector(31 downto 0);
begin
	-- This set of bits is necessary to allow the SD card to accept a VDD level for communication.
	temp_4_bits <= "1111" when ((i_OCR(23) = '1') or (i_OCR(22) = '1') or (i_OCR(21) = '1') or (i_OCR(20) = '1')) else "0000";
	-- Generate the bits to be sent to the SD card. These bits must pass through the CRC generator
	-- to produce error checking code. The error checking code will follow the message. The message terminates with
	-- a logic '1'. Total message length is 48 bits.
	message_data_generator: process(i_clock, i_reset_n)
	begin
		if (i_reset_n = '0') then
			message_bits <= (OTHERS => '0');
		else
			if (rising_edge(i_clock)) then
				if (i_generate = '1') then
					-- Store type of a response.
					response_type_reg <= response_type;
					-- Generate a message. Please note that the predefined messages are used for initialization.
					-- If executed in sequence, they will initialize the SD card to work correctly. Only once these
					-- instructions are completed can the data transfer begin.
					case (i_predefined_message) is
						when "0000" =>
							-- Generate a predefined message - CMD0.
							message_bits <= ("01" & COMMAND_0_GO_IDLE & "00000000000000000000000000000000");
						when "0001" =>
							-- Generate a predefined message - CMD55.
							message_bits <= ("01" & COMMAND_55_APP_CMD & "0000000000000000" & "0000000000000000");
						when "0010" =>
							-- Generate a predefined message - ACMD41.
							message_bits <= ("01" & ACOMMAND_41_SEND_OP_CONDITION & "0000" & temp_4_bits & "000" & i_OCR(20) & "00000000000000000000");
						when "0011" =>
							-- Generate a predefined message - CMD2.
							message_bits <= ("01" & COMMAND_2_ALL_SEND_CID & "00000000000000000000000000000000");
						when "0100" =>
							-- Generate a predefined message - CMD3.
							message_bits <= ("01" & COMMAND_3_SEND_RCA & "00000000000000000000000000000000");
						when "0101" =>
							-- Generate a predefined message - CMD9.
							message_bits <= ("01" & COMMAND_9_SEND_CSD & i_RCA & "0000000000000000");
						when "0110" =>
							-- Generate a predefined message - CMD4.
							message_bits <= ("01" & COMMAND_4_SET_DSR & i_DSR & "0000000000000000");
						when "0111" =>
							-- Generate a predefined message - CMD16. Set block length to 512.
							message_bits <= ("01" & COMMAND_16_SET_BLOCK_LENGTH & "0000000000000000" & "0000001000000000" );
						when "1000" =>
							-- Generate a predefined message - CMD7. Select the card so we can access it's data.
							message_bits <= ("01" & COMMAND_7_SELECT_CARD & i_RCA & "0000001000000000" );
						when "1001" =>
							-- Generate a predefined message - CMD13. Send SD card status.
							message_bits <= ("01" & COMMAND_13_SEND_STATUS & i_RCA & "0000000000000000");
							
						when others =>
							-- Generate a custom message
							message_bits <= ("01" & i_command_ID & cmd_argument);
					end case;
				else
					-- Shift bits out as needed
					if (produce_next_bit = '1') then
						-- Shift message bits.
						message_bits(39 downto 1) <= message_bits(38 downto 0);
						message_bits(0) <= '0';
					end if;
				end if;
			end if;
		end if;
	end process;
	
	-- Generate command argument based on the command_ID. For most commands, the argument is user specified.
	-- For some commands, it is necessary to send a particular SD Card register contents. Hence, these contents are
	-- sent instead of the user data.
	argument_generator: process (i_command_ID, last_command_sent_was_CMD55, i_generate, i_RCA, i_DSR, i_OCR, i_argument)
	begin
		cmd_argument <= i_argument;
		if (i_generate = '1') then
			case (i_command_ID) is
				when COMMAND_4_SET_DSR =>
					cmd_argument <= i_DSR & i_argument(15 downto 0);		
				when COMMAND_7_SELECT_CARD =>
					cmd_argument <= i_RCA & i_argument(15 downto 0);
				when COMMAND_9_SEND_CSD =>
					cmd_argument <= i_RCA & i_argument(15 downto 0);
				when COMMAND_10_SEND_CID =>
					cmd_argument <= i_RCA & i_argument(15 downto 0);
				when COMMAND_13_SEND_STATUS =>
					cmd_argument <= i_RCA & i_argument(15 downto 0);
				when COMMAND_15_GO_INACTIVE =>
					cmd_argument <= i_RCA & i_argument(15 downto 0);
				when COMMAND_55_APP_CMD =>
					cmd_argument <= i_RCA & i_argument(15 downto 0);
				when ACOMMAND_41_SEND_OP_CONDITION =>
					if (last_command_sent_was_CMD55 = '1') then
						cmd_argument <= i_OCR;
					end if;
				when others =>
					cmd_argument <= i_argument;
			end case;
		end if;
	end process;

	-- Validate the message ID before sending it out.
	command_validator: process(i_clock, i_reset_n)
	begin
		if (i_reset_n = '0') then
			command_valid <= '0';
		else
			if (rising_edge(i_clock)) then
				if (i_generate = '1') then
					if (("0" & i_predefined_message) >= ("0" & FIRST_NON_PREDEFINED_COMMAND)) then
						-- Check the custom message
						if (last_command_sent_was_CMD55 = '1') then
							-- Check the application specific messages
							command_valid <= app_specific_valid;
						else
							-- Check the default messages.
							command_valid <= regular_command_valid;
						end if;
					else
						-- A command is valid if the message is predefined.
						command_valid <= '1';
					end if;
				end if;
			end if;
		end if;
	end process;
	
	-- Registers that indicate that the command sent will return contents of a control register.
	-- The contents of the response should therefore be stored in the appropriate register.
	responses_with_control_regs: process(i_clock, i_reset_n, last_command_sent_was_CMD55, last_command_id, message_done)
	begin
		if (i_reset_n = '0') then
			returning_ocr_reg <= '0';
			returning_cid_reg <= '0';
			returning_rca_reg <= '0';
			returning_csd_reg <= '0';
			returning_status_reg <= '0';
		elsif (rising_edge(i_clock)) then
				if (i_generate = '1') then
					returning_ocr_reg <= '0';
					returning_cid_reg <= '0';
					returning_rca_reg <= '0';
					returning_csd_reg <= '0';
					returning_status_reg <= '0';
				elsif (message_done = '1') then
					-- OCR
					if ((last_command_sent_was_CMD55 = '1') and (last_command_id = ACOMMAND_41_SEND_OP_CONDITION)) then
						returning_ocr_reg <= '1';
					end if;	
					 -- CID
					if (last_command_id = COMMAND_2_ALL_SEND_CID) then
						returning_cid_reg <= '1';
					end if;	
					-- RCA
					if (last_command_id = COMMAND_3_SEND_RCA) then
						returning_rca_reg <= '1';
					end if;	
					-- CSD
					if (last_command_id = COMMAND_9_SEND_CSD) then
						returning_csd_reg <= '1';
					end if;	
					-- Status
					if ((last_command_sent_was_CMD55 = '0') and (last_command_id = COMMAND_13_SEND_STATUS)) then
						returning_status_reg <= '1';
					end if;	
								
				end if;
		end if;		
	end process;
	
	-- Count the number of bits sent using a counter.
	sent_bit_counter: process(i_clock, i_reset_n, i_generate, produce_next_bit, counter)
	begin
		if (i_reset_n = '0') then
			counter <= (OTHERS => '0');
		else
			if (rising_edge(i_clock)) then
				if (i_generate = '1') then
					-- Reset the counter indicating the number of bits produced.
					counter <= "0000000";
				else
					if (produce_next_bit = '1') then
						-- Update the number of message bits sent.
						counter <= counter + '1';
					end if;
				end if;
			end if;
		end if;
	end process;
	
	-- Select the source for the output data to be either the message data or the CRC bits.
	source_selector: process(i_clock, i_reset_n, i_generate)
	begin
		if (i_reset_n = '0') then
			sending_CRC <= '0';
		else
			if (rising_edge(i_clock)) then
				if (i_generate = '1') then
					-- Set sending CRC flag to 0.
					sending_CRC <= '0';
				else
					-- If this is the last bit being sent, then bits that follow are the CRC bits.
					if (counter = "0101000") then
						sending_CRC <= '1';
					end if;
				end if;
			end if;
		end if;
	end process;
		
	-- When the message is sent, store its ID. In a special case when CMD55 is sent, the next command can be an application
	-- specific command. We need to check those command IDs to verify the validity of the message.
	CMD55_recognizer: process(i_clock, i_reset_n, i_generate, produce_next_bit, counter, message_done, last_command_id)
	begin
		if (i_reset_n = '0') then
			last_command_sent_was_CMD55 <= '0';
		else
			if (rising_edge(i_clock)) then
				if (i_generate = '0') then
					-- Store the ID of the current command.
					if (produce_next_bit = '1') then	
						if (counter = "0000000") then
							last_command_id <= message_bits(37 downto 32);
						end if;
					end if;
					-- When message has been sent then check if it was CMD55.
					if (message_done = '1') then
						if (last_command_id = COMMAND_55_APP_CMD) then
							last_command_sent_was_CMD55 <= '1';
						else
							last_command_sent_was_CMD55 <= '0';
						end if;
					end if;
				end if;
			end if;
		end if;
	end process;
	
	-- Instantiate a CRC7 generator. Message bits will pass through it to create the CRC code for the message.
	CRC7_Gen: Altera_UP_SD_CRC7_Generator PORT MAP
	(
		i_clock 		=> i_clock,
		i_reset_n 	=> i_reset_n,
		i_enable 	=> i_message_bit_out,
		i_shift		=> sending_CRC,
		i_datain 	=> message_bits(39),
		o_dataout 	=> CRC_generator_out
	);
	
	-- Define the source of the data produced by this module, depending on the counter value and the sending_CRC register state.
	data_bit_register: process(i_clock, i_reset_n, i_generate, produce_next_bit, counter)
	begin
		if (i_reset_n = '0') then
			bit_to_send <= '1';
		else
			if (rising_edge(i_clock)) then
				if (i_generate = '1') then
					bit_to_send <= '1';
				elsif (produce_next_bit = '1') then
					-- Send data to output.
					if (sending_CRC = '0') then
						-- Send message bits
						bit_to_send <= message_bits(39);
					else
						-- Send CRC bits
						if ((counter = "0101111") or (counter = "0110000")) then
							-- At the end of CRC bits put a 1.
							bit_to_send <= '1';
						else
							bit_to_send <= CRC_generator_out;
						end if;
					end if;
				end if;
			end if;
		end if;
	end process;
	
	-- Define conditions to produce the next message bit on the module output port o_dataout.
	produce_next_bit <= i_message_bit_out and (not message_done);
	-- Message is done when the last bit appears at the output. 
	message_done <= '1' when (counter = "0110001") else '0';
	-- Check the application specific messages
	app_specific_valid <= 	'1' when (
										--(i_command_ID = COMMAND_0_GO_IDLE) or
										(i_command_ID = COMMAND_2_ALL_SEND_CID) or
										(i_command_ID = COMMAND_3_SEND_RCA) or
										(i_command_ID = COMMAND_4_SET_DSR) or
										--(i_command_ID = ACOMMAND_6_SET_BUS_WIDTH) or
										--(i_command_ID = COMMAND_7_SELECT_CARD) or
										(i_command_ID = COMMAND_9_SEND_CSD) or
										(i_command_ID = COMMAND_10_SEND_CID) or
										--(i_command_ID = COMMAND_12_STOP_TRANSMISSION) or
										(i_command_ID = ACOMMAND_13_SD_STATUS) or
										--(i_command_ID = COMMAND_15_GO_INACTIVE) or
										--(i_command_ID = COMMAND_16_SET_BLOCK_LENGTH) or
										(i_command_ID = COMMAND_17_READ_BLOCK) or
										--(i_command_ID = COMMAND_18_READ_MULTIPLE_BLOCKS) or
										(i_command_ID = ACOMMAND_22_SEND_NUM_WR_BLOCKS) or
										(i_command_ID = ACOMMAND_23_SET_BLK_ERASE_COUNT) or
										(i_command_ID = COMMAND_24_WRITE_BLOCK) or
										(i_command_ID = COMMAND_25_WRITE_MULTIPLE_BLOCKS) or
										(i_command_ID = COMMAND_27_PROGRAM_CSD) or
										(i_command_ID = COMMAND_28_SET_WRITE_PROTECT) or
										(i_command_ID = COMMAND_29_CLEAR_WRITE_PROTECT) or
										(i_command_ID = COMMAND_30_SEND_PROTECTED_GROUPS) or
										(i_command_ID = COMMAND_32_ERASE_BLOCK_START) or
										(i_command_ID = COMMAND_33_ERASE_BLOCK_END) or
										(i_command_ID = COMMAND_38_ERASE_SELECTED_GROUPS) or
										(i_command_ID = ACOMMAND_41_SEND_OP_CONDITION) or
										(i_command_ID = ACOMMAND_42_SET_CLR_CARD_DETECT) or
										(i_command_ID = ACOMMAND_51_SEND_SCR) or
										(i_command_ID = COMMAND_55_APP_CMD) or
										(i_command_ID = COMMAND_56_GEN_CMD)
									 )
							else '0';
	-- Check the default messages.
	regular_command_valid <= '1' when (
										-------------------------------------------------------
										-- Disabled to prevent malfunction of the core
										-------------------------------------------------------
										--(i_command_ID = COMMAND_0_GO_IDLE) or
										--(i_command_ID = COMMAND_6_SWITCH_FUNCTION) or
										--(i_command_ID = COMMAND_7_SELECT_CARD) or
										--(i_command_ID = COMMAND_15_GO_INACTIVE) or
										--(i_command_ID = COMMAND_27_PROGRAM_CSD) or
										--(i_command_ID = COMMAND_30_SEND_PROTECTED_GROUPS) or
										--(i_command_ID = COMMAND_42_LOCK_UNLOCK) or
										-------------------------------------------------------
										(i_command_ID = COMMAND_2_ALL_SEND_CID) or
										(i_command_ID = COMMAND_3_SEND_RCA) or
										(i_command_ID = COMMAND_4_SET_DSR) or
										(i_command_ID = COMMAND_9_SEND_CSD) or
										(i_command_ID = COMMAND_10_SEND_CID) or
										(i_command_ID = COMMAND_13_SEND_STATUS) or
										-------------------------------------------------------
										-- Disabled to simplify the circuit
										-------------------------------------------------------
										--(i_command_ID = COMMAND_12_STOP_TRANSMISSION) or
										--(i_command_ID = COMMAND_16_SET_BLOCK_LENGTH) or
										--(i_command_ID = COMMAND_18_READ_MULTIPLE_BLOCKS) or
										--(i_command_ID = COMMAND_25_WRITE_MULTIPLE_BLOCKS) or
										-------------------------------------------------------
										(i_command_ID = COMMAND_17_READ_BLOCK) or
										(i_command_ID = COMMAND_24_WRITE_BLOCK) or
										(i_command_ID = COMMAND_28_SET_WRITE_PROTECT) or
										(i_command_ID = COMMAND_29_CLEAR_WRITE_PROTECT) or
										(i_command_ID = COMMAND_32_ERASE_BLOCK_START) or
										(i_command_ID = COMMAND_33_ERASE_BLOCK_END) or
										(i_command_ID = COMMAND_38_ERASE_SELECTED_GROUPS) or
										(i_command_ID = COMMAND_55_APP_CMD) or
										(i_command_ID = COMMAND_56_GEN_CMD)
									  )
							else '0';
	
	response_type <= 	"001"	when	-- Wait for type 1 response when
									(
										(i_predefined_message = "0001") or
										(i_predefined_message = "0111") or
										(i_predefined_message = "1000") or
										(i_predefined_message = "1001") or
										((i_predefined_message = FIRST_NON_PREDEFINED_COMMAND) and
										((i_command_ID = COMMAND_6_SWITCH_FUNCTION) or
										(i_command_ID = COMMAND_7_SELECT_CARD) or
										(i_command_ID = COMMAND_12_STOP_TRANSMISSION) or
										(i_command_ID = COMMAND_13_SEND_STATUS) or
										(i_command_ID = COMMAND_16_SET_BLOCK_LENGTH) or
										(i_command_ID = COMMAND_17_READ_BLOCK) or
										(i_command_ID = COMMAND_18_READ_MULTIPLE_BLOCKS) or
										(i_command_ID = COMMAND_24_WRITE_BLOCK) or
										(i_command_ID = COMMAND_25_WRITE_MULTIPLE_BLOCKS) or
										(i_command_ID = COMMAND_27_PROGRAM_CSD) or
										(i_command_ID = COMMAND_28_SET_WRITE_PROTECT) or
										(i_command_ID = COMMAND_29_CLEAR_WRITE_PROTECT) or
										(i_command_ID = COMMAND_30_SEND_PROTECTED_GROUPS) or
										(i_command_ID = COMMAND_32_ERASE_BLOCK_START) or
										(i_command_ID = COMMAND_33_ERASE_BLOCK_END) or
										(i_command_ID = COMMAND_38_ERASE_SELECTED_GROUPS) or
										(i_command_ID = COMMAND_42_LOCK_UNLOCK) or
										(i_command_ID = COMMAND_55_APP_CMD) or
										(i_command_ID = COMMAND_56_GEN_CMD) or
										((last_command_sent_was_CMD55 = '1') and
											((i_command_ID = ACOMMAND_6_SET_BUS_WIDTH) or
											(i_command_ID = ACOMMAND_13_SD_STATUS) or
											(i_command_ID = ACOMMAND_22_SEND_NUM_WR_BLOCKS) or
											(i_command_ID = ACOMMAND_23_SET_BLK_ERASE_COUNT) or
											(i_command_ID = ACOMMAND_42_SET_CLR_CARD_DETECT) or
											(i_command_ID = ACOMMAND_51_SEND_SCR)))))										
									  ) else
						"010"	when	-- Wait for type 2 response when
									(
										((i_predefined_message = FIRST_NON_PREDEFINED_COMMAND) and
										((i_command_ID = COMMAND_2_ALL_SEND_CID) or
										(i_command_ID = COMMAND_9_SEND_CSD) or
										(i_command_ID = COMMAND_10_SEND_CID))) or
										(i_predefined_message = "0011") or
										(i_predefined_message = "0101")
									) else					
						"011"	when	-- Wait for type 3 response when
									(
										((i_predefined_message = FIRST_NON_PREDEFINED_COMMAND) and (last_command_sent_was_CMD55 = '1') and (i_command_ID = ACOMMAND_41_SEND_OP_CONDITION)) or
										(i_predefined_message = "0010")
									) else
						"110"	when	-- Wait for type 6 response when
										(((i_predefined_message = FIRST_NON_PREDEFINED_COMMAND) and (i_command_ID = COMMAND_3_SEND_RCA)) or
										 (i_predefined_message = "0100"))	
						else "000"; -- Otherwise there is no response pending.

	-- Define circuit outputs
	o_message_done 		<= message_done;
	o_response_type 		<= response_type_reg;
	o_valid 					<= command_valid;
	o_dataout 				<= bit_to_send;
	o_returning_ocr 		<= returning_ocr_reg;
	o_returning_cid 		<= returning_cid_reg;
	o_returning_rca 		<= returning_rca_reg;
	o_returning_csd 		<= returning_csd_reg;
	o_returning_status 	<= returning_status_reg;	
	o_data_read				<= '1' when (last_command_id = COMMAND_17_READ_BLOCK) else '0';
	o_data_write 			<= '1' when (last_command_id = COMMAND_24_WRITE_BLOCK) else '0';
	o_last_cmd_was_55 	<= last_command_sent_was_CMD55;
	o_wait_cmd_busy		<= '1' when	(
									(last_command_id = COMMAND_7_SELECT_CARD) or
									(last_command_id = COMMAND_12_STOP_TRANSMISSION) or
									(last_command_id = COMMAND_28_SET_WRITE_PROTECT) or
									(last_command_id = COMMAND_29_CLEAR_WRITE_PROTECT) or
									(last_command_id = COMMAND_38_ERASE_SELECTED_GROUPS))
						else '0';
end rtl;