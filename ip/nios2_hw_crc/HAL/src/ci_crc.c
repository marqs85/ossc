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


/**********************************************************************
 *
 * Filename:    ci_crc.c
 * 
 * Description: Custom instruction implementations of the CRC.
 *
 * Notes:       A macro is defined that is used to access the CRC custom  
 *              instruction.
 *********************************************************************/
 
#include "system.h"

/*The n values and their corresponding operation are as follow:
 * n = 0, Initialize the custom instruction to the initial remainder value
 * n = 1, Write  8 bits data to custom instruction
 * n = 2, Write 16 bits data to custom instruction
 * n = 3, Write 32 bits data to custom instruction
 * n = 4, Read  32 bits data from the custom instruction
 * n = 5, Read  64 bits data from the custom instruction
 * n = 6, Read  96 bits data from the custom instruction
 * n = 7, Read 128 bits data from the custom instruction*/
#define CRC_CI_MACRO(n, A)        __builtin_custom_ini(ALT_CI_NIOS2_HW_CRC32_0_N + (n & 0x7), (A))

unsigned long crcCI(unsigned char * input_data, unsigned long input_data_length, int do_initialize)
{
  unsigned long index;
  /* copy of the data buffer pointer so that it can advance by different widths */
  void * input_data_copy = (void *)input_data;

  /* The custom instruction CRC will initialize to the inital remainder value */    
  if (do_initialize)
    CRC_CI_MACRO(0,0);
  
  /* Write 32 bit data to the custom instruction.  If the buffer does not end
   * on a 32 bit boundary then the remaining data will be sent to the custom
   * instruction in the 'if' statement below.
   */
  for(index = 0; index < (input_data_length & 0xFFFFFFFC); index+=4)
  {
    CRC_CI_MACRO(3, *(unsigned long *)input_data_copy);
    input_data_copy += 4;  /* void pointer, must move by 4 for each word */
  }

  /* Write the remainder of the buffer if it does not end on a word boundary */  
  if((input_data_length & 0x3) == 0x3)  /* 3 bytes left */
  {
    CRC_CI_MACRO(2, *(unsigned short *)input_data_copy);
    input_data_copy += 2;
    CRC_CI_MACRO(1, *(unsigned char *)input_data_copy);
  }
  else if((input_data_length & 0x3) == 0x2) /* 2 bytes left */
  {
    CRC_CI_MACRO(2, *(unsigned short *)input_data_copy);    
  }
  else if((input_data_length & 0x3) == 0x1) /* 1 byte left */
  {
    CRC_CI_MACRO(1, *(unsigned char *)input_data_copy);    
  }
  
  /* There are 4 registers in the CRC custom instruction.  Since
   * this example uses CRC-32 only the first register must be read
   * in order to receive the full result.
   */
  return CRC_CI_MACRO(4, 0);
}
