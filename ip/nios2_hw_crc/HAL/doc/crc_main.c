/******************************************************************************
*                                                                             *
* License Agreement                                                           *
*                                                                             *
* Copyright (c) 2008 Altera Corporation, San Jose, California, USA.           *
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
* Altera does not recommend, suggest or require that this reference design    *
* file be used in conjunction or combination with any other product.          *
******************************************************************************/


/******************************************************************************
* Author - JCJB                                                               *
*                                                                             *
* This design uses the following CRC-32 implementations:                      *
*                                                                             *
* --> Software - Uses modulo 2 division to perform the remainder calculation. *
* --> Optimized Software - Uses a lookup table of all possible division       *
*                          values.  The calculation operates on 8 bit data.   *
* --> Custom Instruction - Uses a parallel hardware CRC circuit to calculate  *
*                          the remainder.  The calculation operates on 8,     *
*                          16, 24, or 32 bit data.                            *
*                                                                             *
* The software implementations can be changed to CRC-16 or CRC-CCITT however  *
* the custom instruction must be modified as well to support the same         *
* standard.  Simply use the values defined in crc.h to change the standard    *
* used (using the same values in the hardware parameterization) or define     *
* your own standard.                                                          * 
*******************************************************************************/

#include "system.h"
#include "stdio.h"
#include "crc.h"
#include "ci_crc.h"
#include "sys/alt_timestamp.h"
#include "stdlib.h"


/* Modify these values to adjust the test being performed */
#define NUMBER_OF_BUFFERS 32
#define BUFFER_SIZE 256 /* in bytes */

/* Change the name of memory device according to what you are using
 *  e.g.: DDR_SDRAM_0 ##_SPAN
 *        SSRAM_0 ##_SPAN                                        
 */
#define MEMORY_DEVICE_SIZE 32768


/* Make sure there is room left for Nios II text, rodata, rwdata, stack,
 * and heap.  This software and the buffer space must fit within the
 * size of memory device.  A total of 1.5 MBytes is reserved. If BUFFER_SIZE
 * is a multiple of four then exactly 256kB will be left, otherwise is 
 * amount will be less since the column dimension needs some padding to 
 * stay 32 bit aligned
 */
#if ((BUFFER_SIZE * NUMBER_OF_BUFFERS) >= MEMORY_DEVICE_SIZE - 10000)
  #error Your buffer space has exceeded the maximum allowable space.  Please\
         reduce the buffer space so that there is enough room to hold Nios II\
         code.
#endif


/* This will line up the data onto a 32 bit (or greater) boundary.  A 2d array
 * is being used here for simplicity.  The first dimension represents a byte
 * of data and the second dimension represents an individual buffer 
 */
#if ((BUFFER_SIZE & 0x3) == 0)
  unsigned char data_buffer_region[NUMBER_OF_BUFFERS][BUFFER_SIZE] __attribute__ ((aligned(4)));
#else /* need to allocate extra bytes so that all buffers start on a 32 bit
         boundaries by rounding up the column dimension to the next power of 4
       */
  unsigned char data_buffer_region[NUMBER_OF_BUFFERS][BUFFER_SIZE + 4 - (BUFFER_SIZE&0x3)] __attribute__ ((aligned(4)));  
#endif





int main()
{
  unsigned long buffer_counter, data_counter;
  unsigned long sw_slow_results[NUMBER_OF_BUFFERS];
  unsigned long sw_fast_results[NUMBER_OF_BUFFERS];
  unsigned long ci_results[NUMBER_OF_BUFFERS];
  unsigned char random_data = 0x5A;
  //unsigned long sw_slow_timeA, sw_slow_timeB;
 // unsigned long sw_fast_timeA, sw_fast_timeB;
 // unsigned long ci_timeA, ci_timeB;

  alt_u32 sw_slow_timeA, sw_slow_timeB;
  alt_u32 sw_fast_timeA, sw_fast_timeB;
  alt_u32 ci_timeA, ci_timeB;


  printf("+-----------------------------------------------------------+\n");
  printf("| Comparison between software and custom instruction CRC32  |\n");
  printf("+-----------------------------------------------------------+\n\n\n");
  
  printf("System specification\n");
  printf("--------------------\n");

  printf("System clock speed = %lu MHz\n", (unsigned long)ALT_CPU_FREQ /(unsigned long)1000000);
  printf("Number of buffer locations = %d\n", NUMBER_OF_BUFFERS);
  printf("Size of each buffer = %d bytes\n\n\n", BUFFER_SIZE);


  /* Initializing the data buffers */
  printf("Initializing all of the buffers with pseudo-random data\n");
  printf("-------------------------------------------------------\n");
  for(buffer_counter = 0; buffer_counter < NUMBER_OF_BUFFERS; buffer_counter++)
  {
    for(data_counter = 0; data_counter < BUFFER_SIZE; data_counter++)
    {
      data_buffer_region[buffer_counter][data_counter] = random_data;
      random_data = (random_data >> 4) + (random_data << 4) + (data_counter & 0xFF);
    }
  }
  printf("Initialization completed\n\n\n");


  if(alt_timestamp_start() < 0) // starts the timestamp timer
  {
    printf("Please add the high resolution timer to the timestamp timer setting in the syslib properties page.\n");
    exit(1);
  }


  /* Slow software CRC based on a modulo 2 division implementation */
  printf("Running the software CRC\n");
  printf("------------------------\n");
  sw_slow_timeA = alt_timestamp();

  for(buffer_counter = 0; buffer_counter < NUMBER_OF_BUFFERS; buffer_counter++)
  {
    sw_slow_results[buffer_counter] = crcSlow(data_buffer_region[buffer_counter], BUFFER_SIZE);
  }
 sw_slow_timeB = alt_timestamp();

  printf("Completed\n\n\n");


  /* Fast software CRC based on a lookup table implementation */
  crcInit();
  printf("Running the optimized software CRC\n");
  printf("----------------------------------\n");
  sw_fast_timeA = alt_timestamp();
  for(buffer_counter = 0; buffer_counter < NUMBER_OF_BUFFERS; buffer_counter++)
  {
    sw_fast_results[buffer_counter] = crcFast(data_buffer_region[buffer_counter], BUFFER_SIZE);
  }
 sw_fast_timeB = alt_timestamp();
  printf("Completed\n\n\n");


  /* Custom instruction CRC */
  printf("Running the custom instruction CRC\n");
  printf("----------------------------------\n");
 ci_timeA = alt_timestamp();
  for(buffer_counter = 0; buffer_counter < NUMBER_OF_BUFFERS; buffer_counter++)
  {
    ci_results[buffer_counter] = crcCI(data_buffer_region[buffer_counter], BUFFER_SIZE);
  }
  ci_timeB = alt_timestamp();
  printf("Completed\n\n\n");

  /* Validation of results */  
  printf("Validating the CRC results from all implementations\n");
  printf("----------------------------------------------------\n");
  for(buffer_counter = 0; buffer_counter < NUMBER_OF_BUFFERS; buffer_counter++)
  {
    /* Test every combination of results to make sure they are consistant */
    if((sw_slow_results[buffer_counter] != ci_results[buffer_counter]) | 
       (sw_fast_results[buffer_counter] != ci_results[buffer_counter]))
    {
      printf("FAILURE!  Software CRC = 0x%lx, Optimized Software CRC = 0x%lx, Custom Instruction CRC = 0x%lx,\n",
      sw_slow_results[buffer_counter], sw_fast_results[buffer_counter], ci_results[buffer_counter]);       
      exit(1);     
    }
  }
  printf("All CRC implementations produced the same results\n\n\n");


  // Report processing times
  printf("Processing time for each implementation\n");
  printf("---------------------------------------\n");
  printf("Software CRC = %.2lu ms\n", 1000*((unsigned long)(sw_slow_timeB-sw_slow_timeA))/((unsigned long)alt_timestamp_freq()));
  printf("Optimized software CRC = %.2lu ms\n", 1000*((unsigned long)(sw_fast_timeB-sw_fast_timeA))/((unsigned long)alt_timestamp_freq()));
  printf("Custom instruction CRC = %.2lu ms\n\n\n", 1000*((unsigned long)(ci_timeB-ci_timeA))/((unsigned long)alt_timestamp_freq()));

  printf("Processing throughput for each implementation\n"); // throughput = total bits / (time(s) * 1000000)
  printf("---------------------------------------------\n");
  printf("Software CRC = %.2lu Mbps\n", (8 * NUMBER_OF_BUFFERS * BUFFER_SIZE)/(1000000*(unsigned long)(sw_slow_timeB-sw_slow_timeA)/((unsigned long)alt_timestamp_freq())));
  printf("Optimized software CRC = %.2lu Mbps\n", (8 * NUMBER_OF_BUFFERS * BUFFER_SIZE)/(1000000*(unsigned long)(sw_fast_timeB-sw_fast_timeA)/((unsigned long)alt_timestamp_freq())));
  printf("Custom instruction CRC = %.2lu Mbps\n\n\n", (8 * NUMBER_OF_BUFFERS * BUFFER_SIZE)/(1000000*(unsigned long)(ci_timeB-ci_timeA)/((unsigned long)alt_timestamp_freq())));

  printf("Speedup ratio\n");
  printf("-------------\n");
  printf("Custom instruction CRC vs software CRC = %lu\n", ((unsigned long)(sw_slow_timeB-sw_slow_timeA))/((unsigned long)(ci_timeB-ci_timeA)));
  printf("Custom instruction CRC vs optimized software CRC = %lu\n", ((unsigned long)(sw_fast_timeB-sw_fast_timeA))/((unsigned long)(ci_timeB-ci_timeA)));
  printf("Optimized software CRC vs software CRC= %lu\n", ((unsigned long)(sw_slow_timeB-sw_slow_timeA))/((unsigned long)(sw_fast_timeB-sw_fast_timeA)));
  return 0;
}
