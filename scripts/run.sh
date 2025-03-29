export PATH=$PATH:~/intelFPGA_standard/24.1std/quartus/bin:~/intelFPGA_standard/24.1std/quartus/sopc_builder/bin/:/opt/riscv/bin

set -e

touch software/sys_controller_bsp/bsp_timestamp

gcc tools/bin2hex.c -o tools/bin2hex

cd software/sys_controller
make clean
make HAS_SH1107=y generate_hex
cd -

quartus_cdb ossc -c ossc --update_mif
quartus_asm --read_settings_files=on --write_settings_files=off ossc -c ossc

quartus_cpf --convert --frequency=2MHz --voltage=3.3V --operation=p output_files/ossc.sof output_files/ossc.svf
openocd -f scripts/openocd_load.cfg

openocd -f scripts/openocd_gdb.cfg


