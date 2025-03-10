//
// Copyright (C) 2015-2016  Markus Hiienkari <mhiienka@niksula.hut.fi>
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

#ifndef TVP7002_REGS_H_
#define TVP7002_REGS_H_

#define TVP_BASE (0xB8>>1)

#define TVP_CHIPREV         0x00
#define TVP_HPLLDIV_MSB     0x01
#define TVP_HPLLDIV_LSB     0x02
#define TVP_HPLLCTRL        0x03
#define TVP_HPLLPHASE       0x04
#define TVP_CLAMPSTART      0x05
#define TVP_CLAMPWIDTH      0x06
#define TVP_HSOUTWIDTH      0x07
#define TVP_B_FGAIN         0x08
#define TVP_G_FGAIN         0x09
#define TVP_R_FGAIN         0x0A
#define TVP_B_FOFFSET_MSB   0x0B
#define TVP_G_FOFFSET_MSB   0x0C
#define TVP_R_FOFFSET_MSB   0x0D
#define TVP_SYNCCTRL1       0x0E
#define TVP_HPLLCTRL2       0x0F

#define TVP_SOGTHOLD        0x10
#define TVP_SSTHOLD         0x11
#define TVP_HPLLPRECOAST    0x12
#define TVP_HPLLPOSTCOAST   0x13
#define TVP_SYNCSTAT        0x14
#define TVP_OUTFORMAT       0x15
#define TVP_MISCCTRL1       0x16
#define TVP_MISCCTRL2       0x17
#define TVP_MISCCTRL3       0x18
#define TVP_INPMUX1         0x19
#define TVP_INPMUX2         0x1A
#define TVP_BG_CGAIN        0x1B
#define TVP_R_CGAIN         0x1C
#define TVP_FOFFSET_LSB     0x1D
#define TVP_B_COFFSET       0x1E
#define TVP_G_COFFSET       0x1F

#define TVP_R_COFFSET       0x20
#define TVP_HSOUTSTART      0x21
#define TVP_MISCCTRL4       0x22
#define TVP_B_ALCOUT_LSB    0x23
#define TVP_G_ALCOUT_LSB    0x24
#define TVP_R_ALCOUT_LSB    0x25
#define TVP_ALCEN           0x26
#define TVP_ALCOUT_MSB      0x27
#define TVP_ALCFILT         0x28
#define TVP_FCLAMPCTRL      0x2A
#define TVP_POWERCTRL       0x2B
#define TVP_ADCSETUP        0x2C
#define TVP_CCLAMPCTRL      0x2D
#define TVP_SOGCLAMP        0x2E
#define TVP_RGBCCLAMPCTRL   0x2F

#define TVP_SOGCCLAMPCTRL   0x30
#define TVP_ALCPLACE        0x31
#define TVP_MVSWIDTH        0x34
#define TVP_VSOUTALIGN      0x35
#define TVP_SYNCBYPASS      0x36
#define TVP_LINECNT1        0x37
#define TVP_LINECNT2        0x38
#define TVP_CLKCNT1         0x39
#define TVP_CLKCNT2         0x3A
#define TVP_HSINWIDTH       0x3B
#define TVP_VSINWIDTH       0x3C
#define TVP_LINELENTOL      0x3D
#define TVP_VIDEOBWLIM      0x3F

#define TVP_AVIDSTART1      0x40
#define TVP_AVIDSTART2      0x41
#define TVP_AVIDSTOP1       0x42
#define TVP_AVIDSTOP2       0x43
#define TVP_VB0OFF          0x44
#define TVP_VB1OFF          0x45
#define TVP_VB0DUR          0x46
#define TVP_VB1DUR          0x47
#define TVP_CSC1LO          0x4A
#define TVP_CSC1HI          0x4B
#define TVP_CSC2LO          0x4C
#define TVP_CSC2HI          0x4D
#define TVP_CSC3LO          0x4E
#define TVP_CSC3HI          0x4F

#define TVP_CSC4LO          0x50
#define TVP_CSC4HI          0x51
#define TVP_CSC5LO          0x52
#define TVP_CSC5HI          0x53
#define TVP_CSC6LO          0x54
#define TVP_CSC6HI          0x55
#define TVP_CSC7LO          0x56
#define TVP_CSC7HI          0x57
#define TVP_CSC8LO          0x58
#define TVP_CSC8HI          0x59
#define TVP_CSC9LO          0x5A
#define TVP_CSC9HI          0x5B

#endif /* TVP7002_REGS_H_ */
