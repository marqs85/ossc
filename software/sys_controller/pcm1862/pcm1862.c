//
// Copyright (C) 2017  Markus Hiienkari <mhiienka@niksula.hut.fi>
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
#include <unistd.h>
#include "system.h"
#include "i2c_opencores.h"
#include "pcm1862.h"

inline alt_u32 pcm1862_readreg(alt_u8 regaddr)
{
    //Phase 1
    I2C_start(I2CA_BASE, PCM1862_BASE, 0);
    I2C_write(I2CA_BASE, regaddr, 0);

    //Phase 2
    I2C_start(I2CA_BASE, PCM1862_BASE, 1);
    return I2C_read(I2CA_BASE,1);
}

inline void pcm1862_writereg(alt_u8 regaddr, alt_u8 data)
{
    I2C_start(I2CA_BASE, PCM1862_BASE, 0);
    I2C_write(I2CA_BASE, regaddr, 0);
    I2C_write(I2CA_BASE, data, 1);
}

void pcm_source_sel(pcm_input_t input) {
    alt_u8 adc_ch = 1<<input;

    pcm1862_writereg(PCM1862_ADC1L, adc_ch);
    pcm1862_writereg(PCM1862_ADC1R, adc_ch);
}

int pcm1862_init()
{
    if (pcm1862_readreg(0x05) != 0x86)
        return 0;

    //pcm1862_writereg(0x00, 0xff);
    pcm1862_writereg(PCM1862_CLKCONFIG, 0x90);

    pcm1862_writereg(PCM1862_DSP1_CLKDIV, 0x00);
    pcm1862_writereg(PCM1862_DSP2_CLKDIV, 0x00);
    pcm1862_writereg(PCM1862_ADC_CLKDIV,  0x03);
    pcm1862_writereg(PCM1862_PLLCONFIG, 0x00);
    pcm1862_writereg(PCM1862_DSP_CTRL, 0x30);

    return 1;
}
