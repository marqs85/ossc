/*
 * Copyright (c) 2003-2004 Altera Corporation, San Jose, California, USA.  
 * All rights reserved.
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to 
 * deal in the Software without restriction, including without limitation the 
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is 
 * furnished to do so, subject to the following conditions:
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
 * ------------
 *
 * Altera does not recommend, suggest or require that this reference design 
 * file be used in conjunction or combination with any other product.
 *
 * alt_busy_sleep.c - Microsecond delay routine which uses a calibrated busy
 *                    loop to perform the delay. This is used to implement
 *                    usleep for both uC/OS-II and the standalone HAL.  
 *
 * Author PRR
 *
 * Calibrated delay with no timer required
 * 
 * The ASM instructions in the routine are equivalent to 
 *
 * for (i=0;i<us*(ALT_CPU_FREQ/3);i++);
 * 
 * and takes three cycles each time around the loop
 *
 */

#include <limits.h>
#include <string.h>

#include "system.h"
#include "alt_types.h"

#include "priv/alt_busy_sleep.h"

unsigned int alt_busy_sleep (unsigned int us)
{
/*
 * Only delay if ALT_SIM_OPTIMIZE is not defined; i.e., if software
 * is built targetting ModelSim RTL simulation, the delay will be
 * skipped to speed up simulation.
 */
#ifndef ALT_SIM_OPTIMIZE
  unsigned long i, loops;

  // 1 loop >= 7 cyc
  loops = ((ALT_CPU_FREQ/1000000)*us)/7;

  for (i=7; i<loops; i++)
    asm volatile ("nop");
#endif /* #ifndef ALT_SIM_OPTIMIZE */
  return 0;
}
