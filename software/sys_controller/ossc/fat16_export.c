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
 * The beginning of the boot sector, along with the BPB.
 * Volume offsets 0x003 to 0x01a, inclusive.
 * The BPB spans volume offsets 0x00b to 0x01c, inclusive.
 *
 * The jump instruction at volume offsets 0x000 to 0x002, inclusive,
 * is left zeroed out to save a tiny bit of space.
 */
static const alt_u8 bootsec_beg_bpb_16[24] = {
    /* Three zeros */ 0x4d, 0x53, 0x57, 0x49, 0x4e, /* 0x003...0x007 */
    0x34, 0x2e, 0x31, 0x00, 0x02, 0x04, 0x80, 0x00, /* 0x008...0x00f */
    0x02, 0x00, 0x08, 0x00, 0x80, 0xf8, 0x20, 0x00, /* 0x010...0x017 */
    0x3f, 0x00, 0xff, /* Zeros until 0x024       */ /* 0x018...0x01a */
};

/*
 * The rest of the boot sector before the boot code and terminator.
 * Offsets 0x024 to 0x03d, inclusive.
 */
static const alt_u8 bootsec_after_bpb_16[26] = {
    /* Zeros             */ 0x80, 0x00, 0x29, 0xf4, /* 0x024...0x027 */
    0xcf, 0xc6, 0x04, 0x4f, 0x53, 0x53, 0x43, 0x50, /* 0x028...0x02f */
    0x52, 0x4f, 0x46, 0x49, 0x4c, 0x53, 0x46, 0x41, /* 0x030...0x037 */
    0x54, 0x31, 0x36, 0x20, 0x20, 0x20, /* Zeros */ /* 0x038...0x03d */
};

/*
 * After this, we have the boot code (448 bytes) and sector terminator
 * (2 bytes). The former will be left zeroed-out and the latter will
 * be generated.
 */

/* Generates a FAT16 boot sector.
 * buf must be at least FAT16_SECTOR_SIZE bytes long,
 * and is assumed to be pre-zeroed.
 */
void generate_boot_sector_16(alt_u8 *const buf) {
    /* Initial FAT16 boot sector contents + the BPB. */
    memcpy(buf + 3, bootsec_beg_bpb_16, 24);

    /*
     * Then the rest of the boot sector.
     *
     * The boot code is supposed to be 448 bytes filled with 0xf4,
     * but leave it zeroed out to keep the code smaller. This may
     * be a deviation from the FAT16 spec, but should be harmless
     * for our purposes.
     */
    memcpy(buf + 36, bootsec_after_bpb_16, 26);

    /* RISC-V is little-endian, so do a 16-bit write instead. */
    *((alt_u16*)(buf + 510)) = 0xaa55U;
}

/* The fixed 'preamble' of a FAT on a FAT16 volume. */
static const alt_u32 fat16_preamble = 0xfffffff8U;

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
alt_u16 generate_fat16(void *const buf, const alt_u16 written) {
	alt_u16 cur_ofs = 0;
    const alt_u16 start_cluster = 3U + written;
    alt_u16 *const fat = buf;

    /*
     * The total number of FAT entries to write consists of:
     * 1. The FAT "preamble" (2 entries),
     * 2. The cluster chain of the file (512 entries).
     *
     * The latter needs to contain the chain terminator.
     */
    const alt_u16 clusters_remaining = PROF_16_CLUSTER_COUNT - written;
    const alt_u16 preamble_compensation = written ? 0 : 2U;
    const alt_u16 clusters_to_write =
        ((clusters_remaining > FAT16_ENTRIES_PER_SECTOR)
            ? FAT16_ENTRIES_PER_SECTOR
            : clusters_remaining) - preamble_compensation;
    const alt_u16 end_cluster = start_cluster + clusters_to_write;
    const alt_u16 last_fat_cluster = PROF_16_CLUSTER_COUNT + 2U;

    if (!written) {
        *((alt_u32*)fat) = fat16_preamble;
        cur_ofs += sizeof(fat16_preamble)/sizeof(alt_u16);
    }

    for (alt_u16 cluster = start_cluster; cluster < end_cluster; ++cluster) {
        alt_u16 *const cur_entry = fat + cur_ofs;
        /* FAT16 entries are 16-bit little-endian. */
        if (cluster == last_fat_cluster) {
            /* At the last cluster, write the chain terminator. */
            *cur_entry = 0xffffU;
        }
        else {
            *cur_entry = cluster;
        }
        ++cur_ofs;
    }

    return end_cluster - 3U;
}

const alt_u8 prof_dirent_16[PROF_DIRENT_16_SIZE] = {
    0x4f, 0x53, 0x53, 0x43, 0x50, 0x52, 0x4f, 0x46, 0x42, 0x49, 0x4e, 0x20,
    0x00, 0x8e, 0x04, 0xb5, 0x6f, 0x51, 0x6f, 0x51, 0x00, 0x00, 0x17, 0x89,
    0x6f, 0x51, 0x02, 0x00, 0x00, 0x00, 0x10, 0x00,
};
