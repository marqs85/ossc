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

#include <errno.h>
#include <string.h>
#include <unistd.h>
#include "userdata.h"
#include "fat16_export.h"
#include "flash.h"
#include "sdcard.h"
#include "firmware.h"
#include "lcd.h"
#include "controls.h"
#include "av_controller.h"
#include "menu.h"
#include "utils.h"
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
extern alt_u8 osd_enable_pre, osd_status_timeout_pre;
extern SD_DEV sdcard_dev;
extern alt_flash_dev *epcq_dev;
extern char menu_row1[LCD_ROW_LEN+1], menu_row2[LCD_ROW_LEN+1];

char target_profile_name[PROFILE_NAME_LEN+1];

int write_userdata(alt_u8 entry)
{
    alt_u8 databuf[PAGESIZE];
    alt_u16 vm_to_write;
    alt_u16 pageoffset, srcoffset;
    alt_u8 pageno;
    alt_u32 bytes_to_w;
    int retval, i;

    if (entry > MAX_USERDATA_ENTRY) {
        printf("invalid entry\n");
        return -1;
    }

    strncpy(((ude_hdr*)databuf)->userdata_key, "USRDATA", 8);
    ((ude_hdr*)databuf)->type = (entry > MAX_PROFILE) ? UDE_INITCFG : UDE_PROFILE;

    switch (((ude_hdr*)databuf)->type) {
    case UDE_INITCFG:
        ((ude_hdr*)databuf)->version_major = INITCFG_VER_MAJOR;
        ((ude_hdr*)databuf)->version_minor = INITCFG_VER_MINOR;
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
        ((ude_initcfg*)databuf)->osd_enable = osd_enable_pre;
        ((ude_initcfg*)databuf)->osd_status_timeout = osd_status_timeout_pre;
        memcpy(((ude_initcfg*)databuf)->keys, rc_keymap, sizeof(rc_keymap));
        for (i=0; i<sizeof(ude_initcfg); i++)
            databuf[i] = bitswap8(databuf[i]);
        retval = alt_epcq_controller2_write(epcq_dev, (USERDATA_OFFSET+entry*SECTORSIZE), databuf, sizeof(ude_initcfg));
        if (retval != 0)
            return retval;

        printf("Initconfig data written (%u bytes)\n", sizeof(ude_initcfg) - offsetof(ude_initcfg, last_profile));
        break;
    case UDE_PROFILE:
        ((ude_hdr*)databuf)->version_major = PROFILE_VER_MAJOR;
        ((ude_hdr*)databuf)->version_minor = PROFILE_VER_MINOR;
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

        // erase sector and write a full page first, assume sizeof(video_modes) >> PAGESIZE
        memcpy(databuf+pageoffset, (char*)video_modes, PAGESIZE-pageoffset);
        srcoffset = PAGESIZE-pageoffset;
        vm_to_write -= PAGESIZE-pageoffset;
        for (i=0; i<PAGESIZE; i++)
            databuf[i] = bitswap8(databuf[i]);
        retval = alt_epcq_controller2_write(epcq_dev, (USERDATA_OFFSET+entry*SECTORSIZE), databuf, PAGESIZE);
        if (retval != 0)
            return retval;

        // then write the rest page by page
        pageno = 1;
        while (vm_to_write > 0) {
            memcpy(databuf, (char*)video_modes+srcoffset, (vm_to_write > PAGESIZE) ? PAGESIZE : vm_to_write);
            for (i=0; i<PAGESIZE; i++)
                databuf[i] = bitswap8(databuf[i]);
            retval = alt_epcq_controller2_write_block(epcq_dev, (USERDATA_OFFSET+entry*SECTORSIZE), (USERDATA_OFFSET+entry*SECTORSIZE+pageno*PAGESIZE), databuf, (vm_to_write > PAGESIZE) ? PAGESIZE : vm_to_write);
            if (retval != 0)
                return retval;

            srcoffset += PAGESIZE;
            vm_to_write = (vm_to_write < PAGESIZE) ? 0 : (vm_to_write - PAGESIZE);
            pageno++;
        }

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

    retval = alt_epcq_controller2_read(epcq_dev, (USERDATA_OFFSET+entry*SECTORSIZE), databuf, PAGESIZE);
    for (i=0; i<PAGESIZE; i++)
        databuf[i] = bitswap8(databuf[i]);
    if (retval != 0)
        return retval;

    if (strncmp(((ude_hdr*)databuf)->userdata_key, "USRDATA", 8)) {
        printf("No userdata found on entry %u\n", entry);
        return 1;
    }

    switch (((ude_hdr*)databuf)->type) {
    case UDE_INITCFG:
        if ((((ude_hdr*)databuf)->version_major != INITCFG_VER_MAJOR) || (((ude_hdr*)databuf)->version_minor != INITCFG_VER_MINOR)) {
            printf("Initconfig version %u.%u does not match current one\n", ((ude_hdr*)databuf)->version_major, ((ude_hdr*)databuf)->version_minor);
            return 2;
        }
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
            osd_enable_pre = ((ude_initcfg*)databuf)->osd_enable;
            osd_status_timeout_pre = ((ude_initcfg*)databuf)->osd_status_timeout;
            profile_link = ((ude_initcfg*)databuf)->profile_link;
            profile_sel = input_profiles[AV_TESTPAT]; // Global profile
            lcd_bl_timeout = ((ude_initcfg*)databuf)->lcd_bl_timeout;
            memcpy(rc_keymap, ((ude_initcfg*)databuf)->keys, sizeof(rc_keymap));
            printf("RC data read (%u bytes)\n", sizeof(rc_keymap));
        }
        break;
    case UDE_PROFILE:
        if ((((ude_hdr*)databuf)->version_major != PROFILE_VER_MAJOR) || (((ude_hdr*)databuf)->version_minor != PROFILE_VER_MINOR)) {
            printf("Profile version %u.%u does not match current one\n", ((ude_hdr*)databuf)->version_major, ((ude_hdr*)databuf)->version_minor);
            return 2;
        }
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
                    retval = alt_epcq_controller2_read(epcq_dev, (USERDATA_OFFSET+entry*SECTORSIZE+pageno*PAGESIZE), databuf, PAGESIZE);
                    for (i=0; i<PAGESIZE; i++)
                        databuf[i] = bitswap8(databuf[i]);
                    if (retval != 0)
                        return retval;
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
    SDRESULTS res;
    int retval;
    int n, entries_imported=0;
    char *errmsg;
    alt_u8 databuf[SD_BLK_SIZE];
    ude_hdr header;
    alt_u32 btn_vec;

    retval = check_sdcard(databuf);
    SPI_CS_High();
    if (retval != 0)
        goto sd_disable;

    strncpy(menu_row2, "Import? 1=Y, 2=N", LCD_ROW_LEN+1);
    ui_disp_menu(2);

    while (1) {
        btn_vec = IORD_ALTERA_AVALON_PIO_DATA(PIO_1_BASE) & RC_MASK;

        if (btn_vec == rc_keymap[RC_BTN1]) {
            break;
        } else if (btn_vec == rc_keymap[RC_BTN2]) {
            retval = UDATA_IMPT_CANCELLED;
            strncpy(menu_row2, "Cancelled", LCD_ROW_LEN+1);
            goto sd_disable;
        }

        usleep(WAITLOOP_SLEEP_US);
    }

    strncpy(menu_row2, "Loading...", LCD_ROW_LEN+1);
    ui_disp_menu(2);

    // Import the userdata
    for (n=0; n<=MAX_USERDATA_ENTRY; ++n) {
        res = SD_Read(&sdcard_dev, &header, (512+n*SECTORSIZE)/SD_BLK_SIZE, 0, sizeof(header));
        if (res != SD_OK) {
            printf("Failed to read SD card\n");
            retval = -res;
            goto sd_disable;
        }

        if (strncmp(header.userdata_key, "USRDATA", 8)) {
            printf("Not an userdata entry at 0x%x\n", 512+n*SECTORSIZE);
            continue;
        }

        if ((header.version_major != PROFILE_VER_MAJOR) || (header.version_minor != PROFILE_VER_MINOR)) {
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
            goto sd_disable;
        }

        entries_imported++;
    }

    read_userdata(INIT_CONFIG_SLOT, 0);
    profile_sel = input_profiles[target_input];
    read_userdata(profile_sel, 0);

    sniprintf(menu_row2, LCD_ROW_LEN+1, "%d slots loaded", entries_imported);
    retval = 1;

sd_disable:
    SPI_CS_High();

    return retval;
}

static alt_u8 poll_yesno(const useconds_t useconds, alt_u32 *const btn_vec_out)
{
    alt_u32 btn_vec;
    alt_u8 ret = 0U;

    for (alt_u32 i = 0; i < (useconds/WAITLOOP_SLEEP_US); ++i) {
        btn_vec = IORD_ALTERA_AVALON_PIO_DATA(PIO_1_BASE) & RC_MASK;

        for (alt_u32 j = RC_BTN1; j < (REMOTE_MAX_KEYS - 1); ++j) {
            if (btn_vec == rc_keymap[j]) {
                ret = 1U;
                break;
            }
        }

        if (ret)
            break;

        usleep(WAITLOOP_SLEEP_US);
    }

    if (ret)
        *btn_vec_out = btn_vec;

    return ret;
}

int export_userdata()
{
    int retval;
    const char *msg;
    alt_u8 databuf[SD_BLK_SIZE];
    alt_u8 prompt_state = 0;
    useconds_t prompt_delay;
    const alt_u8 prompt_transitions[] = { 1, 2, 0, 0, };
    const alt_u8 prompt_ofs[] = { 0, 16, 31, 48, };
    const char *prompt_msgs =
        "SD CARD WILL BE" "\0" // [ 0..15]
        "OVERWRITTEN!!!"  "\0" // [16..30]
        "Export? 1=Y, 2=N""\0" // [31..47]
        "Press 1 or 2";        // [48..60]
    alt_u32 btn_vec;

    _Static_assert(SD_BLK_SIZE == FAT16_SECTOR_SIZE, "Sector size mismatch");

    retval = check_sdcard(databuf);
    SPI_CS_High();
    if (retval != 0) {
        retval = -retval;
        goto out;
    }

    usleep(100000U);
    while (1) {
        msg = &prompt_msgs[prompt_ofs[prompt_state]];
        prompt_delay = (prompt_state == 2) ? 2000000U
            : ((prompt_state == 3) ? 300000U : 1000000U);
        prompt_state = prompt_transitions[prompt_state];

        strncpy(menu_row2, msg, LCD_ROW_LEN+1);
        ui_disp_menu(2);
        if (poll_yesno(prompt_delay, &btn_vec))
            goto eval_button;

        continue;
eval_button:
        if (btn_vec == rc_keymap[RC_BTN1]) {
            break;
        } else if (btn_vec == rc_keymap[RC_BTN2] ||
            btn_vec == rc_keymap[RC_BACK])
        {
            retval = UDATA_EXPT_CANCELLED;
            goto out;
        }
        prompt_state = 3;
    }

    strncpy(menu_row2, "Exporting...", LCD_ROW_LEN+1);
    ui_disp_menu(2);

    /* Generate and write the boot sector. */
    memset(databuf, 0, SD_BLK_SIZE);
    generate_boot_sector_16(databuf);
    retval = SD_Write(&sdcard_dev, databuf, 0);
    if (retval)
        goto out;

    /* Zero out the FAT area */
    /* TODO: a proper erase would be more ideal... */
    memset(databuf, 0, SD_BLK_SIZE);
    for (alt_u32 fat_sector = FAT16_1_OFS/SD_BLK_SIZE;
        fat_sector < (FAT16_2_OFS + FAT16_SIZE)/SD_BLK_SIZE; ++fat_sector)
    {
        retval = SD_Write(&sdcard_dev, databuf, fat_sector);
        if (retval)
            goto out;
    }

    /* Generate and write the file allocation tables. */
    for (alt_u16 clusters_written = 0, sd_blk_idx = 0;
        clusters_written < (PROF_16_DATA_SIZE/FAT16_CLUSTER_SIZE);)
    {
        alt_u16 count;
        memset(databuf, 0, SD_BLK_SIZE);
        count = generate_fat16(databuf, clusters_written);
        retval = SD_Write(&sdcard_dev, databuf,
            (FAT16_1_OFS/SD_BLK_SIZE) + sd_blk_idx);
        if (retval)
            goto out;

        retval = SD_Write(&sdcard_dev, databuf,
            (FAT16_2_OFS/SD_BLK_SIZE) + sd_blk_idx);
        if (retval)
            goto out;

        ++sd_blk_idx;
        clusters_written = count;
    }

    /* Write the directory entry of the settings file. */
    memset(databuf, 0, SD_BLK_SIZE);
    memcpy(databuf, prof_dirent_16, PROF_DIRENT_16_SIZE);
    retval = SD_Write(&sdcard_dev, databuf, PROF_DIRENT_16_OFS/SD_BLK_SIZE);
    if (retval)
        goto out;

    /* This may wear the SD card a bit more than necessary... */
    retval = copy_flash_to_sd(USERDATA_OFFSET/PAGESIZE,
        PROF_16_DATA_OFS/SD_BLK_SIZE,
        (MAX_USERDATA_ENTRY + 1) * SECTORSIZE,
        databuf);

out:
    SPI_CS_High();

    switch (retval) {
        case 0:
            msg = "Success";
            break;
        case SD_NOINIT:
            msg = "No SD card det.";
            break;
        case -EINVAL:
            msg = "Invalid params.";
            break;
        case UDATA_EXPT_CANCELLED:
            msg = "Cancelled";
            break;
        default:
            msg = "SD/Flash error";
            break;
    }
    strncpy(menu_row2, msg, LCD_ROW_LEN+1);

    if (!retval) {
        return 1;
    } else {
        /*
         * We want the message above to remain on screen, so return a
         * positive value which nevertheless stands out when debugging.
         */
        return 0x0dead;
    }
}
