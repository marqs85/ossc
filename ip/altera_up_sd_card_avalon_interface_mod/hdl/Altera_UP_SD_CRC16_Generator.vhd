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


----------------------------------------------------------------------------------------
-- This generates the necessary 16-CRC for Command and Response
-- Implementation: serial input/parallel output
-- When input stream ends, the crcout output is the CRC checksum for them
--
-- NOTES/REVISIONS:
----------------------------------------------------------------------------------------

library ieee;
use ieee.std_logic_1164.all;
use ieee.numeric_std.all;

entity Altera_UP_SD_CRC16_Generator is

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

end entity;

architecture rtl of Altera_UP_SD_CRC16_Generator is

	-- Local wires
	-- REGISTERED
	signal shift_register : std_logic_vector(15 downto 0);
begin

	process (i_clock, i_reset_n)
	begin
		if (i_reset_n = '0') then
			shift_register <= (OTHERS => '0');
		else
			if (rising_edge(i_clock)) then
				if (i_sync_reset = '1') then
					shift_register <= (OTHERS => '0');
				elsif (i_enable = '1') then
					if (i_shift = '0') then
						shift_register(0) 				<= i_datain XOR shift_register(15);
						shift_register(4 downto 1) 	<= shift_register(3 downto 0);
						shift_register(5) 				<= shift_register(4) XOR i_datain XOR shift_register(15);
						shift_register(11 downto 6) 	<= shift_register(10 downto 5);
						shift_register(12) 				<= shift_register(11) XOR i_datain XOR shift_register(15);
						shift_register(15 downto 13) 	<= shift_register(14 downto 12);
					else -- shift CRC out (no more calculation now)
						shift_register(15 downto 1) 	<= shift_register(14 downto 0);
						shift_register(0) 				<= '0'; 
					end if;
				end if;
			end if;
		end if;
	end process;

	o_dataout 	<= shift_register(15);
	o_crcout 	<= shift_register;
end rtl;



