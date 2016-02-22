/*
 * alt_sys_init.c - HAL initialization source
 *
 * Machine generated for CPU 'nios2_qsys_0' in SOPC Builder design 'sys'
 * SOPC Builder design path: ../../sys.sopcinfo
 *
 * Generated: Wed Dec 30 21:13:11 EET 2015
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

#include "system.h"
#include "sys/alt_irq.h"
#include "sys/alt_sys_init.h"

#include <stddef.h>

/*
 * Device headers
 */

#include "altera_nios2_gen2_irq.h"
#include "Altera_UP_SD_Card_Avalon_Interface_mod.h"
#include "altera_avalon_jtag_uart.h"
#include "altera_epcq_controller_mod.h"
#include "i2c_opencores.h"

/*
 * Allocate the device storage
 */

ALTERA_NIOS2_GEN2_IRQ_INSTANCE ( NIOS2_QSYS_0, nios2_qsys_0);
ALTERA_AVALON_JTAG_UART_INSTANCE ( JTAG_UART_0, jtag_uart_0);
ALTERA_EPCQ_CONTROLLER_MOD_AVL_MEM_AVL_CSR_INSTANCE ( EPCQ_CONTROLLER_0, EPCQ_CONTROLLER_0_AVL_MEM, EPCQ_CONTROLLER_0_AVL_CSR, epcq_controller_0);
ALTERA_UP_SD_CARD_AVALON_INTERFACE_MOD_INSTANCE ( ALTERA_UP_SD_CARD_AVALON_INTERFACE_0, Altera_UP_SD_Card_Avalon_Interface_0);
I2C_OPENCORES_INSTANCE ( I2C_OPENCORES_0, i2c_opencores_0);

/*
 * Initialize the interrupt controller devices
 * and then enable interrupts in the CPU.
 * Called before alt_sys_init().
 * The "base" parameter is ignored and only
 * present for backwards-compatibility.
 */

void alt_irq_init ( const void* base )
{
    ALTERA_NIOS2_GEN2_IRQ_INIT ( NIOS2_QSYS_0, nios2_qsys_0);
    alt_irq_cpu_enable_interrupts();
}

/*
 * Initialize the non-interrupt controller devices.
 * Called after alt_irq_init().
 */

void alt_sys_init( void )
{
    ALTERA_AVALON_JTAG_UART_INIT ( JTAG_UART_0, jtag_uart_0);
    ALTERA_EPCQ_CONTROLLER_MOD_INIT ( EPCQ_CONTROLLER_0, epcq_controller_0);
    ALTERA_UP_SD_CARD_AVALON_INTERFACE_MOD_INIT ( ALTERA_UP_SD_CARD_AVALON_INTERFACE_0, Altera_UP_SD_Card_Avalon_Interface_0);
    I2C_OPENCORES_INIT ( I2C_OPENCORES_0, i2c_opencores_0);
}
