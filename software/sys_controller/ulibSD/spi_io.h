/*
 *  spi_io.h
 *  Author: Nelson Lombardo (C) 2015
 *  e-mail: nelson.lombardo@gmail.com
 *  License at the end of file.
 */

#ifndef _SPI_IO_H_
#define _SPI_IO_H_

#include "sysconfig.h"
#include "integer.h"        /* Type redefinition for portability */


/******************************************************************************
 Public methods
 *****************************************************************************/

/**
    \brief Initialize SPI hardware
 */
void SPI_Init (void);

/**
    \brief Read sequence of bytes
    \param *rd Pointer to array where read bytes are written.
    \param len Length of the array.
 */
void SPI_R (BYTE *rd, int len);

/**
    \brief Write sequence of bytes
    \param *wd Pointer to array which holds the bytes.
    \param len Length of the array.
 */
void SPI_W (BYTE *wd, int len);

/**
    \brief Read a single byte.
    \param d Byte. Ignored.
    \return Byte that arrived.
 */
BYTE SPI_RW (BYTE d);

/**
    \brief Write a single byte.
    \param d Byte to write.
 */
void SPI_WW(BYTE d);

/**
    \brief Flush of SPI buffer.
 */
void SPI_Release (void);

/**
    \brief Selecting function in SPI terms, associated with SPI module.
 */
void SPI_CS_Low (void);

/**
    \brief Deselecting function in SPI terms, associated with SPI module.
 */
void SPI_CS_High (void);

/**
    \brief Setting frequency of SPI's clock to maximun possible.
 */
void SPI_Freq_High (void);

/**
    \brief Setting frequency of SPI's clock equal or lower than 400kHz.
 */
void SPI_Freq_Low (void);

/**
    \brief Start a non-blocking timer.
    \param ms Milliseconds.
 */
int SPI_Timer_On (WORD ms);

/**
    \brief Check the status of non-blocking timer.
    \return Status, TRUE if timeout is not reach yet.
 */
BOOL SPI_Timer_Status (void);

/**
    \brief Stop of non-blocking timer. Mandatory.
 */
void SPI_Timer_Off (void);

#endif

/*
The MIT License (MIT)

Copyright (c) 2015 Nelson Lombardo

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
*/
