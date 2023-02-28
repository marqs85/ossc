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

    pcm1862_writereg(PCM1862_ADC1L, (1<<6)|adc_ch);
    pcm1862_writereg(PCM1862_ADC1R, (1<<6)|adc_ch);
}

void pcm_set_stereo_mode(int mono_enable) {
    uint32_t gain;
    int i;
    const uint8_t chregs[] = {0, 1, 6, 7};
    uint32_t stereo_cfg[] = {0x100000, 0x0, 0x0, 0x100000};
    uint32_t mono_cfg[] = {0x0804DC, 0x0804DC, 0x0804DC, 0x0804DC};
    uint32_t *ch_cfg = mono_enable ? mono_cfg : stereo_cfg;

    pcm1862_writereg(PCM1862_PAGESEL, 1);

    for (i=0; i<sizeof(chregs); i++) {
        pcm1862_writereg(PCM1862_DSP2_ADDR, chregs[i]);
        pcm1862_writereg(PCM1862_DSP2_WDATA0, (ch_cfg[i] >> 16) & 0xff);
        pcm1862_writereg(PCM1862_DSP2_WDATA1, (ch_cfg[i] >> 8) & 0xff);
        pcm1862_writereg(PCM1862_DSP2_WDATA2, ch_cfg[i] & 0xff);
        pcm1862_writereg(PCM1862_DSP2_CFG, (1<<0));
        while ((pcm1862_readreg(PCM1862_DSP2_CFG) & ((1<<0)|(1<<2))) != 0) {}
    }

    /*for (i=0; i<12; i++) {
        pcm1862_writereg(0x02, i);
        pcm1862_writereg(0x01, (1<<1));

        while ((pcm1862_readreg(0x01) & (1<<1)) != 0) {}

        gain = pcm1862_readreg(0x08) << 16;
        gain |= pcm1862_readreg(0x09) << 8;
        gain |= pcm1862_readreg(0x0A);

        printf("ch%u gain: 0x%x\n", i, gain);
    }*/

    pcm1862_writereg(PCM1862_PAGESEL, 0);
}

void pcm_set_gain(alt_8 db_gain) {
    alt_8 gain_val = 2*db_gain;

    pcm1862_writereg(PCM1862_PGA1L, gain_val);
    pcm1862_writereg(PCM1862_PGA1R, gain_val);
}

int pcm1862_init()
{
    if (pcm1862_readreg(0x05) != 0x86)
        return 0;

    pcm1862_writereg(PCM1862_PWR_CTRL, 0x75);

    //pcm1862_writereg(0x00, 0xff);
    pcm1862_writereg(PCM1862_CLKCONFIG, 0x90);

    pcm1862_writereg(PCM1862_DSP1_CLKDIV, 0x00);
    pcm1862_writereg(PCM1862_DSP2_CLKDIV, 0x00);
    pcm1862_writereg(PCM1862_ADC_CLKDIV,  0x03);
    pcm1862_writereg(PCM1862_PLLCONFIG, 0x00);
    pcm1862_writereg(PCM1862_DSP_CTRL, 0xB0);

    pcm1862_writereg(PCM1862_PWR_CTRL, 0x70);

    return 1;
}
