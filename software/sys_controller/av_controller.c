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
#include "Altera_UP_SD_Card_Avalon_Interface_mod.h"
#include "altera_epcq_controller_mod.h"
#include "i2c_opencores.h"
#include "tvp7002.h"
#include "ths7353.h"
#include "video_modes.h"
#include "lcd.h"
#include "sysconfig.h"
#include "it6613.h"
#include "HDMI_TX.h"
#include "hdmitx.h"
#include "ci_crc.h"

alt_u8 fw_ver_major = 0;
alt_u8 fw_ver_minor = 67;
#define FW_UPDATE_RETRIES 3

#define LINECNT_THOLD       1
#define STABLE_THOLD        1
#define MIN_VALID_LINES     100
#define SYNC_LOSS_THOLD     5

#define MAINLOOP_SLEEP_US   10000

#define SCANLINESTR_MAX         0x07
#define HV_MASK_MAX             0x0f
#define L3_MODE_MAX             3
#define S480P_MODE_MAX          2
#define SL_MODE_MAX             2
#define SYNC_LPF_MAX            3
#define VIDEO_LPF_MAX           5
#define SAMPLER_PHASE_MIN       -16
#define SAMPLER_PHASE_MAX       15
#define SYNC_THOLD_MIN          -11
#define SYNC_THOLD_MAX         20

//#define TVP_CLKSEL_BIT          (1<<7)

#define RC_MASK                 0x0000ffff
#define PB_MASK                 0x00030000
#define PB0_MASK                0x00010000
#define PB1_MASK                0x00020000
#define HDMITX_MODE_MASK        0x00040000

static const char *rc_keydesc[] = { "1", "2", "3", "MENU", "BACK", "UP", "DOWN", "LEFT", "RIGHT", "INFO", "LCD_BACKLIGHT", "HOTKEY1", "HOTKEY2", "HOTKEY3"};
#define REMOTE_MAX_KEYS (sizeof(rc_keydesc)/sizeof(char*))
alt_u16 rc_keymap[REMOTE_MAX_KEYS] = {0x3E29, 0x3EA9, 0x3E69, 0x3E4D, 0x3EED, 0x3E2D, 0x3ECD, 0x3EAD, 0x3E6D, 0x3E65, 0x3E01, 0x3EC1, 0x3E41, 0x3EA1};

typedef enum {
    RC_BTN1                 = 0,
    RC_BTN2,
    RC_BTN3,
    RC_MENU,
    RC_BACK,
    RC_UP,
    RC_DOWN,
    RC_LEFT,
    RC_RIGHT,
    RC_INFO,
    RC_LCDBL,
    RC_HOTKEY1,
    RC_HOTKEY2,
    RC_HOTKEY3,
} rc_code_t;

#define lcd_write_menu() lcd_write((char*)&menu_row1, (char*)&menu_row2)
#define lcd_write_status() lcd_write((char*)&row1, (char*)&row2)

static const char *avinput_str[] = { "NO MODE", "AV1: RGBS", "AV1: RGsB", "AV2: YPbPr", "AV2: RGsB", "AV3: RGBHV", "AV3: RGBS", "AV3: RGsB" };
static const char *l3_mode_desc[] = { "Generic 16:9", "Generic 4:3", "320x240 optim.", "256x240 optim." };
static const char *s480p_desc[] = { "Auto", "DTV 480p", "VGA 640x480" };
static const char *sl_mode_desc[] = { "Off", "Horizontal", "Vertical" };
static const char *sync_lpf_desc[] = { "Off", "33MHz", "10MHz", "2.5MHz" };
static const char *video_lpf_desc[] = { "Auto", "Off", "95MHz (HDTV II)", "35MHz (HDTV I)", "16MHz (EDTV)", "9MHz (SDTV)" };

typedef enum {
    AV_KEEP         = 0,
    AV1_RGBs        = 1,
    AV1_RGsB        = 2,
    AV2_YPBPR       = 3,
    AV2_RGsB        = 4,
    AV3_RGBHV       = 5,
    AV3_RGBs        = 6,
    AV3_RGsB        = 7
} avinput_t;

// In reverse order of importance
typedef enum {
    NO_CHANGE           = 0,
    INFO_CHANGE         = 1,
    MODE_CHANGE         = 2,
    TX_MODE_CHANGE      = 3,
    ACTIVITY_CHANGE     = 4
} status_t;

typedef enum {
    SCANLINE_MODE,
    SCANLINE_STRENGTH,
    SCANLINE_ID,
    H_MASK,
    V_MASK,
    SAMPLER_480P,
    SAMPLER_PHASE,
    YPBPR_COLORSPACE,
    SYNC_THOLD,
    SYNC_LPF,
    VIDEO_LPF,
    LINETRIPLE_ENABLE,
    LINETRIPLE_MODE,
    DISABLE_ALC,
    TX_MODE,
#ifndef DEBUG
    FW_UPDATE,
#endif
    SAVE_CONFIG
} menuitem_id;

typedef enum {
    NO_ACTION           = 0,
    NEXT_PAGE           = 1,
    PREV_PAGE           = 2,
    VAL_PLUS            = 3,
    VAL_MINUS           = 4,
} menucode_id;

typedef enum {
    TX_HDMI             = 0,
    TX_DVI              = 1
} tx_mode_t;

typedef struct {
    alt_u8 sl_mode;
    alt_u8 sl_str;
    alt_u8 sl_id;
    alt_u8 linemult_target;
    alt_u8 l3_mode;
    alt_u8 h_mask;
    alt_u8 v_mask;
    alt_u8 tx_mode;
    alt_u8 s480p_mode;
    alt_8 sampler_phase;
    alt_u8 ypbpr_cs;
    alt_8 sync_thold;
    alt_u8 sync_lpf;
    alt_u8 video_lpf;
    alt_u8 disable_alc;
} avconfig_t;

// Target configuration
avconfig_t tc;

//TODO: transform binary values into flags
typedef struct {
    alt_u32 totlines;
    alt_u32 clkcnt;
    alt_u8 progressive;
    alt_u8 macrovis;
    alt_u8 refclk;
    alt_8 id;
    alt_u8 sync_active;
    alt_u8 linemult;
    avinput_t avinput;
    // Current configuration
    avconfig_t cc;
} avmode_t;

// Current mode
avmode_t cm;

typedef struct {
    const menuitem_id id;
    const char *desc;
} menuitem_t;

const menuitem_t menu[] = {
    { SCANLINE_MODE,        "Scanlines" },
    { SCANLINE_STRENGTH,    "Scanline str." },
    { SCANLINE_ID,          "Scanline id" },
    { H_MASK,               "Horizontal mask" },
    { V_MASK,               "Vertical mask" },
    { SAMPLER_480P,         "480p in sampler" },
    { SAMPLER_PHASE,        "Sampling phase" },
    { YPBPR_COLORSPACE,     "YPbPr in ColSpa" },
    { SYNC_THOLD,           "Analog sync thld" },
    { SYNC_LPF,             "Analog sync LPF" },
    { VIDEO_LPF,            "Video LPF" },
    { LINETRIPLE_ENABLE,    "240p/288p lineX3" },
    { LINETRIPLE_MODE,      "Linetriple mode" },
    { DISABLE_ALC,          "Auto Lev. Contr." },
    { TX_MODE,              "TX mode" },
#ifndef DEBUG
    { FW_UPDATE,            "Firmware update" },
#endif
    { SAVE_CONFIG,          "Save settings" },
};

#define MENUITEMS (sizeof(menu)/sizeof(menuitem_t))

typedef struct {
    char fw_key[4];
    alt_u8 version_major;
    alt_u8 version_minor;
    char version_suffix[8];
    alt_u32 hdr_len;
    alt_u32 data_len;
    alt_u32 data_crc;
    alt_u32 hdr_crc;
} fw_hdr;

#define USERDATA_HDR_SIZE 11
typedef struct {
    char userdata_key[8];
    alt_u8 version_major;
    alt_u8 version_minor;
    alt_u8 num_entries;
} userdata_hdr;

#define USERDATA_ENTRY_HDR_SIZE 2
typedef struct {
    alt_u8 type;
    alt_u8 entry_len;
} userdata_entry;

typedef enum {
    UDE_REMOTE_MAP  = 0,
    UDE_AVCONFIG,
} userdata_entry_type;

extern mode_data_t video_modes[];
extern ypbpr_to_rgb_csc_t csc_coeffs[];
extern alt_epcq_controller_dev epcq_controller_0;

alt_epcq_controller_dev *epcq_controller_dev;
alt_up_sd_card_dev *sdcard_dev;

alt_u8 target_typemask;
alt_u8 target_type;
alt_u8 stable_frames;

char row1[LCD_ROW_LEN+1], row2[LCD_ROW_LEN+1], menu_row1[LCD_ROW_LEN+1], menu_row2[LCD_ROW_LEN+1];

// EPCS16 pagesize is 256 bytes
// Flash is split 50-50 to FW and userdata, 1MB each
#define PAGESIZE 256
#define PAGES_PER_SECTOR 256
#define USERDATA_OFFSET 0x100000
#define USERDATA_MAX_SIZE 0x1000    //4KB should be enough

// SD controller uses 512-byte chunks
#define SD_BUFFER_SIZE 512

short int sd_fw_handle;

alt_u8 menu_active, menu_page;
alt_u32 remote_code, remote_code_prev;
alt_u32 btn_code, btn_code_prev;

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
    databuf[8] = fw_ver_major;
    databuf[9] = fw_ver_minor;
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

void setup_rc()
{
    int i, confirm;

    for (i=0; i<REMOTE_MAX_KEYS; i++) {
        strncpy(menu_row1, "Press", LCD_ROW_LEN+1);
        strncpy(menu_row2, rc_keydesc[i], LCD_ROW_LEN+1);
        lcd_write_menu();
        confirm = 0;

        while (1) {
            remote_code = IORD_ALTERA_AVALON_PIO_DATA(PIO_1_BASE) & RC_MASK;

            if ((remote_code_prev == 0) && (remote_code != 0)) {
                if (confirm == 0) {
                    rc_keymap[i] = remote_code;
                    strncpy(menu_row1, "Confirm", LCD_ROW_LEN+1);
                    lcd_write_menu();
                    confirm = 1;
                } else {
                    if (remote_code == rc_keymap[i]) {
                        confirm = 2;
                    } else {
                        strncpy(menu_row1, "Mismatch, retry", LCD_ROW_LEN+1);
                        lcd_write_menu();
                        confirm = 0;
                    }
                }

            }

            remote_code_prev = remote_code;

            if (confirm == 2)
                break;

            usleep(MAINLOOP_SLEEP_US);
        }
    }
}

inline void TX_enable(tx_mode_t mode)
{
    // shut down TX before setting new config
    SetAVMute(TRUE);
    DisableVideoOutput();
    EnableAVIInfoFrame(FALSE, NULL);
    // re-setup
    EnableVideoOutput(PCLK_MEDIUM, COLOR_RGB444, COLOR_RGB444, !mode);
    if (mode == TX_HDMI) {
        HDMITX_SetAVIInfoFrame(1, F_MODE_RGB444, 0, 0);
    }
    // start TX
    SetAVMute(FALSE);
}

void display_menu(alt_u8 forcedisp)
{
    menucode_id code;
    int retval;

    if (remote_code_prev == 0) {
        if (remote_code == rc_keymap[RC_UP])
            code = PREV_PAGE;
        else if (remote_code == rc_keymap[RC_DOWN])
            code = NEXT_PAGE;
        else if (remote_code == rc_keymap[RC_RIGHT])
            code = VAL_PLUS;
        else if (remote_code == rc_keymap[RC_LEFT])
            code = VAL_MINUS;
        else
            code = NO_ACTION;
    } else {
        code = NO_ACTION;
    }

    if (!forcedisp && (code == NO_ACTION))
        return;

    if (code == PREV_PAGE)
        menu_page = (menu_page+MENUITEMS-1) % MENUITEMS;
    else if (code == NEXT_PAGE)
        menu_page = (menu_page+1) % MENUITEMS;

    strncpy(menu_row1, menu[menu_page].desc, LCD_ROW_LEN+1);

    switch (menu[menu_page].id) {
    case SCANLINE_MODE:
        if (code == VAL_MINUS)
            tc.sl_mode = tc.sl_mode ? tc.sl_mode-1 : SL_MODE_MAX;
        else if (code == VAL_PLUS)
            tc.sl_mode = tc.sl_mode < SL_MODE_MAX ? tc.sl_mode+1 : 0;
        strncpy(menu_row2, sl_mode_desc[tc.sl_mode], LCD_ROW_LEN+1);
        break;
    case SCANLINE_STRENGTH:
        if (code == VAL_MINUS)
        	tc.sl_str = tc.sl_str ? tc.sl_str-1 : SCANLINESTR_MAX;
        else if (code == VAL_PLUS)
        	tc.sl_str = tc.sl_str < SCANLINESTR_MAX ? tc.sl_str+1 : 0;
        sniprintf(menu_row2, LCD_ROW_LEN+1, "%u%%", ((tc.sl_str+1)*125)/10);
        break;
    case SCANLINE_ID:
        if ((code == VAL_MINUS) || (code == VAL_PLUS))
            tc.sl_id = !tc.sl_id;
        sniprintf(menu_row2, LCD_ROW_LEN+1, tc.sl_id ? "Odd" : "Even");
        break;
    case H_MASK:
    	if ((code == VAL_MINUS) && (tc.h_mask > 0))
            tc.h_mask--;
        else if ((code == VAL_PLUS) && (tc.h_mask < HV_MASK_MAX))
            tc.h_mask++;
        sniprintf(menu_row2, LCD_ROW_LEN+1, "%u pixels", tc.h_mask<<2);
        break;
    case V_MASK:
        if ((code == VAL_MINUS) && (tc.v_mask > 0))
            tc.v_mask--;
        else if ((code == VAL_PLUS) && (tc.v_mask < HV_MASK_MAX))
            tc.v_mask++;
        sniprintf(menu_row2, LCD_ROW_LEN+1, "%u pixels", tc.v_mask<<2);
        break;
    case SAMPLER_480P:
        if (code == VAL_MINUS)
        	tc.s480p_mode = tc.s480p_mode ? tc.s480p_mode-1 : S480P_MODE_MAX;
        else if (code == VAL_PLUS)
        	tc.s480p_mode = tc.s480p_mode < S480P_MODE_MAX ? tc.s480p_mode+1 : 0;
        strncpy(menu_row2, s480p_desc[tc.s480p_mode], LCD_ROW_LEN+1);
        break;
    case SAMPLER_PHASE:
        if (code == VAL_MINUS)
        	tc.sampler_phase = tc.sampler_phase > SAMPLER_PHASE_MIN ? tc.sampler_phase-1 : SAMPLER_PHASE_MAX;
        else if (code == VAL_PLUS)
        	tc.sampler_phase = tc.sampler_phase < SAMPLER_PHASE_MAX ? tc.sampler_phase+1 : SAMPLER_PHASE_MIN;
        sniprintf(menu_row2, LCD_ROW_LEN+1, "%d deg", ((tc.sampler_phase-SAMPLER_PHASE_MIN)*1125)/100);
        break;
    case YPBPR_COLORSPACE:
        if ((code == VAL_MINUS) || (code == VAL_PLUS))
            tc.ypbpr_cs = !tc.ypbpr_cs;
        strncpy(menu_row2, csc_coeffs[tc.ypbpr_cs].name, LCD_ROW_LEN+1);
        break;
    case SYNC_THOLD:
        if (code == VAL_MINUS)
        	tc.sync_thold = tc.sync_thold > SYNC_THOLD_MIN ? tc.sync_thold-1 : SYNC_THOLD_MAX;
        else if (code == VAL_PLUS)
        	tc.sync_thold = tc.sync_thold < SYNC_THOLD_MAX ? tc.sync_thold+1 : SYNC_THOLD_MIN;
        sniprintf(menu_row2, LCD_ROW_LEN+1, "%d mV", ((tc.sync_thold-SYNC_THOLD_MIN)*1127)/100);
        break;
    case SYNC_LPF:
        if (code == VAL_MINUS)
        	tc.sync_lpf = tc.sync_lpf ? tc.sync_lpf-1 : SYNC_LPF_MAX;
        else if (code == VAL_PLUS)
        	tc.sync_lpf = tc.sync_lpf < SYNC_LPF_MAX ? tc.sync_lpf+1 : 0;
        strncpy(menu_row2, sync_lpf_desc[tc.sync_lpf], LCD_ROW_LEN+1);
        break;
    case VIDEO_LPF:
        if (code == VAL_MINUS)
        	tc.video_lpf = tc.video_lpf ? tc.video_lpf-1 : VIDEO_LPF_MAX;
        else if (code == VAL_PLUS)
        	tc.video_lpf = tc.video_lpf < VIDEO_LPF_MAX ? tc.video_lpf+1 : 0;
        strncpy(menu_row2, video_lpf_desc[tc.video_lpf], LCD_ROW_LEN+1);
        break;
    case LINETRIPLE_ENABLE:
        if ((code == VAL_MINUS) || (code == VAL_PLUS))
            tc.linemult_target = !tc.linemult_target;
        sniprintf(menu_row2, LCD_ROW_LEN+1, tc.linemult_target ? "On" : "Off");
        break;
    case LINETRIPLE_MODE:
        if (code == VAL_MINUS)
        	tc.l3_mode = tc.l3_mode ? tc.l3_mode-1 : L3_MODE_MAX;
        else if (code == VAL_PLUS)
        	tc.l3_mode = tc.l3_mode < L3_MODE_MAX ? tc.l3_mode+1 : 0;
        strncpy(menu_row2, l3_mode_desc[tc.l3_mode], LCD_ROW_LEN+1);
        break;
    case DISABLE_ALC:
        if ((code == VAL_MINUS) || (code == VAL_PLUS))
            tc.disable_alc = !tc.disable_alc;
        sniprintf(menu_row2, LCD_ROW_LEN+1, tc.disable_alc ? "Disabled" : "Enabled");
        break;
    case TX_MODE:
        if (!(IORD_ALTERA_AVALON_PIO_DATA(PIO_1_BASE) & HDMITX_MODE_MASK) && ((code == VAL_MINUS) || (code == VAL_PLUS))) {
            tc.tx_mode = !tc.tx_mode;
            TX_enable(tc.tx_mode);
        }
        sniprintf(menu_row2, LCD_ROW_LEN+1, tc.tx_mode ? "DVI" : "HDMI");
        break;
#ifndef DEBUG
    case FW_UPDATE:
        if ((code == VAL_MINUS) || (code == VAL_PLUS)) {
            retval = fw_update();
            if (retval == 0) {
                sniprintf(menu_row1, LCD_ROW_LEN+1, "Fw update OK");
                sniprintf(menu_row2, LCD_ROW_LEN+1, "Please restart");
            } else {
                sniprintf(menu_row1, LCD_ROW_LEN+1, "FW not");
                sniprintf(menu_row2, LCD_ROW_LEN+1, "updated");
            }
        } else {
            sniprintf(menu_row2, LCD_ROW_LEN+1,  "press <- or ->");
        }
        break;
#endif
    case SAVE_CONFIG:
        if ((code == VAL_MINUS) || (code == VAL_PLUS)) {
            retval = write_userdata();
            if (retval == 0) {
                sniprintf(menu_row2, LCD_ROW_LEN+1, "Done");
            } else {
                sniprintf(menu_row2, LCD_ROW_LEN+1, "error");
            }
        } else {
            sniprintf(menu_row2, LCD_ROW_LEN+1,  "press <- or ->");
        }
        break;
    default:
        sniprintf(menu_row2, LCD_ROW_LEN+1, "MISSING ITEM");
        break;
    }

    lcd_write_menu();

    return;
}

void read_control()
{
    if (remote_code_prev == 0) {
        if (remote_code == rc_keymap[RC_MENU]) {
            menu_active = !menu_active;

            if (menu_active) {
                display_menu(1);
            } else {
                lcd_write_status();
            }
        } else if (remote_code == rc_keymap[RC_BACK]) {
            menu_active = 0;
            lcd_write_status();
        } else if (remote_code == rc_keymap[RC_INFO]) {
            sniprintf(menu_row1, LCD_ROW_LEN+1, "VMod: %s", video_modes[cm.id].name);
            //sniprintf(menu_row1, LCD_ROW_LEN+1, "0x%x 0x%x 0x%x", ths_readreg(THS_CH1), ths_readreg(THS_CH2), ths_readreg(THS_CH3));
            sniprintf(menu_row2, LCD_ROW_LEN+1, "LO: %u VSM: %u", IORD_ALTERA_AVALON_PIO_DATA(PIO_4_BASE) & 0xffff, (IORD_ALTERA_AVALON_PIO_DATA(PIO_4_BASE) >> 16) & 0x3);
            lcd_write_menu();
            printf("Mod: %s\n", video_modes[cm.id].name);
            printf("Lines: %u M: %u\n", IORD_ALTERA_AVALON_PIO_DATA(PIO_4_BASE) & 0xffff, cm.macrovis);
        } else if (remote_code == rc_keymap[RC_LCDBL]) {
            IOWR_ALTERA_AVALON_PIO_DATA(PIO_0_BASE, (IORD_ALTERA_AVALON_PIO_DATA(PIO_0_BASE) ^ (1<<1)));
        } else if (remote_code == rc_keymap[RC_HOTKEY1]) {
            //tc.sl_mode = (tc.sl_mode > 0) ? 0 : 1;
            tc.sl_mode = tc.sl_mode < SL_MODE_MAX ? tc.sl_mode+1 : 0;
        } else if (remote_code == rc_keymap[RC_HOTKEY2]) {
            //if (tc.sl_str > 0)
            //    tc.sl_str--;
                tc.sl_str = tc.sl_str ? tc.sl_str-1 : SCANLINESTR_MAX;
        } else if (remote_code == rc_keymap[RC_HOTKEY3]) {
            //if (tc.sl_str < SCANLINESTR_MAX)
            //    tc.sl_str++;
            tc.sl_str = tc.sl_str < SCANLINESTR_MAX ? tc.sl_str+1 : 0;
        }
    }

    if (btn_code_prev == 0) {
        if (btn_code & PB1_MASK)
            tc.sl_mode = (tc.sl_mode > 0) ? 0 : 1;
    }

    if (menu_active) {
        display_menu(0);
        return;
    }

    if (remote_code_prev != 0)
        return;
}

void set_lpf(alt_u8 lpf)
{
    alt_u32 pclk;
    pclk = (clkrate[cm.refclk]/cm.clkcnt)*video_modes[cm.id].h_total;
    printf("PCLK: %uHz\n", pclk);

    //Auto
    if (lpf == 0) {
        switch (target_type) {
        case VIDEO_PC:
            tvp_set_lpf((pclk < 30000000) ? 0x0F : 0);
            ths_set_lpf(THS_LPF_BYPASS);
            break;
        case VIDEO_HDTV:
            tvp_set_lpf(0);
            ths_set_lpf(THS_LPF_BYPASS);
            break;
        case VIDEO_EDTV:
            tvp_set_lpf(0);
            ths_set_lpf(1);
            break;
        case VIDEO_SDTV:
        case VIDEO_LDTV:
            tvp_set_lpf(0);
            ths_set_lpf(0);
            break;
        default:
            break;
        }
    } else {
        tvp_set_lpf((tc.video_lpf == 2) ? 0x0F : 0);
        ths_set_lpf((tc.video_lpf > 2) ? (VIDEO_LPF_MAX-tc.video_lpf) : THS_LPF_BYPASS);
    }
}

// Check if input video status / target configuration has changed
status_t get_status(tvp_input_t input)
{
    alt_u32 data1, data2;
    alt_u32 totlines, clkcnt;
    alt_u8 progressive;
    //alt_u8 refclk;
    alt_u8 sync_active;
    alt_u8 vsyncmode;
    alt_u16 fpga_totlines;
    status_t status;
    static alt_u8 act_ctr;
    alt_u32 ctr;

    status = NO_CHANGE;

    // Wait until vsync active (avoid noise coupled to I2C bus on earlier prototypes)
    for (ctr=0; ctr<25000; ctr++) {
        if (!(IORD_ALTERA_AVALON_PIO_DATA(PIO_4_BASE) & (1<<31))) {
            //printf("ctrval %u\n", ctr);
            break;
        }
    }

    sync_active = tvp_check_sync(input);
    vsyncmode = IORD_ALTERA_AVALON_PIO_DATA(PIO_4_BASE) >> 16;

    // TVP7002 may randomly report "no sync" (especially with arcade boards),
    // thus disable output only after N consecutive "no sync"-events
    if (!cm.sync_active && sync_active) {
        cm.sync_active = sync_active;
        status = ACTIVITY_CHANGE;
        act_ctr = 0;
    } else if (cm.sync_active && !sync_active) {
        printf("Sync down in %u...\n", SYNC_LOSS_THOLD-act_ctr);
        if (act_ctr >= SYNC_LOSS_THOLD) {
            act_ctr = 0;
            cm.sync_active = sync_active;
            status = ACTIVITY_CHANGE;
        } else {
            act_ctr++;
        }
    } else {
        act_ctr = 0;
    }


    data1 = tvp_readreg(TVP_LINECNT1);
    data2 = tvp_readreg(TVP_LINECNT2);
    totlines = ((data2 & 0x0f) << 8) | data1;
    progressive = !!(data2 & (1<<5));
    cm.macrovis = !!(data2 & (1<<6));

    fpga_totlines = IORD_ALTERA_AVALON_PIO_DATA(PIO_4_BASE) & 0xffff;

    //TODO: check flags instead
    if (vsyncmode == 0x2) {
        progressive = 1;
    } else if ((vsyncmode == 0x1) && fpga_totlines > ((totlines-1)*2)) {
        progressive = 0;
        totlines = fpga_totlines; //ugly hack
    }

    data1 = tvp_readreg(TVP_CLKCNT1);
    data2 = tvp_readreg(TVP_CLKCNT2);
    clkcnt = ((data2 & 0x0f) << 8) | data1;

    //Not fully implemented yet
    /*refclk = !!(cword & TVP_CLKSEL_BIT);
    refclk = 0;

    if (refclk != cm.refclk)
        status = (status < REFCLK_CHANGE) ? REFCLK_CHANGE : status;*/

    /*if (tc.tx_mode != cm.cc.tx_mode)
        status = (status < TX_MODE_CHANGE) ? TX_MODE_CHANGE : status;*/

    // TODO: avoid random sync losses?
    if ((abs((alt_16)totlines - (alt_16)cm.totlines) > LINECNT_THOLD) || (clkcnt != cm.clkcnt) || (progressive != cm.progressive)) {
        printf("totlines: %u (cur) / %u (prev), clkcnt: %u (cur) / %u (prev). Data1: 0x%.2x, Data2: 0x%.2x\n", (unsigned)totlines, (unsigned)cm.totlines, (unsigned)clkcnt, (unsigned)cm.clkcnt, (unsigned)data1, (unsigned)data2);
        stable_frames = 0;
    } else if (stable_frames != STABLE_THOLD) {
        stable_frames++;
        if (stable_frames == STABLE_THOLD)
            status = (status < MODE_CHANGE) ? MODE_CHANGE : status;
    }

    if ((tc.linemult_target != cm.cc.linemult_target) ||
	(tc.l3_mode != cm.cc.l3_mode) ||
	((tc.s480p_mode != cm.cc.s480p_mode) && (video_modes[cm.id].flags & (MODE_DTV480P|MODE_VGA480P))) ||
	(tc.disable_alc != cm.cc.disable_alc))
        status = (status < MODE_CHANGE) ? MODE_CHANGE : status;

    cm.totlines = totlines;
    cm.clkcnt = clkcnt;
    cm.progressive = progressive;

    if ((tc.sl_mode != cm.cc.sl_mode) ||
        (tc.sl_str != cm.cc.sl_str) ||
        (tc.sl_id != cm.cc.sl_id) ||
        (tc.h_mask != cm.cc.h_mask) ||
        (tc.v_mask != cm.cc.v_mask))
        status = (status < INFO_CHANGE) ? INFO_CHANGE : status;

    if (tc.sampler_phase != cm.cc.sampler_phase)
        tvp_set_hpll_phase(tc.sampler_phase-SAMPLER_PHASE_MIN);

    if (tc.sync_thold != cm.cc.sync_thold)
        tvp_set_sog_thold(tc.sync_thold-SYNC_THOLD_MIN);

    if (tc.ypbpr_cs != cm.cc.ypbpr_cs)
        tvp_sel_csc(&csc_coeffs[tc.ypbpr_cs]);

    if (tc.video_lpf != cm.cc.video_lpf)
        set_lpf(tc.video_lpf);

    if (tc.sync_lpf != cm.cc.sync_lpf)
        tvp_set_sync_lpf(tc.sync_lpf);

    // use memcpy instead?
    cm.cc = tc;

    return status;
}

// h_info:     [31:30]          [29:28]      [27]      [26:16]      [15:12]      [11:8]             [7:0]
//           | H_LINEMULT[1:0] | H_L3MODE[1:0] |    | H_ACTIVE[10:0] |         | H_MASK[3:0] |  H_BACKPORCH[7:0] |
//
// v_info:     [31:30]         [29]                                         [26:24]             [23:13]       [15:10]       [9:6]         [5:0]
//           |              | V_SCANLINES | V_SCANLINEDIR | V_SCANLINEID | V_SCANLINESTR[2:0] | V_ACTIVE[10:0] |         | V_MASK[3:0]|  V_BACKPORCH[5:0] |
void set_videoinfo()
{
    alt_u8 slid_target;

    if (video_modes[cm.id].flags & MODE_L3ENABLE_MASK) {
        cm.linemult = 2;
        slid_target = cm.cc.sl_id ? 2 : 0;
    } else if (video_modes[cm.id].flags & MODE_L2ENABLE) {
        cm.linemult = 1;
        slid_target = cm.cc.sl_id;
    } else {
        cm.linemult = 0;
        slid_target = cm.cc.sl_id;
    }

    IOWR_ALTERA_AVALON_PIO_DATA(PIO_2_BASE, (cm.linemult<<30) | (cm.cc.l3_mode<<28) | (video_modes[cm.id].h_active<<16) | (cm.cc.h_mask)<<8 | video_modes[cm.id].h_backporch);
    IOWR_ALTERA_AVALON_PIO_DATA(PIO_3_BASE, ((!!cm.cc.sl_mode)<<29) | (cm.cc.sl_mode > 0 ? (cm.cc.sl_mode-1)<<28 : 0) | (slid_target<<27) | (cm.cc.sl_str<<24) | (video_modes[cm.id].v_active<<13) | (cm.cc.v_mask<<6) | video_modes[cm.id].v_backporch);
}

// Configure TVP7002 and scan converter logic based on the video mode
void program_mode()
{
    alt_u32 data1, data2;
    alt_u32 h_hz, v_hz_x100;

    // Mark as stable (needed after sync up to avoid unnecessary mode switch)
    stable_frames = STABLE_THOLD;

    if ((cm.clkcnt != 0) && (cm.totlines != 0)) { //prevent div by 0
        h_hz = clkrate[cm.refclk]/cm.clkcnt;
        v_hz_x100 = cm.progressive ? (100*clkrate[cm.refclk]/cm.clkcnt)/cm.totlines : (2*(100*clkrate[cm.refclk]/cm.clkcnt))/cm.totlines;
    } else {
        h_hz = 15700;
        v_hz_x100 = 6000;
    }

    printf("\nLines: %u %c\n", (unsigned)cm.totlines, cm.progressive ? 'p' : 'i');
    printf("Clocks per line: %u : HS %u.%.3u kHz  VS %u.%.2u Hz\n", (unsigned)cm.clkcnt, (unsigned)(h_hz/1000), (unsigned)(h_hz%1000), (unsigned)(v_hz_x100/100), (unsigned)(v_hz_x100%100));

    data1 = tvp_readreg(TVP_HSINWIDTH);
    data2 = tvp_readreg(TVP_VSINWIDTH);
    printf("Hswidth: %u  Vswidth: %u  Macrovision: %u\n", (unsigned)data1, (unsigned)(data2 & 0x1F), (unsigned)cm.macrovis);

    //TODO: rewrite with strncpy to reduce code size
    sniprintf(row1, LCD_ROW_LEN+1, "%s %u%c", avinput_str[cm.avinput], (unsigned)cm.totlines, cm.progressive ? 'p' : 'i');
    sniprintf(row2, LCD_ROW_LEN+1, "%u.%.2ukHz %u.%.2uHz", (unsigned)(h_hz/1000), (unsigned)((h_hz%1000)/10), (unsigned)(v_hz_x100/100), (unsigned)(v_hz_x100%100));
    //strncpy(row1, avinput_str[cm.avinput], LCD_ROW_LEN+1);
    //strncpy(row2, avinput_str[cm.avinput], LCD_ROW_LEN+1);
    if (!menu_active)
        lcd_write_status();

    //printf ("Get mode id with %u %u %f\n", totlines, progressive, hz);
    cm.id = get_mode_id(cm.totlines, cm.progressive, v_hz_x100/100, target_typemask, cm.cc.linemult_target, cm.cc.l3_mode, cm.cc.s480p_mode);

    if ( cm.id == -1) {
        printf ("Error: no suitable mode found, defaulting to 240p\n");
        cm.id = 4;
    }

    target_type = target_typemask & video_modes[cm.id].type;

    printf("Mode %s selected\n", video_modes[cm.id].name);

    tvp_source_setup(cm.id, target_type, cm.cc.disable_alc, (cm.progressive ? cm.totlines : cm.totlines/2), v_hz_x100/100, cm.refclk);
    set_lpf(cm.cc.video_lpf);
    set_videoinfo();
}

// Initialize hardware
int init_hw()
{
    alt_u32 chiprev;

    // Reset error vector and scan converter
    IOWR_ALTERA_AVALON_PIO_DATA(PIO_0_BASE, 0x00);
    IOWR_ALTERA_AVALON_PIO_DATA(PIO_2_BASE, 0x00000000);
    IOWR_ALTERA_AVALON_PIO_DATA(PIO_3_BASE, 0x00000000);
    usleep(10000);

    // unreset hw
    IOWR_ALTERA_AVALON_PIO_DATA(PIO_0_BASE, 0x03);

    //wait >500ms for SD card interface to be stable
    //over 200ms and LCD may be buggy?
    usleep(200000);

    // IT6613 officially supports only 100kHz, but 400kHz seems to work
    I2C_init(I2CA_BASE,ALT_CPU_FREQ,400000);

    /* Initialize the character display */
    lcd_init();

    if (!ths_init()) {
        printf("Error: could not read from THS7353\n");
        return -2;
    }

    /* check if TVP is found */
    chiprev = tvp_readreg(TVP_CHIPREV);
    //printf("chiprev %d\n", chiprev);

    if ( chiprev == 0xff) {
        printf("Error: could not read from TVP7002\n");
        return -3;
    }

    tvp_init();

    chiprev = HDMITX_ReadI2C_Byte(IT_DEVICEID);

    if ( chiprev != 0x13) {
        printf("Error: could not read from IT6613\n");
        return -4;
    }

    InitIT6613();

    if (check_flash() != 0) {
        printf("Error: incorrect flash type detected\n");
        return -1;
    }

    // safe?
    read_userdata();

    // enforce DVI mode on non-DIY boards
    if ((IORD_ALTERA_AVALON_PIO_DATA(PIO_1_BASE) & HDMITX_MODE_MASK)) {
        cm.cc.tx_mode = TX_DVI;
        tc.tx_mode = TX_DVI;
    }

    if (!(IORD_ALTERA_AVALON_PIO_DATA(PIO_1_BASE) & PB1_MASK))
        setup_rc();

    //enable TX (videogen)
    usleep(200000);
    TX_enable(cm.cc.tx_mode);

    return 0;
}

// Enable chip outputs
void enable_outputs()
{
    // program video mode
    program_mode();
    // enable TVP output
    tvp_enable_output();

    // enable and unmute HDMITX
    // TODO: check pclk
    TX_enable(cm.cc.tx_mode);
}

int main()
{
    tvp_input_t target_input = 0;
    ths_input_t target_ths = 0;
    video_format target_format = 0;
    avinput_t target_mode;

    alt_u8 av_init = 0;
    status_t status;

    int init_stat;

    init_stat = init_hw();

    if (init_stat >= 0) {
        printf("### DIY VIDEO DIGITIZER / SCANCONVERTER INIT OK ###\n\n");
        sniprintf(row1, LCD_ROW_LEN+1, "OSSC  fw. %u.%.2u", fw_ver_major, fw_ver_minor);
        strncpy(row2, "2014-2016  marqs", LCD_ROW_LEN+1);
        lcd_write_status();
    } else {
        sniprintf(row1, LCD_ROW_LEN+1, "Init error  %d", init_stat);
        strncpy(row2, "", LCD_ROW_LEN+1);
        lcd_write_status();
        while (1) {}
    }

    while(1) {
        // Select target input and mode
        remote_code = IORD_ALTERA_AVALON_PIO_DATA(PIO_1_BASE) & RC_MASK;
        btn_code = ~IORD_ALTERA_AVALON_PIO_DATA(PIO_1_BASE) & PB_MASK;

        if (remote_code_prev == 0 && remote_code != 0)
            printf("RCODE: 0x%.4x\n", remote_code);

        if (btn_code_prev == 0 && btn_code != 0)
            printf("BCODE: 0x%.2x\n", btn_code>>16);

        target_mode = AV_KEEP;

        if (remote_code_prev == 0) {
            if (remote_code == rc_keymap[RC_BTN1]) {
                if (cm.avinput == AV1_RGBs)
                    target_mode = AV1_RGsB;
                else
                    target_mode = AV1_RGBs;
            } else if (remote_code == rc_keymap[RC_BTN2]) {
                if (cm.avinput == AV2_YPBPR)
                    target_mode = AV2_RGsB;
                else
                    target_mode = AV2_YPBPR;
            } else if (remote_code == rc_keymap[RC_BTN3]) {
                if (cm.avinput == AV3_RGBHV)
                    target_mode = AV3_RGBs;
                else if (cm.avinput == AV3_RGBs)
                    target_mode = AV3_RGsB;
                else
                    target_mode = AV3_RGBHV;
            }
        }
        if ((btn_code_prev == 0) && (btn_code & PB0_MASK)) {
            target_mode = (cm.avinput == AV3_RGsB) ? AV1_RGBs : (cm.avinput+1);
        }

        if (target_mode == cm.avinput)
            target_mode = AV_KEEP;

        if (target_mode != AV_KEEP)
            printf("### SWITCH MODE TO %s ###\n", avinput_str[target_mode]);

        switch (target_mode) {
        case AV1_RGBs:
            target_input = TVP_INPUT1;
            target_format = FORMAT_RGBS;
            target_typemask = VIDEO_LDTV|VIDEO_SDTV|VIDEO_EDTV|VIDEO_HDTV;
            target_ths = THS_INPUT_B;
            break;
        case AV1_RGsB:
            target_input = TVP_INPUT1;
            target_format = FORMAT_RGsB;
            target_typemask = VIDEO_LDTV|VIDEO_SDTV|VIDEO_EDTV|VIDEO_HDTV;
            target_ths = THS_INPUT_B;
            break;
        case AV2_YPBPR:
            target_input = TVP_INPUT1;
            target_format = FORMAT_YPbPr;
            target_typemask = VIDEO_LDTV|VIDEO_SDTV|VIDEO_EDTV|VIDEO_HDTV;
            target_ths = THS_INPUT_A;
            break;
        case AV2_RGsB:
            target_input = TVP_INPUT1;
            target_format = FORMAT_RGsB;
            target_typemask = VIDEO_LDTV|VIDEO_SDTV|VIDEO_EDTV|VIDEO_HDTV;
            target_ths = THS_INPUT_A;
            break;
        case AV3_RGBHV:
            target_input = TVP_INPUT3;
            target_format = FORMAT_RGBHV;
            target_typemask = VIDEO_PC;
            target_ths = THS_STANDBY;
            break;
        case AV3_RGBs:
            target_input = TVP_INPUT3;
            target_format = FORMAT_RGBS;
            target_typemask = VIDEO_LDTV|VIDEO_SDTV|VIDEO_EDTV|VIDEO_HDTV;
            target_ths = THS_STANDBY;
            break;
        case AV3_RGsB:
            target_input = TVP_INPUT3;
            target_format = FORMAT_RGsB;
            target_typemask = VIDEO_LDTV|VIDEO_SDTV|VIDEO_EDTV|VIDEO_HDTV;
            target_ths = THS_STANDBY;
            break;
        default:
            break;
        }

        if (target_mode != AV_KEEP) {
            av_init = 1;
            cm.avinput = target_mode;
            cm.sync_active = 0;
            ths_source_sel(target_ths, (cm.cc.video_lpf > 1) ? (VIDEO_LPF_MAX-cm.cc.video_lpf) : THS_LPF_BYPASS);
            tvp_disable_output();
            tvp_source_sel(target_input, target_format, cm.refclk);
            cm.clkcnt = 0; //TODO: proper invalidate
            strncpy(row1, avinput_str[cm.avinput], LCD_ROW_LEN+1);
            strncpy(row2, "    NO SYNC", LCD_ROW_LEN+1);
            if (!menu_active)
                lcd_write_status();
        }

        //usleep(MAINLOOP_SLEEP_US);
        usleep(300);    //avoid triggering multiple times per vsync
        read_control();

        if (av_init) {
            status = get_status(target_input);

            switch (status) {
            case ACTIVITY_CHANGE:
                if (cm.sync_active) {
                    printf("Sync up\n");
                    IOWR_ALTERA_AVALON_PIO_DATA(PIO_0_BASE, (IORD_ALTERA_AVALON_PIO_DATA(PIO_0_BASE) | (1<<2)));  //disable videogen
                    enable_outputs();
                } else {
                    printf("Sync lost\n");
                    cm.clkcnt = 0; //TODO: proper invalidate
                    tvp_disable_output();
                    //ths_source_sel(THS_STANDBY, 0);
                    strncpy(row1, avinput_str[cm.avinput], LCD_ROW_LEN+1);
                    strncpy(row2, "    NO SYNC", LCD_ROW_LEN+1);
                    if (!menu_active)
                        lcd_write_status();
                }
                break;
                /*case TX_MODE_CHANGE:
                    if (cm.sync_active)
                        TX_enable(cm.cc.tx_mode);
                    printf("TX mode change\n");
                  break;*/
                /*case REFCLK_CHANGE:
                    if (cm.sync_active) {
                        printf("Refclk change\n");
                        tvp_sel_clk(cm.refclk);
                    }
                  break;*/
            case MODE_CHANGE:
                if (cm.sync_active && (cm.totlines >= MIN_VALID_LINES)) {
                    printf("Mode change\n");
                    program_mode();
                }
                break;
            case INFO_CHANGE:
                if (cm.sync_active) {
                    printf("Info change\n");
                    set_videoinfo();
                }
                break;
            default:
                break;
            }
        }

        remote_code_prev = remote_code;
        btn_code_prev = btn_code;
    }

    return 0;
}
