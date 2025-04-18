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

puts "Resetting system"
master_write_32 $claim_path 0x40 0x00000003
after 1
master_write_32 $claim_path 0x40 0x00000001
master_write_32 $claim_path 0x40 0x00000000

close_service master $claim_path
puts "Done"