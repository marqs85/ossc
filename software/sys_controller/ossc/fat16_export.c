//
// Copyright (C) 2020 Ari Sundholm <megari@iki.fi>
//
// This file has been contributed to the Open Source Scan Converter project
// developed by Markus Hiienkari Markus Hiienkari <mhiienka@niksula.hut.fi>
// and other members of the retro gaming community.
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
#include "fat16_export.h"

/*
 * The beginning of the boot sector. Will be followed by the BPB.
 * Offsets 0x000 to 0x00a, inclusive.
 */
static const alt_u8 bootsec_before_bpb_16[11] = {
    0xeb, 0x00, 0x90, 0x4d, 0x53, 0x57, 0x49, 0x4e,  0x34, 0x2e, 0x31,
/*    ^-----------^--- Maybe zero these? */
};

/* The BPB will span offsets 0x00b to 0x23, inclusive. Will be generated. */

/*
 * The rest of the boot sector before the boot code and terminator.
 * Offsets 0x024 to 0x03d, inclusive.
 */
static const alt_u8 bootsec_after_bpb_16[26] = {
    0x80, 0x00, 0x29, 0xf4, 0xcf, 0xc6, 0x04, 0x4f, 0x53, 0x53, 0x43, 0x50,
    0x52, 0x4f, 0x46, 0x49, 0x4c, 0x53, 0x46, 0x41, 0x54, 0x31, 0x36, 0x20,
    0x20, 0x20,
};

/*
 * After this, we have the boot code (448 bytes) and sector terminator
 * (2 bytes). These will be generated.
 */

/* Generates a FAT16 boot sector.
 * buf must be at least FAT16_SECTOR_SIZE bytes long,
 * and is assumed to be pre-zeroed.
 */
void generate_boot_sector_16(alt_u8 *const buf) {
    /* Initial FAT16 boot sector contents. */
    memcpy(buf, bootsec_before_bpb_16, 11);

    /* BPB */
    buf[12] = 0x02;
    buf[13] = 0x04;
    buf[14] = 0x80;
    buf[16] = 0x02;
    buf[18] = 0x08;
    buf[20] = 0x80;
    buf[21] = 0xf8;
    buf[22] = 0x20;
    buf[24] = 0x3f;
    buf[26] = 0xff;

    /*
     * Then the rest of the boot sector.
     * The boot code is just 448 bytes of 0xf4.
     */
    memcpy(buf + 36, bootsec_after_bpb_16, 26);
    memset(buf + 62, 0xf4, 448);
    buf[510] = 0x55;
    buf[511] = 0xaa;
}

/* The fixed 'preamble' of a FAT on a FAT16 volume. */
static const alt_u8 fat16_preamble[4] = {
    0xf8, 0xff, 0xff, 0xff,
};

#if 0
/*
 * To generate each FAT, we need 1040 bytes of memory.
 * The buffer is assumed to be zeroed out.
 * XXX: This is a problem!
 */
#define FAT16_ALLOC_SIZE 0x420U
#endif

/*
 * Generate a FAT.
 * The buffer is assumed to be zeroed out and have a size of at least
 * FAT16_SECTOR_SIZE bytes.
 * The number of clusters already written is given as an argument.
 * The function returns the total number of clusters written so far.
 *
 * The intention is to be able to generate and write the FAT in chunks
 * that do not exhaust all the remaining RAM.
 */
alt_u16 generate_fat16(alt_u8 *const fat, const alt_u16 written) {
	alt_u16 cur_ofs = 0;
    const alt_u16 start_cluster = 3U + written;
    const alt_u16 clusters_to_write = !written
        ? ((FAT16_SECTOR_SIZE - sizeof(fat16_preamble)) >> FAT16_ENTRY_SHIFT)
        : (((514U - written) > (512U >> FAT16_ENTRY_SHIFT))
            ? (512U >> FAT16_ENTRY_SHIFT)
            : (514U - written));
    const alt_u16 end_cluster = start_cluster + clusters_to_write;

    if (!written) {
        memcpy(fat, fat16_preamble, sizeof(fat16_preamble));
        cur_ofs += sizeof(fat16_preamble);
    }

    for (alt_u16 cluster = start_cluster; cluster < end_cluster; ++cluster) {
        /* FAT16 entries are 16-bit little-endian. */
        fat[cur_ofs++] = cluster & 0xffU;
        fat[cur_ofs++] = (cluster >> 8U) & 0xffU;
    }

    /* After the last cluster, write the chain terminator. */
    if (end_cluster == 514U) {
    	fat[cur_ofs++] = 0xff;
    	fat[cur_ofs++] = 0xff;
    }
	return end_cluster;
}

const alt_u8 prof_dirent_16[PROF_DIRENT_16_SIZE] = {
    0x4f, 0x53, 0x53, 0x43, 0x50, 0x52, 0x4f, 0x46, 0x42, 0x49, 0x4e, 0x20,
    0x00, 0x8e, 0x04, 0xb5, 0x6f, 0x51, 0x6f, 0x51, 0x00, 0x00, 0x17, 0x89,
    0x6f, 0x51, 0x02, 0x00, 0x00, 0x00, 0x10, 0x00,
};
