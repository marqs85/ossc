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

#ifndef PCM1862_REGS_H_
#define PCM1862_REGS_H_

#define PCM1862_BASE (0x94>>1)

#define PCM1862_PAGESEL         0x00
#define PCM1862_PGA1L           0x01
#define PCM1862_PGA1R           0x02
#define PCM1862_PGA2L           0x03
#define PCM1862_PGA2R           0x04
#define PCM1862_PGACONFIG       0x05
#define PCM1862_ADC1L           0x06
#define PCM1862_ADC1R           0x07
#define PCM1862_ADC2L           0x08
#define PCM1862_ADC2R           0x09
#define PCM1862_ADC2CONFIG      0x0A
#define PCM1862_PCMFMT          0x0B
#define PCM1862_TDMFMT          0x0C
#define PCM1862_TXTDM_OFFSET    0x0D
#define PCM1862_RXTDM_OFFSET    0x0E
#define PCM1862_DPGA1_GAIN_L    0x0F

#define PCM1862_DPGA1_GAIN_R    0x16
#define PCM1862_DPGA2_GAIN_L    0x17
#define PCM1862_DPGA2_GAIN_R    0x18
#define PCM1862_DPGA_CONFIG     0x19
#define PCM1862_DMIC_CONFIG     0x1A
#define PCM1862_DIN_RESAMPLE    0x1B

#define PCM1862_CLKCONFIG       0x20

#endif /* PCM1862_REGS_H_ */
