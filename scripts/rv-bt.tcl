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

master_write_32 $claim_path 0x5c [expr 0x2207b1]
puts "DPC:     [master_read_32 $claim_path 0x10 1]"
master_write_32 $claim_path 0x5c [expr 0x220341]
puts "MEPC:    [master_read_32 $claim_path 0x10 1]"
master_write_32 $claim_path 0x5c [expr 0x220342]
puts "MCAUSE:  [master_read_32 $claim_path 0x10 1]"

set offset 0x1001
foreach i {ra sp gp tp t0 t1 t2 s0 s1 a0 a1 a2 a3 a4 a5} {
    master_write_32 $claim_path 0x5c [expr 0x220000 + $offset]
    puts "$i: [master_read_32 $claim_path 0x10 1]"
    set offset [expr $offset + 1]
}

master_write_32 $claim_path 0x40 0x40000000
