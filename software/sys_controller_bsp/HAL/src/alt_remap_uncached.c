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
 * Convert a pointer to a block of cached memory into a block of uncached memory.
 * Return a pointer that should be used to access the uncached memory.
 *
 * This routine was created for Nios II Gen1 cores which allow mixing cacheable and
 * uncachable data in the same data cache line. So, they could take any memory region
 * and make it uncached. However, Nios II Gen2 cores don't support mixing cacheable
 * and uncachable data in the same data cache line so require the memory region to
 * be aligned to a cache line boundary and must be an integer number of cache line
 * bytes in size. So, software on a Nios II Gen2 core shouldn't really be using this
 * function so it fails with a link error.
 */

volatile void* 
alt_remap_uncached(void* ptr, alt_u32 len)
{
  /* Generate a link time error, should this function ever be called. */
  ALT_LINK_ERROR("alt_remap_uncached() is not available because Nios II Gen2 cores with data caches don't support mixing cacheable and uncacheable data on the same line.");
  return NULL;
}
