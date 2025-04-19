# flash details
set flash_base                  0x02000000
set flash_imem_offset           0x00080000
set flash_imem_base             [format 0x%.8x [expr "$flash_base + $flash_imem_offset"]]
set flash_secsize               65536

# flash controller register addresses
set control_register            0x00020100
set operating_protocols_setting 0x00020110
set read_instr                  0x00020114
set write_instr                 0x00020118
set flash_cmd_setting           0x0002011c
set flash_cmd_ctrl              0x00020120
set flash_cmd_addr_register     0x00020124
set flash_cmd_write_data_0      0x00020128
set flash_cmd_read_data_0       0x00020130