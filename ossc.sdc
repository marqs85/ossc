### CPU clock constraints ###

create_clock -period 27MHz -name clk27 [get_ports clk27]

set_input_delay -clock clk27 0 [get_ports {sda scl SD_CMD SD_DAT* *ALTERA_DATA0}]
set_false_path -from [get_ports {btn* ir_rx HDMI_TX_INT_N HDMI_TX_MODE}]
set_false_path -to {sys:sys_inst|sys_pio_1:pio_1|readdata*}


### Scanconverter clock constraints ###

create_clock -period 165MHz -name pclk_tvp_high [get_ports TVP_PCLK_i]
create_clock -period 33MHz -name pclk_tvp_low [get_ports TVP_PCLK_i] -add

#derive_pll_clocks
create_generated_clock -name pclk_5x -master_clock pclk_tvp_low -source {pll_pclk|altpll_component|auto_generated|pll1|inclk[1]} -multiply_by 5 -duty_cycle 50.00 {pll_pclk|altpll_component|auto_generated|pll1|clk[0]}
create_generated_clock -name pclk_27mhz -master_clock clk27 -source {pll_pclk|altpll_component|auto_generated|pll1|inclk[0]} -multiply_by 1 -duty_cycle 50.00 {pll_pclk|altpll_component|auto_generated|pll1|clk[0]} -add

# retrieve post-mapping clkmux output pin
set clkmux_output [get_pins clkctrl1|outclk]

# specify postmux clocks which clock postprocess pipeline
create_generated_clock -name pclk_1x_postmux -master_clock pclk_tvp_high -source [get_pins clkctrl1|inclk[0]] -multiply_by 1 $clkmux_output
create_generated_clock -name pclk_5x_postmux -master_clock pclk_5x -source [get_pins clkctrl1|inclk[2]] -multiply_by 1 $clkmux_output -add
create_generated_clock -name pclk_27mhz_postmux -master_clock pclk_27mhz -source [get_pins clkctrl1|inclk[2]] -multiply_by 1 $clkmux_output -add

# specify output clocks that drive PCLK output pin
set pclk_out_port [get_ports HDMI_TX_PCLK]
create_generated_clock -name pclk_1x_out -master_clock pclk_1x_postmux -source $clkmux_output -multiply_by 1 $pclk_out_port
create_generated_clock -name pclk_5x_out -master_clock pclk_5x_postmux -source $clkmux_output -multiply_by 1 $pclk_out_port -add
create_generated_clock -name pclk_27mhz_out -master_clock pclk_27mhz_postmux -source $clkmux_output -multiply_by 1 $pclk_out_port -add

derive_clock_uncertainty

# input delay constraints
set TVP_dmin 0
set TVP_dmax 1.5
set critinputs [get_ports {TVP_R_i* TVP_G_i* TVP_B_i* TVP_HS_i TVP_HSYNC_i TVP_VSYNC_i TVP_FID_i}]
foreach_in_collection c [get_clocks "pclk_tvp*"] {
    set_input_delay -clock $c -min $TVP_dmin $critinputs -add_delay
    set_input_delay -clock $c -max $TVP_dmax $critinputs -add_delay
}

# output delay constraints as documented in the IT6613 datasheet
# -- increased IT_Tsu from 1.0 to 1.5 due to #52
set IT_Tsu 1.5
set IT_Th -0.5
set critoutputs_hdmi [get_ports {HDMI_TX_RD* HDMI_TX_GD* HDMI_TX_BD* HDMI_TX_DE HDMI_TX_HS HDMI_TX_VS}]
foreach_in_collection c [get_clocks pclk_*_out] {
    set_output_delay -clock $c -min $IT_Th $critoutputs_hdmi -add
    set_output_delay -clock $c -max $IT_Tsu $critoutputs_hdmi -add
}
set_false_path -to [remove_from_collection [all_outputs] $critoutputs_hdmi]


### CPU/scanconverter clock relations ###

# Treat CPU clock asynchronous to pixel clocks
set_clock_groups -asynchronous -group \
                            {clk27} \
                            {pclk_27mhz pclk_27mhz_postmux pclk_27mhz_out} \
                            {pclk_tvp_low} \
                            {pclk_tvp_high} \
                            {pclk_1x_postmux pclk_1x_out} \
                            {pclk_5x pclk_5x_postmux pclk_5x_out}


### JTAG Signal Constraints ###

#constrain the TCK port
#create_clock -name tck -period "10MHz" [get_ports altera_reserved_tck]
#cut all paths to and from tck
set_clock_groups -exclusive -group [get_clocks altera_reserved_tck]
#constrain the TDI port
set_input_delay -clock altera_reserved_tck 20 [get_ports altera_reserved_tdi]
#constrain the TMS port
set_input_delay -clock altera_reserved_tck 20 [get_ports altera_reserved_tms]
#constrain the TDO port
#set_output_delay -clock altera_reserved_tck 20 [get_ports altera_reserved_tdo]
