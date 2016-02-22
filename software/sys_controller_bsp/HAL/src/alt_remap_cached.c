/******************************************************************************
*                                                                             *
* License Agreement                                                           *
*                                                                             *
* Copyright (c) 2003,2007 Altera Corporation, San Jose, California, USA.      *
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

#include "sys/alt_warning.h"
#include "sys/alt_cache.h"
#include "system.h"

/*
 * Convert a pointer to a block of uncached memory into a block of cached memory.
 * Return a pointer that should be used to access the cached memory.
 */

void* 
alt_remap_cached(volatile void* ptr, alt_u32 len)
{
#if ALT_CPU_DCACHE_SIZE > 0
#ifdef ALT_CPU_DCACHE_BYPASS_MASK
  return (void*) (((alt_u32)ptr) & ~ALT_CPU_DCACHE_BYPASS_MASK);
#else /* No address mask option enabled. */
  /* Generate a link time error, should this function ever be called. */
  ALT_LINK_ERROR("alt_remap_cached() is not available because CPU is not configured to use bit 31 of address to bypass data cache");
  return NULL;
#endif /* No address mask option enabled. */
#else /* No data cache */
  /* Nothing needs to be done to the pointer. */
  return (void*) ptr;
#endif /* No data cache */
}
