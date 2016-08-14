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

#include <stdio.h>
#include <unistd.h>
#include "system.h"
#include "altera_avalon_pio_regs.h"
#include "i2c_opencores.h"
#include "tvp7002.h"

//#define SYNCBYPASS    // Bypass VGA syncs (for debug - needed for interlace?)
//#define EXTADCCLK     // Use external ADC clock (external osc)
//#define ADCPOWERDOWN  // Power-down ADCs
//#define PLLPOSTDIV    // Double-rate PLL with div-by-2 (decrease jitter?)

/* Y'Pb'Pr' to R'G'B' CSC coefficients.
 *
 * Coefficients from "Colour Space Conversions" (http://www.poynton.com/PDFs/coloureq.pdf).
 */
const ypbpr_to_rgb_csc_t csc_coeffs[] = {
    { "Rec. 601", 0x2000, 0x0000, 0x2CE5, 0x2000, 0xF4FD, 0xE926, 0x2000, 0x38BC, 0x0000 },    // eq. 101
    { "Rec. 709", 0x2000, 0x0000, 0x323E, 0x2000, 0xFA04, 0xF113, 0x2000, 0x3B61, 0x0000 },    // eq. 105
};

extern mode_data_t video_modes[];

static void tvp_set_clamp(video_format fmt)
{
    switch (fmt) {
    case FORMAT_RGBS:
    case FORMAT_RGBHV:
    case FORMAT_RGsB:
        //select bottom clamp (RGB)
        tvp_writereg(TVP_SOGTHOLD, 0x58);
        break;
    case FORMAT_YPbPr:
        //select mid clamp for Pb & Pr
        tvp_writereg(TVP_SOGTHOLD, 0x5D);
        break;
    default:
        break;
    }
}

static void tvp_set_clamp_position(video_type type)
{
    switch (type) {
    case VIDEO_LDTV:
        tvp_writereg(TVP_CLAMPSTART, 0x2);
        tvp_writereg(TVP_CLAMPWIDTH, 0x6);
        break;
    case VIDEO_SDTV:
    case VIDEO_EDTV:
    case VIDEO_PC:
        tvp_writereg(TVP_CLAMPSTART, 0x6);
        tvp_writereg(TVP_CLAMPWIDTH, 0x10);
        break;
    case VIDEO_HDTV:
        tvp_writereg(TVP_CLAMPSTART, 0x32);
        tvp_writereg(TVP_CLAMPWIDTH, 0x20);
        break;
    default:
        break;
    }
}

inline alt_u32 tvp_readreg(alt_u32 regaddr)
{
    I2C_start(I2CA_BASE, TVP_BASE, 0);
    I2C_write(I2CA_BASE, regaddr, 1);   //don't use repeated start as it seems unreliable at 400kHz
    I2C_start(I2CA_BASE, TVP_BASE, 1);
    return I2C_read(I2CA_BASE,1);
}

inline void tvp_writereg(alt_u32 regaddr, alt_u8 data)
{
    I2C_start(I2CA_BASE, TVP_BASE, 0);
    I2C_write(I2CA_BASE, regaddr, 0);
    I2C_write(I2CA_BASE, data, 1);
}

inline void tvp_reset()
{
    usleep(10000);
    IOWR_ALTERA_AVALON_PIO_DATA(PIO_0_BASE, 0x00);
    usleep(10000);
    IOWR_ALTERA_AVALON_PIO_DATA(PIO_0_BASE, 0x01);
    usleep(10000);
}

inline void tvp_disable_output()
{
    alt_u8 syncproc_rst = tvp_readreg(TVP_MISCCTRL4) | (1<<7);

    tvp_writereg(TVP_MISCCTRL1, 0x13);
    usleep(10000);
    tvp_writereg(TVP_MISCCTRL2, 0x03);
    usleep(10000);
    tvp_writereg(TVP_MISCCTRL4, syncproc_rst);
    usleep(1000);
    tvp_writereg(TVP_MISCCTRL4, syncproc_rst & 0x7F);
}

inline void tvp_enable_output()
{
    usleep(10000);
    tvp_writereg(TVP_MISCCTRL1, 0x11);
    usleep(10000);
    tvp_writereg(TVP_MISCCTRL2, 0x02);
    usleep(10000);
}

inline void tvp_set_hpllcoast(alt_u8 pre, alt_u8 post)
{
    tvp_writereg(TVP_HPLLPRECOAST, pre);
    tvp_writereg(TVP_HPLLPOSTCOAST, post);
}

inline void tvp_set_ssthold(alt_u8 vsdetect_thold)
{
    tvp_writereg(TVP_SSTHOLD, vsdetect_thold);
}

void tvp_init()
{
    // disable output
    tvp_disable_output();

    //Set global defaults

    // Configure external refclk
    tvp_sel_clk(REFCLK_EXT27);

    // Hsync input->output delay (horizontal shift)
    // Default is 13, which maintains alignment of RGB and hsync at output
    //tvp_writereg(TVP_HSOUTSTART, 0);

    // Hsync edge->Vsync edge delay
    tvp_writereg(TVP_VSOUTALIGN, 0);

    // Set default CSC coeffs.
    tvp_sel_csc(&csc_coeffs[0]);

    // Set default phase
    tvp_set_hpll_phase(0x10);

    // Set min LPF
    tvp_set_lpf(0);
    tvp_set_sync_lpf(0);

    // Increase line length tolerance
    tvp_writereg(TVP_LINELENTOL, 0x06);

    // Common sync separator threshold
    // Some arcade games need more that the default 0x40
    tvp_set_ssthold(DEFAULT_VSYNC_THOLD);

    //set analog (coarse) gain to max recommended value (-> 91% of the ADC range with 0.7Vpp input)
    tvp_writereg(TVP_BG_CGAIN, 0x88);
    tvp_writereg(TVP_R_CGAIN, 0x08);

    //set rest of the gain digitally (fine) to utilize 100% of the range at the output (0.91*(1+(26/256)) = 1)
    tvp_writereg(TVP_R_FGAIN, 26);
    tvp_writereg(TVP_G_FGAIN, 26);
    tvp_writereg(TVP_B_FGAIN, 26);
}

// Configure H-PLL (sampling rate, VCO gain and charge pump current)
void tvp_setup_hpll(alt_u16 h_samplerate, alt_u16 v_lines, alt_u8 hz, alt_u8 plldivby2)
{
    alt_u32 pclk_est;
    alt_u8 vco_range;
    alt_u8 cp_current;

    alt_u8 status = tvp_readreg(TVP_HPLLPHASE) & 0xF8;

    // Enable PLL post-div-by-2 with double samplerate
    if (plldivby2) {
        tvp_writereg(TVP_HPLLPHASE, status|1);
        h_samplerate = 2*h_samplerate;
    } else {
        tvp_writereg(TVP_HPLLPHASE, status);
    }

    tvp_writereg(TVP_HPLLDIV_MSB, (h_samplerate >> 4));
    tvp_writereg(TVP_HPLLDIV_LSB, ((h_samplerate & 0xf) << 4));

    printf("Horizontal samplerate set to %u\n", h_samplerate);

    pclk_est = ((alt_u32)h_samplerate * v_lines * hz) / 1000; //in kHz

    printf("Estimated PCLK: %lu.%.3lu MHz\n", pclk_est/1000, pclk_est%1000);

    if (pclk_est < 36000) {
        vco_range = 0;
    } else if (pclk_est < 70000) {
        vco_range = 1;
    } else if (pclk_est < 135000) {
        vco_range = 2;
    } else {
        vco_range = 3;
    }

    cp_current = (40*Kvco[vco_range]+h_samplerate/2) / h_samplerate; //"+h_samplerate/2" for fast rounding
    if (cp_current > 7)
        cp_current = 7;

    printf("VCO range: %s\nCPC: %u\n", Kvco_str[vco_range], cp_current);
    tvp_writereg(TVP_HPLLCTRL, ((vco_range << 6) | (cp_current << 3)));
}

void tvp_sel_clk(alt_u8 refclk)
{
    alt_u8 status = tvp_readreg(TVP_INPMUX2) & 0xFA;

    //TODO: set SOG and CLP LPF based on mode
    if (refclk == REFCLK_INTCLK) {
        tvp_writereg(TVP_INPMUX2, status|0x2);
    } else {
#ifdef EXTADCCLK
        tvp_writereg(TVP_INPMUX2, status|0x8);
#else
        tvp_writereg(TVP_INPMUX2, status|0xA);
#endif
    }
}

void tvp_sel_csc(const ypbpr_to_rgb_csc_t *csc)
{
    tvp_writereg(TVP_CSC1HI, (csc->G_Y >> 8));
    tvp_writereg(TVP_CSC1LO, (csc->G_Y & 0xff));
    tvp_writereg(TVP_CSC2HI, (csc->G_Pb >> 8));
    tvp_writereg(TVP_CSC2LO, (csc->G_Pb & 0xff));
    tvp_writereg(TVP_CSC3HI, (csc->G_Pr >> 8));
    tvp_writereg(TVP_CSC3LO, (csc->G_Pr & 0xff));

    tvp_writereg(TVP_CSC4HI, (csc->R_Y >> 8));
    tvp_writereg(TVP_CSC4LO, (csc->R_Y & 0xff));
    tvp_writereg(TVP_CSC5HI, (csc->R_Pb >> 8));
    tvp_writereg(TVP_CSC5LO, (csc->R_Pb & 0xff));
    tvp_writereg(TVP_CSC6HI, (csc->R_Pr >> 8));
    tvp_writereg(TVP_CSC6LO, (csc->R_Pr & 0xff));

    tvp_writereg(TVP_CSC7HI, (csc->B_Y >> 8));
    tvp_writereg(TVP_CSC7LO, (csc->B_Y & 0xff));
    tvp_writereg(TVP_CSC8HI, (csc->B_Pb >> 8));
    tvp_writereg(TVP_CSC8LO, (csc->B_Pb & 0xff));
    tvp_writereg(TVP_CSC9HI, (csc->B_Pr >> 8));
    tvp_writereg(TVP_CSC9LO, (csc->B_Pr & 0xff));
}

void tvp_set_lpf(alt_u8 val)
{
    alt_u8 status = tvp_readreg(TVP_VIDEOBWLIM) & 0xF0;
    tvp_writereg(TVP_VIDEOBWLIM, status|val);
    printf("TVP LPF value set to 0x%x\n", val);
}

void tvp_set_sync_lpf(alt_u8 val)
{
    alt_u8 status = tvp_readreg(TVP_INPMUX2) & 0x3F;
    tvp_writereg(TVP_INPMUX2, status|((3-val)<<6));
    printf("Sync LPF value set to 0x%x\n", (3-val));
}

void tvp_set_hpll_phase(alt_u8 val)
{
    alt_u8 status = tvp_readreg(TVP_HPLLPHASE) & 0x07;
    tvp_writereg(TVP_HPLLPHASE, (val<<3)|status);
    printf("Phase value set to 0x%x\n", val);
}

void tvp_set_sog_thold(alt_u8 val)
{
    alt_u8 status = tvp_readreg(TVP_SOGTHOLD) & 0x07;
    tvp_writereg(TVP_SOGTHOLD, (val<<3)|status);
    printf("SOG thold set to 0x%x\n", val);
}

void tvp_set_alc(alt_u8 en_alc, video_type type)
{
    if (en_alc) {
        tvp_writereg(TVP_ALCEN, 0x80); //enable ALC

        //select ALC placement
        switch (type) {
        case VIDEO_LDTV:
            tvp_writereg(TVP_ALCPLACE, 0x9);
            break;
        case VIDEO_SDTV:
        case VIDEO_EDTV:
        case VIDEO_PC:
            tvp_writereg(TVP_ALCPLACE, 0x18);
            break;
        case VIDEO_HDTV:
            tvp_writereg(TVP_ALCPLACE, 0x5A);
            break;
        default:
            break;
        }
    } else {
        tvp_writereg(TVP_ALCEN, 0x00); //disable ALC
    }
}

void tvp_setup_glitchstripper(video_type type, alt_u8 sd_winwidth)
{
    // Setup Macrovision stripper and H-PLL coast.
    // Coast needs to be enabled when HSYNC is missing during VSYNC. Disabled only for RGBHV.
    // Macrovision stripper filters out glitches and serration pulses that may occur outside of sync window (HSYNC_lead +- TVP_MVSWIDTH*37ns). Enabled for all inputs.
    switch (type) {
    case VIDEO_PC:
        tvp_writereg(TVP_MISCCTRL4, 0x0C);
        tvp_writereg(TVP_MVSWIDTH, 0x03);
        break;
    case VIDEO_HDTV:
        tvp_writereg(TVP_MISCCTRL4, 0x08);
        tvp_writereg(TVP_MVSWIDTH, 0x0E);
        break;
    case VIDEO_EDTV:
        tvp_writereg(TVP_MISCCTRL4, 0x08);
        tvp_writereg(TVP_MVSWIDTH, 0x44);
        break;
    case VIDEO_LDTV:
    case VIDEO_SDTV:
        tvp_writereg(TVP_MISCCTRL4, 0x08);
        tvp_writereg(TVP_MVSWIDTH, sd_winwidth);
        break;
    default:
        break;
    }
}

void tvp_source_setup(alt_8 modeid, video_type type, alt_u8 en_alc, alt_u32 vlines, alt_u8 hz, alt_u8 pre_coast, alt_u8 post_coast, alt_u8 vsync_thold, alt_u8 sd_sync_win)
{
    // Clamp position and ALC
    tvp_set_clamp_position(type);
    tvp_set_alc(en_alc, type);

    tvp_set_ssthold(vsync_thold);

    tvp_setup_glitchstripper(type, sd_sync_win);

    tvp_setup_hpll(video_modes[modeid].h_total, vlines, hz, !!(video_modes[modeid].flags & MODE_PLLDIVBY2));

    //Long coast may lead to PLL frequency drift and sync loss (e.g. SNES)
    /*if (video_modes[modeid].v_active < 720)
        tvp_set_hpllcoast(3, 3);
    else
        tvp_set_hpllcoast(1, 0);*/
    tvp_set_hpllcoast(pre_coast, post_coast);

    // Hsync output width
    tvp_writereg(TVP_HSOUTWIDTH, video_modes[modeid].h_synclen);
}

void tvp_source_sel(tvp_input_t input, video_format fmt)
{
    alt_u8 sync_status;
    alt_u8 sog_ch;

    if ((fmt == FORMAT_RGsB) || (fmt == FORMAT_YPbPr))
        sog_ch = (input == TVP_INPUT3) ? 2 : 0;
    else if ((input == TVP_INPUT1) && (fmt == FORMAT_RGBS))
        sog_ch = 1;
    else
        sog_ch = 2;

    // RGB+SOG input select
    tvp_writereg(TVP_INPMUX1, (sog_ch<<6) | (input<<4) | (input<<2) | input);

    // Clamp setup
    tvp_set_clamp(fmt);

    // HV/SOG sync select
    if ((input == TVP_INPUT3) && ((fmt == FORMAT_RGBHV) || (fmt == FORMAT_RGBS))) {
        if (fmt == FORMAT_RGBHV)
            tvp_writereg(TVP_SYNCCTRL1, 0x52);
        else // RGBS
            tvp_writereg(TVP_SYNCCTRL1, 0x53);

        usleep(1000);
        sync_status = tvp_readreg(TVP_SYNCSTAT);
        if (sync_status & (1<<7))
            printf("%s detected, %s polarity\n", (sync_status & (1<<3)) ? "Csync" : "Hsync", (sync_status & (1<<5)) ? "pos" : "neg");
        if (sync_status & (1<<4))
            printf("Vsync detected, %s polarity\n", (sync_status & (1<<2)) ? "pos" : "neg");
    } else {
        tvp_writereg(TVP_SYNCCTRL1, 0x5B);
        usleep(1000);
        sync_status = tvp_readreg(TVP_SYNCSTAT);
        if (sync_status & (1<<1))
            printf("SOG detected\n");
        else
            printf("SOG not detected\n");
    }

    // Enable CSC for YPbPr
    if (fmt == FORMAT_YPbPr)
        tvp_writereg(TVP_MISCCTRL3, 0x10);
    else
        tvp_writereg(TVP_MISCCTRL3, 0x00);

#ifdef SYNCBYPASS
    tvp_writereg(TVP_SYNCBYPASS, 0x03);
#else
    tvp_writereg(TVP_SYNCBYPASS, 0x00);
#endif

    //TODO:
    //clamps
    //TVP_ADCSETUP

    printf("\n");
}

alt_u8 tvp_check_sync(tvp_input_t input, video_format fmt)
{
    alt_u8 sync_status;

    sync_status = tvp_readreg(TVP_SYNCSTAT);

    if ((input == TVP_INPUT3) && (fmt == FORMAT_RGBHV))
        return ((sync_status & 0x90) == 0x90);
    else if ((input == TVP_INPUT3) && (fmt == FORMAT_RGBS))
        return ((sync_status & 0x88) == 0x88);
    else
        return !!(sync_status & (1<<1));
}
