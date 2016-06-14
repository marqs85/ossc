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

#include <string.h>
#include <unistd.h>
#include "firmware.h"
#include "sdcard.h"
#include "flash.h"
#include "sysconfig.h"
#include "controls.h"
#include "tvp7002.h"
#include "av_controller.h"
#include "lcd.h"
#include "ci_crc.h"
#include "altera_avalon_pio_regs.h"

extern char menu_row1[LCD_ROW_LEN+1], menu_row2[LCD_ROW_LEN+1];
extern alt_u16 rc_keymap[REMOTE_MAX_KEYS];

static int check_fw_header(alt_u8 *databuf, fw_hdr *hdr)
{
    alt_u32 crcval, tmp;

    strncpy(hdr->fw_key, (char*)databuf, 4);
    if (strncmp(hdr->fw_key, "OSSC", 4)) {
        sniprintf(menu_row1, LCD_ROW_LEN+1, "Invalid image");
        menu_row2[0] = '\0';
        return 1;
    }

    hdr->version_major = databuf[4];
    hdr->version_minor = databuf[5];
    strncpy(hdr->version_suffix, (char*)(databuf+6), 8);
    hdr->version_suffix[7] = 0;

    memcpy(&tmp, databuf+14, 4);
    hdr->hdr_len = ALT_CI_NIOS_CUSTOM_INSTR_ENDIANCONVERTER_0(tmp);
    memcpy(&tmp, databuf+18, 4);
    hdr->data_len = ALT_CI_NIOS_CUSTOM_INSTR_ENDIANCONVERTER_0(tmp);
    memcpy(&tmp, databuf+22, 4);
    hdr->data_crc = ALT_CI_NIOS_CUSTOM_INSTR_ENDIANCONVERTER_0(tmp);
    // Always at bytes [508-511]
    memcpy(&tmp, databuf+508, 4);
    hdr->hdr_crc = ALT_CI_NIOS_CUSTOM_INSTR_ENDIANCONVERTER_0(tmp);

    if (hdr->hdr_len < 26 || hdr->hdr_len > 508) {
        sniprintf(menu_row1, LCD_ROW_LEN+1, "Invalid header");
        menu_row2[0] = '\0';
        return -1;
    }

    crcval = crcCI(databuf, hdr->hdr_len, 1);

    if (crcval != hdr->hdr_crc) {
        sniprintf(menu_row1, LCD_ROW_LEN+1, "Invalid hdr CRC");
        menu_row2[0] = '\0';
        return -2;
    }

    return 0;
}

static int check_fw_image(alt_u32 offset, alt_u32 size, alt_u32 golden_crc, alt_u8 *tmpbuf)
{
    alt_u32 crcval=0, i, bytes_to_read;
    int retval;

    for (i=0; i<size; i=i+SD_BUFFER_SIZE) {
        bytes_to_read = ((size-i < SD_BUFFER_SIZE) ? (size-i) : SD_BUFFER_SIZE);
        retval = read_sd_block(offset+i, bytes_to_read, tmpbuf);
        if (retval != 0)
            return -2;

        crcval = crcCI(tmpbuf, bytes_to_read, (i==0));
    }

    if (crcval != golden_crc) {
        sniprintf(menu_row1, LCD_ROW_LEN+1, "Invalid data CRC");
        menu_row2[0] = '\0';
        return -3;
    }

    return 0;
}

#ifdef DEBUG
int fw_update()
{
    sniprintf(menu_row2, LCD_ROW_LEN+1, "Not implemented");
    lcd_write_menu();
    usleep(1000000);
    return -1;
}
#else
int fw_update()
{
    int retval, i;
    int retries = FW_UPDATE_RETRIES;
    alt_u8 databuf[SD_BUFFER_SIZE];
    alt_u32 btn_vec;
    alt_u32 bytes_to_rw;
    fw_hdr fw_header;

    retval = check_sdcard(databuf);
    if (retval != 0)
        goto failure;

    retval = check_fw_header(databuf, &fw_header);
    if (retval != 0)
        goto failure;

    sniprintf(menu_row1, LCD_ROW_LEN+1, "Validating data");
    sniprintf(menu_row2, LCD_ROW_LEN+1, "%u bytes", (unsigned)fw_header.data_len);
    lcd_write_menu();
    retval = check_fw_image(512, fw_header.data_len, fw_header.data_crc, databuf);
    if (retval != 0)
        goto failure;

    sniprintf(menu_row1, LCD_ROW_LEN+1, "%u.%.2u%s%s", fw_header.version_major, fw_header.version_minor, (fw_header.version_suffix[0] == 0) ? "" : "-", fw_header.version_suffix);
    strncpy(menu_row2, "Update? 1=Y, 2=N", LCD_ROW_LEN+1);
    lcd_write_menu();

    while (1) {
        btn_vec = IORD_ALTERA_AVALON_PIO_DATA(PIO_1_BASE) & RC_MASK;

        if (btn_vec == rc_keymap[RC_BTN1]) {
            break;
        } else if (btn_vec == rc_keymap[RC_BTN2]) {
            retval = 2;
            return 1;
        }

        usleep(WAITLOOP_SLEEP_US);
    }

    //disable video output
    tvp_disable_output();
    IOWR_ALTERA_AVALON_PIO_DATA(PIO_0_BASE, (IORD_ALTERA_AVALON_PIO_DATA(PIO_0_BASE) | (1<<2)));
    usleep(10000);

    strncpy(menu_row1, "Updating FW", LCD_ROW_LEN+1);
update_init:
    strncpy(menu_row2, "please wait...", LCD_ROW_LEN+1);
    lcd_write_menu();

    for (i=0; i<fw_header.data_len; i=i+SD_BUFFER_SIZE) {
        bytes_to_rw = ((fw_header.data_len-i < SD_BUFFER_SIZE) ? (fw_header.data_len-i) : SD_BUFFER_SIZE);
        retval = read_sd_block(512+i, bytes_to_rw, databuf);
        if (retval != 0)
            return -200;

        retval = write_flash_page(databuf, ((bytes_to_rw < PAGESIZE) ? bytes_to_rw : PAGESIZE), (i/PAGESIZE));
        if (retval != 0)
            goto failure;
        //TODO: support multiple page sizes
        if (bytes_to_rw > PAGESIZE) {
            retval = write_flash_page(databuf+PAGESIZE, (bytes_to_rw-PAGESIZE), (i/PAGESIZE)+1);
            if (retval != 0)
                goto failure;
        }
    }

    strncpy(menu_row1, "Verifying flash", LCD_ROW_LEN+1);
    strncpy(menu_row2, "please wait...", LCD_ROW_LEN+1);
    lcd_write_menu();
    retval = verify_flash(0, fw_header.data_len, fw_header.data_crc, databuf);
    if (retval != 0)
        goto failure;

    return 0;


failure:
    lcd_write_menu();
    usleep(1000000);

    // Probable rw error, retry update
    if ((retval <= -200) && (retries > 0)) {
        sniprintf(menu_row1, LCD_ROW_LEN+1, "Retrying update");
        retries--;
        goto update_init;
    }

    return -1;
}
#endif
