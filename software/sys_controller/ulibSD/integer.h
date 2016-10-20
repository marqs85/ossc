/*
 *  File: integer.h
 *  Author: Nelson Lombardo
 *  Year: 2015
 *  e-mail: nelson.lombardo@gmail.com
 *  License at the end of file.
 */
 
/*****************************************************************************/
/* Integer type definitions                                                  */
/*****************************************************************************/
#ifndef _INTEGER_H_
#define _INTEGER_H_

#include <stdint.h>
#include <typedef.h>
#if 0
/* 16-bit, 32-bit or larger integer */
typedef int16_t         INT;
typedef uint16_t        UINT;

/* 8-bit integer */
typedef int8_t          CHAR;
typedef uint8_t         UCHAR;
typedef uint8_t         BYTE;
typedef uint8_t         BOOL;

/* 16-bit integer */
typedef int16_t         SHORT;
typedef uint16_t        USHORT;
typedef uint16_t        WORD;
typedef uint16_t        WCHAR;

/* 32-bit integer */
typedef int32_t         LONG;
typedef uint32_t        ULONG;
typedef uint32_t        DWORD;

/* Boolean type */
typedef enum { FALSE = 0, TRUE } BOOLEAN;
#endif
typedef enum { LOW = 0, HIGH } THROTTLE;
#endif

// «integer.h» is part of:
/*----------------------------------------------------------------------------/
/  ulibSD - Library for SD cards semantics            (C)Nelson Lombardo, 2015
/-----------------------------------------------------------------------------/
/ ulibSD library is a free software that opened under license policy of
/ following conditions.
/
/ Copyright (C) 2015, ChaN, all right reserved.
/
/ 1. Redistributions of source code must retain the above copyright notice,
/    this condition and the following disclaimer.
/
/ This software is provided by the copyright holder and contributors "AS IS"
/ and any warranties related to this software are DISCLAIMED.
/ The copyright owner or contributors be NOT LIABLE for any damages caused
/ by use of this software.
/----------------------------------------------------------------------------*/

// Derived from Mister Chan works on FatFs code (http://elm-chan.org/fsw/ff/00index_e.html):
/*----------------------------------------------------------------------------/
/  FatFs - FAT file system module  R0.11                 (C)ChaN, 2015
/-----------------------------------------------------------------------------/
/ FatFs module is a free software that opened under license policy of
/ following conditions.
/
/ Copyright (C) 2015, ChaN, all right reserved.
/
/ 1. Redistributions of source code must retain the above copyright notice,
/    this condition and the following disclaimer.
/
/ This software is provided by the copyright holder and contributors "AS IS"
/ and any warranties related to this software are DISCLAIMED.
/ The copyright owner or contributors be NOT LIABLE for any damages caused
/ by use of this software.
/----------------------------------------------------------------------------*/
