Open Source Scan Converter
==============

Open Source Scan Converter is a low-latency video digitizer and scan conversion board designed mainly for connecting retro video game consoles and home computers into modern displays. Please check the [wikipage](http://junkerhq.net/xrgb/index.php?title=OSSC) for more detailed description and latest features.

Requirements for building and debugging firmware
---------------------------------------------------
* Hardware
  * OSSC board
  * USB Blaster compatible JTAG debugger, e.g. Terasic Blaster (for FW installation and debugging)
  * micro SD/SDHC card (for FW update via SD card)

* Software
  * [Altera Quartus II + Cyclone IV support](http://dl.altera.com/?edition=lite) (v 16.1 or higher - free Lite Edition suffices)
  * [RISC-V GNU Compiler Toolchain](https://github.com/riscv/riscv-gnu-toolchain)
  * [Picolibc library for RISC-V](https://github.com/picolibc/picolibc)
  * GCC (or another C compiler) for host architecture (for building a SD card image)
  * Make
  * [iconv](https://en.wikipedia.org/wiki/Iconv) (for building with JP lang menu)


Architecture
------------------------------
* [Reference board schematics](https://github.com/marqs85/ossc_pcb/raw/v1.8/doc/ossc_board.pdf)
* [Reference PCB project](https://github.com/marqs85/ossc_pcb)


SW toolchain build procedure
--------------------------
1. Download, configure, build and install RISC-V toolchain (with RV32EMC support) and Picolibc.

From sources:
~~~~
git clone --recursive https://github.com/riscv/riscv-gnu-toolchain
git clone --recursive https://github.com/picolibc/picolibc
cd riscv-gnu-toolchain
./configure --prefix=/opt/riscv --with-arch=rv32emc --with-abi=ilp32e
sudo make    # sudo needed if installing under default /opt/riscv location
~~~~
On Debian-style Linux distros:
~~~~
sudo apt install gcc-riscv64-unknown-elf binutils-riscv64-unknown-elf picolibc-riscv64-unknown-elf
~~~~

2. Compile custom binary to IHEX converter:
~~~~
gcc tools/bin2hex.c -o tools/bin2hex
~~~~


Building RTL (bitstream)
--------------------------
1. Initialize project submodules (once after cloning ossc project or when submoduled have been updated)
~~~~
git submodule update --init --recursive
~~~~
2. Load the project (ossc.qpf) in Quartus
3. Generate QSYS output files (only needed before first compilation or when QSYS structure has been modified)
    * Open Platform Designer (Tools -> Platform Designer)
    * Load platform configuration (sys.qsys)
    * Generate output (Generate -> Generate HDL, Generate)
    * Close Platform Designer
    * Run "patch -p0 <scripts/qsys.patch" to patch generated files to optimize block RAM usage
    * Run "touch software/sys_controller_bsp/bsp_timestamp" to acknowledge QSYS update
4. Generate the FPGA bitstream (Processing -> Start Compilation)
5. Ensure that there are no timing violations by looking into Timing Analyzer report

Building software image
--------------------------
1. Enter software root directory:
~~~~
cd software/sys_controller
~~~~
2. Build SW for target configuration:
~~~~
make [OPTIONS] [TARGET]
~~~~
OPTIONS may include following definitions:
* OSDLANG=JP (Japanese language menu)

TARGET is typically one of the following:
* all (Default target. Compiles an ELF file)
* clean (cleans ELF and intermediate files. Should be invoked every time OPTIONS are changed between compilations, expect with generate_hex where it is done automatically)

3. Optionally test updated SW by directly downloading SW image to flash via JTAG (requires valid FPGA bitstream to be present):
~~~~
make rv-reprogram
~~~~


Installing firmware via JTAG
--------------------------
The bitstream can be either directly programmed into FPGA (volatile method, suitable for quick testing), or into serial flash chip alongside SW image where it is automatically loaded every time FPGA is subsequently powered on (nonvolatile method, suitable for long-term use).

To directly program FPGA, open Programmer in Quartus, select your USB Blaster device, add configuration file (output_files/ossc.sof) and press Start. Download SW image if it not present / up to date in flash.

To program flash, a combined FPGA image must be first generated and converted into JTAG indirect Configuration file (.jic). Open conversion tool ("File->Convert Programming Files") in Quartus, click "Open Conversion Setup Data", select "ossc.cof" and press Generate. Then open Programmer and ensure that "Initiate configuration after programming" and "Unprotect EPCS/EPCQ devices selected for Erase/Program operation" are checked in Tools->Options. Then clear file list, add generated file (output_files/ossc.jic) and press Start after which flash is programmed. Installed/updated firmware is activated when programming finishes (or after power-cycling the board in case of a fresh flash chip).


Generating SD card image
--------------------------
Bitstream file (Altera propiertary format) must be wrapped with custom header structure (including checksums) so that it can be processed reliably on the CPU. This can be done with included helper application which generates an image file which can written on FAT32/exFAT-formatted SD card and subsequently loaded on OSSC:

1. Compile tools/create_fw_img.c
~~~~
cd tools && gcc create_fw_img.c -o create_fw_img
~~~~
2. Generate the firmware image:
~~~~
./create_fw_img <rbf> <sw_image> <offset> <version> [version_suffix]
~~~~
where
* \<rbf\> is RBF format bitstream file (typically ../output_files/ossc.rbf)
* \<sw_image\> is SW image binary (typically ../output_files/ossc.rbf)
* \<offset\> is target offset for the SW image binary (typically 0x50000)
* \<version\> is version string (e.g. 1.20)
* \[version_suffix\] is optional max. 8 character suffix name (e.g. "mytest")

The command creates ossc_\<version\>-\<version_suffix\>.bin which can be copied to fw folder of SD card. A secondary FW (identified by specific key in header) gets automatically installed at flash address 0x00080000.

Debugging
--------------------------
1. Rebuild the software in debug mode:
~~~~
make clean && make APP_CFLAGS_DEBUG_LEVEL="-DDEBUG"
~~~~

2. Flash SW image via JTAG and open terminal for UART
~~~~
make rv-reprogram && nios2-terminal
~~~~
Remember to close nios2-terminal after debug session, otherwise any JTAG transactions will hang/fail.


License
---------------------------------------------------
[GPL3](LICENSE)