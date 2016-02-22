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
-- This is an FSM that controls the SD Card interface circuitry.
--
-- On reset, the FSM will initiate a predefined set of commands in an attempt to connect to the SD Card.
-- When successful, it will allow commands to be issued to the SD Card, otherwise it will return a signal that
-- no card is present in the SD Card slot. 
-- 
-- NOTES/REVISIONS:
----------------------------------------------------------------------------------------------------------------

library ieee;
use ieee.std_logic_1164.all;
use ieee.std_logic_arith.all;
use ieee.std_logic_unsigned.all;

entity Altera_UP_SD_Card_Control_FSM is
	generic (
		PREDEFINED_COMMAND_GET_STATUS	: STD_LOGIC_VECTOR(3 downto 0) := "1001"		
	);	
	port
	(
		-- Clock and Reset signals 
		i_clock	 						: in STD_LOGIC;
		i_reset_n						: in STD_LOGIC;
				
		-- FSM Inputs
		i_user_command_ready 		: in std_logic;
		i_response_received 			: in STD_LOGIC;
		i_response_timed_out 		: in STD_LOGIC;
		i_response_crc_passed 		: in STD_LOGIC;
		i_command_sent 				: in STD_LOGIC;
		i_powerup_busy_n 				: in STD_LOGIC;		
		i_clocking_pulse_enable 	: in std_logic;
		i_current_clock_mode 		: in std_logic;
		i_user_message_valid 		: in std_logic;
		i_last_cmd_was_55 			: in std_logic;
		i_allow_partial_rw 			: in std_logic;		
						
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

end entity;

architecture rtl of Altera_UP_SD_Card_Control_FSM is
	
	-- Build an enumerated type for the state machine. On reset always reset the DE2 and read the state
	-- of the switches.
	type state_type is (s_RESET, s_WAIT_74_CYCLES, s_GENERATE_PREDEFINED_COMMAND, s_WAIT_PREDEFINED_COMMAND_TRANSMITTED, s_WAIT_PREDEFINED_COMMAND_RESPONSE,
						s_GO_TO_NEXT_COMMAND, s_TOGGLE_CLOCK_FREQUENCY, s_AWAIT_USER_COMMAND, s_REACTIVATE_CLOCK,
						s_GENERATE_COMMAND, s_SEND_COMMAND, s_WAIT_RESPONSE, s_WAIT_FOR_CLOCK_EDGE_BEFORE_DISABLE, s_WAIT_DEASSERT,
						s_PERIODIC_STATUS_CHECK);

	-- Register to hold the current state
	signal current_state : state_type;
	signal next_state : state_type;
	
	-------------------
	-- Local signals
	-------------------
	-- REGISTERED
	signal SD_clock_mode, waiting_for_vdd_setup 	: std_logic;
	signal id_sequence_step_index 					: std_logic_vector(3 downto 0);
	signal delay_counter 								: std_logic_vector(6 downto 0);
	signal periodic_status_check 						: std_logic_vector(23 downto 0);
	-- UNREGISTERED

begin
	-- Define state transitions.
	state_transitions: process (current_state, i_command_sent, i_response_received, id_sequence_step_index,
								i_response_timed_out, i_response_crc_passed, delay_counter, waiting_for_vdd_setup,
								i_user_command_ready, i_clocking_pulse_enable, i_current_clock_mode,
								i_user_message_valid, i_last_cmd_was_55, periodic_status_check)
	begin
		case current_state is
			when s_RESET =>
				-- Reset local registers and begin identification process. 
				next_state <= s_WAIT_74_CYCLES;
			
			when s_WAIT_74_CYCLES =>
				-- Wait 74 cycles before the card can be sent commands to.
				if (delay_counter = "1001010") then
					next_state <= s_GENERATE_PREDEFINED_COMMAND;
				else
					next_state <= s_WAIT_74_CYCLES;
				end if;
				
			when s_GENERATE_PREDEFINED_COMMAND =>
				-- Generate a predefined command to the SD card. This is the identification process for the SD card.
				next_state <= s_WAIT_PREDEFINED_COMMAND_TRANSMITTED;
				
			when s_WAIT_PREDEFINED_COMMAND_TRANSMITTED =>
				-- Send a predefined command to the SD card. This is the identification process for the SD card.
				if (i_command_sent = '1') then
					next_state <= s_WAIT_PREDEFINED_COMMAND_RESPONSE;
				else
					next_state <= s_WAIT_PREDEFINED_COMMAND_TRANSMITTED;
				end if;				
			
			when s_WAIT_PREDEFINED_COMMAND_RESPONSE =>
				-- Wait for a response from SD card.
				if (i_response_received = '1') then
					if (i_response_timed_out = '1') then					
						if (waiting_for_vdd_setup = '1') then
							next_state <= s_GO_TO_NEXT_COMMAND;	
						else
							next_state <= s_RESET;
						end if;
					else
						if (i_response_crc_passed = '0') then
							next_state <= s_GENERATE_PREDEFINED_COMMAND;
						else
							next_state <= s_GO_TO_NEXT_COMMAND;
						end if;
					end if;
				else
					next_state <= s_WAIT_PREDEFINED_COMMAND_RESPONSE;
				end if;
				
			when s_GO_TO_NEXT_COMMAND =>
				-- Process the next command in the ID sequence. 
				if (id_sequence_step_index = PREDEFINED_COMMAND_GET_STATUS) then
					next_state <= s_TOGGLE_CLOCK_FREQUENCY;
				else
					next_state <= s_GENERATE_PREDEFINED_COMMAND;
				end if;
				
			when s_TOGGLE_CLOCK_FREQUENCY =>
				-- Now that the card has been initialized, increase the SD card clock frequency to 25MHz.
				-- Wait for the clock generator to switch operating mode before proceeding further.
				if (i_current_clock_mode = '1') then
					next_state <= s_AWAIT_USER_COMMAND;
				else
					next_state <= s_TOGGLE_CLOCK_FREQUENCY;
				end if;
				
			when s_AWAIT_USER_COMMAND =>
				-- Wait for the user to send a command to the SD card
				if (i_user_command_ready = '1') then
					next_state <= s_REACTIVATE_CLOCK;
				else
					-- Every 5 million cycles, or 0.1 of a second.
					if (periodic_status_check = "010011000100101101000000") then
						next_state <= s_PERIODIC_STATUS_CHECK;
					else
						next_state <= s_AWAIT_USER_COMMAND;
					end if;
				end if;		
			
			when s_PERIODIC_STATUS_CHECK =>
				-- Update status every now and then.
				next_state <= s_GENERATE_PREDEFINED_COMMAND;
			
			when s_REACTIVATE_CLOCK =>
				-- Activate the clock signal and wait 8 clock cycles.
				if (delay_counter = "0001000") then
					next_state <= s_GENERATE_COMMAND;	
				else
					next_state <= s_REACTIVATE_CLOCK;
				end if;
				
			when s_GENERATE_COMMAND =>
				-- Generate user command. If valid, proceed further. Otherwise, indicate that the command is invalid.
				if (i_user_message_valid = '0') then
					next_state <= s_WAIT_DEASSERT;
				else
					next_state <= s_SEND_COMMAND;
				end if;
				
			when s_SEND_COMMAND => 
				-- Wait for the command to be sent.
				if (i_command_sent = '1') then
					next_state <= s_WAIT_RESPONSE;
				else
					next_state <= s_SEND_COMMAND;
				end if;				
				
			when s_WAIT_RESPONSE =>
				-- Wait for the SD card to respond.
				if (i_response_received = '1') then
					if (i_response_timed_out = '1') then					
						next_state <= s_WAIT_DEASSERT;
					else
						next_state <= s_WAIT_FOR_CLOCK_EDGE_BEFORE_DISABLE;
					end if;
				else
					next_state <= s_WAIT_RESPONSE;
				end if;				
				
			when s_WAIT_FOR_CLOCK_EDGE_BEFORE_DISABLE =>
				-- Wait for a positive clock edge before you disable the clock.
				if (i_clocking_pulse_enable = '1') then
					next_state <= s_WAIT_DEASSERT;
				else
					next_state <= s_WAIT_FOR_CLOCK_EDGE_BEFORE_DISABLE;
				end if;
			
			when s_WAIT_DEASSERT =>
				-- wait for the user to release command generation request.
				if (i_user_command_ready = '1') then
					next_state <= s_WAIT_DEASSERT;
				else
					if (i_last_cmd_was_55 = '1') then
						next_state <= s_AWAIT_USER_COMMAND;
					else
						-- Send a get status command to obtain the result of sending the last command.
						next_state <= s_GENERATE_PREDEFINED_COMMAND;
					end if;
				end if;	
				
			when others =>
				-- Make sure to start in the reset state if the circuit powers up in an odd state.
				next_state <= s_RESET;
		end case;
	end process;
	
	-- State registers. 
	state_registers: process (i_clock, i_reset_n)
	begin
		if (i_reset_n = '0') then
			current_state <= s_RESET;
		elsif (rising_edge(i_clock)) then
			current_state <= next_state;
		end if;
	end process;
	
	-- Local FFs:
	local_ffs:process (	i_clock, i_reset_n, i_powerup_busy_n, current_state, 
						id_sequence_step_index, i_response_received, i_response_timed_out,
						i_allow_partial_rw)
	begin
		if (i_reset_n = '0') then
			SD_clock_mode <= '0';
			id_sequence_step_index <= (OTHERS => '0');
			periodic_status_check <= (OTHERS => '0');
			waiting_for_vdd_setup <= '0';
		elsif (rising_edge(i_clock)) then
			-- Set SD clock mode to 0 initially, thereby using a clock with frequency between 100 kHz and 400 kHz as
			-- per SD card specifications. When the card is initialized change the clock to run at 25 MHz.
			if (current_state = s_WAIT_DEASSERT) then
				periodic_status_check <= (OTHERS => '0');
			elsif (current_state = s_AWAIT_USER_COMMAND) then 
				periodic_status_check <= periodic_status_check + '1';
			end if;
			
			if (current_state = s_RESET) then
				SD_clock_mode <= '0';
			elsif (current_state = s_TOGGLE_CLOCK_FREQUENCY) then
				SD_clock_mode <= '1';
			end if;
			-- Update the ID sequence step as needed.
			if (current_state = s_RESET) then
				id_sequence_step_index <= (OTHERS => '0');
			elsif (current_state = s_GO_TO_NEXT_COMMAND) then
				if ((i_powerup_busy_n = '0') and (id_sequence_step_index = "0010")) then
					id_sequence_step_index <= "0001";
				else
					if (id_sequence_step_index = "0110") then
						if (i_allow_partial_rw = '0') then
							-- If partial read-write not allowed, then skip SET_BLK_LEN command - it will fail.
							id_sequence_step_index 	<= "1000";
						else
							id_sequence_step_index 	<= "0111";
						end if;
					else
						id_sequence_step_index 		<= id_sequence_step_index + '1';							
					end if;
				end if;
			elsif (current_state = s_WAIT_DEASSERT) then
				if (i_last_cmd_was_55 = '0') then
					-- After each command execute a get status command.
					id_sequence_step_index 	<= PREDEFINED_COMMAND_GET_STATUS;
				end if;
			elsif (current_state = s_PERIODIC_STATUS_CHECK) then
				id_sequence_step_index 		<= PREDEFINED_COMMAND_GET_STATUS;
			end if;
			
			-- Do not reset the card when SD card is having its VDD set up. Wait for it to respond, this may take some time.
			if (id_sequence_step_index = "0010") then
				waiting_for_vdd_setup <= '1';
			elsif ((id_sequence_step_index = "0011") or (current_state = s_RESET)) then
				waiting_for_vdd_setup <= '0';
			end if;
			
		end if;
	end process;

	-- Counter that counts to 74 to delay any commands.
	initial_delay_counter: process(i_clock, i_reset_n, i_clocking_pulse_enable )
	begin
		if (i_reset_n = '0') then
			delay_counter <= (OTHERS => '0');
		elsif (rising_edge(i_clock)) then
			if ((current_state = s_RESET) or (current_state = s_AWAIT_USER_COMMAND))then
				delay_counter <= (OTHERS => '0');
			elsif (((current_state = s_WAIT_74_CYCLES) or (current_state = s_REACTIVATE_CLOCK)) and
					(i_clocking_pulse_enable = '1')) then
				delay_counter <= delay_counter + '1';
			end if;
		end if;
	end process;
	
	-- FSM outputs.
	o_SD_clock_mode <= SD_clock_mode;
	o_generate_command <= '1' when ((current_state = s_GENERATE_PREDEFINED_COMMAND) or 
									(current_state = s_GENERATE_COMMAND)) 
						else '0';
	o_receive_response <= '1' when ((current_state = s_WAIT_PREDEFINED_COMMAND_RESPONSE) or
									(current_state = s_WAIT_RESPONSE))
							else '0';
	o_drive_CMD_line 		<= '1' when (	(current_state = s_WAIT_PREDEFINED_COMMAND_TRANSMITTED) or 
									(current_state = s_SEND_COMMAND)) else '0';
	o_predefined_command_ID <= id_sequence_step_index;
	o_card_connected <= '1' when (id_sequence_step_index(3) = '1') and (
									(id_sequence_step_index(2) = '1') or
									(id_sequence_step_index(1) = '1') or
									(id_sequence_step_index(0) = '1'))
						else '0';
	o_resetting 					<= '1' when (current_state = s_RESET) else '0';
	o_command_completed 			<= '1' when (current_state = s_WAIT_DEASSERT) else '0';
	o_enable_clock_generator 	<= '0' when (current_state = s_AWAIT_USER_COMMAND) else '1';
	o_clear_response_register 	<= '1' when (current_state = s_REACTIVATE_CLOCK) else '0';

end rtl;

