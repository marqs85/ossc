# flash details
set flash_base                  0x02000000
set flash_imem_offset           0x00050000
set flash_imem_base             [format 0x%.8x [expr $flash_base + $flash_imem_offset]]
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

# target file details
set bin_file mem_init/flash.bin
set bin_size [file size $bin_file]
set num_sectors [expr ($bin_size / $flash_secsize) + (($bin_size % $flash_secsize) ne 0)]

#Select the master service type and check for available service paths.
while 1 {
    set service_paths [get_service_paths master]
    if {[llength $service_paths] > 0} {
        break
    }
    puts "Refreshing connections..."
    refresh_connections
    after 100
}

#Set the master service path.
set master_service_path [lindex $service_paths 0]

#Open the master service.
set claim_path [claim_service master $master_service_path mylib]

puts "Halting CPU"
master_write_32 $claim_path 0x40 0x00000001
master_write_32 $claim_path 0x40 0x80000001

#write enable
master_write_32 $claim_path $flash_cmd_setting 0x00000006
master_write_32 $claim_path $flash_cmd_ctrl 0x1

#write status register (clear BP)
master_write_32 $claim_path $flash_cmd_setting 0x00001001
#master_write_32 $claim_path $flash_cmd_write_data_0 0x0000005c
master_write_32 $claim_path $flash_cmd_write_data_0 0x00000000
master_write_32 $claim_path $flash_cmd_ctrl 0x1

puts "Erasing $num_sectors flash sectors"
set addr $flash_imem_offset
for {set i 0} {$i<$num_sectors} {incr i} {
    master_write_32 $claim_path $flash_cmd_setting 0x00000006
    master_write_32 $claim_path $flash_cmd_ctrl 0x1
    master_write_32 $claim_path $flash_cmd_setting 0x000003D8
    master_write_32 $claim_path $flash_cmd_addr_register $addr
    master_write_32 $claim_path $flash_cmd_ctrl 0x1
    set addr [expr $addr + $flash_secsize]
    after 500
}

puts "Writing flash"
# JTAG to Avalon master does not support sink backpressure
#master_write_from_file $claim_path mem_init/flash.bin $flash_imem_base

# work around lack of backpressure support by writing chunks of master FIFO size
set chunks [llength [glob mem_init/chunks/*]]
puts "Programming $chunks chunks"
set addr $flash_imem_base
for {set i 0} {$i<$chunks} {incr i} {
    set file [format "flash.%04d" $i]
    master_write_from_file $claim_path mem_init/chunks/$file $addr
    set addr [expr $addr + 1024]
}
#master_read_to_file $claim_path mem_init/flash_readback.bin $flash_imem_base $bin_size
#master_read_to_file $claim_path mem_init/ram_readback.bin 0x010000 65536

# flush flashctrl cmd fifo to ensure writes have finished
master_read_32 $claim_path $flash_base 1

puts "Resetting system"
master_write_32 $claim_path 0x40 0x00000003
after 1
master_write_32 $claim_path 0x40 0x00000001
master_write_32 $claim_path 0x40 0x00000000

close_service master $claim_path
puts "Done"
