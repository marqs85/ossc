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
* Altera does not recommend, suggest or require that this reference design    *
* file be used in conjunction or combination with any other product.          *
******************************************************************************/
#include "io.h"
#include "sys/alt_exceptions.h"
#include "sys/alt_cache.h"

/*
 * This file implements support for calling a user-registered handler
 * when a likely fatal ECC error exception occurs.
 */

/* 
 * Global variable containing address to jump to when likely fatal
 * ECC error exception occurs.
 */
alt_u32 alt_exception_ecc_fatal_handler = 0x0;

/*
 * Pull in the exception entry assembly code. This will not be linked in 
 * unless this object is linked into the executable (i.e. only if 
 * alt_ecc_fatal_exception_register() is called).
 */
__asm__( "\n\t.globl alt_exception" );

/*
 * alt_ecc_fatal_exception_register() is called to register a handler to
 * service likely fatal ECC error exceptions. 
 * 
 * Passing null (0x0) in the handler argument will disable a previously-
 * registered handler.
 *
 * Note that if no handler is registered, just normal exception processing
 * occurs on a likely fatal ECC exception and the exception processing
 * code might trigger an infinite exception loop.
 */
void 
alt_ecc_fatal_exception_register(alt_u32 handler)
{
    alt_exception_ecc_fatal_handler = handler;

    /* 
     * Flush this from the cache. Required because the exception handler uses ldwio
     * to read this value to avoid trigger another data cache ECC error exception.
     */
    alt_dcache_flush(&alt_exception_ecc_fatal_handler, sizeof(alt_exception_ecc_fatal_handler));
}
