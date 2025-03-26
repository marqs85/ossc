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

close_service master $claim_path


set jtag_debug_list [get_service_paths jtag_debug]
set jd [ lindex $jtag_debug_list 0 ]
open_service jtag_debug $jd
puts "Resetting system"
jtag_debug_reset_system $jd
close_service jtag_debug $jd
puts "Done"
