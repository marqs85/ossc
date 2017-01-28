### CPU clock constraints ###

create_clock -period 27MHz -name clk27 [get_ports clk27]

set_input_delay -clock clk27 0 [get_ports {sda scl SD_CMD SD_DAT* *ALTERA_DATA0}]
set_false_path -from [get_ports {btn* ir_rx HDMI_TX_INT_N HDMI_TX_MODE}]
set_false_path -to {sys:sys_inst|sys_pio_1:pio_1|readdata*}


### Scanconverter clock constraints ###

create_clock -period 108MHz -name pclk_hdtv [get_ports PCLK_in]
create_clock -period 13.5MHz -name pclk_sdtv_L2 [get_ports PCLK_in] -add
create_clock -period 27MHz -name pclk_sdtv_L3 [get_ports PCLK_in] -add
create_clock -period 27MHz -name pclk_sdtv_L4 [get_ports PCLK_in] -add
create_clock -period 16MHz -name pclk_sdtv_L5 [get_ports PCLK_in] -add

#derive_pll_clocks
create_generated_clock -master_clock pclk_sdtv_L2 -source {scanconverter_inst|pll_linedouble|altpll_component|auto_generated|pll1|inclk[0]} -multiply_by 2 -duty_cycle 50.00 -name pclk_2x {scanconverter_inst|pll_linedouble|altpll_component|auto_generated|pll1|clk[0]}
create_generated_clock -master_clock pclk_sdtv_L3 -source {scanconverter_inst|pll_linetriple|altpll_component|auto_generated|pll1|inclk[0]} -multiply_by 3 -duty_cycle 50.00 -name pclk_3x {scanconverter_inst|pll_linetriple|altpll_component|auto_generated|pll1|clk[0]}
create_generated_clock -master_clock pclk_sdtv_L4 -source {scanconverter_inst|pll_linetriple|altpll_component|auto_generated|pll1|inclk[0]} -multiply_by 4 -duty_cycle 50.00 -name pclk_4x {scanconverter_inst|pll_linetriple|altpll_component|auto_generated|pll1|clk[1]}
create_generated_clock -master_clock pclk_sdtv_L5 -source {scanconverter_inst|pll_linedouble|altpll_component|auto_generated|pll1|inclk[0]} -multiply_by 5 -duty_cycle 50.00 -name pclk_5x {scanconverter_inst|pll_linedouble|altpll_component|auto_generated|pll1|clk[1]}

derive_clock_uncertainty

# input delay constraints
set TVP_dmin 0
set TVP_dmax 1.5
set critinputs [get_ports {R_in* G_in* B_in* HSYNC_in VSYNC_in FID_in}]
set_input_delay -clock pclk_hdtv -min $TVP_dmin $critinputs
set_input_delay -clock pclk_hdtv -max $TVP_dmax $critinputs
set_input_delay -clock pclk_sdtv_L2 -min $TVP_dmin $critinputs -add_delay
set_input_delay -clock pclk_sdtv_L2 -max $TVP_dmax $critinputs -add_delay
set_input_delay -clock pclk_sdtv_L3 -min $TVP_dmin $critinputs -add_delay
set_input_delay -clock pclk_sdtv_L3 -max $TVP_dmax $critinputs -add_delay
set_input_delay -clock pclk_sdtv_L4 -min $TVP_dmin $critinputs -add_delay
set_input_delay -clock pclk_sdtv_L4 -max $TVP_dmax $critinputs -add_delay
set_input_delay -clock pclk_sdtv_L5 -min $TVP_dmin $critinputs -add_delay
set_input_delay -clock pclk_sdtv_L5 -max $TVP_dmax $critinputs -add_delay

# output delay constraints (TODO: add vsync)
set IT_Tsu 1.0
set IT_Th -0.5
#todo VS
set critoutputs_hdmi {HDMI_TX_RD* HDMI_TX_GD* HDMI_TX_BD* HDMI_TX_DE HDMI_TX_HS}
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

# Set pixel clocks as exclusive clocks
set_clock_groups -exclusive \
-group {pclk_hdtv} \
-group {pclk_sdtv_L2 pclk_2x} \
-group {pclk_sdtv_L3 pclk_3x} \
-group {pclk_sdtv_L4 pclk_4x} \
-group {pclk_sdtv_L5 pclk_5x}

# Treat CPU clock asynchronous to pixel clocks 
set_clock_groups -asynchronous -group {clk27}

# Filter out impossible output mux combinations
set clkmuxregs [get_cells {scanconverter:scanconverter_inst|R_out* scanconverter:scanconverter_inst|G_out* scanconverter:scanconverter_inst|B_out* scanconverter:scanconverter_inst|HSYNC_out* scanconverter:scanconverter_inst|VSYNC_out* scanconverter:scanconverter_inst|DE_out* scanconverter:scanconverter_inst|*_pp1* scanconverter:scanconverter_inst|*_pp2*}]
set clkmuxnodes [get_pins {scanconverter_inst|linebuf_*|altsyncram_*|auto_generated|ram_*|portbaddr*}]
set_false_path -from [get_clocks {pclk_sdtv_L2 pclk_sdtv_L3 pclk_sdtv_L4 pclk_sdtv_L5}] -through $clkmuxregs

# Ignore paths from registers which are updated only at the end of vsync
set_false_path -from [get_cells {scanconverter_inst|H_* scanconverter_inst|V_* scanconverter:scanconverter_inst|lines_*}]

# Ignore paths from registers which are updated only at the end of hsync
set_false_path -from [get_cells {scanconverter:scanconverter_inst|vcnt_* scanconverter:scanconverter_inst|line_idx scanconverter:scanconverter_inst|line_out_idx* scanconverter:scanconverter_inst|HSYNC_start*}]

# Ignore paths to registers which do not drive critical logic
set_false_path -to [get_cells {scanconverter:scanconverter_inst|line_out_idx*}]

# Ignore following clock transfers
set_false_path -from [get_clocks pclk_2x] -to [get_clocks pclk_sdtv_L2]
set_false_path -from [get_clocks pclk_3x] -to [get_clocks {pclk_sdtv_L3}]
set_false_path -from [get_clocks pclk_4x] -to [get_clocks {pclk_sdtv_L4}]
set_false_path -from [get_clocks pclk_5x] -to [get_clocks {pclk_sdtv_L5}]


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
