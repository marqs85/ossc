# 
# request TCL package from ACDS 16.1
# 
package require -exact qsys 16.1


# 
# module pulpino
# 
set_module_property DESCRIPTION "HW CRC32"
set_module_property NAME hw_crc32
#set_module_property VERSION 1.0
set_module_property INTERNAL false
set_module_property OPAQUE_ADDRESS_MAP true
set_module_property GROUP "DSP"
set_module_property AUTHOR ""
set_module_property DISPLAY_NAME hw_crc32
set_module_property INSTANTIATE_IN_SYSTEM_MODULE true
set_module_property EDITABLE true
set_module_property REPORT_TO_TALKBACK false
set_module_property ALLOW_GREYBOX_GENERATION false
set_module_property REPORT_HIERARCHY false


set adv_dbg_if false

# 
# file sets
# 
add_fileset QUARTUS_SYNTH QUARTUS_SYNTH "" ""
set_fileset_property QUARTUS_SYNTH TOP_LEVEL CRC_Component
set_fileset_property QUARTUS_SYNTH ENABLE_RELATIVE_INCLUDE_PATHS false
set_fileset_property QUARTUS_SYNTH ENABLE_FILE_OVERWRITE_MODE false
add_fileset_file CRC_Component.v SYSTEM_VERILOG PATH CRC_Component.v TOP_LEVEL_FILE


add_fileset sim_verilog SIM_VERILOG "" "Verilog Simulation"
set_fileset_property SIM_VERILOG ENABLE_RELATIVE_INCLUDE_PATHS false
set_fileset_property SIM_VERILOG ENABLE_FILE_OVERWRITE_MODE false
set_fileset_property SIM_VERILOG TOP_LEVEL CRC_Component
add_fileset_file CRC_Component.v SYSTEM_VERILOG PATH CRC_Component.v TOP_LEVEL_FILE



#
# connection point clk_sink
#
add_interface clk_sink clock end
set_interface_property clk_sink ENABLED true
set_interface_property clk_sink EXPORT_OF ""
set_interface_property clk_sink PORT_NAME_MAP ""
set_interface_property clk_sink CMSIS_SVD_VARIABLES ""
set_interface_property clk_sink SVD_ADDRESS_GROUP ""

add_interface_port clk_sink clk clk Input 1



# 
# connection point reset_sink
# 
add_interface reset_sink reset end
set_interface_property reset_sink associatedClock clk_sink
set_interface_property reset_sink synchronousEdges DEASSERT
set_interface_property reset_sink ENABLED true
set_interface_property reset_sink EXPORT_OF ""
set_interface_property reset_sink PORT_NAME_MAP ""
set_interface_property reset_sink CMSIS_SVD_VARIABLES ""
set_interface_property reset_sink SVD_ADDRESS_GROUP ""

add_interface_port reset_sink reset reset Input 1



# 
# connection point avalon_slave
# 
add_interface avalon_slave avalon end
set_interface_property avalon_slave addressUnits WORDS
set_interface_property avalon_slave associatedClock clk_sink
set_interface_property avalon_slave associatedReset reset_sink
set_interface_property avalon_slave bitsPerSymbol 8
set_interface_property avalon_slave burstOnBurstBoundariesOnly false
set_interface_property avalon_slave burstcountUnits WORDS
set_interface_property avalon_slave explicitAddressSpan 0
set_interface_property avalon_slave holdTime 0
set_interface_property avalon_slave linewrapBursts false
set_interface_property avalon_slave maximumPendingReadTransactions 0
set_interface_property avalon_slave readLatency 1
set_interface_property avalon_slave readWaitTime 1
set_interface_property avalon_slave setupTime 0
set_interface_property avalon_slave timingUnits Cycles
set_interface_property avalon_slave writeWaitTime 0
set_interface_property avalon_slave ENABLED true
set_interface_property avalon_slave EXPORT_OF ""
set_interface_property avalon_slave PORT_NAME_MAP ""
set_interface_property avalon_slave CMSIS_SVD_VARIABLES ""
set_interface_property avalon_slave SVD_ADDRESS_GROUP ""
add_interface_port avalon_slave address address Input 3
add_interface_port avalon_slave readdata readdata Output 32
add_interface_port avalon_slave read read Input 1
add_interface_port avalon_slave chipselect chipselect Input 1
add_interface_port avalon_slave byteenable byteenable Input 4
add_interface_port avalon_slave write write Input 1
add_interface_port avalon_slave writedata writedata Input 32
