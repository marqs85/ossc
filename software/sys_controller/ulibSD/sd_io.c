/*
 *  File: sd_io.c
 *  Author: Nelson Lombardo
 *  Year: 2015
 *  e-mail: nelson.lombardo@gmail.com
 *  License at the end of file.
 */

#include "sd_io.h"

#ifdef _M_IX86  // For use over x86
/*****************************************************************************/
/* Private Methods Prototypes - Direct work with PC file                     */
/*****************************************************************************/

/**
 * \brief Get the total numbers of sectors in SD card.
 * \param dev Device descriptor.
 * \return Quantity of sectors. Zero if fail.
 */
DWORD __SD_Sectors (SD_DEV* dev);

/*****************************************************************************/
/* Private Methods - Direct work with PC file                                */
/*****************************************************************************/

DWORD __SD_Sectors (SD_DEV *dev)
{
    if (dev->fp == NULL) return(0); // Fail
    else {
        fseek(dev->fp, 0L, SEEK_END);
        return (((DWORD)(ftell(dev->fp)))/((DWORD)512)-1);
    }
}
#else   // For use with uControllers
/******************************************************************************
 Private Methods Prototypes - Direct work with SD card
******************************************************************************/

/**
    \brief Simple function to calculate power of two.
    \param e Exponent.
    \return Math function result.
*/
DWORD __SD_Power_Of_Two(BYTE e);

/**
     \brief Assert the SD card (SPI CS low).
 */
inline void __SD_Assert (void);

/**
    \brief Deassert the SD (SPI CS high).
 */
inline void __SD_Deassert (void);

/**
    \brief Change to max the speed transfer.
    \param throttle
 */
void __SD_Speed_Transfer (BYTE throttle);

/**
    \brief Send SPI commands.
    \param cmd Command to send.
    \param arg Argument to send.
    \return R1 response.
 */
BYTE __SD_Send_Cmd(BYTE cmd, DWORD arg);

/**
    \brief Write a data block on SD card.
    \param dat Storage the data to transfer.
    \param token Inidicates the type of transfer (single or multiple).
 */
SDRESULTS __SD_Write_Block(SD_DEV *dev, void *dat, BYTE token);

/**
    \brief Get the total numbers of sectors in SD card.
    \param dev Device descriptor.
    \return Quantity of sectors. Zero if fail.
 */
DWORD __SD_Sectors (SD_DEV *dev);

/******************************************************************************
 Private Methods - Direct work with SD card
******************************************************************************/

DWORD __SD_Power_Of_Two(BYTE e)
{
    DWORD partial = 1;
    BYTE idx;
    for(idx=0; idx!=e; idx++) partial *= 2;
    return(partial);
}

inline void __SD_Assert(void){
    SPI_CS_Low();
}

inline void __SD_Deassert(void){
    SPI_CS_High();
}

void __SD_Speed_Transfer(BYTE throttle) {
    if(throttle == HIGH) SPI_Freq_High();
    else SPI_Freq_Low();
}

BYTE __SD_Send_Cmd(BYTE cmd, DWORD arg)
{
    BYTE wiredata[10];
    BYTE crc, res;
    int timer_set;

    //printf("Sending SD CMD 0x%x with arg 0x%x\n", cmd, arg);

    // ACMD«n» is the command sequense of CMD55-CMD«n»
    if(cmd & 0x80) {
        cmd &= 0x7F;
        res = __SD_Send_Cmd(CMD55, 0);
        if (res > 1) return (res);
    }

    // Select the card
    __SD_Deassert();
    SPI_R(NULL, 4);
    __SD_Assert();

    // Send complete command set
    wiredata[0] = cmd;                  // Start and command index
    wiredata[1] = (arg >> 24);          // Arg[31-24]
    wiredata[2] = (arg >> 16);          // Arg[23-16]
    wiredata[3] = (arg >> 8 );          // Arg[15-08]
    wiredata[4] = (arg >> 0 );          // Arg[07-00]

    // CRC?
    crc = 0x01;                         // Dummy CRC and stop
    if(cmd == CMD0) crc = 0x95;         // Valid CRC for CMD0(0)
    if(cmd == CMD8) crc = 0x87;         // Valid CRC for CMD8(0x1AA)
    wiredata[5] = crc;

    SPI_W(wiredata, 6);

    // Receive command response
    // Wait for a valid response in timeout of 5 milliseconds
    timer_set = SPI_Timer_On(5);
    do {
        SPI_R(&res, 1);
    } while((res & 0x80)&&(SPI_Timer_Status()==TRUE));
    if (timer_set == 0)
        SPI_Timer_Off();
    // Return with the response value
    //printf("CMD_res: %u\n", res);
    return(res);
}

SDRESULTS __SD_Write_Block(SD_DEV *dev, void *dat, BYTE token)
{
    WORD idx;
    BYTE line;
    // Send token (single or multiple)
    SPI_WW(token);
    // Single block write?
    if(token != 0xFD)
    {
        // Send block data
        for(idx=0; idx!=SD_BLK_SIZE; idx++) SPI_WW(*((BYTE*)dat + idx));
        /* Dummy CRC */
        SPI_WW(0xFF);
        SPI_WW(0xFF);
        // If not accepted, returns the reject error
        if((SPI_RW(0xFF) & 0x1F) != 0x05) return(SD_REJECT);
    }
#ifdef SD_IO_WRITE_WAIT_BLOCKER
    // Waits until finish of data programming (blocked)
    while(SPI_RW(0xFF)==0);
    return(SD_OK);
#else
    // Waits until finish of data programming with a timeout
    SPI_Timer_On(SD_IO_WRITE_TIMEOUT_WAIT);
    do {
        line = SPI_RW(0xFF);
    } while((line==0)&&(SPI_Timer_Status()==TRUE));
    SPI_Timer_Off();
#ifdef SD_IO_DBG_COUNT
    dev->debug.write++;
#endif
    if(line==0) return(SD_BUSY);
    else return(SD_OK);
#endif
}

DWORD __SD_Sectors (SD_DEV *dev)
{
    BYTE csd[18];
    BYTE tkn;
    DWORD ss = 0;
    WORD C_SIZE = 0;
    BYTE C_SIZE_MULT = 0;
    BYTE READ_BL_LEN = 0;
    int timer_set;

    if(__SD_Send_Cmd(CMD9, 0)==0)
    {
        // Wait for response
        timer_set = SPI_Timer_On(5);  // Wait for data packet (timeout of 5ms)
        do {
            SPI_R(&tkn, 1);
        } while((tkn==0xFF)&&(SPI_Timer_Status()==TRUE));
        if (timer_set == 0)
            SPI_Timer_Off();

        if(tkn!=0xFE)
            return 0;

        // TODO: CRC check
        SPI_R(csd, 18);

        if(dev->cardtype & SDCT_SD1)
        {
            ss = csd[0];
            // READ_BL_LEN[83:80]: max. read data block length
            READ_BL_LEN = (csd[5] & 0x0F);
            // C_SIZE [73:62]
            C_SIZE = (csd[6] & 0x03);
            C_SIZE <<= 8;
            C_SIZE |= (csd[7]);
            C_SIZE <<= 2;
            C_SIZE |= ((csd[8] >> 6) & 0x03);
            // C_SIZE_MULT [49:47]
            C_SIZE_MULT = (csd[9] & 0x03);
            C_SIZE_MULT <<= 1;
            C_SIZE_MULT |= ((csd[10] >> 7) & 0x01);
        }
        else if(dev->cardtype & SDCT_SD2)
        {
            // C_SIZE [69:48]
            C_SIZE = (csd[7] & 0x3F);
            C_SIZE <<= 8;
            C_SIZE |= (csd[8] & 0xFF);
            C_SIZE <<= 8;
            C_SIZE |= (csd[9] & 0xFF);
            // C_SIZE_MULT [--]. don't exits
            C_SIZE_MULT = 17;   //C_SIZE_MULT+2 = 19
            printf("csize: %u\n", C_SIZE);
        }
        ss = (C_SIZE + 1);
        // SD_BLK_SIZE = 2^9
        //ss *= __SD_Power_Of_Two(C_SIZE_MULT + 2 + READ_BL_LEN - 9);
        ss *= 1 << (C_SIZE_MULT + 2 + READ_BL_LEN - 9);
        //ss /= SD_BLK_SIZE;
        return (ss);
    } else return (0); // Error
}
#endif // Private methods for uC

/******************************************************************************
 Public Methods - Direct work with SD card
******************************************************************************/

SDRESULTS SD_Init(SD_DEV *dev)
{
    BYTE initdata[] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
#if defined(_M_IX86)    // x86
    dev->fp = fopen(dev->fn, "r+");
    if (dev->fp == NULL)
        return (SD_ERROR);
    else
    {
        dev->last_sector = __SD_Sectors(dev);
#ifdef SD_IO_DBG_COUNT
        dev->debug.read = 0;
        dev->debug.write = 0;
#endif
        return (SD_OK);
    }
#else   // uControllers
    BYTE n, cmd, ct, ocr[4];
    BYTE idx;
    BYTE init_trys;
    ct = 0;
    for(init_trys=0; ((init_trys!=SD_INIT_TRYS)&&(!ct)); init_trys++)
    {
        // Initialize SPI for use with the memory card
        SPI_Init();

        __SD_Deassert();
        SPI_Freq_Low();

        // 80 dummy clocks
        //for(idx = 0; idx != 10; idx++) SPI_RW(0xFF);
        SPI_W(initdata, sizeof(initdata));

        /*SPI_Timer_On(500);
        while(SPI_Timer_Status()==TRUE);
        SPI_Timer_Off();*/

        dev->mount = FALSE;
        /*SPI_Timer_On(500);
        while ((__SD_Send_Cmd(CMD0, 0) != 1)&&(SPI_Timer_Status()==TRUE));
        SPI_Timer_Off();*/
        if (__SD_Send_Cmd(CMD0, 0) != 1)
            continue;
        // Idle state

        // SD version 2?
        if (__SD_Send_Cmd(CMD8, 0x1AA) == 1) {
            // Get trailing return value of R7 resp
            SPI_R(ocr, 4);
            // VDD range of 2.7-3.6V is OK?
            if ((ocr[2] == 0x01)&&(ocr[3] == 0xAA))
            {
                // Wait for leaving idle state (ACMD41 with HCS bit)...
                SPI_Timer_On(1000);
                while ((SPI_Timer_Status()==TRUE)&&(__SD_Send_Cmd(ACMD41, 1UL << 30)));
                // CCS in the OCR?
                if ((SPI_Timer_Status()==TRUE)&&(__SD_Send_Cmd(CMD58, 0) == 0))
                {
                    SPI_R(ocr, 4);
                    // SD version 2?
                    ct = (ocr[0] & 0x40) ? SDCT_SD2 | SDCT_BLOCK : SDCT_SD2;
                }
                SPI_Timer_Off();
            }
        } else {
            // SD version 1 or MMC?
            if (__SD_Send_Cmd(ACMD41, 0) <= 1)
            {
                // SD version 1
                ct = SDCT_SD1;
                cmd = ACMD41;
            } else {
                // MMC version 3
                ct = SDCT_MMC;
                cmd = CMD1;
            }
            // Wait for leaving idle state
            SPI_Timer_On(250);
            while((SPI_Timer_Status()==TRUE)&&(__SD_Send_Cmd(cmd, 0)));

            if(SPI_Timer_Status()==FALSE) ct = 0;
            SPI_Timer_Off();
            if(__SD_Send_Cmd(CMD59, 0))   ct = 0;   // Deactivate CRC check (default)
            if(__SD_Send_Cmd(CMD16, 512)) ct = 0;   // Set R/W block length to 512 bytes
        }
    }
    if(ct) {
        dev->cardtype = ct;
        dev->mount = TRUE;
        dev->last_sector = __SD_Sectors(dev) - 1;
        printf("lastsec %lu\n", dev->last_sector);
#ifdef SD_IO_DBG_COUNT
        dev->debug.read = 0;
        dev->debug.write = 0;
#endif
        __SD_Speed_Transfer(HIGH); // High speed transfer
    }
    SPI_Release();
    return (ct ? SD_OK : SD_NOINIT);
#endif
}

SDRESULTS SD_Read(SD_DEV *dev, void *dat, DWORD sector, WORD ofs, WORD cnt)
{
#if defined(_M_IX86)    // x86
    // Check the sector query
    if((sector > dev->last_sector)||(cnt == 0)) return(SD_PARERR);
    if(dev->fp!=NULL)
    {
        if (fseek(dev->fp, ((512 * sector) + ofs), SEEK_SET)!=0)
            return(SD_ERROR);
        else {
            if(fread(dat, 1, (cnt - ofs),dev->fp)==(cnt - ofs))
            {
#ifdef SD_IO_DBG_COUNT
                dev->debug.read++;
#endif
                return(SD_OK);
            }
            else return(SD_ERROR);
        }
    } else {
        return(SD_ERROR);
    }
#else   // uControllers
    SDRESULTS res;
    BYTE tkn;
    WORD remaining;
    res = SD_ERROR;
    if ((sector > dev->last_sector)||(cnt == 0)) return(SD_PARERR);
    // Convert sector number to byte address (sector * SD_BLK_SIZE) for SDC1
    if (!(dev->cardtype & SDCT_BLOCK))
        sector *= SD_BLK_SIZE;

    if (__SD_Send_Cmd(CMD17, sector) == 0) {
        SPI_Timer_On(100);  // Wait for data packet (timeout of 100ms)
        do {
            SPI_R(&tkn, 1);
        } while((tkn==0xFF)&&(SPI_Timer_Status()==TRUE));
        SPI_Timer_Off();
        // Token of single block?
        if(tkn==0xFE) {
            // Size block (512 bytes) + CRC (2 bytes) - offset - bytes to count
            remaining = SD_BLK_SIZE + 2 - ofs - cnt;
            // Skip offset
            if(ofs) {
                SPI_R(NULL, ofs);
            }
            // I receive the data and I write in user's buffer
            SPI_R((BYTE*)dat, cnt);
            // Skip remaining
            // TODO: CRC
            SPI_R(NULL, remaining);
            res = SD_OK;
        }
    }
    SPI_Release();
#ifdef SD_IO_DBG_COUNT
    dev->debug.read++;
#endif
    return(res);
#endif
}

#ifdef SD_IO_WRITE
SDRESULTS SD_Write(SD_DEV *dev, void *dat, DWORD sector)
{
#if defined(_M_IX86)    // x86
    // Query ok?
    if(sector > dev->last_sector) return(SD_PARERR);
    if(dev->fp != NULL)
    {
        if(fseek(dev->fp, SD_BLK_SIZE * sector, SEEK_SET)!=0)
            return(SD_ERROR);
        else {
            if(fwrite(dat, 1, SD_BLK_SIZE, dev->fp)==SD_BLK_SIZE)
            {
#ifdef SD_IO_DBG_COUNT
                dev->debug.write++;
#endif
                return(SD_OK);
            }
            else return(SD_ERROR);
        }
    } else return(SD_ERROR);
#else   // uControllers
    // Query ok?
    if(sector > dev->last_sector) return(SD_PARERR);
    // Convert sector number to byte address (sector * SD_BLK_SIZE) for SDC1
    if (!(dev->cardtype & SDCT_BLOCK))
        sector *= SD_BLK_SIZE;

    // Single block write (token <- 0xFE)
    // Convert sector number to bytes address (sector * SD_BLK_SIZE)
    if(__SD_Send_Cmd(CMD24, sector)==0)
        return(__SD_Write_Block(dev, dat, 0xFE));
    else
        return(SD_ERROR);
#endif
}
#endif

SDRESULTS SD_Status(SD_DEV *dev)
{
#if defined(_M_IX86)
    return((dev->fp == NULL) ? SD_OK : SD_NORESPONSE);
#else
    return(__SD_Send_Cmd(CMD0, 0) ? SD_OK : SD_NORESPONSE);
#endif
}

// «sd_io.c» is part of:
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
