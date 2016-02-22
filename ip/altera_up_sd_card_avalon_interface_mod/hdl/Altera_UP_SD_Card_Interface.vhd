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
-- This module is an interface to the Secure Data Card. This module is intended to be
-- used with the DE2 board.
--
-- This version of the interface supports only a 1-bit serial data transfer. This
-- allows the interface to support a MultiMedia card as well.
--
-- NOTES/REVISIONS:
-------------------------------------------------------------------------------------
library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity Altera_UP_SD_Card_Interface is

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

end entity;

architecture rtl of Altera_UP_SD_Card_Interface is
	
	component Altera_UP_SD_Card_Clock

	port
	(
		i_clock		  		: in std_logic;
		i_reset_n	  		: in std_logic;
		i_enable				: in std_logic;
		i_mode 				: in std_logic; -- 0 for card identification mode, 1 for data transfer mode.
		o_SD_clock			: out std_logic;
		o_clock_mode		: out std_logic;
		o_trigger_receive	: out std_logic;
		o_trigger_send		: out std_logic
	);

	end component;
	
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

	component Altera_UP_SD_CRC16_Generator
	port
	(
		i_clock 		: in std_logic;
		i_enable		: in std_logic;
		i_reset_n	: in std_logic;
		i_shift		: in std_logic;
		i_datain		: in std_logic;
		o_dataout	: out std_logic;
		o_crcout		: out std_logic_vector(15 downto 0)
	);
	end component;	

	component Altera_UP_SD_Signal_Trigger
	port
	(
		i_clock 		: in std_logic;
		i_reset_n	: in std_logic;
		i_signal		: in std_logic;
		o_trigger	: out std_logic
	);
	end component;
	
	component Altera_UP_SD_Card_48_bit_Command_Generator
	generic (
		-- Basic commands
		COMMAND_0_GO_IDLE 					: STD_LOGIC_VECTOR(5 downto 0) := "000000";
		COMMAND_2_ALL_SEND_CID 				: STD_LOGIC_VECTOR(5 downto 0) := "000010";
		COMMAND_3_SEND_RCA 					: STD_LOGIC_VECTOR(5 downto 0) := "000011";
		COMMAND_4_SET_DSR 					: STD_LOGIC_VECTOR(5 downto 0) := "000100";
		COMMAND_6_SWITCH_FUNCTION			: STD_LOGIC_VECTOR(5 downto 0) := "000110";
		COMMAND_7_SELECT_CARD 				: STD_LOGIC_VECTOR(5 downto 0) := "000111";
		COMMAND_9_SEND_CSD 					: STD_LOGIC_VECTOR(5 downto 0) := "001001";
		COMMAND_10_SEND_CID	 				: STD_LOGIC_VECTOR(5 downto 0) := "001010";
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
		COMMAND_38_ERASE_SELECTED_GROUPS	: STD_LOGIC_VECTOR(5 downto 0) := "100110";
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
	end component;	
	
	component Altera_UP_SD_Card_Response_Receiver
	generic (
		TIMEOUT				: std_logic_vector(7 downto 0) := "00111000";
		BUSY_WAIT			: std_logic_vector(7 downto 0) := "00110000";		
		PROCESSING_DELAY	: std_logic_vector(7 downto 0) := "00001000"
	);
	port
	(
		i_clock		  		: in 	std_logic;
		i_reset_n	  		: in 	std_logic;
		i_begin				: in 	std_logic;
		i_scan_pulse		: in 	std_logic;
		i_datain				: in 	std_logic;
		i_wait_cmd_busy	: in 	std_logic;
		i_response_type	: in 	std_logic_vector(2 downto 0);
		o_data				: out std_logic_vector(127 downto 0);
		o_CRC_passed		: out std_logic;
		o_timeout			: out std_logic;
		o_done				: out std_logic
	);
	end component;
	
	component Altera_UP_SD_Card_Control_FSM
	generic (
		PREDEFINED_COMMAND_GET_STATUS	: STD_LOGIC_VECTOR(3 downto 0) := "1001"		
	);	
	port
	(
		-- Clock and Reset signals 
		i_clock	 	: in STD_LOGIC;
		i_reset_n	: in STD_LOGIC;
				
		-- FSM Inputs
		i_user_command_ready 		: in std_logic;		
		i_response_received 			: in STD_LOGIC;
		i_response_timed_out		 	: in STD_LOGIC;
		i_response_crc_passed 		: in STD_LOGIC;
		i_command_sent 				: in STD_LOGIC;
		i_powerup_busy_n 				: in STD_LOGIC;
		i_clocking_pulse_enable 	: in std_logic;
		i_current_clock_mode 		: in std_logic;
		i_user_message_valid 		: in std_logic;		
		i_last_cmd_was_55 			: in std_logic;
		i_allow_partial_rw		 	: in std_logic;				

		-- FSM Outputs
		o_generate_command 			: out STD_LOGIC;			  			
		o_predefined_command_ID 	: out STD_LOGIC_VECTOR(3 downto 0);
		o_receive_response 			: out STD_LOGIC;
		o_drive_CMD_line 				: out STD_LOGIC;
		o_SD_clock_mode 				: out STD_LOGIC; -- 0 means slow clock for card identification, 1 means fast clock for transfer mode.
		o_resetting						: out std_logic;
		o_card_connected 				: out STD_LOGIC;
		o_command_completed 			: out std_logic;
		o_clear_response_register 	: out std_logic;
		o_enable_clock_generator 	: out std_logic
	);
	end component;
	
	component Altera_UP_SD_Card_Buffer
	generic (
		TIMEOUT		: std_logic_vector(15 downto 0) := "1111111111111111";
		BUSY_WAIT	: std_logic_vector(15 downto 0) := "0000001111110000"		
	);
	port
	(
		i_clock		  	: in std_logic;
		i_reset_n	  	: in std_logic;

		-- 1 bit port to transmit and receive data on the data line.
		i_begin							: in 	std_logic;
		i_sd_clock_pulse_trigger	: in 	std_logic;
		i_transmit						: in 	std_logic;
		i_1bit_data_in 				: in 	std_logic;
		o_1bit_data_out 				: out std_logic;
		o_operation_complete			: out std_logic;
		o_crc_passed					: out std_logic;
		o_timed_out						: out std_logic;
		o_dat_direction				: out std_logic; -- set to 1 to send data, set to 0 to receive it.
		
		-- 16 bit port to be accessed by a user circuit.
		i_enable_16bit_port 			: in 	std_logic;
		i_address_16bit_port			: in 	std_logic_vector(7 downto 0);
		i_write_16bit					: in 	std_logic;
		i_16bit_data_in 				: in 	std_logic_vector(15 downto 0);
		o_16bit_data_out 				: out std_logic_vector(15 downto 0)
	);
	end component;	
	
	-- Local wires
	-- REGISTERED
	signal sd_mode : std_logic;
	-- SD Card Registers:
	signal SD_REG_card_identification_number 		: std_logic_vector(127 downto 0);
	signal SD_REG_response_R1 							: std_logic_vector(31 downto 0);
	signal SD_REG_relative_card_address 			: std_logic_vector(15 downto 0);
	signal SD_REG_driver_stage_register 			: std_logic_vector(15 downto 0);
	signal SD_REG_card_specific_data 				: std_logic_vector(127 downto 0);
	signal SD_REG_operating_conditions_register 	: std_logic_vector(31 downto 0);
	signal SD_REG_status_register 					: std_logic_vector(31 downto 0);
	signal SD_REG_status_register_valid 			: std_logic;
	-- UNREGISTERED
	signal data_from_buffer 							: std_logic_vector(15 downto 0);
	signal clock_generator_mode, enable_generator, SD_clock, create_message 			: std_logic;
	signal send_next_bit, receive_next_bit 		: std_logic;
	signal timed_out, response_done, passed_crc, begin_reading_response, resetting 	: std_logic;
	signal returning_cid, returning_rca, returning_csd, returning_ocr 					: std_logic;
	signal response_type 								: std_logic_vector(2 downto 0);
	signal message_valid, messange_sent, data_to_CMD_line, CMD_tristate_buffer_enable, message_sent	: std_logic;
	signal predef_message_ID 							: std_logic_vector(3 downto 0);
	signal receive_data_out 							: std_logic_vector(127 downto 0);
	signal data_line_done, data_line_crc, data_line_timeout, data_line_direction, data_line_out 		: std_logic;
	signal data_read, data_write, wait_cmd_busy, clear_response_register 				: std_logic;
	signal response_done_combined 					: std_logic;
	signal timeout_combined 							: std_logic;
	signal crc_combined, allow_partial_rw 			: std_logic;
	signal begin_data_line_operations, last_cmd_was_55, message_sent_trigger, returning_status 		: std_logic;
	signal data_line_sd_clock_pulse_trigger			: std_logic;
begin
	-- Glue logic
	SD_REG_driver_stage_register <= (OTHERS => '0');
	response_done_combined <= (response_done and (not data_read) and (not data_write)) or
							  (response_done and (data_read or data_write) and data_line_done);
	timeout_combined <= (timed_out and (not data_read) and (not data_write)) or
						(timed_out and (data_read or data_write) and data_line_timeout);
	crc_combined <= (passed_crc and (not data_read) and (not data_write)) or
					(passed_crc and (data_read or data_write) and data_line_crc);
	begin_data_line_operations <= (data_read and message_sent) or (data_write and response_done);
	
	-- Partial read and write are only allowed when both bit 79 (partial read allowed) is high and
	-- bit 21 (partial write allowed) is high.
	allow_partial_rw <= SD_REG_card_specific_data(79) and SD_REG_card_specific_data(21);

	-- SD Card control registers
	control_regs: process (i_clock, i_reset_n)
	begin
		if (i_reset_n = '0') then
			SD_REG_operating_conditions_register 	<= (OTHERS => '0');
			SD_REG_card_identification_number 		<= (OTHERS => '0');
			SD_REG_relative_card_address 				<= (OTHERS => '0');
			SD_REG_card_specific_data 					<= (OTHERS => '0');
			SD_REG_status_register 						<= (OTHERS => '0');
			SD_REG_response_R1 							<= (OTHERS => '1');
			SD_REG_status_register_valid 				<= '0';
		elsif (rising_edge(i_clock)) then
			if ((response_type = "001") and (response_done = '1') and (returning_status = '0') and (clear_response_register = '0')) then
				SD_REG_response_R1 <= receive_data_out(31 downto 0);
			elsif (clear_response_register = '1') then
				SD_REG_response_R1 <= (OTHERS => '1');
			end if;
			if (resetting = '1') then
				SD_REG_operating_conditions_register <= (OTHERS => '0');
			elsif ((returning_ocr = '1') and (passed_crc = '1') and (response_done = '1') and (timed_out = '0')) then
				SD_REG_operating_conditions_register <= receive_data_out(31 downto 0);
			end if;
			if ((returning_cid = '1') and (passed_crc = '1') and (response_done = '1') and (timed_out = '0')) then
				SD_REG_card_identification_number <= receive_data_out;
			end if;
			if ((returning_rca = '1') and (passed_crc = '1') and (response_done = '1') and (timed_out = '0')) then
				SD_REG_relative_card_address <= receive_data_out(31 downto 16);
			end if;
			if ((returning_csd = '1') and (passed_crc = '1') and (response_done = '1') and (timed_out = '0')) then
				SD_REG_card_specific_data <= receive_data_out;
			end if;
			if (message_sent_trigger = '1') then
				SD_REG_status_register_valid <= '0';
			elsif ((returning_status = '1') and (passed_crc = '1') and (response_done = '1') and (timed_out = '0')) then
				SD_REG_status_register <= receive_data_out(31 downto 0);
				SD_REG_status_register_valid <= '1';
			end if;			
		end if;
	end process;
	
	-- Instantiated components 
	command_generator: Altera_UP_SD_Card_48_bit_Command_Generator PORT MAP
	(
		i_clock					=> i_clock,
		i_reset_n 				=> i_reset_n,
		i_message_bit_out 	=> send_next_bit,
		i_command_ID 			=> i_command_ID,
		i_argument 				=> i_argument,
		i_predefined_message => predef_message_ID,
		i_generate 				=> create_message,
		i_DSR 					=> SD_REG_driver_stage_register,
		i_OCR 					=> SD_REG_operating_conditions_register,
		i_RCA 					=> SD_REG_relative_card_address,
		o_dataout 				=> data_to_CMD_line,
		o_message_done 		=> message_sent,
		o_valid					=> message_valid,
		o_returning_ocr		=> returning_ocr,
		o_returning_cid 		=> returning_cid,
		o_returning_rca 		=> returning_rca,
		o_returning_csd 		=> returning_csd,
		o_returning_status 	=> returning_status,
		o_data_read				=> data_read,
		o_data_write 			=> data_write,
		o_wait_cmd_busy		=> wait_cmd_busy,
		o_last_cmd_was_55 	=> last_cmd_was_55,
		o_response_type 		=> response_type
	);
	
	response_receiver: Altera_UP_SD_Card_Response_Receiver PORT MAP
	(
		i_clock					=> i_clock,
		i_reset_n 				=> i_reset_n,
		i_begin					=> begin_reading_response,
		i_scan_pulse 			=> receive_next_bit,
		i_datain 				=> b_SD_cmd,
		i_response_type		=> response_type,
		i_wait_cmd_busy 		=> wait_cmd_busy,
		o_data 					=> receive_data_out,
		o_CRC_passed 			=> passed_crc,
		o_timeout 				=> timed_out,
		o_done 					=> response_done
	);
	
	control_FSM: Altera_UP_SD_Card_Control_FSM PORT MAP
	(
		-- Clock and Reset signals 
		i_clock						=> i_clock,
		i_reset_n 					=> i_reset_n,
				
		-- FSM Inputs
		i_user_command_ready 	=> i_user_command_ready,
		i_clocking_pulse_enable => receive_next_bit,
		i_response_received 		=> response_done_combined,
		i_response_timed_out 	=> timeout_combined,
		i_response_crc_passed 	=> crc_combined,
		i_command_sent 			=> message_sent,
		i_powerup_busy_n 			=> SD_REG_operating_conditions_register(31),
		i_current_clock_mode 	=> clock_generator_mode,
		i_user_message_valid 	=> message_valid,
		i_last_cmd_was_55 		=> last_cmd_was_55,
		i_allow_partial_rw 		=> allow_partial_rw,
		
		-- FSM Outputs
		o_generate_command 			=> create_message,
		o_predefined_command_ID 	=> predef_message_ID,
		o_receive_response 			=> begin_reading_response,
		o_drive_CMD_line 				=> CMD_tristate_buffer_enable,
		o_SD_clock_mode 				=> sd_mode, -- 0 means slow clock for card identification, 1 means fast clock for transfer mode.
		o_card_connected 				=> o_card_connected,
		o_command_completed 			=> o_command_completed,	
		o_resetting 					=> resetting,
		o_clear_response_register 	=> clear_response_register,
		o_enable_clock_generator 	=> enable_generator
	);
	
	clock_generator: Altera_UP_SD_Card_Clock PORT MAP
	(
		i_clock				=> i_clock,
		i_reset_n 			=> i_reset_n,
		i_mode 				=> sd_mode,
		i_enable 			=> enable_generator,
		o_SD_clock 			=> SD_clock,
		o_clock_mode 		=> clock_generator_mode,
		o_trigger_receive => receive_next_bit,
		o_trigger_send 	=> send_next_bit
	);
	
	SD_clock_pulse_trigger: Altera_UP_SD_Signal_Trigger PORT MAP
	(
		i_clock 		=> i_clock,
		i_reset_n 	=> i_reset_n,
		i_signal 	=> message_sent,
		o_trigger 	=> message_sent_trigger
	);
	
	data_line: Altera_UP_SD_Card_Buffer
	port map
	(
		i_clock 		=> i_clock,
		i_reset_n 	=> i_reset_n,

		-- 1 bit port to transmit and receive data on the data line.
		i_begin							=> begin_data_line_operations,
		i_sd_clock_pulse_trigger 	=> data_line_sd_clock_pulse_trigger,
		i_transmit 						=> data_write,
		i_1bit_data_in 				=> b_SD_dat,
		o_1bit_data_out 				=> data_line_out,
		o_operation_complete 		=> data_line_done,
		o_crc_passed 					=> data_line_crc,
		o_timed_out						=> data_line_timeout,
		o_dat_direction 				=> data_line_direction,
		
		-- 16 bit port to be accessed by a user circuit.
		i_enable_16bit_port 			=> i_buffer_enable,
		i_address_16bit_port 		=> i_buffer_address,
		i_write_16bit 					=> i_buffer_write,
		i_16bit_data_in 				=> i_buffer_data_in,
		o_16bit_data_out 				=> data_from_buffer
	);
	data_line_sd_clock_pulse_trigger <= (data_write and send_next_bit) or ((not data_write) and receive_next_bit);
	
	-- Buffer output registers.
	buff_regs: process(i_clock, i_reset_n, data_from_buffer)
	begin
		if (i_reset_n = '0') then
			o_buffer_data_out <= (OTHERS=> '0');
		elsif (rising_edge(i_clock)) then
			o_buffer_data_out <= data_from_buffer;
		end if;
	end process;
	
	-- Circuit outputs.
	o_command_valid 		<= message_valid;
	o_command_timed_out	<= timeout_combined;
	o_command_crc_failed <= not crc_combined;
	o_SD_clock 				<= SD_clock;
	b_SD_cmd 				<= data_to_CMD_line when (CMD_tristate_buffer_enable = '1') else 'Z';
	b_SD_dat 				<= data_line_out when (data_line_direction = '1') else 'Z';
	b_SD_dat3 				<= 'Z'; -- Set SD card to SD mode.
	-- SD card registers
	o_SD_REG_card_identification_number 	<= SD_REG_card_identification_number;
	o_SD_REG_relative_card_address 			<= SD_REG_relative_card_address;
	o_SD_REG_operating_conditions_register <= SD_REG_operating_conditions_register;
	o_SD_REG_card_specific_data 				<= SD_REG_card_specific_data;
	o_SD_REG_status_register 					<= SD_REG_status_register;
	o_SD_REG_response_R1 						<= SD_REG_response_R1;	
	o_SD_REG_status_register_valid 			<= SD_REG_status_register_valid;		

end rtl;
