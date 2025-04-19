#include <stdio.h>
#include <unistd.h>
#include "system.h"
#include "i2c_opencores.h"
#include "hdmitx.h"
#include "it6613.h"

inline alt_u32 read_it2(alt_u32 regaddr)
{
    I2C_start(I2CA_BASE, IT_BASE, 0);
    I2C_write(I2CA_BASE, regaddr, 0);
    I2C_start(I2CA_BASE, IT_BASE, 1);
    return I2C_read(I2CA_BASE,1);
}

inline void write_it2(alt_u32 regaddr, alt_u8 data)
{
    I2C_start(I2CA_BASE, IT_BASE, 0);
    I2C_write(I2CA_BASE, regaddr, 0);
    I2C_write(I2CA_BASE, data, 1);
}

BYTE I2C_Read_Byte(BYTE Addr,BYTE RegAddr)
{
    I2C_start(I2CA_BASE, Addr, 0);
    I2C_write(I2CA_BASE, RegAddr, 0);
    I2C_start(I2CA_BASE, Addr, 1);
    return I2C_read(I2CA_BASE,1);
}

SYS_STATUS I2C_Write_Byte(BYTE Addr,BYTE RegAddr,BYTE Data)
{
    I2C_start(I2CA_BASE, Addr, 0);
    I2C_write(I2CA_BASE, RegAddr, 0);
    I2C_write(I2CA_BASE, Data, 1);
    return 0;
}

SYS_STATUS I2C_Read_ByteN(BYTE Addr,BYTE RegAddr,BYTE *pData,int N)
{
    int i;

    for (i=0; i<N; i++)
        pData[i] = I2C_Read_Byte(Addr, RegAddr+i);

    return 0;
}

SYS_STATUS I2C_Write_ByteN(BYTE Addr,BYTE RegAddr,BYTE *pData,int N)
{
    int i;

    for (i=0; i<N; i++)
        I2C_Write_Byte(Addr, RegAddr+i, pData[i]);

    return 0;
}

BYTE HDMITX_ReadI2C_Byte(BYTE RegAddr)
{
    return read_it2(RegAddr);
}

SYS_STATUS HDMITX_WriteI2C_Byte(BYTE RegAddr,BYTE val)
{
    write_it2(RegAddr, val);
    return 0;
}

SYS_STATUS HDMITX_ReadI2C_ByteN(BYTE RegAddr,BYTE *pData,int N)
{
    int i;

    for (i=0; i<N; i++)
        pData[i] =  HDMITX_ReadI2C_Byte(RegAddr+i);

    return 0;
}

SYS_STATUS HDMITX_WriteI2C_ByteN(BYTE RegAddr,BYTE *pData,int N)
{
    int i;

    for (i=0; i<N; i++)
        HDMITX_WriteI2C_Byte(RegAddr+i, pData[i]);

    return 0;
}

void DelayMS(unsigned int ms)
{
    usleep(1000*ms);
}
