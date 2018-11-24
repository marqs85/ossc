//
// Copyright (C) 2015-2018  Markus Hiienkari <mhiienka@niksula.hut.fi>
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
#include "userdata.h"
#include "flash.h"
#include "sdcard.h"
#include "firmware.h"
#include "lcd.h"
#include "controls.h"
#include "av_controller.h"
#include "altera_avalon_pio_regs.h"

extern alt_u16 rc_keymap[REMOTE_MAX_KEYS];
extern avmode_t cm;
extern avconfig_t tc;
extern mode_data_t video_modes[];
extern avinput_t target_input;
extern alt_u8 update_cur_vm;
extern alt_u8 input_profiles[AV_LAST];
extern alt_u8 profile_sel;
extern alt_u8 def_input, profile_link;
extern alt_u8 lcd_bl_timeout;
extern alt_u8 auto_input, auto_av1_ypbpr, auto_av2_ypbpr, auto_av3_ypbpr;
extern SD_DEV sdcard_dev;
extern char menu_row1[LCD_ROW_LEN+1], menu_row2[LCD_ROW_LEN+1];

char target_profile_name[PROFILE_NAME_LEN+1];

int write_userdata(alt_u8 entry)
{
    alt_u8 databuf[PAGESIZE];
    alt_u16 vm_to_write;
    alt_u16 pageoffset, srcoffset;
    alt_u8 pageno;
    alt_u32 bytes_to_w;
    int retval;

    if (entry > MAX_USERDATA_ENTRY) {
        printf("invalid entry\n");
        return -1;
    }

    strncpy(((ude_hdr*)databuf)->userdata_key, "USRDATA", 8);
    ((ude_hdr*)databuf)->version_major = FW_VER_MAJOR;
    ((ude_hdr*)databuf)->version_minor = FW_VER_MINOR;
    ((ude_hdr*)databuf)->type = (entry > MAX_PROFILE) ? UDE_INITCFG : UDE_PROFILE;

    switch (((ude_hdr*)databuf)->type) {
    case UDE_INITCFG:
        ((ude_initcfg*)databuf)->data_len = sizeof(ude_initcfg) - offsetof(ude_initcfg, last_profile);
        memcpy(((ude_initcfg*)databuf)->last_profile, input_profiles, sizeof(input_profiles));
        ((ude_initcfg*)databuf)->last_input = target_input;
        ((ude_initcfg*)databuf)->def_input = def_input;
        ((ude_initcfg*)databuf)->profile_link = profile_link;
        ((ude_initcfg*)databuf)->lcd_bl_timeout = lcd_bl_timeout;
        ((ude_initcfg*)databuf)->auto_input = auto_input;
        ((ude_initcfg*)databuf)->auto_av1_ypbpr = auto_av1_ypbpr;
        ((ude_initcfg*)databuf)->auto_av2_ypbpr = auto_av2_ypbpr;
        ((ude_initcfg*)databuf)->auto_av3_ypbpr = auto_av3_ypbpr;
        memcpy(((ude_initcfg*)databuf)->keys, rc_keymap, sizeof(rc_keymap));
        retval = write_flash_page(databuf, sizeof(ude_initcfg), (USERDATA_OFFSET+entry*SECTORSIZE)/PAGESIZE);
        if (retval != 0)
            return -1;

        printf("Initconfig data written (%u bytes)\n", sizeof(ude_initcfg) - offsetof(ude_initcfg, last_profile));
        break;
    case UDE_PROFILE:
        vm_to_write = VIDEO_MODES_SIZE;
        ((ude_profile*)databuf)->avc_data_len = sizeof(avconfig_t);
        ((ude_profile*)databuf)->vm_data_len = vm_to_write;

        if (target_profile_name[0] == 0)
            sniprintf(target_profile_name, PROFILE_NAME_LEN+1, "<used>");

        strncpy(((ude_profile*)databuf)->name, target_profile_name, PROFILE_NAME_LEN+1);

        pageoffset = offsetof(ude_profile, avc);

        // assume that sizeof(avconfig_t) << PAGESIZE
        memcpy(databuf+pageoffset, &tc, sizeof(avconfig_t));
        pageoffset += sizeof(avconfig_t);

        // write a full page first
        memcpy(databuf+pageoffset, (char*)video_modes, PAGESIZE-pageoffset);
        srcoffset = PAGESIZE-pageoffset;
        vm_to_write -= PAGESIZE-pageoffset;
        write_flash_page(databuf, PAGESIZE, ((USERDATA_OFFSET+entry*SECTORSIZE)/PAGESIZE));

        // then write the rest
        if (vm_to_write > 0)
            write_flash((alt_u8*)video_modes+srcoffset, vm_to_write, ((USERDATA_OFFSET+entry*SECTORSIZE)/PAGESIZE) + 1);

        printf("Profile %u data written (%u bytes)\n", entry, sizeof(avconfig_t)+VIDEO_MODES_SIZE);
        break;
    default:
        break;
    }

    return 0;
}

int read_userdata(alt_u8 entry, int dry_run)
{
    int retval, i;
    alt_u8 databuf[PAGESIZE];
    alt_u16 vm_to_read;
    alt_u16 pageoffset, dstoffset;
    alt_u8 pageno;

    target_profile_name[0] = 0;

    if (entry > MAX_USERDATA_ENTRY) {
        printf("invalid entry\n");
        return -1;
    }

    retval = read_flash(USERDATA_OFFSET+(entry*SECTORSIZE), PAGESIZE, databuf);
    if (retval != 0) {
        printf("Flash read error\n");
        return -1;
    }

    if (strncmp(((ude_hdr*)databuf)->userdata_key, "USRDATA", 8)) {
        printf("No userdata found on entry %u\n", entry);
        return 1;
    }

    if ((((ude_hdr*)databuf)->version_major != FW_VER_MAJOR) || (((ude_hdr*)databuf)->version_minor != FW_VER_MINOR)) {
        printf("Data version %u.%u does not match fw\n", ((ude_hdr*)databuf)->version_major, ((ude_hdr*)databuf)->version_minor);
        return 2;
    }

    switch (((ude_hdr*)databuf)->type) {
    case UDE_INITCFG:
        if (((ude_initcfg*)databuf)->data_len == sizeof(ude_initcfg) - offsetof(ude_initcfg, last_profile)) {
            if (dry_run)
                return 0;

            for (i = 0; i < sizeof(input_profiles)/sizeof(*input_profiles); ++i)
                if (((ude_initcfg*)databuf)->last_profile[i] <= MAX_PROFILE)
                    input_profiles[i] = ((ude_initcfg*)databuf)->last_profile[i];
            def_input = ((ude_initcfg*)databuf)->def_input;
            if (def_input < AV_LAST)
                target_input = def_input;
            else if (((ude_initcfg*)databuf)->last_input < AV_LAST)
                target_input = ((ude_initcfg*)databuf)->last_input;
            auto_input = ((ude_initcfg*)databuf)->auto_input;
            auto_av1_ypbpr = ((ude_initcfg*)databuf)->auto_av1_ypbpr;
            auto_av2_ypbpr = ((ude_initcfg*)databuf)->auto_av2_ypbpr;
            auto_av3_ypbpr = ((ude_initcfg*)databuf)->auto_av3_ypbpr;
            profile_link = ((ude_initcfg*)databuf)->profile_link;
            profile_sel = input_profiles[AV_TESTPAT]; // Global profile
            lcd_bl_timeout = ((ude_initcfg*)databuf)->lcd_bl_timeout;
            memcpy(rc_keymap, ((ude_initcfg*)databuf)->keys, sizeof(rc_keymap));
            printf("RC data read (%u bytes)\n", sizeof(rc_keymap));
        }
        break;
    case UDE_PROFILE:
        if ((((ude_profile*)databuf)->avc_data_len == sizeof(avconfig_t)) && (((ude_profile*)databuf)->vm_data_len == VIDEO_MODES_SIZE)) {
            strncpy(target_profile_name, ((ude_profile*)databuf)->name, PROFILE_NAME_LEN+1);
            if (dry_run)
                return 0;

            vm_to_read = ((ude_profile*)databuf)->vm_data_len;

            pageno = 0;
            pageoffset = offsetof(ude_profile, avc);

            // assume that sizeof(avconfig_t) << PAGESIZE
            memcpy(&tc, databuf+pageoffset, sizeof(avconfig_t));
            pageoffset += sizeof(avconfig_t);

            dstoffset = 0;
            while (vm_to_read > 0) {
                if (vm_to_read >= PAGESIZE-pageoffset) {
                    memcpy((char*)video_modes+dstoffset, databuf+pageoffset, PAGESIZE-pageoffset);
                    dstoffset += PAGESIZE-pageoffset;
                    vm_to_read -= PAGESIZE-pageoffset;
                    pageoffset = 0;
                    pageno++;
                    // check
                    read_flash(USERDATA_OFFSET+(entry*SECTORSIZE)+pageno*PAGESIZE, PAGESIZE, databuf);
                } else {
                    memcpy((char*)video_modes+dstoffset, databuf+pageoffset, vm_to_read);
                    pageoffset += vm_to_read;
                    vm_to_read = 0;
                }
            }
            update_cur_vm = 1;

            printf("Profile %u data read (%u bytes)\n", entry, sizeof(avconfig_t)+VIDEO_MODES_SIZE);
        }
        break;
    default:
        printf("Unknown userdata entry\n");
        break;
    }

    return 0;
}

int import_userdata()
{
    int retval;
    int n, entries_imported=0;
    char *errmsg;
    alt_u8 databuf[SD_BLK_SIZE];
    ude_hdr header;
    alt_u32 btn_vec;

    retval = check_sdcard(databuf);
    SPI_CS_High();
    if (retval != 0)
        goto failure;

    strncpy(menu_row1, "Import? 1=Y, 2=N", LCD_ROW_LEN+1);
    *menu_row2 = '\0';
    lcd_write_menu();

    while (1) {
        btn_vec = IORD_ALTERA_AVALON_PIO_DATA(PIO_1_BASE) & RC_MASK;

        if (btn_vec == rc_keymap[RC_BTN1]) {
            break;
        } else if (btn_vec == rc_keymap[RC_BTN2]) {
            retval = UDATA_IMPT_CANCELLED;
            goto failure;
        }

        usleep(WAITLOOP_SLEEP_US);
    }

    strncpy(menu_row1, "Loading settings", LCD_ROW_LEN+1);
    strncpy(menu_row2, "please wait...", LCD_ROW_LEN+1);
    lcd_write_menu();

    // Import the userdata
    for (n=0; n<=MAX_USERDATA_ENTRY; ++n) {
        retval = SD_Read(&sdcard_dev, &header, (512+n*SECTORSIZE)/SD_BLK_SIZE, 0, sizeof(header));
        if (retval != 0) {
            printf("Failed to read SD card\n");
            retval = -retval;
            goto failure;
        }

        if (strncmp(header.userdata_key, "USRDATA", 8)) {
            printf("Not an userdata entry at 0x%x\n", 512+n*SECTORSIZE);
            continue;
        }

        if ((header.version_major != FW_VER_MAJOR) || (header.version_minor != FW_VER_MINOR)) {
            printf("Data version %u.%u does not match fw\n", header.version_major, header.version_minor);
            continue;
        }

        if (header.type > UDE_PROFILE) {
            printf("Unknown userdata entry type %u\n", header.type);
            continue;
        }

        // Just blindly write the entry to flash
        retval = copy_sd_to_flash((512+n*SECTORSIZE)/SD_BLK_SIZE, (n*PAGES_PER_SECTOR)+(USERDATA_OFFSET/PAGESIZE),
            (header.type == UDE_PROFILE) ? sizeof(ude_profile) : sizeof(ude_initcfg), databuf);
        if (retval != 0) {
            printf("Copy from SD to flash failed (error %d)\n", retval);
            goto failure;
        }

        entries_imported++;
    }

    SPI_CS_High();

    read_userdata(INIT_CONFIG_SLOT, 0);
    profile_sel = input_profiles[target_input];
    read_userdata(profile_sel, 0);

    sniprintf(menu_row1, LCD_ROW_LEN+1, "%d entries", entries_imported);
    strncpy(menu_row2, "imported", LCD_ROW_LEN+1);
    lcd_write_menu();
    usleep(1000000);

    return 0;

failure:
    SPI_CS_High();

    return -1;
}
