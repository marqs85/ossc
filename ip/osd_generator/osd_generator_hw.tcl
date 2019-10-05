# 
# request TCL package from ACDS 16.1
# 
package require -exact qsys 16.1

# 
# module
# 
set_module_property DESCRIPTION "OSD generator"
set_module_property NAME osd_generator
#set_module_property VERSION 18.0
set_module_property INTERNAL false
set_module_property OPAQUE_ADDRESS_MAP true
set_module_property GROUP "Processors and Peripherals"
set_module_property AUTHOR ""
set_module_property DISPLAY_NAME osd_generator
set_module_property INSTANTIATE_IN_SYSTEM_MODULE true
set_module_property EDITABLE true
set_module_property REPORT_TO_TALKBACK false
set_module_property ALLOW_GREYBOX_GENERATION false
set_module_property REPORT_HIERARCHY false

#
# parameters
#
add_parameter USE_MEMORY_BLOCKS INTEGER 1
set_parameter_property USE_MEMORY_BLOCKS DISPLAY_NAME "Use memory blocks for character array"
set_parameter_property USE_MEMORY_BLOCKS DISPLAY_HINT boolean
set_parameter_property USE_MEMORY_BLOCKS UNITS None
set_parameter_property USE_MEMORY_BLOCKS HDL_PARAMETER true

# 
# file sets
# 
add_fileset QUARTUS_SYNTH QUARTUS_SYNTH "" ""
set_fileset_property QUARTUS_SYNTH TOP_LEVEL osd_generator_top
set_fileset_property QUARTUS_SYNTH ENABLE_RELATIVE_INCLUDE_PATHS false
set_fileset_property QUARTUS_SYNTH ENABLE_FILE_OVERWRITE_MODE false
add_fileset_file osd_generator_top.sv VERILOG PATH osd_generator_top.sv

add_fileset SIM_VERILOG SIM_VERILOG "" ""
set_fileset_property SIM_VERILOG ENABLE_RELATIVE_INCLUDE_PATHS false
set_fileset_property SIM_VERILOG ENABLE_FILE_OVERWRITE_MODE false
set_fileset_property SIM_VERILOG TOP_LEVEL osd_generator_top
add_fileset_file osd_generator_top.sv VERILOG PATH osd_generator_top.sv

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
#add_sv_interface bus osd_if

# Setting the parameter property to add SV interface parameters
#set_parameter_property my_interface_parameter SV_INTERFACE_PARAMETER bus

# Setting the port properties to add them to SV interface port set_port_property clk SV_INTERFACE_PORT bus #set_port_property p1 SV_INTERFACE_PORT bus
#set_port_property p2 SV_INTERFACE_PORT bus
#set_port_property p1 SV_INTERFACE_SIGNAL bus
#set_port_property p2 SV_INTERFACE_SIGNAL bus

#Adding the SV Interface File
#add_fileset_file osd_if.sv SYSTEM_VERILOG PATH osd_if.sv SYSTEMVERILOG_INTERFACE


# 
# connection point osd_if
# 
add_interface osd_if conduit end
set_interface_property osd_if associatedClock ""
set_interface_property osd_if associatedReset ""
set_interface_property osd_if ENABLED true
set_interface_property osd_if EXPORT_OF ""
set_interface_property osd_if PORT_NAME_MAP ""
set_interface_property osd_if CMSIS_SVD_VARIABLES ""
set_interface_property osd_if SVD_ADDRESS_GROUP ""

add_interface_port osd_if vclk vclk Input 1
add_interface_port osd_if xpos xpos Input 11
add_interface_port osd_if ypos ypos Input 11
add_interface_port osd_if osd_enable osd_enable Output 1
add_interface_port osd_if osd_color osd_color Output 1
