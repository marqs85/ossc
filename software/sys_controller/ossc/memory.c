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
#include "string.h"
#include "altera_avalon_pio_regs.h"
#include "altera_epcq_controller_mod.h"
#include "Altera_UP_SD_Card_Avalon_Interface_mod.h"
#include "tvp7002.h"
#include "sysconfig.h"
#include "cfg.h"
#include "av_controller.h"
#include "lcd.h"
#include "ci_crc.h"
#include "memory.h"
#include "controls.h"
#include "fw_version.h"


extern alt_epcq_controller_dev epcq_controller_0;
alt_epcq_controller_dev *epcq_controller_dev;

extern char row1[LCD_ROW_LEN+1], row2[LCD_ROW_LEN+1], menu_row1[LCD_ROW_LEN+1], menu_row2[LCD_ROW_LEN+1];

extern alt_u16 rc_keymap[REMOTE_MAX_KEYS];

// Target configuration
extern avconfig_t tc;

// Current mode
extern avmode_t cm;

alt_up_sd_card_dev *sdcard_dev;

int check_flash()
{
    epcq_controller_dev = &epcq_controller_0;

    if ((epcq_controller_dev == NULL) || !(epcq_controller_dev->is_epcs && (epcq_controller_dev->page_size == PAGESIZE)))
        return -1;

    //printf("Flash size in bytes: %d\nSector size: %d (%d pages)\nPage size: %d\n", epcq_controller_dev->size_in_bytes, epcq_controller_dev->sector_size, epcq_controller_dev->sector_size/epcq_controller_dev->page_size, epcq_controller_dev->page_size);

    return 0;
}



int read_flash(alt_u32 offset, alt_u32 length, alt_u8 *dstbuf)
{
    int retval, i;

    retval = alt_epcq_controller_read(&epcq_controller_dev->dev, offset, dstbuf, length);
    if (retval != 0)
        return -1;

    for (i=0; i<length; i++)
        dstbuf[i] = ALT_CI_NIOS_CUSTOM_INSTR_BITSWAP_0(dstbuf[i]) >> 24;

    return 0;
}

int write_flash_page(alt_u8 *pagedata, alt_u32 length, alt_u32 pagenum)
{
    int retval, i;

    if ((pagenum % PAGES_PER_SECTOR) == 0) {
        printf("Erasing sector %u\n", (unsigned)(pagenum/PAGES_PER_SECTOR));
        retval = alt_epcq_controller_erase_block(&epcq_controller_dev->dev, pagenum*PAGESIZE);

        if (retval != 0) {
            strncpy(menu_row1, "Flash erase", LCD_ROW_LEN+1);
            sniprintf(menu_row1, LCD_ROW_LEN+1, "error %d", retval);
            menu_row2[0] = '\0';
            printf("Flash erase error, sector %u\nRetval %d\n", (unsigned)(pagenum/PAGES_PER_SECTOR), retval);
            return -200;
        }
    }

    // Bit-reverse bytes for flash
    for (i=0; i<length; i++)
        pagedata[i] = ALT_CI_NIOS_CUSTOM_INSTR_BITSWAP_0(pagedata[i]) >> 24;

    retval = alt_epcq_controller_write_block(&epcq_controller_dev->dev, (pagenum/PAGES_PER_SECTOR)*PAGES_PER_SECTOR*PAGESIZE, pagenum*PAGESIZE, pagedata, length);

    if (retval != 0) {
        strncpy(menu_row1, "Flash write", LCD_ROW_LEN+1);
        strncpy(menu_row2, "error", LCD_ROW_LEN+1);
        printf("Flash write error, page %u\nRetval %d\n", (unsigned)pagenum, retval);
        return -201;
    }

    return retval;
}

int verify_flash(alt_u32 offset, alt_u32 length, alt_u32 golden_crc, alt_u8 *tmpbuf)
{
    alt_u32 crcval=0, i, bytes_to_read;
    int retval;

    for (i=0; i<length; i=i+PAGESIZE) {
        bytes_to_read = ((length-i < PAGESIZE) ? (length-i) : PAGESIZE);

        retval = read_flash(i, bytes_to_read, tmpbuf);
        if (retval != 0)
            return -202;

        crcval = crcCI(tmpbuf, bytes_to_read, (i==0));
    }

    if (crcval != golden_crc) {
        sniprintf(menu_row1, LCD_ROW_LEN+1, "Flash verif fail");
        menu_row2[0] = '\0';
        return -203;
    }

    return 0;
}


int read_sd_block(alt_u32 offset, alt_u32 size, alt_u8 *dstbuf)
{
    int i;
    alt_u32 tmp;

    if ((offset % SD_BUFFER_SIZE) || (size > 512)) {
        sniprintf(menu_row1, LCD_ROW_LEN+1, "Invalid read cmd");
        menu_row2[0] = '\0';
        return -1;
    }

    if (!Read_Sector_Data((offset/SD_BUFFER_SIZE), 0)) {
        sniprintf(menu_row1, LCD_ROW_LEN+1, "SD read failure");
        menu_row2[0] = '\0';
        return -2;
    }

    // Copy buffer to SW
    for (i=0; i<size; i=i+4) {
        tmp = IORD_32DIRECT(sdcard_dev->base, i);
        *((alt_u32*)(dstbuf+i)) = tmp;
    }

    return 0;
}

int check_sdcard(alt_u8 *databuf)
{
    sdcard_dev = alt_up_sd_card_open_dev(ALTERA_UP_SD_CARD_AVALON_INTERFACE_0_NAME);

    if ((sdcard_dev == NULL) || !alt_up_sd_card_is_Present()) {
        sniprintf(menu_row1, LCD_ROW_LEN+1, "No SD card det.");
        menu_row2[0] = '\0';
        return 1;
    }

    return read_sd_block(0, 512, databuf);
}

int check_fw_header(alt_u8 *databuf, fw_hdr *hdr)
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

int check_fw_image(alt_u32 offset, alt_u32 size, alt_u32 golden_crc, alt_u8 *tmpbuf)
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

        usleep(MAINLOOP_SLEEP_US);
    }

    //disable video output
    tvp_disable_output();
    IOWR_ALTERA_AVALON_PIO_DATA(PIO_0_BASE, (IORD_ALTERA_AVALON_PIO_DATA(PIO_0_BASE) | (1<<2)));
    usleep(MAINLOOP_SLEEP_US);

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

int write_userdata()
{
    alt_u8 databuf[PAGESIZE];
    int retval;

    strncpy((char*)databuf, "USRDATA", 8);
    databuf[8] = FW_VER_MAJOR;
    databuf[9] = FW_VER_MINOR;
    databuf[10] = 2;

    retval = write_flash_page(databuf, USERDATA_HDR_SIZE, (USERDATA_OFFSET/PAGESIZE));
    if (retval != 0) {
        return -1;
    }

    databuf[0] = UDE_REMOTE_MAP;
    databuf[1] = 4+sizeof(rc_keymap);
    databuf[2] = databuf[3] = 0;    //padding
    memcpy(databuf+4, rc_keymap, sizeof(rc_keymap));

    retval = write_flash_page(databuf, databuf[1], (USERDATA_OFFSET/PAGESIZE)+1);
    if (retval != 0) {
        return -1;
    }

    databuf[0] = UDE_AVCONFIG;
    databuf[1] = 4+sizeof(avconfig_t);
    databuf[2] = databuf[3] = 0;    //padding
    memcpy(databuf+4, &tc, sizeof(avconfig_t));

    retval = write_flash_page(databuf, databuf[1], (USERDATA_OFFSET/PAGESIZE)+2);
    if (retval != 0) {
        return -1;
    }

    return 0;
}

int read_userdata()
{
    int retval, i;
    alt_u8 databuf[PAGESIZE];
    userdata_hdr udhdr;
    userdata_entry udentry;

    retval = read_flash(USERDATA_OFFSET, USERDATA_HDR_SIZE, databuf);
    if (retval != 0) {
        printf("Flash read error\n");
        return -1;
    }

    strncpy(udhdr.userdata_key, (char*)databuf, 8);
    if (strncmp(udhdr.userdata_key, "USRDATA", 8)) {
        printf("No userdata found on flash\n");
        return 1;
    }

    udhdr.version_major = databuf[8];
    udhdr.version_minor = databuf[9];
    udhdr.num_entries = databuf[10];

    //TODO: check version compatibility
    printf("Userdata: v%u.%.2u, %u entries\n", udhdr.version_major, udhdr.version_minor, udhdr.num_entries);

    for (i=0; i<udhdr.num_entries; i++) {
        retval = read_flash(USERDATA_OFFSET+((i+1)*PAGESIZE), USERDATA_ENTRY_HDR_SIZE, databuf);
        if (retval != 0) {
            printf("Flash read error\n");
            return -1;
        }

        udentry.type = databuf[0];
        udentry.entry_len = databuf[1];

        retval = read_flash(USERDATA_OFFSET+((i+1)*PAGESIZE), udentry.entry_len, databuf);
        if (retval != 0) {
            printf("Flash read error\n");
            return -1;
        }

        switch (udentry.type) {
        case UDE_REMOTE_MAP:
            if ((udentry.entry_len-4) == sizeof(rc_keymap)) {
                memcpy(rc_keymap, databuf+4, sizeof(rc_keymap));
                printf("RC data read (%u bytes)\n", sizeof(rc_keymap));
            }
            break;
        case UDE_AVCONFIG:
            if ((udentry.entry_len-4) == sizeof(avconfig_t)) {
                memcpy(&cm.cc, databuf+4, sizeof(avconfig_t));
                memcpy(&tc, databuf+4, sizeof(avconfig_t));
                printf("Avconfig data read (%u bytes)\n", sizeof(avconfig_t));
            }
            break;
        default:
            printf("Unknown userdata entry\n");
            break;
        }
    }

    return 0;
}
