### CPU clock constraints ###

create_clock -period 27MHz -name clk27 [get_ports clk27]

set_input_delay -clock clk27 0 [get_ports {sda scl SD_CMD SD_DAT* *ALTERA_DATA0}]
set_false_path -from [get_ports {btn* ir_rx HDMI_TX_INT_N HDMI_TX_MODE}]
set_false_path -to {sys:sys_inst|sys_pio_1:pio_1|readdata*}


### Scanconverter clock constraints ###

create_clock -period 108MHz -name pclk_hdtv [get_ports PCLK_in]
create_clock -period 27MHz -name pclk_sdtv [get_ports PCLK_in] -add

#derive_pll_clocks
create_generated_clock -master_clock pclk_sdtv -source {scanconverter_inst|pll_linedouble|altpll_component|auto_generated|pll1|inclk[0]} -multiply_by 2 -duty_cycle 50.00 -name pclk_2x {scanconverter_inst|pll_linedouble|altpll_component|auto_generated|pll1|clk[0]}
create_generated_clock -master_clock pclk_sdtv -source {scanconverter_inst|pll_linetriple|altpll_component|auto_generated|pll1|inclk[0]} -multiply_by 3 -duty_cycle 50.00 -name pclk_3x {scanconverter_inst|pll_linetriple|altpll_component|auto_generated|pll1|clk[0]}
create_generated_clock -master_clock pclk_sdtv -source {scanconverter_inst|pll_linetriple|altpll_component|auto_generated|pll1|inclk[0]} -multiply_by 4 -duty_cycle 50.00 -name pclk_4x {scanconverter_inst|pll_linetriple|altpll_component|auto_generated|pll1|clk[1]}
create_generated_clock -master_clock pclk_sdtv -source {scanconverter_inst|pll_linedouble|altpll_component|auto_generated|pll1|inclk[0]} -multiply_by 5 -duty_cycle 50.00 -name pclk_5x {scanconverter_inst|pll_linedouble|altpll_component|auto_generated|pll1|clk[1]}

derive_clock_uncertainty

# input delay constraints
set TVP_dmin 0
set TVP_dmax 1.5
set critinputs [get_ports {R_in* G_in* B_in* HSYNC_in VSYNC_in FID_in}]
set_input_delay -clock pclk_hdtv -min $TVP_dmin $critinputs
set_input_delay -clock pclk_hdtv -max $TVP_dmax $critinputs
set_input_delay -clock pclk_sdtv -min $TVP_dmin $critinputs -add_delay
set_input_delay -clock pclk_sdtv -max $TVP_dmax $critinputs -add_delay

# output delay constraints
set IT_Tsu 1.0
set IT_Th -0.5
set critoutputs_hdmi [get_ports {HDMI_TX_RD* HDMI_TX_GD* HDMI_TX_BD* HDMI_TX_DE HDMI_TX_HS HDMI_TX_VS}]
set_output_delay -reference_pin HDMI_TX_PCLK -clock pclk_hdtv -min $IT_Th $critoutputs_hdmi
set_output_delay -reference_pin HDMI_TX_PCLK -clock pclk_hdtv -max $IT_Tsu $critoutputs_hdmi
set_output_delay -reference_pin HDMI_TX_PCLK -clock pclk_2x -min $IT_Th $critoutputs_hdmi -add_delay
set_output_delay -reference_pin HDMI_TX_PCLK -clock pclk_2x -max $IT_Tsu $critoutputs_hdmi -add_delay
set_output_delay -reference_pin HDMI_TX_PCLK -clock pclk_3x -min $IT_Th $critoutputs_hdmi -add_delay
set_output_delay -reference_pin HDMI_TX_PCLK -clock pclk_3x -max $IT_Tsu $critoutputs_hdmi -add_delay
set_output_delay -reference_pin HDMI_TX_PCLK -clock pclk_4x -min $IT_Th $critoutputs_hdmi -add_delay
set_output_delay -reference_pin HDMI_TX_PCLK -clock pclk_4x -max $IT_Tsu $critoutputs_hdmi -add_delay
set_output_delay -reference_pin HDMI_TX_PCLK -clock pclk_5x -min $IT_Th $critoutputs_hdmi -add_delay
set_output_delay -reference_pin HDMI_TX_PCLK -clock pclk_5x -max $IT_Tsu $critoutputs_hdmi -add_delay
set_false_path -to [remove_from_collection [all_outputs] $critoutputs_hdmi]


### CPU/scanconverter clock relations ###

# Set hdtv pixel clock group as exclusive
set_clock_groups -exclusive -group {pclk_hdtv}

# Treat CPU clock asynchronous to pixel clocks 
set_clock_groups -asynchronous -group {clk27}

# Ignore following clock transfers
set_false_path -from [get_clocks pclk_2x] -to [get_clocks {pclk_sdtv pclk_3x pclk_4x pclk_5x}]
set_false_path -from [get_clocks pclk_3x] -to [get_clocks {pclk_sdtv pclk_2x pclk_4x pclk_5x}]
set_false_path -from [get_clocks pclk_4x] -to [get_clocks {pclk_sdtv pclk_2x pclk_3x pclk_5x}]
set_false_path -from [get_clocks pclk_5x] -to [get_clocks {pclk_sdtv pclk_2x pclk_3x pclk_4x}]

# Ignore paths which would result from pclk_act switchover during postprocess chain
set pclk_act_regs [get_cells {scanconverter:scanconverter_inst|R_out* \
                            scanconverter:scanconverter_inst|G_out* \
                            scanconverter:scanconverter_inst|B_out* \
                            scanconverter:scanconverter_inst|HSYNC_out* \
                            scanconverter:scanconverter_inst|VSYNC_out* \
                            scanconverter:scanconverter_inst|DE_out* \
                            scanconverter:scanconverter_inst|*_pp1* \
                            scanconverter:scanconverter_inst|*_pp2* \
                            scanconverter:scanconverter_inst|*_pp3*}]
set_false_path -from [get_clocks {pclk_sdtv}] -to $pclk_act_regs
set_false_path -from [get_clocks {pclk_sdtv}] -to [get_ports HDMI_TX_*]

# Ignore paths from registers which are updated only at leading edge of vsync
set_false_path -from [get_cells {scanconverter_inst|H_* scanconverter_inst|V_* scanconverter_inst|X_* scanconverter_inst|FID_1x}]

# Ignore paths from registers which are updated only at leading edge of hsync
#set_false_path -from [get_cells {scanconverter:scanconverter_inst|line_idx scanconverter:scanconverter_inst|line_out_idx* scanconverter:scanconverter_inst|hmax*}]


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
