//
// Copyright (C) 2018  Markus Hiienkari <mhiienka@niksula.hut.fi>
//
// This file is part of Open Source Scan Converter project.
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//

#include <stdio.h>
#include <stdarg.h>
#include "sys/alt_stdio.h"
#include "utils.h"
#include "system.h"
#include "sysconfig.h"
#include "io.h"

inline unsigned char bitswap8(unsigned char v)
{
    return ((v * 0x0802LU & 0x22110LU) |
            (v * 0x8020LU & 0x88440LU)) * 0x10101LU >> 16;
}

alt_u32 bswap32(alt_u32 w)
{
    return (((w << 24) & 0xff000000) |
            ((w <<  8) & 0x00ff0000) |
            ((w >>  8) & 0x0000ff00) |
            ((w >> 24) & 0x000000ff));
}

unsigned long crc32(unsigned char *input_data, unsigned long input_data_length, int do_initialize)
{
    unsigned long index;

    /* copy of the data buffer pointer so that it can advance by different widths */
    void * input_data_copy = (void *)input_data;

    /* The custom instruction CRC will initialize to the inital remainder value */
    if (do_initialize)
        IOWR_32DIRECT(HW_CRC32_0_BASE, 0x0, 0x0);

    /* Write 32 bit data to the custom instruction.  If the buffer does not end
    * on a 32 bit boundary then the remaining data will be sent to the custom
    * instruction in the 'if' statement below.
    */
    for(index = 0; index < (input_data_length & 0xFFFFFFFC); index+=4)
    {
        IOWR_32DIRECT(HW_CRC32_0_BASE, 0x4, *(unsigned long *)input_data_copy);
        input_data_copy += 4;  /* void pointer, must move by 4 for each word */
    }

    /* Write the remainder of the buffer if it does not end on a word boundary */
    if((input_data_length & 0x3) == 0x3)  /* 3 bytes left */
    {
        IOWR_16DIRECT(HW_CRC32_0_BASE, 0x4, *(unsigned short *)input_data_copy);
        input_data_copy += 2;
        IOWR_8DIRECT(HW_CRC32_0_BASE, 0x4, *(unsigned char *)input_data_copy);
    }
    else if((input_data_length & 0x3) == 0x2) /* 2 bytes left */
    {
        IOWR_16DIRECT(HW_CRC32_0_BASE, 0x4, *(unsigned short *)input_data_copy);
    }
    else if((input_data_length & 0x3) == 0x1) /* 1 byte left */
    {
        IOWR_8DIRECT(HW_CRC32_0_BASE, 0x4, *(unsigned char *)input_data_copy);
    }

    /* There are 4 registers in the CRC custom instruction.  Since
    * this example uses CRC-32 only the first register must be read
    * in order to receive the full result.
    */
    return IORD_32DIRECT(HW_CRC32_0_BASE, 0x10);
}

/* printf for direct driver interface */
int dd_printf(const char *__restrict fmt, ...) {
    int ret;
    va_list ap;
    char buf[PRINTF_BUFSIZE];

    va_start(ap, fmt);
    ret = vsnprintf(buf, PRINTF_BUFSIZE, fmt, ap);
    va_end(ap);
    if (ret > 0)
        alt_putstr(buf);

    return 0;
}
