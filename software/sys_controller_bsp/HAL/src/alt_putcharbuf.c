/******************************************************************************
*                                                                             *
* License Agreement                                                           *
*                                                                             *
* Copyright (c) 2015 Altera Corporation, San Jose, California, USA.           *
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


#ifdef ALT_SEMIHOSTING
#include "sys/alt_stdio.h"
#include "unistd.h"

#ifndef ALT_PUTBUF_SIZE
#define ALT_PUTBUF_SIZE 64
#endif

// Buffer for the printed chars
static char buf[ALT_PUTBUF_SIZE] ={0};
// index into the buffer
static unsigned int fill_index;

/* 
 * ALT putcharbuf funtion
 * Used only for semihosting. 
 * Not thread safe!
 * This fucntion buffers up chars to be printed until either alt_putbufflush()
 * is called or the buffer is full.
 * It is called by alt_printf when semihosting is turned on
 * Its purpose is to minimize the number of Break 1 issuesd by the semihosting
 * libraries. 
 */
int 
alt_putcharbuf(int c)
{
    buf[fill_index++] = (char)(c & 0xff);
    if(fill_index >= ALT_PUTBUF_SIZE)
        alt_putbufflush();
    return c;
}

/*
 * ALT putbufflush 
 * used only for smehosting
 * Not thread safe!
 * Dumps all the chars in the buffer to STDOUT
 */
int 
alt_putbufflush()
{
    int results;
    results = write(STDOUT_FILENO,buf,fill_index);
    fill_index = 0;
    return results;
}
#endif
