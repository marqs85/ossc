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
-- This module is a clock generator for the SD card interface. It takes a 50 MHz
-- clock as input and produces a clock signal that depends on the mode in which the
-- SD card interface is in. For a card identification mode a clock with a frequency of
-- 390.625 kHz is generated. For the data transfer mode, a clock with a frequency of
-- 12.5MHz is generated. 
--
-- In addition, the generator produces a clock_mode value that identifies the frequency
-- of the o_SD_clock that is currently being generated.
--
-- NOTES/REVISIONS:
-------------------------------------------------------------------------------------
library ieee;
use ieee.std_logic_1164.all;
use ieee.std_logic_arith.all;
use ieee.std_logic_unsigned.all;

entity Altera_UP_SD_Card_Clock is

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

end entity;

architecture rtl of Altera_UP_SD_Card_Clock is

	-- Local wires
	-- REGISTERED
	signal	counter 	: std_logic_vector(6 downto 0);
	signal  local_mode : std_logic;
	-- UNREGISTERED
begin
	process(i_clock, i_reset_n)
	begin
		if (i_reset_n = '0') then
			counter 		<= (OTHERS => '0');
			local_mode 	<= '0';
		else
			if (rising_edge(i_clock)) then
				if (i_enable = '1') then
					counter <= counter + '1';
				end if;
				-- Change the clock pulse only when at the positive edge of the clock
				if (counter = "1000000") then
					local_mode <= i_mode;
				end if;
			end if;
		end if;
	end process;
	
	o_clock_mode 	<= local_mode;
	o_SD_clock 		<= counter(6) when (local_mode = '0') else counter(1);
	o_trigger_receive <= '1'				when ((local_mode = '0') and (counter = "0111111")) else
						((not counter(1)) and (counter(0)))	when (local_mode = '1') else '0';		
	o_trigger_send <= '1'				when ((local_mode = '0') and (counter = "0011111")) else
						((counter(1)) and (counter(0)))		when (local_mode = '1') else '0';		

end rtl;