# 
# request TCL package from ACDS 16.1
# 
package require -exact qsys 16.1

# 
# module
# 
set_module_property DESCRIPTION "Scanconverter config"
set_module_property NAME sc_config
#set_module_property VERSION 18.0
set_module_property INTERNAL false
set_module_property OPAQUE_ADDRESS_MAP true
set_module_property GROUP "Interface Protocols"
set_module_property AUTHOR ""
set_module_property DISPLAY_NAME sc_config
set_module_property INSTANTIATE_IN_SYSTEM_MODULE true
set_module_property EDITABLE true
set_module_property REPORT_TO_TALKBACK false
set_module_property ALLOW_GREYBOX_GENERATION false
set_module_property REPORT_HIERARCHY false

# 
# file sets
# 
add_fileset QUARTUS_SYNTH QUARTUS_SYNTH "" ""
set_fileset_property QUARTUS_SYNTH TOP_LEVEL sc_config_top
set_fileset_property QUARTUS_SYNTH ENABLE_RELATIVE_INCLUDE_PATHS false
set_fileset_property QUARTUS_SYNTH ENABLE_FILE_OVERWRITE_MODE false
add_fileset_file sc_config_top.sv VERILOG PATH sc_config_top.sv

add_fileset SIM_VERILOG SIM_VERILOG "" ""
set_fileset_property SIM_VERILOG ENABLE_RELATIVE_INCLUDE_PATHS false
set_fileset_property SIM_VERILOG ENABLE_FILE_OVERWRITE_MODE false
set_fileset_property SIM_VERILOG TOP_LEVEL sc_config_top
add_fileset_file sc_config_top.sv VERILOG PATH sc_config_top.sv

# 
# parameters
# 


# 
# display items
# 


# 
# connection point clock_sink
# 
add_interface clock_sink clock end
set_interface_property clock_sink clockRate 0
set_interface_property clock_sink ENABLED true
set_interface_property clock_sink EXPORT_OF ""
set_interface_property clock_sink PORT_NAME_MAP ""
set_interface_property clock_sink CMSIS_SVD_VARIABLES ""
set_interface_property clock_sink SVD_ADDRESS_GROUP ""

add_interface_port clock_sink clk_i clk Input 1


# 
# connection point reset_sink
# 
add_interface reset_sink reset end
set_interface_property reset_sink associatedClock clock_sink
set_interface_property reset_sink synchronousEdges DEASSERT
set_interface_property reset_sink ENABLED true
set_interface_property reset_sink EXPORT_OF ""
set_interface_property reset_sink PORT_NAME_MAP ""
set_interface_property reset_sink CMSIS_SVD_VARIABLES ""
set_interface_property reset_sink SVD_ADDRESS_GROUP ""

add_interface_port reset_sink rst_i reset Input 1


# 
# connection point avalon_s
# 
add_interface avalon_s avalon end
set_interface_property avalon_s addressUnits WORDS
set_interface_property avalon_s associatedClock clock_sink
set_interface_property avalon_s associatedReset reset_sink
set_interface_property avalon_s bitsPerSymbol 8
set_interface_property avalon_s burstOnBurstBoundariesOnly false
set_interface_property avalon_s burstcountUnits WORDS
set_interface_property avalon_s explicitAddressSpan 0
set_interface_property avalon_s holdTime 0
set_interface_property avalon_s linewrapBursts false
set_interface_property avalon_s maximumPendingReadTransactions 0
set_interface_property avalon_s maximumPendingWriteTransactions 0
set_interface_property avalon_s readLatency 0
set_interface_property avalon_s readWaitTime 1
set_interface_property avalon_s setupTime 0
set_interface_property avalon_s timingUnits Cycles
set_interface_property avalon_s writeWaitTime 0
set_interface_property avalon_s ENABLED true
set_interface_property avalon_s EXPORT_OF ""
set_interface_property avalon_s PORT_NAME_MAP ""
set_interface_property avalon_s CMSIS_SVD_VARIABLES ""
set_interface_property avalon_s SVD_ADDRESS_GROUP ""

add_interface_port avalon_s avalon_s_address address Input 4
add_interface_port avalon_s avalon_s_writedata writedata Input 32
add_interface_port avalon_s avalon_s_readdata readdata Output 32
add_interface_port avalon_s avalon_s_byteenable byteenable Input 4
add_interface_port avalon_s avalon_s_write write Input 1
add_interface_port avalon_s avalon_s_read read Input 1
add_interface_port avalon_s avalon_s_chipselect chipselect Input 1
add_interface_port avalon_s avalon_s_waitrequest_n waitrequest_n Output 1
set_interface_assignment avalon_s embeddedsw.configuration.isFlash 0
set_interface_assignment avalon_s embeddedsw.configuration.isMemoryDevice 0
set_interface_assignment avalon_s embeddedsw.configuration.isNonVolatileStorage 0
set_interface_assignment avalon_s embeddedsw.configuration.isPrintableDevice 0


# 
# connection point bus
# 
#add_sv_interface bus sc_if

# Setting the parameter property to add SV interface parameters
#set_parameter_property my_interface_parameter SV_INTERFACE_PARAMETER bus

# Setting the port properties to add them to SV interface port set_port_property clk SV_INTERFACE_PORT bus #set_port_property p1 SV_INTERFACE_PORT bus
#set_port_property p2 SV_INTERFACE_PORT bus
#set_port_property p1 SV_INTERFACE_SIGNAL bus
#set_port_property p2 SV_INTERFACE_SIGNAL bus

#Adding the SV Interface File
#add_fileset_file sc_if.sv SYSTEM_VERILOG PATH sc_if.sv SYSTEMVERILOG_INTERFACE


# 
# connection point sc_if
# 
add_interface sc_if conduit end
set_interface_property sc_if associatedClock ""
set_interface_property sc_if associatedReset ""
set_interface_property sc_if ENABLED true
set_interface_property sc_if EXPORT_OF ""
set_interface_property sc_if PORT_NAME_MAP ""
set_interface_property sc_if CMSIS_SVD_VARIABLES ""
set_interface_property sc_if SVD_ADDRESS_GROUP ""

add_interface_port sc_if sc_status_i sc_status_i Input 32
add_interface_port sc_if sc_status2_i sc_status2_i Input 32
add_interface_port sc_if lt_status_i lt_status_i Input 32
add_interface_port sc_if h_config_o h_config_o Output 32
add_interface_port sc_if h_config2_o h_config2_o Output 32
add_interface_port sc_if v_config_o v_config_o Output 32
add_interface_port sc_if misc_config_o misc_config_o Output 32
add_interface_port sc_if sl_config_o sl_config_o Output 32
add_interface_port sc_if sl_config2_o sl_config2_o Output 32
