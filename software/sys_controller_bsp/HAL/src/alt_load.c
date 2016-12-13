/******************************************************************************
*                                                                             *
* License Agreement                                                           *
*                                                                             *
* Copyright (c) 2004-2005 Altera Corporation, San Jose, California, USA.      *
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

#include "sys/alt_load.h"
#include "sys/alt_cache.h"

/*
 * Linker defined symbols.
   These used to be
 *    extern alt_u32 __flash_rwdata_start;
 *    extern alt_u32 __ram_rwdata_start;
 *    extern alt_u32 __ram_rwdata_end;
 * but that results in a fatal error when compiling -mgpopt=global
 * because gcc assumes they are normal C variables in .sdata
 * and therefore addressable from gp using a 16-bit offset,
 * when in fact they are special values defined by linker.x
 * and located nowhere near .sdata. 
 * Specifying __attribute__((section(".data"))) will force these
 * in .data. (CASE:258384.)
 */

extern alt_u32 __flash_rwdata_start __attribute__((section(".data")));
extern alt_u32 __ram_rwdata_start __attribute__((section(".data")));
extern alt_u32 __ram_rwdata_end __attribute__((section(".data")));
extern alt_u32 __flash_rodata_start __attribute__((section(".data")));
extern alt_u32 __ram_rodata_start __attribute__((section(".data")));
extern alt_u32 __ram_rodata_end __attribute__((section(".data")));
extern alt_u32 __flash_exceptions_start __attribute__((section(".data")));  
extern alt_u32 __ram_exceptions_start __attribute__((section(".data")));
extern alt_u32 __ram_exceptions_end __attribute__((section(".data")));

/*
 * alt_load() is called when the code is executing from flash. In this case
 * there is no bootloader, so this application is responsible for loading to
 * RAM any sections that are required.
 */  

void alt_load (void)
{
  /* 
   * Copy the .rwdata section. 
   */

  alt_load_section (&__flash_rwdata_start, 
		               &__ram_rwdata_start,
		               &__ram_rwdata_end);

  /*
   * Copy the exception handler.
   */

  alt_load_section (&__flash_exceptions_start, 
		                &__ram_exceptions_start,
		                &__ram_exceptions_end);

  /*
   * Copy the .rodata section.
   */

  alt_load_section (&__flash_rodata_start, 
		                &__ram_rodata_start,
		                &__ram_rodata_end);
  
  /*
   * Now ensure that the caches are in synch.
   */
  
  alt_dcache_flush_all();
  alt_icache_flush_all();
}
