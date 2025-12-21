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

#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include "system.h"
#include "userdata.h"
#include "flash.h"
#include "sdcard.h"
#include "firmware.h"
#include "lcd.h"
#include "controls.h"
#include "av_controller.h"
#include "menu.h"
#include "ff.h"
#include "file.h"

#define UDE_ITEM(ID, VER, ITEM) {{ID, VER, sizeof(ITEM)}, &ITEM}

// include mode array definitions so that sizeof() can be used
#define VM_STATIC_INCLUDE
#include "video_modes_list.c"
#undef VM_STATIC_INCLUDE

extern flash_ctrl_dev flashctrl_dev;
extern uint16_t rc_keymap[REMOTE_MAX_KEYS];
extern uint8_t input_profiles[AV_LAST];
extern avconfig_t tc;
extern settings_t ts;
extern mode_data_t video_modes_plm[];
extern uint8_t update_cur_vm;
extern SD_DEV sdcard_dev;
extern c_shmask_t c_shmask;
extern c_lc_palette_set_t c_lc_palette_set;

char target_profile_name[USERDATA_NAME_LEN+1], cur_profile_name[USERDATA_NAME_LEN+1];

const ude_item_map ude_initcfg_items[] = {
    UDE_ITEM(0, 120, rc_keymap),
    UDE_ITEM(1, 120, input_profiles),
    UDE_ITEM(2, 120, ts.profile_link),
    UDE_ITEM(3, 120, ts.def_input),
    UDE_ITEM(4, 120, ts.auto_input),
    UDE_ITEM(5, 120, ts.auto_av1_ypbpr),
    UDE_ITEM(6, 120, ts.auto_av2_ypbpr),
    UDE_ITEM(7, 120, ts.auto_av3_ypbpr),
    UDE_ITEM(8, 120, ts.lcd_bl_timeout),
    UDE_ITEM(9, 120, ts.osd_enable),
    UDE_ITEM(10, 120, ts.osd_status_timeout),
    UDE_ITEM(11, 120, ts.osd_highlight_color),
    UDE_ITEM(12, 120, ts.phase_hotkey_enable),
};

const ude_item_map ude_profile_items[] = {
    {{0, 120, sizeof(video_modes_plm_default)}, video_modes_plm},
    UDE_ITEM(1, 120, c_shmask),
    UDE_ITEM(65, 120, c_lc_palette_set),
    // avconfig_t
    UDE_ITEM(2, 120, tc.pm_240p),
    UDE_ITEM(3, 120, tc.pm_384p),
    UDE_ITEM(4, 120, tc.pm_480i),
    UDE_ITEM(5, 120, tc.pm_480p),
    UDE_ITEM(6, 120, tc.pm_1080i),
    UDE_ITEM(7, 120, tc.pt_mode),
    UDE_ITEM(8, 120, tc.l2_mode),
    UDE_ITEM(9, 120, tc.l3_mode),
    UDE_ITEM(10, 120, tc.l4_mode),
    UDE_ITEM(11, 120, tc.l5_mode),
    UDE_ITEM(12, 120, tc.l6_mode),
    UDE_ITEM(13, 120, tc.l5_fmt),
    UDE_ITEM(14, 120, tc.s480p_mode),
    UDE_ITEM(15, 120, tc.s400p_mode),
    UDE_ITEM(16, 120, tc.upsample2x),
    UDE_ITEM(17, 120, tc.ar_256col),
    UDE_ITEM(18, 120, tc.default_vic),
    UDE_ITEM(19, 120, tc.clamp_offset),
    UDE_ITEM(20, 120, tc.tvp_hpll2x),
    UDE_ITEM(21, 120, tc.adc_pll_bw),
    UDE_ITEM(22, 120, tc.fpga_pll_bw),
    UDE_ITEM(23, 120, tc.sl_mode),
    UDE_ITEM(24, 120, tc.sl_type),
    UDE_ITEM(25, 120, tc.sl_hybr_str),
    UDE_ITEM(26, 120, tc.sl_method),
    UDE_ITEM(27, 120, tc.sl_altern),
    UDE_ITEM(28, 120, tc.sl_str),
    UDE_ITEM(29, 120, tc.sl_id),
    UDE_ITEM(30, 120, tc.sl_cust_l_str),
    UDE_ITEM(31, 120, tc.sl_cust_c_str),
    UDE_ITEM(32, 120, tc.sl_cust_iv_x),
    UDE_ITEM(33, 120, tc.sl_cust_iv_y),
    UDE_ITEM(34, 120, tc.mask_br),
    UDE_ITEM(35, 120, tc.mask_color),
    UDE_ITEM(36, 120, tc.reverse_lpf),
    UDE_ITEM(37, 120, tc.shmask_mode),
    UDE_ITEM(38, 120, tc.shmask_str),
    UDE_ITEM(39, 120, tc.lumacode_mode),
    UDE_ITEM(40, 120, tc.lumacode_pal),
    UDE_ITEM(41, 120, tc.sync_vth),
    UDE_ITEM(42, 120, tc.linelen_tol),
    UDE_ITEM(43, 120, tc.vsync_thold),
    UDE_ITEM(44, 120, tc.pre_coast),
    UDE_ITEM(45, 120, tc.post_coast),
    UDE_ITEM(46, 120, tc.ypbpr_cs),
    UDE_ITEM(47, 120, tc.video_lpf),
    UDE_ITEM(48, 120, tc.sync_lpf),
    UDE_ITEM(49, 120, tc.stc_lpf),
    UDE_ITEM(50, 120, tc.alc_h_filter),
    UDE_ITEM(51, 120, tc.alc_v_filter),
    UDE_ITEM(52, 120, tc.col),
    UDE_ITEM(53, 120, tc.full_vs_bypass),
    UDE_ITEM(54, 120, tc.audio_dw_sampl),
    UDE_ITEM(55, 120, tc.audio_swap_lr),
    UDE_ITEM(56, 120, tc.audio_gain),
    UDE_ITEM(57, 120, tc.audio_mono),
    UDE_ITEM(58, 120, tc.tx_mode),
    UDE_ITEM(59, 120, tc.hdmi_itc),
    UDE_ITEM(60, 120, tc.hdmi_hdr),
    UDE_ITEM(61, 120, tc.hdmi_vrr),
    UDE_ITEM(62, 120, tc.full_tx_setup),
    UDE_ITEM(63, 120, tc.av3_alt_rgb),
    UDE_ITEM(64, 120, tc.link_av),
    // 65 reserved
    UDE_ITEM(66, 120, tc.panasonic_hack),
    UDE_ITEM(67, 120, tc.o480p_pbox),
};

int write_userdata(uint8_t entry) {
    ude_hdr hdr;
    FIL name_file;
    char p_filename[14];
    const ude_item_map *target_map;
    uint32_t flash_addr, bytes_written;
    int i=0;

    if (entry > MAX_USERDATA_ENTRY) {
        printf("invalid entry\n");
        return -1;
    }

    memset(&hdr, 0x00, sizeof(ude_hdr));
    strlcpy(hdr.userdata_key, "USRDATA", 8);
    hdr.type = (entry > MAX_PROFILE) ? UDE_INITCFG : UDE_PROFILE;

    if (hdr.type == UDE_INITCFG) {
        target_map = ude_initcfg_items;
        hdr.num_items = sizeof(ude_initcfg_items)/sizeof(ude_item_map);

        sniprintf(hdr.name, USERDATA_NAME_LEN+1, "INITCFG");
    } else if (hdr.type == UDE_PROFILE) {
        target_map = ude_profile_items;
        hdr.num_items = sizeof(ude_profile_items)/sizeof(ude_item_map);

        // Check if name override file exists
        sniprintf(p_filename, sizeof(p_filename), "prof_n_i.txt");
        if (!file_open(&name_file, p_filename)) {

            for (i=0; i<=entry; i++) {
                if (file_get_string(&name_file, target_profile_name, sizeof(target_profile_name)) == NULL)
                    break;
            }

            file_close(&name_file);
        }

        if (i == entry+1) {
            // strip CR / CRLF
            target_profile_name[strcspn(target_profile_name, "\r\n")] = 0;

            strlcpy(hdr.name, target_profile_name, USERDATA_NAME_LEN+1);
        } else if (cur_profile_name[0] == 0) {
            sniprintf(hdr.name, USERDATA_NAME_LEN+1, "<used>");
        } else {
            strlcpy(hdr.name, cur_profile_name, USERDATA_NAME_LEN+1);
        }
    }

    flash_addr = flashctrl_dev.flash_size - (16-entry)*FLASH_SECTOR_SIZE;

    // Disable flash write protect and erase sector
    flash_write_protect(&flashctrl_dev, 0);
    flash_sector_erase(&flashctrl_dev, flash_addr);

    // Write data into erased sector
    memcpy((uint32_t*)(INTEL_GENERIC_SERIAL_FLASH_INTERFACE_TOP_0_AVL_MEM_BASE + flash_addr), &hdr, sizeof(ude_hdr));
    bytes_written = sizeof(ude_hdr);
    for (i=0; i<hdr.num_items; i++) {
        memcpy((uint32_t*)(INTEL_GENERIC_SERIAL_FLASH_INTERFACE_TOP_0_AVL_MEM_BASE + flash_addr + bytes_written), &target_map[i].hdr, sizeof(ude_item_hdr));
        bytes_written += sizeof(ude_item_hdr);
        memcpy((uint32_t*)(INTEL_GENERIC_SERIAL_FLASH_INTERFACE_TOP_0_AVL_MEM_BASE + flash_addr + bytes_written), target_map[i].data, target_map[i].hdr.data_size);
        bytes_written += target_map[i].hdr.data_size;
    }

    // Re-enable write protection
    flash_write_protect(&flashctrl_dev, 1);

    printf("%lu bytes written into userdata entry %u\n", bytes_written, entry);

    return 0;
}

int read_userdata(uint8_t entry, int dry_run) {
    ude_hdr hdr;
    ude_item_hdr item_hdr;
    const ude_item_map *target_map;
    uint32_t flash_addr, bytes_read;
    int i, j, target_map_items;

    if (entry > MAX_USERDATA_ENTRY) {
        printf("invalid entry\n");
        return -1;
    }

    flash_addr = flashctrl_dev.flash_size - (16-entry)*FLASH_SECTOR_SIZE;
    memcpy(&hdr, (uint32_t*)(INTEL_GENERIC_SERIAL_FLASH_INTERFACE_TOP_0_AVL_MEM_BASE + flash_addr), sizeof(ude_hdr));
    bytes_read = sizeof(ude_hdr);

    if (strncmp(hdr.userdata_key, "USRDATA", 8)) {
        printf("No userdata found on entry %u\n", entry);
        return 1;
    }

    strlcpy(target_profile_name, hdr.name, USERDATA_NAME_LEN+1);
    if (dry_run)
        return 0;

    target_map = (hdr.type == UDE_INITCFG) ? ude_initcfg_items : ude_profile_items;
    target_map_items = (hdr.type == UDE_INITCFG) ? sizeof(ude_initcfg_items)/sizeof(ude_item_map) : sizeof(ude_profile_items)/sizeof(ude_item_map);

    for (i=0; i<hdr.num_items; i++) {
        memcpy(&item_hdr, (uint32_t*)(INTEL_GENERIC_SERIAL_FLASH_INTERFACE_TOP_0_AVL_MEM_BASE + flash_addr + bytes_read), sizeof(ude_item_hdr));
        bytes_read += sizeof(ude_item_hdr);
        for (j=0; j<target_map_items; j++) {
            if (!memcmp(&item_hdr, &target_map[j].hdr, sizeof(ude_item_hdr))) {
                memcpy(target_map[j].data, (uint32_t*)(INTEL_GENERIC_SERIAL_FLASH_INTERFACE_TOP_0_AVL_MEM_BASE + flash_addr + bytes_read), item_hdr.data_size);
                break;
            }
        }
        bytes_read += item_hdr.data_size;

        if (bytes_read >= FLASH_SECTOR_SIZE) {
            printf("userdata entry %u corrupted\n", entry);
            return -1;
        }
    }

    if (hdr.type == UDE_PROFILE) {
        invalidate_loaded_arrays();
        update_cur_vm = 1;
    }

    strlcpy(cur_profile_name, target_profile_name, USERDATA_NAME_LEN+1);
    printf("%lu bytes read from userdata entry %u\n", bytes_read, entry);

    return 0;
}

int write_userdata_sd(uint8_t entry) {
    FIL p_file, name_file;
    ude_hdr hdr;
    const ude_item_map *target_map;
    unsigned int bytes_written, bytes_written_tot;
    char p_filename[14];
    int i=0, retval=0;

    if (entry == SD_INIT_CONFIG_SLOT)
        sniprintf(p_filename, sizeof(p_filename), "settings.bin");
    else
        sniprintf(p_filename, sizeof(p_filename), "prof%.2u.bin", entry);

    if (entry > MAX_SD_USERDATA_ENTRY) {
        printf("invalid entry\n");
        return -1;
    }

    if (!sdcard_dev.mount) {
        retval = file_mount();
        if (retval != 0) {
            printf("SD card not detected %d\n", retval);
            return -2;
        }
    }

    if (f_open(&p_file, p_filename, FA_WRITE|FA_CREATE_ALWAYS) != F_OK) {
        return -3;
    }

    memset(&hdr, 0x00, sizeof(ude_hdr));
    strlcpy(hdr.userdata_key, "USRDATA", 8);
    hdr.type = (entry > MAX_SD_PROFILE) ? UDE_INITCFG : UDE_PROFILE;

    if (hdr.type == UDE_INITCFG) {
        target_map = ude_initcfg_items;
        hdr.num_items = sizeof(ude_initcfg_items)/sizeof(ude_item_map);

        sniprintf(hdr.name, USERDATA_NAME_LEN+1, "INITCFG");
    } else if (hdr.type == UDE_PROFILE) {
        target_map = ude_profile_items;
        hdr.num_items = sizeof(ude_profile_items)/sizeof(ude_item_map);

        // Check if name override file exists
        sniprintf(p_filename, sizeof(p_filename), "prof_n.txt");
        if (!file_open(&name_file, p_filename)) {

            for (i=0; i<=entry; i++) {
                if (file_get_string(&name_file, target_profile_name, sizeof(target_profile_name)) == NULL)
                    break;
            }

            file_close(&name_file);
        }

        if (i == entry+1) {
            // strip CR / CRLF
            target_profile_name[strcspn(target_profile_name, "\r\n")] = 0;

            strlcpy(hdr.name, target_profile_name, USERDATA_NAME_LEN+1);
        } else if (cur_profile_name[0] == 0) {
            sniprintf(hdr.name, USERDATA_NAME_LEN+1, "<used>");
        } else {
            strlcpy(hdr.name, cur_profile_name, USERDATA_NAME_LEN+1);
        }
    }

    // Write header
    if ((f_write(&p_file, &hdr, sizeof(ude_hdr), &bytes_written) != F_OK) || (bytes_written != sizeof(ude_hdr))) {
        retval = -4;
        goto close_file;
    }
    bytes_written_tot = bytes_written;

    // Write data
    for (i=0; i<hdr.num_items; i++) {
        if ((f_write(&p_file, &target_map[i].hdr, sizeof(ude_item_hdr), &bytes_written) != F_OK) || (bytes_written != sizeof(ude_item_hdr))) {
            retval = -5;
            goto close_file;
        }
        bytes_written_tot += bytes_written;

        if ((f_write(&p_file, target_map[i].data, target_map[i].hdr.data_size, &bytes_written) != F_OK) || (bytes_written != target_map[i].hdr.data_size)) {
            retval = -6;
            goto close_file;
        }
        bytes_written_tot += bytes_written;
    }

    printf("%u bytes written into userdata entry %u\n", bytes_written_tot, entry);

close_file:
    file_close(&p_file);
    return retval;
}

int read_userdata_sd(uint8_t entry, int dry_run) {
    FIL p_file;
    ude_hdr hdr;
    ude_item_hdr item_hdr;
    const ude_item_map *target_map;
    unsigned int bytes_read, bytes_read_tot;
    char p_filename[14];
    int i, j, target_map_items, retval=0;

    if (entry == SD_INIT_CONFIG_SLOT)
        sniprintf(p_filename, 14, "settings.bin");
    else
        sniprintf(p_filename, 14, "prof%.2u.bin", entry);

    if (entry > MAX_SD_USERDATA_ENTRY) {
        printf("invalid entry\n");
        return -1;
    }

    if (!sdcard_dev.mount) {
        retval = file_mount();
        if (retval != 0) {
            printf("SD card not detected %d\n", retval);
            return -2;
        }
    }

    if (file_open(&p_file, p_filename) != F_OK) {
        return -3;
    }

    if ((f_read(&p_file, &hdr, sizeof(ude_hdr), &bytes_read) != F_OK) || (bytes_read != sizeof(ude_hdr))) {
        printf("Hdr read error\n");
        retval = -4;
        goto close_file;
    }
    bytes_read_tot = bytes_read;

    if (strncmp(hdr.userdata_key, "USRDATA", 8)) {
        printf("No userdata found on file\n");
        retval = -5;
        goto close_file;
    }

    strlcpy(target_profile_name, hdr.name, USERDATA_NAME_LEN+1);
    if (dry_run)
        goto close_file;

    target_map = (hdr.type == UDE_INITCFG) ? ude_initcfg_items : ude_profile_items;
    target_map_items = (hdr.type == UDE_INITCFG) ? sizeof(ude_initcfg_items)/sizeof(ude_item_map) : sizeof(ude_profile_items)/sizeof(ude_item_map);

    for (i=0; i<hdr.num_items; i++) {
        if ((f_read(&p_file, &item_hdr, sizeof(ude_item_hdr), &bytes_read) != F_OK) || (bytes_read != sizeof(ude_item_hdr))) {
            printf("Item header read fail\n");
            retval = -6;
            goto close_file;
        }
        bytes_read_tot += sizeof(ude_item_hdr);
        for (j=0; j<target_map_items; j++) {
            if (!memcmp(&item_hdr, &target_map[j].hdr, sizeof(ude_item_hdr))) {
                if ((f_read(&p_file, target_map[j].data, item_hdr.data_size, &bytes_read) != F_OK) || (bytes_read != item_hdr.data_size)) {
                    printf("Item data read fail\n");
                    retval = -7;
                    goto close_file;
                }
                break;
            }
        }
        bytes_read_tot += item_hdr.data_size;
        if (j == target_map_items)
            f_lseek(&p_file, bytes_read_tot);
    }

    if (hdr.type == UDE_PROFILE) {
        invalidate_loaded_arrays();
        update_cur_vm = 1;
    }

    strlcpy(cur_profile_name, target_profile_name, USERDATA_NAME_LEN+1);
    printf("%u bytes read from userdata entry %u\n", bytes_read_tot, entry);

close_file:
    file_close(&p_file);
    return retval;
}
