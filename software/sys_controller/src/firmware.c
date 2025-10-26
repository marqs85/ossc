//
// Copyright (C) 2015-2023  Markus Hiienkari <mhiienka@niksula.hut.fi>
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
#include <stdio.h>
#include "system.h"
#include "firmware.h"
#include "sdcard.h"
#include "flash.h"
#include "controls.h"
#include "tvp7002.h"
#include "av_controller.h"
#include "lcd.h"
#include "utils.h"
#include "menu.h"
#include "ff.h"
#include "file.h"
#include "altera_avalon_pio_regs.h"

#define SECONDARY_FW_ADDR 0x00080000
#define FW_FINGERPRINT 0xEFEFEF56
#define FW_FINGERPRINT_OFFSET 0x20

extern char menu_row1[LCD_ROW_LEN+1], menu_row2[LCD_ROW_LEN+1];
extern alt_u16 rc_keymap[REMOTE_MAX_KEYS];
extern char tmpbuf[SD_BLK_SIZE];
extern SD_DEV sdcard_dev;
extern alt_u32 sys_ctrl;
extern flash_ctrl_dev flashctrl_dev;
extern rem_update_dev rem_reconfig_dev;

int fw_init_secondary() {
    uint32_t fingerprint = *(uint32_t*)(INTEL_GENERIC_SERIAL_FLASH_INTERFACE_TOP_0_AVL_MEM_BASE + SECONDARY_FW_ADDR + FW_FINGERPRINT_OFFSET);

    if (fingerprint != FW_FINGERPRINT) {
        printf("Invalid FW fingerprint (0x%.8x instead of 0x%.8x)\n", fingerprint, FW_FINGERPRINT);
        return -1;
    }

    rem_reconfig_dev.regs->image_addr[0] = SECONDARY_FW_ADDR;
    rem_reconfig_dev.regs->wdog_enable[0] = 0;
    rem_reconfig_dev.regs->reconfig_start = 1;

    while (1) {}

    return 0;
}

int fw_update(char *dirname, char *filename) {
    FIL fw_file;
    SDRESULTS res;
    char dirname_root[10];
    fw_hdr hdr;
    int retval;
    unsigned bytes_read, bytes_to_copy;
    uint32_t crcval, hdr_len, btn_vec;
    uint32_t cluster_idx[100]; // enough for >=4kB cluster size
    uint16_t fs_csize, fs_startsec, cl_iter, cl_soffs;
    uint32_t flash_addr;

    if (!sdcard_dev.mount) {
        retval = file_mount();
        if (retval != 0) {
            printf("SD card not detected %d\n", retval);
            return -1;
        }
    }

    sniprintf(dirname_root, sizeof(dirname_root), "/%s", dirname);
    f_chdir(dirname_root);

    if (!file_open(&fw_file, filename)) {
        strlcpy(menu_row1, "Checking FW", LCD_ROW_LEN+1);
        strlcpy(menu_row2, "Please wait...", LCD_ROW_LEN+1);
        ui_disp_menu(1);

        if (f_read(&fw_file, &hdr, sizeof(hdr), &bytes_read) != F_OK) {
            printf("FW hdr read error\n");
            retval = -3;
            goto close_file;
        }

        hdr_len = bswap32(hdr.hdr_len);

        if (!(!strncmp(hdr.fw_key, "OSSC", 4) || !strncmp(hdr.fw_key, "OSS2", 4)) || (hdr_len < 26 || hdr_len > 508)) {
            printf("Invalid FW header\n");
            retval = -4;
            goto close_file;
        }

        // Set target flash address
        flash_addr = hdr.fw_key[3] == 'C' ? 0x00000000 : SECONDARY_FW_ADDR;

        crcval = crc32((unsigned char *)&hdr, hdr_len, 1);
        hdr.hdr_len = bswap32(hdr.hdr_len);
        hdr.data_len = bswap32(hdr.data_len);
        hdr.data_crc = bswap32(hdr.data_crc);
        hdr.hdr_crc = bswap32(hdr.hdr_crc);
        if (crcval != hdr.hdr_crc) {
            printf("Invalid FW header CRC (0x%.8x instead of 0x%.8x)\n", crcval, hdr.hdr_crc);
            retval = -5;
            goto close_file;
        }

        printf("Firmware %u.%u%s%s\n", hdr.version_major, hdr.version_minor, hdr.version_suffix[0] ? "-" : "", hdr.version_suffix);
        bytes_to_copy = hdr.data_len;
        printf("    Image: %u bytes, crc 0x%.8x\n", hdr.data_len, hdr.data_crc);
        if (hdr.data_len >= 16*FLASH_SECTOR_SIZE) {
            printf("Image exceeds flash allocation\n");
            retval = -6;
            goto close_file;
        }

        sniprintf(menu_row1, LCD_ROW_LEN+1, "v%u.%u%s%s%s", hdr.version_major, hdr.version_minor, hdr.version_suffix[0] ? "-" : "", hdr.version_suffix, (hdr.fw_key[3] == 'C') ? "" : " (sec)" );
        sniprintf(menu_row2, LCD_ROW_LEN+1, "Update? 1=Y, 2=N");
        ui_disp_menu(1);

        while (1) {
            btn_vec = IORD_ALTERA_AVALON_PIO_DATA(PIO_1_BASE) & RC_MASK;

            if (btn_vec == rc_keymap[RC_BTN1]) {
                break;
            } else if (btn_vec == rc_keymap[RC_BTN2]) {
                set_func_ret_msg("Cancelled");
                retval = 1;
                goto close_file;
            }

            usleep(WAITLOOP_SLEEP_US);
        }

        strlcpy(menu_row2, "Please wait...", LCD_ROW_LEN+1);
        ui_disp_menu(1);

        // check if 512-byte header is on first or second cluster
        fs_startsec = fw_file.obj.fs->database;
        fs_csize = fw_file.obj.fs->csize;
        if (fs_csize == 1) {
            cl_iter = 1;
            cl_soffs = 0;
            cluster_idx[0] = fw_file.obj.sclust;
        } else {
            cl_iter = 0;
            cl_soffs = 1;
        }
        // record cluster IDs to an array
        if ((f_read_cc(&fw_file, &cluster_idx[cl_iter], bytes_to_copy, &bytes_read, (sizeof(cluster_idx)/sizeof(cluster_idx[0]))-1) != F_OK) || (bytes_read != bytes_to_copy)) {
            printf("FW cluster error\n");
            retval = -7;
            goto close_file;
        }

        file_close(&fw_file);
        f_chdir("/");

        printf("Checking copied data...\n");
        while (bytes_to_copy > 0) {
            bytes_read = (bytes_to_copy > SD_BLK_SIZE) ? SD_BLK_SIZE : bytes_to_copy;
            res = SD_Read(&sdcard_dev, tmpbuf, ((cluster_idx[cl_iter]-2)*fs_csize+fs_startsec+cl_soffs), 0, bytes_read);
            if (res != SD_OK) {
                printf("FW data read error\n");
                retval = -8;
                goto close_file;
            }

            crcval = crc32((unsigned char *)&tmpbuf, bytes_read, (bytes_to_copy==hdr.data_len));
            bytes_to_copy -= bytes_read;

            cl_soffs += 1;
            if (cl_soffs == fs_csize) {
                cl_iter += 1;
                cl_soffs = 0;
            }
        }

        if (crcval != hdr.data_crc) {
            printf("Image: Invalid CRC (0x%.8x)\n", crcval);
            retval = -9;
            goto close_file;
        }

        printf("Starting update procedure...\n");
        strlcpy(menu_row1, "Updating", LCD_ROW_LEN+1);
        ui_disp_menu(1);

        //disable video output
        tvp_powerdown();
        sys_ctrl |= VIDGEN_OFF;
        IOWR_ALTERA_AVALON_PIO_DATA(PIO_0_BASE, sys_ctrl);
        usleep(10000);

        // No return from here
        fw_update_commit(cluster_idx, hdr.data_len, fs_csize, fs_startsec, flash_addr);
        return 0;
    } else {
        printf("FW file not found\n");
        f_chdir("/");
        return -2;
    }

close_file:
    file_close(&fw_file);
    f_chdir("/");
    return retval;
}

// commit FW update. Do not call functions located in flash during update
void __attribute__((noinline, flatten, noreturn, __section__(".text_bram"))) fw_update_commit(uint32_t* cluster_idx, uint32_t bytes_to_copy, uint16_t fs_csize, uint16_t fs_startsec, uint32_t flash_addr) {
    int i, sectors;
    SDRESULTS res;
    uint16_t cl_iter, cl_soffs;
    uint32_t addr, bytes_read;
    uint32_t *data_to;

    flash_write_protect(&flashctrl_dev, 0);

    // Erase sectors
    addr = flash_addr;
    sectors = (bytes_to_copy/FLASH_SECTOR_SIZE) + ((bytes_to_copy % FLASH_SECTOR_SIZE) != 0);
    data_to = (uint32_t*)(INTEL_GENERIC_SERIAL_FLASH_INTERFACE_TOP_0_AVL_MEM_BASE + flash_addr);

    for (i=0; i<sectors; i++) {
        flash_sector_erase(&flashctrl_dev, addr);
        addr += FLASH_SECTOR_SIZE;
    }

    if (fs_csize == 1) {
        cl_iter = 1;
        cl_soffs = 0;
    } else {
        cl_iter = 0;
        cl_soffs = 1;
    }

    // Write data
    while (bytes_to_copy > 0) {
        bytes_read = (bytes_to_copy > SD_BLK_SIZE) ? SD_BLK_SIZE : bytes_to_copy;
        res = SD_Read(&sdcard_dev, tmpbuf, ((cluster_idx[cl_iter]-2)*fs_csize+fs_startsec+cl_soffs), 0, bytes_read);
        //TODO: retry if read fails

        bytes_to_copy -= bytes_read;
        for (i=0; i<bytes_read; i++)
            tmpbuf[i] = bitswap8(tmpbuf[i]);
        for (i=0; i<bytes_read; i+=4)
            *data_to++ = *((uint32_t*)&tmpbuf[i]);

        cl_soffs += 1;
        if (cl_soffs == fs_csize) {
            cl_iter += 1;
            cl_soffs = 0;
        }
    }

    // flush command FIFO before FPGA reconfiguration start
    *(volatile uint32_t*)(INTEL_GENERIC_SERIAL_FLASH_INTERFACE_TOP_0_AVL_MEM_BASE);

    rem_reconfig_dev.regs->reconfig_start = 1;

    while (1) {}
}
