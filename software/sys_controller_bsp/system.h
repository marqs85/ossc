/*
 * system.h - SOPC Builder system and BSP software package information
 *
 * Machine generated for CPU 'nios2_qsys_0' in SOPC Builder design 'sys'
 * SOPC Builder design path: ../../sys.sopcinfo
 *
 * Generated: Sun Mar 25 16:51:03 EEST 2018
 */

/*
 * DO NOT MODIFY THIS FILE
 *
 * Changing this file will have subtle consequences
 * which will almost certainly lead to a nonfunctioning
 * system. If you do modify this file, be aware that your
 * changes will be overwritten and lost when this file
 * is generated again.
 *
 * DO NOT MODIFY THIS FILE
 */

/*
 * License Agreement
 *
 * Copyright (c) 2008
 * Altera Corporation, San Jose, California, USA.
 * All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * This agreement shall be governed in all respects by the laws of the State
 * of California and by the laws of the United States of America.
 */

#ifndef __SYSTEM_H_
#define __SYSTEM_H_

/* Include definitions from linker script generator */
#include "linker.h"


/*
 * CPU configuration
 *
 */

#define ALT_CPU_ARCHITECTURE "altera_nios2_gen2"
#define ALT_CPU_BIG_ENDIAN 0
#define ALT_CPU_BREAK_ADDR 0x00820820
#define ALT_CPU_CPU_ARCH_NIOS2_R1
#define ALT_CPU_CPU_FREQ 27000000u
#define ALT_CPU_CPU_ID_SIZE 1
#define ALT_CPU_CPU_ID_VALUE 0x00000000
#define ALT_CPU_CPU_IMPLEMENTATION "tiny"
#define ALT_CPU_DATA_ADDR_WIDTH 0x18
#define ALT_CPU_DCACHE_LINE_SIZE 0
#define ALT_CPU_DCACHE_LINE_SIZE_LOG2 0
#define ALT_CPU_DCACHE_SIZE 0
#define ALT_CPU_EXCEPTION_ADDR 0x00810020
#define ALT_CPU_FLASH_ACCELERATOR_LINES 0
#define ALT_CPU_FLASH_ACCELERATOR_LINE_SIZE 0
#define ALT_CPU_FLUSHDA_SUPPORTED
#define ALT_CPU_FREQ 27000000
#define ALT_CPU_HARDWARE_DIVIDE_PRESENT 0
#define ALT_CPU_HARDWARE_MULTIPLY_PRESENT 0
#define ALT_CPU_HARDWARE_MULX_PRESENT 0
#define ALT_CPU_HAS_DEBUG_CORE 1
#define ALT_CPU_HAS_DEBUG_STUB
#define ALT_CPU_HAS_ILLEGAL_INSTRUCTION_EXCEPTION
#define ALT_CPU_HAS_JMPI_INSTRUCTION
#define ALT_CPU_ICACHE_LINE_SIZE 0
#define ALT_CPU_ICACHE_LINE_SIZE_LOG2 0
#define ALT_CPU_ICACHE_SIZE 0
#define ALT_CPU_INST_ADDR_WIDTH 0x18
#define ALT_CPU_NAME "nios2_qsys_0"
#define ALT_CPU_OCI_VERSION 1
#define ALT_CPU_RESET_ADDR 0x00810000


/*
 * CPU configuration (with legacy prefix - don't use these anymore)
 *
 */

#define NIOS2_BIG_ENDIAN 0
#define NIOS2_BREAK_ADDR 0x00820820
#define NIOS2_CPU_ARCH_NIOS2_R1
#define NIOS2_CPU_FREQ 27000000u
#define NIOS2_CPU_ID_SIZE 1
#define NIOS2_CPU_ID_VALUE 0x00000000
#define NIOS2_CPU_IMPLEMENTATION "tiny"
#define NIOS2_DATA_ADDR_WIDTH 0x18
#define NIOS2_DCACHE_LINE_SIZE 0
#define NIOS2_DCACHE_LINE_SIZE_LOG2 0
#define NIOS2_DCACHE_SIZE 0
#define NIOS2_EXCEPTION_ADDR 0x00810020
#define NIOS2_FLASH_ACCELERATOR_LINES 0
#define NIOS2_FLASH_ACCELERATOR_LINE_SIZE 0
#define NIOS2_FLUSHDA_SUPPORTED
#define NIOS2_HARDWARE_DIVIDE_PRESENT 0
#define NIOS2_HARDWARE_MULTIPLY_PRESENT 0
#define NIOS2_HARDWARE_MULX_PRESENT 0
#define NIOS2_HAS_DEBUG_CORE 1
#define NIOS2_HAS_DEBUG_STUB
#define NIOS2_HAS_ILLEGAL_INSTRUCTION_EXCEPTION
#define NIOS2_HAS_JMPI_INSTRUCTION
#define NIOS2_ICACHE_LINE_SIZE 0
#define NIOS2_ICACHE_LINE_SIZE_LOG2 0
#define NIOS2_ICACHE_SIZE 0
#define NIOS2_INST_ADDR_WIDTH 0x18
#define NIOS2_OCI_VERSION 1
#define NIOS2_RESET_ADDR 0x00810000


/*
 * Custom instruction macros
 *
 */

#define ALT_CI_NIOS2_HW_CRC32_0(n,A) __builtin_custom_ini(ALT_CI_NIOS2_HW_CRC32_0_N+(n&ALT_CI_NIOS2_HW_CRC32_0_N_MASK),(A))
#define ALT_CI_NIOS2_HW_CRC32_0_N 0x0
#define ALT_CI_NIOS2_HW_CRC32_0_N_MASK ((1<<3)-1)
#define ALT_CI_NIOS_CUSTOM_INSTR_BITSWAP_0(A) __builtin_custom_ini(ALT_CI_NIOS_CUSTOM_INSTR_BITSWAP_0_N,(A))
#define ALT_CI_NIOS_CUSTOM_INSTR_BITSWAP_0_N 0x9
#define ALT_CI_NIOS_CUSTOM_INSTR_ENDIANCONVERTER_0(A) __builtin_custom_ini(ALT_CI_NIOS_CUSTOM_INSTR_ENDIANCONVERTER_0_N,(A))
#define ALT_CI_NIOS_CUSTOM_INSTR_ENDIANCONVERTER_0_N 0x8


/*
 * Define for each module class mastered by the CPU
 *
 */

#define __ALTERA_AVALON_JTAG_UART
#define __ALTERA_AVALON_ONCHIP_MEMORY2
#define __ALTERA_AVALON_PIO
#define __ALTERA_AVALON_TIMER
#define __ALTERA_EPCQ_CONTROLLER_MOD
#define __ALTERA_NIOS2_GEN2
#define __ALTERA_NIOS_CUSTOM_INSTR_BITSWAP
#define __ALTERA_NIOS_CUSTOM_INSTR_ENDIANCONVERTER
#define __I2C_OPENCORES
#define __NIOS2_HW_CRC32


/*
 * System configuration
 *
 */

#define ALT_DEVICE_FAMILY "Cyclone IV E"
#define ALT_ENHANCED_INTERRUPT_API_PRESENT
#define ALT_IRQ_BASE NULL
#define ALT_LOG_PORT "/dev/null"
#define ALT_LOG_PORT_BASE 0x0
#define ALT_LOG_PORT_DEV null
#define ALT_LOG_PORT_TYPE ""
#define ALT_NUM_EXTERNAL_INTERRUPT_CONTROLLERS 0
#define ALT_NUM_INTERNAL_INTERRUPT_CONTROLLERS 1
#define ALT_NUM_INTERRUPT_CONTROLLERS 1
#define ALT_STDERR "/dev/jtag_uart_0"
#define ALT_STDERR_BASE 0x821110
#define ALT_STDERR_DEV jtag_uart_0
#define ALT_STDERR_IS_JTAG_UART
#define ALT_STDERR_PRESENT
#define ALT_STDERR_TYPE "altera_avalon_jtag_uart"
#define ALT_STDIN "/dev/jtag_uart_0"
#define ALT_STDIN_BASE 0x821110
#define ALT_STDIN_DEV jtag_uart_0
#define ALT_STDIN_IS_JTAG_UART
#define ALT_STDIN_PRESENT
#define ALT_STDIN_TYPE "altera_avalon_jtag_uart"
#define ALT_STDOUT "/dev/jtag_uart_0"
#define ALT_STDOUT_BASE 0x821110
#define ALT_STDOUT_DEV jtag_uart_0
#define ALT_STDOUT_IS_JTAG_UART
#define ALT_STDOUT_PRESENT
#define ALT_STDOUT_TYPE "altera_avalon_jtag_uart"
#define ALT_SYSTEM_NAME "sys"


/*
 * epcq_controller_0_avl_csr configuration
 *
 */

#define ALT_MODULE_CLASS_epcq_controller_0_avl_csr altera_epcq_controller_mod
#define EPCQ_CONTROLLER_0_AVL_CSR_BASE 0x821020
#define EPCQ_CONTROLLER_0_AVL_CSR_FLASH_TYPE "EPCS64"
#define EPCQ_CONTROLLER_0_AVL_CSR_IRQ 2
#define EPCQ_CONTROLLER_0_AVL_CSR_IRQ_INTERRUPT_CONTROLLER_ID 0
#define EPCQ_CONTROLLER_0_AVL_CSR_IS_EPCS 1
#define EPCQ_CONTROLLER_0_AVL_CSR_NAME "/dev/epcq_controller_0_avl_csr"
#define EPCQ_CONTROLLER_0_AVL_CSR_NUMBER_OF_SECTORS 128
#define EPCQ_CONTROLLER_0_AVL_CSR_PAGE_SIZE 256
#define EPCQ_CONTROLLER_0_AVL_CSR_SECTOR_SIZE 65536
#define EPCQ_CONTROLLER_0_AVL_CSR_SPAN 32
#define EPCQ_CONTROLLER_0_AVL_CSR_SUBSECTOR_SIZE 4096
#define EPCQ_CONTROLLER_0_AVL_CSR_TYPE "altera_epcq_controller_mod"


/*
 * epcq_controller_0_avl_mem configuration
 *
 */

#define ALT_MODULE_CLASS_epcq_controller_0_avl_mem altera_epcq_controller_mod
#define EPCQ_CONTROLLER_0_AVL_MEM_BASE 0x0
#define EPCQ_CONTROLLER_0_AVL_MEM_FLASH_TYPE "EPCS64"
#define EPCQ_CONTROLLER_0_AVL_MEM_IRQ -1
#define EPCQ_CONTROLLER_0_AVL_MEM_IRQ_INTERRUPT_CONTROLLER_ID -1
#define EPCQ_CONTROLLER_0_AVL_MEM_IS_EPCS 1
#define EPCQ_CONTROLLER_0_AVL_MEM_NAME "/dev/epcq_controller_0_avl_mem"
#define EPCQ_CONTROLLER_0_AVL_MEM_NUMBER_OF_SECTORS 128
#define EPCQ_CONTROLLER_0_AVL_MEM_PAGE_SIZE 256
#define EPCQ_CONTROLLER_0_AVL_MEM_SECTOR_SIZE 65536
#define EPCQ_CONTROLLER_0_AVL_MEM_SPAN 8388608
#define EPCQ_CONTROLLER_0_AVL_MEM_SUBSECTOR_SIZE 4096
#define EPCQ_CONTROLLER_0_AVL_MEM_TYPE "altera_epcq_controller_mod"


/*
 * hal configuration
 *
 */

#define ALT_MAX_FD 32
#define ALT_SYS_CLK none
#define ALT_TIMESTAMP_CLK TIMER_0


/*
 * i2c_opencores_0 configuration
 *
 */

#define ALT_MODULE_CLASS_i2c_opencores_0 i2c_opencores
#define I2C_OPENCORES_0_BASE 0x821060
#define I2C_OPENCORES_0_IRQ 3
#define I2C_OPENCORES_0_IRQ_INTERRUPT_CONTROLLER_ID 0
#define I2C_OPENCORES_0_NAME "/dev/i2c_opencores_0"
#define I2C_OPENCORES_0_SPAN 32
#define I2C_OPENCORES_0_TYPE "i2c_opencores"


/*
 * i2c_opencores_1 configuration
 *
 */

#define ALT_MODULE_CLASS_i2c_opencores_1 i2c_opencores
#define I2C_OPENCORES_1_BASE 0x821040
#define I2C_OPENCORES_1_IRQ 4
#define I2C_OPENCORES_1_IRQ_INTERRUPT_CONTROLLER_ID 0
#define I2C_OPENCORES_1_NAME "/dev/i2c_opencores_1"
#define I2C_OPENCORES_1_SPAN 32
#define I2C_OPENCORES_1_TYPE "i2c_opencores"


/*
 * jtag_uart_0 configuration
 *
 */

#define ALT_MODULE_CLASS_jtag_uart_0 altera_avalon_jtag_uart
#define JTAG_UART_0_BASE 0x821110
#define JTAG_UART_0_IRQ 1
#define JTAG_UART_0_IRQ_INTERRUPT_CONTROLLER_ID 0
#define JTAG_UART_0_NAME "/dev/jtag_uart_0"
#define JTAG_UART_0_READ_DEPTH 64
#define JTAG_UART_0_READ_THRESHOLD 8
#define JTAG_UART_0_SPAN 8
#define JTAG_UART_0_TYPE "altera_avalon_jtag_uart"
#define JTAG_UART_0_WRITE_DEPTH 64
#define JTAG_UART_0_WRITE_THRESHOLD 8


/*
 * onchip_memory2_0 configuration
 *
 */

#define ALT_MODULE_CLASS_onchip_memory2_0 altera_avalon_onchip_memory2
#define ONCHIP_MEMORY2_0_ALLOW_IN_SYSTEM_MEMORY_CONTENT_EDITOR 0
#define ONCHIP_MEMORY2_0_ALLOW_MRAM_SIM_CONTENTS_ONLY_FILE 0
#define ONCHIP_MEMORY2_0_BASE 0x810000
#define ONCHIP_MEMORY2_0_CONTENTS_INFO ""
#define ONCHIP_MEMORY2_0_DUAL_PORT 0
#define ONCHIP_MEMORY2_0_GUI_RAM_BLOCK_TYPE "AUTO"
#define ONCHIP_MEMORY2_0_INIT_CONTENTS_FILE "sys_onchip_memory2_0"
#define ONCHIP_MEMORY2_0_INIT_MEM_CONTENT 1
#define ONCHIP_MEMORY2_0_INSTANCE_ID "NONE"
#define ONCHIP_MEMORY2_0_IRQ -1
#define ONCHIP_MEMORY2_0_IRQ_INTERRUPT_CONTROLLER_ID -1
#define ONCHIP_MEMORY2_0_NAME "/dev/onchip_memory2_0"
#define ONCHIP_MEMORY2_0_NON_DEFAULT_INIT_FILE_ENABLED 0
#define ONCHIP_MEMORY2_0_RAM_BLOCK_TYPE "AUTO"
#define ONCHIP_MEMORY2_0_READ_DURING_WRITE_MODE "DONT_CARE"
#define ONCHIP_MEMORY2_0_SINGLE_CLOCK_OP 0
#define ONCHIP_MEMORY2_0_SIZE_MULTIPLE 1
#define ONCHIP_MEMORY2_0_SIZE_VALUE 40960
#define ONCHIP_MEMORY2_0_SPAN 40960
#define ONCHIP_MEMORY2_0_TYPE "altera_avalon_onchip_memory2"
#define ONCHIP_MEMORY2_0_WRITABLE 1


/*
 * pio_0 configuration
 *
 */

#define ALT_MODULE_CLASS_pio_0 altera_avalon_pio
#define PIO_0_BASE 0x821100
#define PIO_0_BIT_CLEARING_EDGE_REGISTER 0
#define PIO_0_BIT_MODIFYING_OUTPUT_REGISTER 0
#define PIO_0_CAPTURE 0
#define PIO_0_DATA_WIDTH 16
#define PIO_0_DO_TEST_BENCH_WIRING 0
#define PIO_0_DRIVEN_SIM_VALUE 0
#define PIO_0_EDGE_TYPE "NONE"
#define PIO_0_FREQ 27000000
#define PIO_0_HAS_IN 0
#define PIO_0_HAS_OUT 1
#define PIO_0_HAS_TRI 0
#define PIO_0_IRQ -1
#define PIO_0_IRQ_INTERRUPT_CONTROLLER_ID -1
#define PIO_0_IRQ_TYPE "NONE"
#define PIO_0_NAME "/dev/pio_0"
#define PIO_0_RESET_VALUE 0
#define PIO_0_SPAN 16
#define PIO_0_TYPE "altera_avalon_pio"


/*
 * pio_1 configuration
 *
 */

#define ALT_MODULE_CLASS_pio_1 altera_avalon_pio
#define PIO_1_BASE 0x8210f0
#define PIO_1_BIT_CLEARING_EDGE_REGISTER 0
#define PIO_1_BIT_MODIFYING_OUTPUT_REGISTER 0
#define PIO_1_CAPTURE 0
#define PIO_1_DATA_WIDTH 32
#define PIO_1_DO_TEST_BENCH_WIRING 0
#define PIO_1_DRIVEN_SIM_VALUE 0
#define PIO_1_EDGE_TYPE "NONE"
#define PIO_1_FREQ 27000000
#define PIO_1_HAS_IN 1
#define PIO_1_HAS_OUT 0
#define PIO_1_HAS_TRI 0
#define PIO_1_IRQ -1
#define PIO_1_IRQ_INTERRUPT_CONTROLLER_ID -1
#define PIO_1_IRQ_TYPE "NONE"
#define PIO_1_NAME "/dev/pio_1"
#define PIO_1_RESET_VALUE 0
#define PIO_1_SPAN 16
#define PIO_1_TYPE "altera_avalon_pio"


/*
 * pio_2 configuration
 *
 */

#define ALT_MODULE_CLASS_pio_2 altera_avalon_pio
#define PIO_2_BASE 0x8210e0
#define PIO_2_BIT_CLEARING_EDGE_REGISTER 0
#define PIO_2_BIT_MODIFYING_OUTPUT_REGISTER 0
#define PIO_2_CAPTURE 0
#define PIO_2_DATA_WIDTH 32
#define PIO_2_DO_TEST_BENCH_WIRING 0
#define PIO_2_DRIVEN_SIM_VALUE 0
#define PIO_2_EDGE_TYPE "NONE"
#define PIO_2_FREQ 27000000
#define PIO_2_HAS_IN 1
#define PIO_2_HAS_OUT 0
#define PIO_2_HAS_TRI 0
#define PIO_2_IRQ -1
#define PIO_2_IRQ_INTERRUPT_CONTROLLER_ID -1
#define PIO_2_IRQ_TYPE "NONE"
#define PIO_2_NAME "/dev/pio_2"
#define PIO_2_RESET_VALUE 0
#define PIO_2_SPAN 16
#define PIO_2_TYPE "altera_avalon_pio"


/*
 * pio_3 configuration
 *
 */

#define ALT_MODULE_CLASS_pio_3 altera_avalon_pio
#define PIO_3_BASE 0x8210d0
#define PIO_3_BIT_CLEARING_EDGE_REGISTER 0
#define PIO_3_BIT_MODIFYING_OUTPUT_REGISTER 0
#define PIO_3_CAPTURE 0
#define PIO_3_DATA_WIDTH 32
#define PIO_3_DO_TEST_BENCH_WIRING 0
#define PIO_3_DRIVEN_SIM_VALUE 0
#define PIO_3_EDGE_TYPE "NONE"
#define PIO_3_FREQ 27000000
#define PIO_3_HAS_IN 0
#define PIO_3_HAS_OUT 1
#define PIO_3_HAS_TRI 0
#define PIO_3_IRQ -1
#define PIO_3_IRQ_INTERRUPT_CONTROLLER_ID -1
#define PIO_3_IRQ_TYPE "NONE"
#define PIO_3_NAME "/dev/pio_3"
#define PIO_3_RESET_VALUE 0
#define PIO_3_SPAN 16
#define PIO_3_TYPE "altera_avalon_pio"


/*
 * pio_4 configuration
 *
 */

#define ALT_MODULE_CLASS_pio_4 altera_avalon_pio
#define PIO_4_BASE 0x8210c0
#define PIO_4_BIT_CLEARING_EDGE_REGISTER 0
#define PIO_4_BIT_MODIFYING_OUTPUT_REGISTER 0
#define PIO_4_CAPTURE 0
#define PIO_4_DATA_WIDTH 32
#define PIO_4_DO_TEST_BENCH_WIRING 0
#define PIO_4_DRIVEN_SIM_VALUE 0
#define PIO_4_EDGE_TYPE "NONE"
#define PIO_4_FREQ 27000000
#define PIO_4_HAS_IN 0
#define PIO_4_HAS_OUT 1
#define PIO_4_HAS_TRI 0
#define PIO_4_IRQ -1
#define PIO_4_IRQ_INTERRUPT_CONTROLLER_ID -1
#define PIO_4_IRQ_TYPE "NONE"
#define PIO_4_NAME "/dev/pio_4"
#define PIO_4_RESET_VALUE 0
#define PIO_4_SPAN 16
#define PIO_4_TYPE "altera_avalon_pio"


/*
 * pio_5 configuration
 *
 */

#define ALT_MODULE_CLASS_pio_5 altera_avalon_pio
#define PIO_5_BASE 0x8210b0
#define PIO_5_BIT_CLEARING_EDGE_REGISTER 0
#define PIO_5_BIT_MODIFYING_OUTPUT_REGISTER 0
#define PIO_5_CAPTURE 0
#define PIO_5_DATA_WIDTH 32
#define PIO_5_DO_TEST_BENCH_WIRING 0
#define PIO_5_DRIVEN_SIM_VALUE 0
#define PIO_5_EDGE_TYPE "NONE"
#define PIO_5_FREQ 27000000
#define PIO_5_HAS_IN 0
#define PIO_5_HAS_OUT 1
#define PIO_5_HAS_TRI 0
#define PIO_5_IRQ -1
#define PIO_5_IRQ_INTERRUPT_CONTROLLER_ID -1
#define PIO_5_IRQ_TYPE "NONE"
#define PIO_5_NAME "/dev/pio_5"
#define PIO_5_RESET_VALUE 0
#define PIO_5_SPAN 16
#define PIO_5_TYPE "altera_avalon_pio"


/*
 * pio_6 configuration
 *
 */

#define ALT_MODULE_CLASS_pio_6 altera_avalon_pio
#define PIO_6_BASE 0x8210a0
#define PIO_6_BIT_CLEARING_EDGE_REGISTER 0
#define PIO_6_BIT_MODIFYING_OUTPUT_REGISTER 0
#define PIO_6_CAPTURE 0
#define PIO_6_DATA_WIDTH 32
#define PIO_6_DO_TEST_BENCH_WIRING 0
#define PIO_6_DRIVEN_SIM_VALUE 0
#define PIO_6_EDGE_TYPE "NONE"
#define PIO_6_FREQ 27000000
#define PIO_6_HAS_IN 0
#define PIO_6_HAS_OUT 1
#define PIO_6_HAS_TRI 0
#define PIO_6_IRQ -1
#define PIO_6_IRQ_INTERRUPT_CONTROLLER_ID -1
#define PIO_6_IRQ_TYPE "NONE"
#define PIO_6_NAME "/dev/pio_6"
#define PIO_6_RESET_VALUE 0
#define PIO_6_SPAN 16
#define PIO_6_TYPE "altera_avalon_pio"


/*
 * pio_7 configuration
 *
 */

#define ALT_MODULE_CLASS_pio_7 altera_avalon_pio
#define PIO_7_BASE 0x821090
#define PIO_7_BIT_CLEARING_EDGE_REGISTER 0
#define PIO_7_BIT_MODIFYING_OUTPUT_REGISTER 0
#define PIO_7_CAPTURE 0
#define PIO_7_DATA_WIDTH 32
#define PIO_7_DO_TEST_BENCH_WIRING 0
#define PIO_7_DRIVEN_SIM_VALUE 0
#define PIO_7_EDGE_TYPE "NONE"
#define PIO_7_FREQ 27000000
#define PIO_7_HAS_IN 1
#define PIO_7_HAS_OUT 0
#define PIO_7_HAS_TRI 0
#define PIO_7_IRQ -1
#define PIO_7_IRQ_INTERRUPT_CONTROLLER_ID -1
#define PIO_7_IRQ_TYPE "NONE"
#define PIO_7_NAME "/dev/pio_7"
#define PIO_7_RESET_VALUE 0
#define PIO_7_SPAN 16
#define PIO_7_TYPE "altera_avalon_pio"


/*
 * pio_8 configuration
 *
 */

#define ALT_MODULE_CLASS_pio_8 altera_avalon_pio
#define PIO_8_BASE 0x821080
#define PIO_8_BIT_CLEARING_EDGE_REGISTER 0
#define PIO_8_BIT_MODIFYING_OUTPUT_REGISTER 0
#define PIO_8_CAPTURE 0
#define PIO_8_DATA_WIDTH 32
#define PIO_8_DO_TEST_BENCH_WIRING 0
#define PIO_8_DRIVEN_SIM_VALUE 0
#define PIO_8_EDGE_TYPE "NONE"
#define PIO_8_FREQ 27000000
#define PIO_8_HAS_IN 1
#define PIO_8_HAS_OUT 0
#define PIO_8_HAS_TRI 0
#define PIO_8_IRQ -1
#define PIO_8_IRQ_INTERRUPT_CONTROLLER_ID -1
#define PIO_8_IRQ_TYPE "NONE"
#define PIO_8_NAME "/dev/pio_8"
#define PIO_8_RESET_VALUE 0
#define PIO_8_SPAN 16
#define PIO_8_TYPE "altera_avalon_pio"


/*
 * timer_0 configuration
 *
 */

#define ALT_MODULE_CLASS_timer_0 altera_avalon_timer
#define TIMER_0_ALWAYS_RUN 0
#define TIMER_0_BASE 0x821000
#define TIMER_0_COUNTER_SIZE 32
#define TIMER_0_FIXED_PERIOD 0
#define TIMER_0_FREQ 27000000
#define TIMER_0_IRQ 0
#define TIMER_0_IRQ_INTERRUPT_CONTROLLER_ID 0
#define TIMER_0_LOAD_VALUE 26
#define TIMER_0_MULT 1.0E-6
#define TIMER_0_NAME "/dev/timer_0"
#define TIMER_0_PERIOD 1
#define TIMER_0_PERIOD_UNITS "us"
#define TIMER_0_RESET_OUTPUT 0
#define TIMER_0_SNAPSHOT 1
#define TIMER_0_SPAN 32
#define TIMER_0_TICKS_PER_SEC 1000000
#define TIMER_0_TIMEOUT_PULSE_OUTPUT 0
#define TIMER_0_TYPE "altera_avalon_timer"

#endif /* __SYSTEM_H_ */
