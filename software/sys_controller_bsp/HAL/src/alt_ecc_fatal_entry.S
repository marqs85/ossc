/******************************************************************************
*                                                                             *
* License Agreement                                                           *
*                                                                             *
* Copyright (c) 2013 Altera Corporation, San Jose, California, USA.           *
* All rights reserved.                                                        *
*                                                                             *
* Permission is hereby granted, free of charge, to any person obtaining a     *
* copy of this software and associated documentation files (the "Software"),  *
* to deal in the Software without restriction, including without limitation   *
* the rights to use, copy, modify, merge, publish, distribute, sublicense,    *
* and/or sell copies of the Software, and to permit persons to whom the       *
* Software is furnished to do so, subject to the following conditions:        *
*                                                                             *
* The above copyright notice and this permission notice shall be included in  *
* all copies or substantial portions of the Software.                         *
*                                                                             *
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR  *
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,    *
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE *
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER      *
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING     *
* FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER         *
* DEALINGS IN THE SOFTWARE.                                                   *
*                                                                             *
* This agreement shall be governed in all respects by the laws of the State   *
* of California and by the laws of the United States of America.              *
*                                                                             *
******************************************************************************/

/*
 * This is the code called at the beginning of the exception handler
 * to detect a likely fatal ECC error exception and then jump to
 * user-provided code to handle it.
 *
 * This code is pulled in from a .globl in alt_ecc_fatal_exception.c.
 * This scheme is used so that if a handler is never registered, then this
 * code will not appear in the generated executable, thereby improving 
 * code footprint.
 *
 * This code is located in its own section that the linker script
 * explicitly mentions and ensures it gets linked at the beginning
 * of the exception handler.
 */

        /*
         * Pull in the exception handler register save code.
         */
        .globl alt_exception

        .section .exceptions.entry.ecc_fatal, "xa"

        /*
         * This might be handling an unrecoverable ECC error exception
         * in the register file and/or data cache.
         * Must avoid reading registers or performing load/store instructions
         * before this is determined because they could trigger another 
         * unrecoverable ECC error exception and create an infinite loop.
         *
         * The EXCEPTION register is always present when ECC is present.
         * Bit 31 of this register indicates that there was an unrecoverable 
         * ECC error exception in the register file and/or data cache.
         * Test this (using blt to check sign bit) to determine if this is
         * what we are dealing with.  Otherwise, just do normal processing.
         *
         * Jump to an application-provided routine to handle this condition.
         * Pass in the return address in the et register in case this code
         * can clean up the ECC error and then return here (unlikely).
         *
         * Runtime stack checking can't be enabled when ECC is present
         * because they both want to use the et register.
         */
        rdctl et, exception
        bge et, r0, alt_exception_not_ecc_fatal /* Not ECCFTL if bit 31 is 0 */

        /*
         * Load ECC fatal handler pointer into et register.
         * Using a ldwio is safe because it completely bypasses the data cache.
         */
        movhi et, %hi(alt_exception_ecc_fatal_handler)
        ori   et, et, %lo(alt_exception_ecc_fatal_handler) 
        ldwio et, 0(et)
 
        /* 
         * If ECC fatal handler pointer is not 0, assume a handler
         * has been provided by the application.
         */
        beq et, r0, alt_exception_not_ecc_fatal
        
        /*
         * The et register contains the address of the ECC fatal handler.
         * Jump to this address to invoke the handler.
         */
        jmp et

        /*
         * An ECC fatal handler can jump to this label if it able
         * to recover from the fatal error (rare) and wants to continue
         * with normal exception processing.
         */
.globl alt_exception_not_ecc_fatal
alt_exception_not_ecc_fatal:
