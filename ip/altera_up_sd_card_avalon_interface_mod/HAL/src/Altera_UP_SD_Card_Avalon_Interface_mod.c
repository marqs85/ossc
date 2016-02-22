/******************************************************************************
*                                                                             *
* License Agreement                                                           *
*                                                                             *
* Copyright (c) 2006 Altera Corporation, San Jose, California, USA.           *
* All rights reserved.                                                        *
*                                                                             *
* Permission is hereby granted, free of charge, to any person obtaining a     *
* copy of this software and associated documentation files (the "Software"),  *
* to deal in the Software without restriction, including without limitation   *
* the rights to use, copy, modify, merge, publish, distribute, sublicense,    *
* and/or sell copies of the Software, and to permit persons to whom the       *
* Software is furnished to do so, subject to the following conditions:        *
*                                                                             *
* The above copyright notice and this permission notice shall be included in  *
* all copies or substantial portions of the Software.                         *
*                                                                             *
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR  *
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,    *
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE *
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER      *
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING     *
* FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER         *
* DEALINGS IN THE SOFTWARE.                                                   *
*                                                                             *
* This agreement shall be governed in all respects by the laws of the State   *
* of California and by the laws of the United States of America.              *
*                                                                             *
******************************************************************************/

#include <errno.h>
#include <priv/alt_file.h>
#include <io.h>
#include <stdio.h>
#include <string.h>
#include "Altera_UP_SD_Card_Avalon_Interface_mod.h"

///////////////////////////////////////////////////////////////////////////
// Local Define Statements
///////////////////////////////////////////////////////////////////////////

#define CHAR_TO_UPPER(ch)	((char) (((ch >= 'a') && (ch <= 'z')) ? ((ch-'a')+'A'): ch))

// Data Buffer Address
#define SD_CARD_BUFFER(base, x)			(base + x)
// 128-bit Card Identification Number
#define SD_CARD_CID(base, x)			(base + 0x0200 + x)
// 128-bit Card Specific Data Register
#define SD_CARD_CSD(base, x)			(base + 0x0210 + x)
// 32-bit Operating Conditions Register
#define SD_CARD_OCR(base)				(base + 0x0220)
// 32-bit Card Status Register
#define SD_CARD_STATUS(base)			(base + 0x0224)
// 16-bit Relative Card Address Register
#define SD_CARD_RCA(base)				(base + 0x0228)
// 32-bit Card Argument Register
#define SD_CARD_ARGUMENT(base)			(base + 0x022C)
// 16-bit Card Command Register
#define SD_CARD_COMMAND(base)			(base + 0x0230)
// 16-bit Card Auxiliary Status Register
#define SD_CARD_AUX_STATUS(base)		(base + 0x0234)
// 32-bit R1 Response Register
#define SD_CARD_R1_RESPONSE(base)		(base + 0x0238)

#define CMD_READ_BLOCK					17
#define CMD_WRITE_BLOCK					24

// FAT 12/16 related stuff
//#define BOOT_SECTOR_DATA_SIZE			0x005A
#define MAX_FILES_OPENED				2

/******************************************************************************/
/******  LOCAL DATA STRUCTURES  ***********************************************/
/******************************************************************************/


typedef struct s_FAT_12_16_boot_sector {
	unsigned char jump_instruction[3];
	char OEM_name[8];
	unsigned short int sector_size_in_bytes;
	unsigned char sectors_per_cluster;
	unsigned short int reserved_sectors;
	unsigned char number_of_FATs;
	unsigned short int max_number_of_dir_entires;
	unsigned short int number_of_sectors_in_partition;
	unsigned char media_descriptor;
	unsigned short int number_of_sectors_per_table;
	unsigned short int number_of_sectors_per_track;
	unsigned short int number_of_heads;
	unsigned int number_of_hidden_sectors;
	unsigned int total_sector_count_if_above_32MB;
	unsigned char drive_number;
	unsigned char current_head;
	unsigned char boot_signature;
	unsigned char volume_id[4];
	char volume_label[11];
	unsigned char file_system_type[8];
	unsigned char bits_for_cluster_index;
	unsigned int first_fat_sector_offset;
	unsigned int second_fat_sector_offset;
	unsigned int root_directory_sector_offset; 
	unsigned int data_sector_offset; 
} t_FAT_12_16_boot_sector;


typedef struct s_file_record {
	unsigned char name[8];
	unsigned char extension[3];
	unsigned char attributes;
	unsigned short int create_time;
	unsigned short int create_date;
	unsigned short int last_access_date;
	unsigned short int last_modified_time;
	unsigned short int last_modified_date;
	unsigned short int start_cluster_index;
	unsigned int file_size_in_bytes;
	/* The following fields are only used when a file has been created or opened. */
	unsigned int current_cluster_index;
    unsigned int current_sector_in_cluster;
	unsigned int current_byte_position;
    // Absolute location of the file record on the SD Card.
    unsigned int file_record_cluster;
    unsigned int file_record_sector_in_cluster;
    short int    file_record_offset;
    // Is this record in use and has the file been modified.
    unsigned int home_directory_cluster;
    bool         modified;
	bool		 in_use;
} t_file_record;


typedef struct s_find_data {
	unsigned int directory_root_cluster; // 0 means root directory.
	unsigned int current_cluster_index;
	unsigned int current_sector_in_cluster;
	short int file_index_in_sector;
	bool valid;
} t_find_data;


///////////////////////////////////////////////////////////////////////////
// Local Variables
///////////////////////////////////////////////////////////////////////////


bool		initialized = false;
bool		is_sd_card_formated_as_FAT16 = false;
volatile short int	*aux_status_register = NULL;
volatile int		*status_register = NULL;
volatile short int	*CSD_register_w0 = NULL;
volatile short int	*command_register = NULL;
volatile int		*command_argument_register = NULL;
volatile char		*buffer_memory = NULL;
int			fat_partition_offset_in_512_byte_sectors = 0;
int			fat_partition_size_in_512_byte_sectors = 0;

#ifndef SD_RAW_IFACE
t_FAT_12_16_boot_sector boot_sector_data;
#endif

alt_up_sd_card_dev	*device_pointer = NULL;

#ifndef SD_RAW_IFACE
// Pointers to currently opened files.
t_file_record active_files[MAX_FILES_OPENED];
#endif
bool current_sector_modified = false;
unsigned int current_sector_index = 0;

#ifndef SD_RAW_IFACE
t_find_data search_data;
#endif


///////////////////////////////////////////////////////////////////////////
// Local Functions
///////////////////////////////////////////////////////////////////////////

#ifndef SD_RAW_IFACE
static bool Write_Sector_Data(int sector_index, int partition_offset)
#else
bool Write_Sector_Data(int sector_index, int partition_offset)
#endif
// This function writes a sector at the specified address on the SD Card.
{
    bool result = false;
    
    if (alt_up_sd_card_is_Present())
    {
        short int reg_state = 0xff;

		/* Multiply sector offset by sector size to get the address. Sector size is 512. Also,
         * the SD card reads data in 512 byte chunks, so the address must be a multiple of 512. */
        IOWR_32DIRECT(command_argument_register, 0, (sector_index + partition_offset)*512);
        IOWR_16DIRECT(command_register, 0, CMD_WRITE_BLOCK);
        do {
            reg_state = (short int) IORD_16DIRECT(aux_status_register,0);
        } while ((reg_state & 0x04)!=0);
        // Make sure the request did not time out.
        if ((reg_state & 0x10) == 0)
        {
            result = true;
            current_sector_modified = false;
            current_sector_index = sector_index+partition_offset;
        }
    }
    return result;
}

#ifndef SD_RAW_IFACE
static bool Save_Modified_Sector()
#else
bool Save_Modified_Sector()
#endif
// If the sector has been modified, then save it to the SD Card.
{
    bool result = true;
    if (current_sector_modified)
    {
        result = Write_Sector_Data(current_sector_index, 0);
    }
    return result;
}

#ifndef SD_RAW_IFACE
static bool Read_Sector_Data(int sector_index, int partition_offset)
#else
bool Read_Sector_Data(int sector_index, int partition_offset)
#endif
// This function reads a sector at the specified address on the SD Card.
{
	bool result = false;
    
	if (alt_up_sd_card_is_Present())
	{
		short int reg_state = 0xff;
        
        /* Write data to the SD card if the current buffer is out of date. */
        if (current_sector_modified)
        {
            if (Write_Sector_Data(current_sector_index, 0) == false)
            {
                return false;
            }
        }
		/* Multiply sector offset by sector size to get the address. Sector size is 512. Also,
		 * the SD card reads data in 512 byte chunks, so the address must be a multiple of 512. */
        IOWR_32DIRECT(command_argument_register, 0, (sector_index + partition_offset)*512);
        IOWR_16DIRECT(command_register, 0, CMD_READ_BLOCK);
		do {
			reg_state = (short int) IORD_16DIRECT(aux_status_register,0);
		} while ((reg_state & 0x04)!=0);
		// Make sure the request did not time out.
		if ((reg_state & 0x10) == 0)
		{
			result = true;
            current_sector_modified = false;
            current_sector_index = sector_index+partition_offset;
		}
	}
	return result;
}

#ifndef SD_RAW_IFACE
static bool get_cluster_flag(unsigned int cluster_index, unsigned short int *flag)
// Read a cluster flag.
{
    unsigned int sector_index = (cluster_index / 256) + fat_partition_offset_in_512_byte_sectors;
    
    sector_index  = sector_index + boot_sector_data.first_fat_sector_offset;
     
    if (sector_index != current_sector_index)
    {
        if (Read_Sector_Data(sector_index, 0) == false)
        {
            return false;
        }
    }
    *flag = (unsigned short int) IORD_16DIRECT(device_pointer->base, 2*(cluster_index % 256));
    return true;
}


static bool mark_cluster(unsigned int cluster_index, short int flag, bool first_fat)
// Place a marker on the specified cluster in a given FAT.
{
    unsigned int sector_index = (cluster_index / 256) +  fat_partition_offset_in_512_byte_sectors;
    
    if (first_fat)
    {
        sector_index  = sector_index + boot_sector_data.first_fat_sector_offset;
    }
    else
    {
        sector_index  = sector_index + boot_sector_data.second_fat_sector_offset;
    }
     
    if (sector_index != current_sector_index)
    {
        if (Read_Sector_Data(sector_index, 0) == false)
        {
            return false;
        }
    }
    IOWR_16DIRECT(device_pointer->base, 2*(cluster_index % 256), flag);
    current_sector_modified = true;
    return true;
}


static bool Check_for_Master_Boot_Record(void)
// This function reads the first 512 bytes on the SD Card. This data should
// contain the Master Boot Record. If it does, then print
// relevant information and return true. Otherwise, return false. 
{
	bool result = false;
	int index;
	int end, offset, partition_size;

	/* Load the first 512 bytes of data from SD card. */
	if (Read_Sector_Data(0, 0))
	{
		end =  (short int) IORD_16DIRECT(device_pointer->base,0x1fe);

		// Check if the end of the sector contains an end string 0xaa55.
		if ((end & 0x0000ffff) == 0x0000aa55)
		{
			// Check four partition entries and see if any are valid
			for (index = 0; index < 4; index++)
			{
				int partition_data_offset = (index * 16) + 0x01be;
				char type;
		        
				// Read Partition type
				type = (unsigned char) IORD_8DIRECT(device_pointer->base,partition_data_offset + 0x04);

				// Check if this is an FAT parition
				if ((type == 1) || (type == 4) || (type == 6) || (type == 14))
				{
					// Get partition offset and size.
					offset = (((unsigned short int) IORD_16DIRECT(device_pointer->base,partition_data_offset + 0x0A)) << 16) | ((unsigned short int) IORD_16DIRECT(device_pointer->base,partition_data_offset + 0x08));
					partition_size = (((unsigned short int) IORD_16DIRECT(device_pointer->base,partition_data_offset + 0x0E)) << 16) | ((unsigned short int) IORD_16DIRECT(device_pointer->base,partition_data_offset + 0x0C));
		            
					// Check if the partition is valid
					if (partition_size > 0)
					{
						result = true;
						fat_partition_size_in_512_byte_sectors = partition_size;
						fat_partition_offset_in_512_byte_sectors = offset;
						break;
					}                
				}
			}
		}
	}

	return result;
}


static bool Read_File_Record_At_Offset(int offset, t_file_record *record, unsigned int cluster_index, unsigned int sector_in_cluster)
// This function reads a file record
{
	bool result = false;
	if (((offset & 0x01f) == 0) && (alt_up_sd_card_is_Present()) && (is_sd_card_formated_as_FAT16))
	{
		int counter;

		for (counter = 0; counter < 8; counter++)
		{
			record->name[counter] = (char) IORD_8DIRECT(device_pointer->base, offset+counter);
		}        
		for (counter = 0; counter < 3; counter++)
		{
			record->extension[counter] = (char) IORD_8DIRECT(device_pointer->base, offset+counter+8);
		}        
		record->attributes          =   (char) IORD_8DIRECT(device_pointer->base, offset+11);
		/* Ignore reserved bytes at locations 12 and 13. */
		record->create_time         =   (unsigned short int) IORD_16DIRECT(device_pointer->base, offset+14);
		record->create_date         =   (unsigned short int) IORD_16DIRECT(device_pointer->base, offset+16);
		record->last_access_date    =   (unsigned short int) IORD_16DIRECT(device_pointer->base, offset+18);
		/* Ignore reserved bytes at locations 20 and 21. */
		record->last_modified_time  =	(unsigned short int) IORD_16DIRECT(device_pointer->base, offset+22);
		record->last_modified_date  =	(unsigned short int) IORD_16DIRECT(device_pointer->base, offset+24);
		record->start_cluster_index =	(unsigned short int) IORD_16DIRECT(device_pointer->base, offset+26);
		record->file_size_in_bytes  =	(unsigned int) IORD_32DIRECT(device_pointer->base, offset+28);
		record->file_record_cluster = cluster_index;
		record->file_record_sector_in_cluster = sector_in_cluster;
		record->file_record_offset = offset;
		result = true;
	}
	return result;
}


static bool Write_File_Record_At_Offset(int offset, t_file_record *record)
// This function writes a file record at a given offset. The offset is given in bytes.
{
    bool result = false;
    if (((offset & 0x01f) == 0) && (alt_up_sd_card_is_Present()) && (is_sd_card_formated_as_FAT16))
    {
        int counter;

        for (counter = 0; counter < 8; counter=counter+2)
        {
            short int two_chars = (short int) record->name[counter+1];
            two_chars = two_chars << 8;
            two_chars = two_chars | record->name[counter];
            IOWR_16DIRECT(device_pointer->base, offset+counter, two_chars);
        }        
        for (counter = 0; counter < 3; counter++)
        {
            IOWR_8DIRECT(device_pointer->base, offset+counter+8, record->extension[counter]);
        }        
        IOWR_8DIRECT(device_pointer->base, offset+11, record->attributes);
        /* Ignore reserved bytes at locations 12 and 13. */
        IOWR_16DIRECT(device_pointer->base, offset+14, record->create_time);
        IOWR_16DIRECT(device_pointer->base, offset+16, record->create_date);
        IOWR_16DIRECT(device_pointer->base, offset+18, record->last_access_date);
        /* Ignore reserved bytes at locations 20 and 21. */
        IOWR_16DIRECT(device_pointer->base, offset+22, record->last_modified_time);
        IOWR_16DIRECT(device_pointer->base, offset+24, record->last_modified_date);
        IOWR_16DIRECT(device_pointer->base, offset+26, record->start_cluster_index);
        IOWR_32DIRECT(device_pointer->base, offset+28, record->file_size_in_bytes);
        current_sector_modified = true;                  
        result = true;
    }
    return result;
}


static bool Check_for_DOS_FAT(int FAT_partition_start_sector)
// This function reads the boot sector for the FAT file system on the SD Card.
// The offset_address should point to the sector on the card where the boot sector is located.
// The sector number is specified either in the master Boot Record, or is 0 by default for a purely FAT
// based file system. If the specified sector contains a FAT boot sector, then this function prints the
// relevant information and returns 1. Otherwise, it returns 0. 
{
	bool result = false;
	int counter = 0;
	short int end;

	result = Read_Sector_Data(0, FAT_partition_start_sector);
	end =  (short int) IORD_16DIRECT(device_pointer->base, 0x1fe);
	if (((end & 0x0000ffff) == 0x0000aa55) && (result))
	{
		int num_clusters = 0;

		boot_sector_data.jump_instruction[0] = (char) IORD_8DIRECT(device_pointer->base, 0);
		boot_sector_data.jump_instruction[1] = (char) IORD_8DIRECT(device_pointer->base, 1);
		boot_sector_data.jump_instruction[2] = (char) IORD_8DIRECT(device_pointer->base, 2);
		for (counter = 0; counter < 8; counter++)
		{
			boot_sector_data.OEM_name[counter] = (char) IORD_8DIRECT(device_pointer->base, 3+counter);
		}
		boot_sector_data.sector_size_in_bytes = (((unsigned char) IORD_8DIRECT(device_pointer->base, 12)) << 8 ) | ((char) IORD_8DIRECT(device_pointer->base, 11));
		boot_sector_data.sectors_per_cluster = ((unsigned char) IORD_8DIRECT(device_pointer->base, 13));
		boot_sector_data.reserved_sectors = ((unsigned short int) IORD_16DIRECT(device_pointer->base, 14));
		boot_sector_data.number_of_FATs = ((unsigned char) IORD_8DIRECT(device_pointer->base, 16));
		boot_sector_data.max_number_of_dir_entires = (((unsigned short int)(((unsigned char) IORD_8DIRECT(device_pointer->base, 18)))) << 8 ) | ((unsigned char) IORD_8DIRECT(device_pointer->base, 17));
		boot_sector_data.number_of_sectors_in_partition = (((unsigned short int)(((unsigned char) IORD_8DIRECT(device_pointer->base, 20)))) << 8 ) | ((unsigned char) IORD_8DIRECT(device_pointer->base, 19));
		boot_sector_data.media_descriptor = ((unsigned char) IORD_8DIRECT(device_pointer->base, 21));
		boot_sector_data.number_of_sectors_per_table = ((unsigned short int) IORD_16DIRECT(device_pointer->base, 22));
		boot_sector_data.number_of_sectors_per_track = ((unsigned short int) IORD_16DIRECT(device_pointer->base, 24));
		boot_sector_data.number_of_heads = ((unsigned short int) IORD_16DIRECT(device_pointer->base, 26));
		boot_sector_data.number_of_hidden_sectors = ((unsigned int) IORD_32DIRECT(device_pointer->base, 28));
		boot_sector_data.total_sector_count_if_above_32MB = ((unsigned int) IORD_32DIRECT(device_pointer->base, 32));
		boot_sector_data.drive_number = ((unsigned char) IORD_8DIRECT(device_pointer->base, 36));
		boot_sector_data.current_head = ((unsigned char) IORD_8DIRECT(device_pointer->base, 37));
		boot_sector_data.boot_signature = ((unsigned char) IORD_8DIRECT(device_pointer->base, 38));
		boot_sector_data.first_fat_sector_offset = boot_sector_data.reserved_sectors;
		boot_sector_data.second_fat_sector_offset = boot_sector_data.first_fat_sector_offset + boot_sector_data.number_of_sectors_per_table;
		boot_sector_data.root_directory_sector_offset = boot_sector_data.second_fat_sector_offset + boot_sector_data.number_of_sectors_per_table; 
		boot_sector_data.data_sector_offset = boot_sector_data.root_directory_sector_offset + (32*boot_sector_data.max_number_of_dir_entires / boot_sector_data.sector_size_in_bytes);    
	    
		if (boot_sector_data.number_of_sectors_in_partition > 0)
		{
			num_clusters = (boot_sector_data.number_of_sectors_in_partition / boot_sector_data.sectors_per_cluster);
		}
		else
		{
			num_clusters = (boot_sector_data.total_sector_count_if_above_32MB / boot_sector_data.sectors_per_cluster);
		}
		if (num_clusters < 4087)
		{
			boot_sector_data.bits_for_cluster_index = 12;
		}
		else if (num_clusters <= 65517)
		{
			boot_sector_data.bits_for_cluster_index = 16;
		}
		else
		{
			boot_sector_data.bits_for_cluster_index = 32;
		}
	    
		for (counter = 0; counter < 4; counter++)
		{
			boot_sector_data.volume_id[counter] = ((char) IORD_8DIRECT(device_pointer->base, 39+counter));
		}    
		for (counter = 0; counter < 11; counter++)
		{
			boot_sector_data.volume_label[counter] = ((char) IORD_8DIRECT(device_pointer->base, 43+counter));
		}    
		for (counter = 0; counter < 8; counter++)
		{
			boot_sector_data.file_system_type[counter] = ((char) IORD_8DIRECT(device_pointer->base, 54+counter));
		}    
		// Clear file records
		for (counter = 0; counter < MAX_FILES_OPENED; counter++)
		{
			active_files[counter].in_use = false;
		}
		result = true;
	}
    else
    {
        result = false;
    }
	return result;
}


static bool Look_for_FAT16(void)
// Read the SD card to determine if it contains a FAT16 partition.
{
	bool result = false;

	if (alt_up_sd_card_is_Present())
	{
		short int csd_file_format = *CSD_register_w0;
        
		fat_partition_offset_in_512_byte_sectors = 0;
		fat_partition_size_in_512_byte_sectors = 0;              

		if (((csd_file_format & 0x8000) == 0) && ((csd_file_format & 0x0c00) != 0x0c00))
		{
			if ((csd_file_format & 0x0c00) == 0x0400)
			{
				/* SD Card contains files stored in a DOS FAT (floppy like) file format, without a partition table */
				result = Check_for_DOS_FAT(0);
			}
			if ((csd_file_format & 0x0c00) == 0x0000)
			{
				/* SD Card contains files stored in a Hard disk-like file format that contains a partition table */
				if (Check_for_Master_Boot_Record())
				{
					result = Check_for_DOS_FAT(fat_partition_offset_in_512_byte_sectors);
				}                        
			}
			if (result == true)
			{
				// Accept only FAT16, not FAT12.
				if (boot_sector_data.bits_for_cluster_index != 16)
				{
					result = false;
				}
				else
				{
					fat_partition_size_in_512_byte_sectors = boot_sector_data.number_of_sectors_in_partition;
				}
			}
		}
	}
	return result;
}
 

static void filename_to_upper_case(char *file_name)
// Change file name to upper case.
{
    int index;
    int length = strlen(file_name);
    
    for (index = 0; index < length; index++)
    {
        if ((file_name[index] >= 'a') && (file_name[index] <= 'z'))
        {
            file_name[index] = (file_name[index] - 'a') + 'A';
        }
    }
}


static bool check_file_name_for_FAT16_compliance(char *file_name)
// Check if the file complies with FAT16 naming convention.
{
    int length = strlen(file_name);
    int index;
    int last_dir_break_position = -1;
    int last_period = -1;
    bool result = true;
    
    for(index = 0; index < length; index++)
    {
        if ((file_name[index] == ' ') ||
            ((last_dir_break_position == (index - 1)) && ((file_name[index] == '\\') || (file_name[index] == '/'))) ||
            ((index - last_period == 9) && (file_name[index] != '.')) ||
            ((last_dir_break_position != last_period) && (index - last_period > 3) &&
             (file_name[index] != '\\') && (file_name[index] != '/'))
           )
        {
            result = false;
            break;
        }
        if ((file_name[index] == '\\') || (file_name[index] == '/'))
        {
            last_period = index;
            last_dir_break_position = index;
        }
        if (file_name[index] == '.')
        {
            last_period = index;
        }
    }
    if ((file_name[length-1] == '\\') || (file_name[length-1] == '/'))
    {
        result = false;
    }
    return result;
}


static int get_dir_divider_location(char *name)
// Find a directory divider location.
{
    int index = 0;
    int length = strlen(name);
    
    for(index = 0; index < length; index++)
    {
        if ((name[index] == '\\') || (name[index] == '/'))
        {
            break;
        }
    }
    
    if (index == length)
    {
        index = -1;
    }
    
    return index;
}


static bool match_file_record_to_name_ext(t_file_record *file_record, char *name, char *extension)
/* See if the given name and extension match the file record. Return true if this is so, false otherwise. */
{
    bool match = true;
	int index;

    for (index = 0; index < 8; index++)
    {
        if (CHAR_TO_UPPER(file_record->name[index]) != CHAR_TO_UPPER(name[index]))
        {
            match = false;
			break;
        }
    }
    for (index = 0; index < 3; index++)
    {
        if (CHAR_TO_UPPER(file_record->extension[index]) != CHAR_TO_UPPER(extension[index]))
        {
            match = false;
			break;
        }
    }
	return match;
}


static bool get_home_directory_cluster_for_file(char *file_name, int *home_directory_cluster, t_file_record *file_record)
// Scan the directories in given in the file name and find the root directory for the file.
{
    bool result = false;
    int home_dir_cluster = 0;
    int location, index;
    int start_location = 0;
    
    /* Get Next Directory. */
    location = get_dir_divider_location( file_name );
    while (location > 0)
    {
        char name[8] = { ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ' };
        char extension[3] = { ' ', ' ', ' ' };
        int ext_index = -1;
        int new_cluster = home_dir_cluster;
        
        // Get the name of the directory in name/extension format.
        for (index = 0; index < location; index++)
        {
            if (file_name[index+start_location] == '.')
            {
                ext_index = index;
            }
            else if (ext_index < 0)
            {
                name[index] = file_name[index+start_location];
            }
            else
            {
                extension[index-ext_index] = file_name[index+start_location];
            }
        }
        
        if (home_dir_cluster == 0)
        {
            /* We are in the root directory. Scan the directory (of predefined size) and see if you can find the specified file. */
            int max_root_dir_sectors = ((32*boot_sector_data.max_number_of_dir_entires) / boot_sector_data.sector_size_in_bytes);
            int sector_index;
            
            for (sector_index = 0; sector_index < max_root_dir_sectors; sector_index++)
            {
                if (Read_Sector_Data(sector_index+boot_sector_data.root_directory_sector_offset, fat_partition_offset_in_512_byte_sectors))
                {
                    int file_counter;
                    
                    for (file_counter = 0; file_counter < 16; file_counter++)
                    {
                       
                        // Read file record.
                        Read_File_Record_At_Offset(file_counter*32, file_record, 0, sector_index);
                        if ((file_record->name[0] != 0xe5) && (file_record->name[0] != 0x00))
                        {
                            bool match = match_file_record_to_name_ext(file_record, name, extension);
                            if (match)
                            {
                                new_cluster = file_record->start_cluster_index;
                                file_record->file_record_cluster = 1; // Home directory is a subdirectory in the root directory.
                                break;
                            }
                        }
                    }
                }
                else
                {
                    break;
                }
                if (new_cluster != home_dir_cluster)
                {
                    break;
                }
            }
            if (new_cluster != home_dir_cluster)
            {
                // A valid directory is found, so go to it.
                home_dir_cluster = new_cluster;
                start_location = start_location+location+1;
            }
            else
            {
                // Directory path is invalid. 
                return false;
            }
        } else {
            // This is a subdirectory that can have any number of elements. So scan through it as though it was a file
            // and see if you can find the directory of interest.
            int cluster = home_dir_cluster;
            
            do {
                int start_sector = ( cluster - 2 ) * ( boot_sector_data.sectors_per_cluster ) + boot_sector_data.data_sector_offset;
                int sector_index;
                
                for (sector_index = 0; sector_index < boot_sector_data.sectors_per_cluster; sector_index++)
                {
                    if (Read_Sector_Data(sector_index + start_sector, fat_partition_offset_in_512_byte_sectors))
                    {
                        int file_counter;
                        
                        for (file_counter = 0; file_counter < 16; file_counter++)
                        {                         
                            // Read file record.
                            Read_File_Record_At_Offset(file_counter*32, file_record, cluster, sector_index);
                            if ((file_record->name[0] != 0xe5) && (file_record->name[0] != 0x00))
                            {
								bool match = match_file_record_to_name_ext(file_record, name, extension);
                                if (match)
                                {
                                    new_cluster = file_record->start_cluster_index;                                   
                                    break;
                                }
                            }
                        }
                    }
                    else
                    {
                        break;
                    }
                    if (new_cluster != home_dir_cluster)
                    {
                        break;
                    }
                }
                // If this is the end of the cluster and the directory has not been found, then see if there is another cluster
                // that holds data for the current directory.
                if (new_cluster == home_dir_cluster)
                {
					unsigned short int next_cluster;

					if (get_cluster_flag(new_cluster, &next_cluster))
					{
						// The directory needs to be expanded to store more files.
						if ((next_cluster & 0x0000fff8) == 0x0000fff8)
						{
							return false;
						}
						new_cluster = (next_cluster & 0x0000fff8);
					}
					else
					{
						// Directory path is invalid.                 
						return false;
					}
                }              
            } while ((cluster < 0x0000fff8) && (new_cluster == home_dir_cluster));
            if (new_cluster != home_dir_cluster)
            {
                // A valid directory is found, so go to it.
                home_dir_cluster = new_cluster;
                start_location = start_location+location+1;
            }
            else
            {
                // Directory path is invalid. 
                return false;
            }            
        }
        location = get_dir_divider_location(&(file_name[start_location]));
        if (location < 0)
        {
            // Directory has been located.
            result = true;
        }
    }
    
    *home_directory_cluster = home_dir_cluster;
    if (home_dir_cluster == 0)
    {
        file_record->file_record_cluster = 0; // Home directory is the root directory.
		result = true;
    }
    return result;
}


static bool find_file_in_directory(int directory_start_cluster, char *file_name, t_file_record *file_record)
// Given a cluster and a file name, check if the file already exists. Return the file record if the file is found.
{
    int location = get_dir_divider_location( file_name );
    int last_dir_separator = 0;
    char name[8] = { ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ' };
    char extension[3] = { ' ', ' ', ' ' };
    int ext_index = -1;
    int cluster = directory_start_cluster;
    int index;
	int length = strlen(file_name);
    bool result = false;
    
    // Skip through all directory separators.
    while (location > 0)
    {
        last_dir_separator = last_dir_separator+location+1;
        location = get_dir_divider_location( &(file_name[last_dir_separator]) );
    }
        
    // Get the name of the file in name/extension format.
    for (index = last_dir_separator; index < length; index++)
    {
        if (file_name[index] == '.')
        {
            ext_index = index;
        }
        else if (ext_index < 0)
        {
            name[index-last_dir_separator] = file_name[index];
        }
        else
        {
            extension[index-ext_index-1] = file_name[index];
        }
    }

    // Look for the file.
    if (directory_start_cluster == 0)
    {
        /* We are in the root directory. Scan the directory (of predefined size) and see if you can find the specified file. */
        int max_root_dir_sectors = ((32*boot_sector_data.max_number_of_dir_entires) / boot_sector_data.sector_size_in_bytes);
        int sector_index;
        
        for (sector_index = 0; sector_index < max_root_dir_sectors; sector_index++)
        {
            if (Read_Sector_Data(   sector_index + boot_sector_data.root_directory_sector_offset,
                                    fat_partition_offset_in_512_byte_sectors))
            {
                int file_counter;
                
                for (file_counter = 0; file_counter < 16; file_counter++)
                {
                    // Read file record.
                    Read_File_Record_At_Offset(file_counter*32, file_record, 0, sector_index);
                    if ((file_record->name[0] != 0xe5) && (file_record->name[0] != 0x00))
                    {
                        bool match = match_file_record_to_name_ext(file_record, name, extension);

                        if (match)
                        {
                            result = true;
                            break;
                        }
                    }
                }
            }
            else
            {
                break;
            }
            if (result)
            {
                break;
            }
        }
    }
    else
    {          
        do {
            int start_sector = ( cluster - 2 ) * ( boot_sector_data.sectors_per_cluster ) + boot_sector_data.data_sector_offset;
            int sector_index;
            
            for (sector_index = 0; sector_index < boot_sector_data.sectors_per_cluster; sector_index++)
            {
                if (Read_Sector_Data(sector_index + start_sector, fat_partition_offset_in_512_byte_sectors))
                {
                    int file_counter;
                    
                    for (file_counter = 0; file_counter < 16; file_counter++)
                    {
                        // Read file record.
                        Read_File_Record_At_Offset(file_counter*32, file_record, cluster, sector_index);
                        if ((file_record->name[0] != 0xe5) && (file_record->name[0] != 0x00))
                        {
                            bool match = match_file_record_to_name_ext(file_record, name, extension);

                            if (match)
                            {                               
                                result = true;
                                break;
                            }
                        }
                    }
                }
                else
                {
                    break;
                }
                if (result)
                {
                    break;
                }
            }
            // If this is the end of the cluster and the file has not been found, then see if there is another cluster
            // that holds data for the current directory.
            if (result == false)
            {
				unsigned short int new_cluster;

				if (get_cluster_flag(cluster, &new_cluster))
				{
					// The directory needs to be expanded to store more files.
					if ((new_cluster & 0x0000fff8) == 0x0000fff8)
					{
						return false;
					}
					cluster = (new_cluster & 0x0000fff8);
				}
				else
                {
                    // Directory path is invalid.                 
                    return false;
                }
            }              
        } while ((cluster < 0x0000fff8) && (result == false));
    }
    
    return result;   
}


static bool find_first_empty_cluster(unsigned int *cluster_number)
// Find the first empty cluster. It will be marked by a 0 entry in the File Allocation Table.
{
    unsigned int sector = boot_sector_data.first_fat_sector_offset;
    unsigned int cluster_index = 2;
    short int cluster = -1;
    bool result = false;
	unsigned max_cluster_index = 0;
	unsigned int non_data_sectors = boot_sector_data.data_sector_offset;
	unsigned int less_than_32 = boot_sector_data.number_of_sectors_in_partition;
	unsigned int greater_than_32 = boot_sector_data.total_sector_count_if_above_32MB;

	if (less_than_32 > greater_than_32)
	{
		max_cluster_index = ((less_than_32 - non_data_sectors) / boot_sector_data.sectors_per_cluster) + 1;
	}
	else
	{
		max_cluster_index = ((greater_than_32 - non_data_sectors) / boot_sector_data.sectors_per_cluster) + 1;
	}
    // Find an empty cluster for the file.
    while (sector != boot_sector_data.second_fat_sector_offset)
    {
        if (Read_Sector_Data( sector, fat_partition_offset_in_512_byte_sectors))
        {
            do {
                cluster = ((unsigned short int) IORD_16DIRECT(device_pointer->base, 2*(cluster_index % 256)));
                if (cluster == 0)
                {
                    // Free cluster found.
                    break;
                }
                else
                {
                    cluster_index++;
                } 
            } while ((cluster_index % 256) != 0);
        }
        if (cluster == 0)
        {
            break;
        }
        sector++;
    }
    if ((cluster == 0) && (cluster <= max_cluster_index))
    {
        *cluster_number = cluster_index;
		result = true;
    }
    return result;
}


static int find_first_empty_record_in_a_subdirectory(int start_cluster_index)
// Search for a free spot in a subdirectory. Return an encoded location for the file record.
{
    int result = -1;
    int cluster = start_cluster_index;
    do {
        int start_sector = ( cluster - 2 ) * ( boot_sector_data.sectors_per_cluster ) + boot_sector_data.data_sector_offset;
        int sector_index;
        
        for (sector_index = 0; sector_index < boot_sector_data.sectors_per_cluster; sector_index++)
        {
            if (Read_Sector_Data(sector_index + start_sector, fat_partition_offset_in_512_byte_sectors))
            {
                int file_counter;
                
                for (file_counter = 0; file_counter < 16; file_counter++)
                {
                    unsigned short int leading_char;
                    
                    // Read file record.
                    leading_char = ((unsigned char) IORD_8DIRECT(device_pointer->base, file_counter*32));
                    if ((leading_char == 0x00e5) || (leading_char == 0))
                    {
                        result = (cluster) | ((sector_index*16 + file_counter) << 16);
                        return result;
                    }
                }
            }
            else
            {
                break;
            }
        }
        // If this is the end of the cluster and the file has not been found, then see if there is another cluster
        // that holds data for the current directory.
        if (result < 0)
        {
			unsigned short int new_cluster;
			if (get_cluster_flag(cluster, &new_cluster))
			{
                // The directory needs to be expanded to store more files.
				if ((new_cluster & 0x0000fff8) == 0x0000fff8)
				{
					unsigned int new_dir_cluster; 
					if (find_first_empty_cluster(&new_dir_cluster))
					{
						// Add the new cluster to the linked list of the given directory.
						if (mark_cluster(cluster, ((short int) (new_dir_cluster)), true) &&
							mark_cluster(new_dir_cluster, ((short int) (0xffff)), true) &&
							mark_cluster(cluster, ((short int) (new_dir_cluster)), false) &&
							mark_cluster(new_dir_cluster, ((short int) (0xffff)), false))
						{
							Save_Modified_Sector();
							// The new file will begin at the first entry of the directory.
							result = new_dir_cluster;                           
						}
					}
					cluster = (new_cluster & 0x0000fff8);
				}
			}
			else
			{
				// Error encountered.                 
				result = -1;
			}
        }              
    } while ((cluster < 0x0000fff8) && (result == -1)); 
    return result; 
}


static int find_first_empty_record_in_root_directory()
// Find a first unused record location to use. Return -1 if none is found.
{
    int max_root_dir_sectors = ((32*boot_sector_data.max_number_of_dir_entires) / boot_sector_data.sector_size_in_bytes);
    int sector_index;
    int result = -1;
    
    for (sector_index = 0; sector_index < max_root_dir_sectors; sector_index++)
    {
        if (Read_Sector_Data(   sector_index + boot_sector_data.root_directory_sector_offset,
                                fat_partition_offset_in_512_byte_sectors))
        {
            int file_counter;
            
            for (file_counter = 0; file_counter < 16; file_counter++)
            {
                unsigned short int leading_char;
                
                // Read first character of the file record.
                leading_char = ((unsigned char) IORD_8DIRECT(device_pointer->base, file_counter*32));
                if ((leading_char == 0x00e5) || (leading_char == 0))
                {
                    result = (sector_index*16 + file_counter) << 16;
                    return result;
                }
            }
        }
        else
        {
            break;
        }
    }
    return result;
}

static void convert_filename_to_name_extension(char *filename, char *name, char *extension)
// This function converts the file name into a name . extension format.
{
    int counter;
    int local = 0;
    
    for(counter = 0; counter < 8; counter++)
    {
        if (filename[local] != '.')
        {
            name[counter] = filename[local];
            if (filename[local] != 0) local++;
        }
        else
        {
            name[counter] = ' ';
        }
    }
    if (filename[local] == '.') local++;
    for(counter = 0; counter < 3; counter++)
    {
        if (filename[local] != 0)
        {
            extension[counter] = filename[local];
            local++;
        }
        else
        {
            extension[counter] = ' ';
        }
    }

}

static bool create_file(char *name, t_file_record *file_record, t_file_record *home_dir)
// Create a file in a given directory. Expand the directory if needed.
{
    unsigned int cluster_number;
    bool result = false;
    
    if (find_first_empty_cluster(&cluster_number))
    {
        int record_index;
        
        if (home_dir->file_record_cluster == 0)
        {
            // Put a file in the root directory.
            record_index = find_first_empty_record_in_root_directory();
        }
        else
        {
            // Put a file in a subdirectory.
            record_index = find_first_empty_record_in_a_subdirectory(home_dir->start_cluster_index);           
        }
        if (record_index >= 0)
        {   
            unsigned int file_record_sector;
            int location = get_dir_divider_location( name );
            int last_dir_separator = 0;

            // Skip through all directory separators.
            while (location > 0)
            {
                last_dir_separator = last_dir_separator+location+1;
                location = get_dir_divider_location( &(name[last_dir_separator]) );
            }
            
            convert_filename_to_name_extension(&(name[last_dir_separator]), (char *)file_record->name, (char *)file_record->extension);
                         
            file_record->attributes = 0;
            file_record->create_time = 0;
            file_record->create_date = 0;
            file_record->last_access_date = 0;
            file_record->last_modified_time = 0;
            file_record->last_modified_date = 0;
            file_record->start_cluster_index = cluster_number;
            file_record->file_size_in_bytes = 0;
            file_record->current_cluster_index = cluster_number;
            file_record->current_sector_in_cluster = 0;
            file_record->current_byte_position = 0;
            file_record->file_record_cluster = record_index & 0x0000ffff;
            file_record->file_record_sector_in_cluster = ((record_index >> 16) & 0x0000ffff) / 16;
            file_record->file_record_offset = (((record_index >> 16) & 0x0000ffff) % 16)*32;   
            file_record->home_directory_cluster = home_dir->start_cluster_index;
            file_record->in_use = true;
            file_record->modified = true;
            // Now write the record at the specified location.
            file_record_sector = (file_record->file_record_cluster == 0) ? 
                                    (boot_sector_data.root_directory_sector_offset + file_record->file_record_sector_in_cluster):  
                                    (boot_sector_data.data_sector_offset + (file_record->file_record_cluster-2)*boot_sector_data.sectors_per_cluster +
                                     file_record->file_record_sector_in_cluster);

			if (Read_Sector_Data(file_record_sector, fat_partition_offset_in_512_byte_sectors))
            {
                if (Write_File_Record_At_Offset(file_record->file_record_offset, file_record))
                {
                    Save_Modified_Sector();
                    // Mark the first cluster of the file as the last cluster at first.
                    mark_cluster(cluster_number, ((short int) (0xffff)), true);
                    if (mark_cluster(cluster_number, ((short int) (0xffff)), false))
                    {
                        result = true;
                    }
                }
            }
        }

    }
    return result;           
}


static void copy_file_record_name_to_string(t_file_record *file_record, char *file_name)
/* Copy a file name from the file record to a given string */
{
	int index;
	int flength = 0;

	/* Copy file name.*/
	for (index = 0; index < 8; index++)
	{
		if (file_record->name[index] != ' ')
		{
			file_name[flength] = file_record->name[index];
			flength = flength + 1;
		}
	}
	if (file_record->extension[0] != ' ')
	{
		file_name[flength] = '.';
		flength = flength + 1;
		for (index = 0; index < 3; index++)
		{
			if (file_record->extension[index] != ' ')
			{
				file_name[flength] = file_record->extension[index];
				flength = flength + 1;
			}
		}
	}
	file_name[flength] = 0;
}
#endif //SD_RAW_IFACE

///////////////////////////////////////////////////////////////////////////
// Direct functions
///////////////////////////////////////////////////////////////////////////


alt_up_sd_card_dev* alt_up_sd_card_open_dev(const char* name)
{
	// find the device from the device list 
	// (see altera_hal/HAL/inc/priv/alt_file.h 
	// and altera_hal/HAL/src/alt_find_dev.c 
	// for details)
	alt_up_sd_card_dev *dev = (alt_up_sd_card_dev *) alt_find_dev(name, &alt_dev_list);

	if (dev != NULL)
	{
		aux_status_register = ((short int *) SD_CARD_AUX_STATUS(dev->base));
		status_register = ((int *) SD_CARD_STATUS(dev->base));
		CSD_register_w0 = ((short int *) SD_CARD_CSD(dev->base, 0));
		command_register = ((short int *) SD_CARD_COMMAND(dev->base));
		command_argument_register = ((int *) SD_CARD_ARGUMENT(dev->base));
		buffer_memory = (char *) SD_CARD_BUFFER(dev->base, 0);
		device_pointer = dev;
		initialized = false;
#ifndef SD_RAW_IFACE
		is_sd_card_formated_as_FAT16 = false;
		search_data.valid = false;
#endif
	}
	return dev;
}


bool alt_up_sd_card_is_Present(void)
// Check if there is an SD Card insterted into the SD Card socket.
{
    bool result = false;

    if ((device_pointer != NULL) && ((IORD_16DIRECT(aux_status_register,0) & 0x02) != 0))
    {
        result = true;
    }
	else if (initialized == true)
	{
		int index;

		initialized = false;
#ifndef SD_RAW_IFACE
		search_data.valid = false;
		is_sd_card_formated_as_FAT16 = false;

		for(index = 0; index < MAX_FILES_OPENED; index++)
		{
			active_files[index].in_use = false;
			active_files[index].modified = false;
		}
#endif
	}
    return result;
}

#ifndef SD_RAW_IFACE
bool alt_up_sd_card_is_FAT16(void)
/* This function reads the SD card data in an effort to determine if the card is formated as a FAT16
 * volume. Please note that FAT12 has a similar format, but will not be supported by this driver.
 * If the card contains a FAT16 volume, the local data structures will be initialized to allow reading and writing
 * to the SD card as though it was a hard drive.
 */
{
	bool result = false;

	if (alt_up_sd_card_is_Present())
	{
		// Check if an SD Card is in the SD Card slot.
		if (initialized == false)
		{
			// Now determine if the card is formatted as FAT 16.
			is_sd_card_formated_as_FAT16 = Look_for_FAT16();
			initialized = is_sd_card_formated_as_FAT16;
			search_data.valid = false;
		}
		result = is_sd_card_formated_as_FAT16;
	}
	else
	{
		// If not then you may as well not open the device.
		initialized = false;
		is_sd_card_formated_as_FAT16 = false;
	}

	return result;
}


short int alt_up_sd_card_find_first(char *directory_to_search_through, char *file_name)
/* This function sets up a search algorithm to go through a given directory looking for files.
 * If the search directory is valid, then the function searches for the first file it finds.
 * Inputs:
 *		directory_to_search_through - name of the directory to search through
 *		file_name - an array to store a name of the file found. Must be 13 bytes long (12 bytes for file name and 1 byte of NULL termination).
 * Outputs:
 *		0 - success
 *		1 - invalid directory
 *		2 - No card or incorrect card format.
 *
 * To specify a directory give the name in a format consistent with the following regular expression:
 * [{[valid_chars]+}/]*.
 * 
 * In other words, give a path name starting at the root directory, where each directory name is followed by a '/'.
 * Then, append a '.' to the directory name. Examples:
 * "." - look through the root directory
 * "first/." - look through a directory named "first" that is located in the root directory.
 * "first/sub/." - look through a directory named "sub", that is located within the subdirectory named "first". "first" is located in the root directory.
 * Invalid examples include:
 * "/.", "/////." - this is not the root directory.
 * "/first/." - the first character may not be a '/'.
 */
{
	short int result = 2;
	if ((alt_up_sd_card_is_Present()) && (is_sd_card_formated_as_FAT16))
	{
		int home_directory_cluster;
		t_file_record file_record;

		if (get_home_directory_cluster_for_file(directory_to_search_through, &home_directory_cluster, &file_record))
		{
			search_data.directory_root_cluster = home_directory_cluster;
			search_data.current_cluster_index = home_directory_cluster;
			search_data.current_sector_in_cluster = 0;
			search_data.file_index_in_sector = -1;
			search_data.valid = true;
			result = alt_up_sd_card_find_next(file_name);
		}
		else
		{
			result = 1;
		}
	}
	return result;
}


short int alt_up_sd_card_find_next(char *file_name)
/* This function searches for the next file in a given directory, as specified by the find_first function.
 * Inputs:
 *		file_name - an array to store a name of the file found. Must be 13 bytes long (12 bytes for file name and 1 byte of NULL termination).
 * Outputs:
 *		-1 - end of directory.
 *		0 - success
 *		2 - No card or incorrect card format.
 *		3 - find_first has not been called successfully.
 */
{
	short int result = 2;
	if ((alt_up_sd_card_is_Present()) && (is_sd_card_formated_as_FAT16))
	{
		if (search_data.valid)
		{
			t_file_record file_record;
			int cluster = search_data.current_cluster_index;

			if (cluster == 0)
			{
				// Searching through the root directory
				int max_root_dir_sectors = ((32*boot_sector_data.max_number_of_dir_entires) / boot_sector_data.sector_size_in_bytes);
				int sector_index = search_data.current_sector_in_cluster;
				int file_counter = search_data.file_index_in_sector+1;
    
				for (; sector_index < max_root_dir_sectors; sector_index++)
				{
					if (Read_Sector_Data(   sector_index + boot_sector_data.root_directory_sector_offset,
											fat_partition_offset_in_512_byte_sectors))
					{
						for (; file_counter < 16; file_counter++)
						{
							if (Read_File_Record_At_Offset(file_counter*32, &file_record, 0, sector_index))
							{
								if ((file_record.name[0] != 0) && (file_record.name[0] != 0xe5))
								{
									/* Update search structure. */
									search_data.file_index_in_sector = file_counter;
									search_data.current_sector_in_cluster = sector_index;

									/* Copy file name.*/
									copy_file_record_name_to_string(&file_record, file_name);
									return 0;
								}
							}
						}
						file_counter = 0;
					}
					else
					{
						break;
					}
				}
				result = -1;
			}
			else
			{
				int file_counter = search_data.file_index_in_sector+1;
				do 
				{
					int start_sector = ( cluster - 2 ) * ( boot_sector_data.sectors_per_cluster ) + boot_sector_data.data_sector_offset;
					int sector_index = search_data.current_sector_in_cluster;
			        
					for (; sector_index < boot_sector_data.sectors_per_cluster; sector_index++)
					{
						if (Read_Sector_Data(sector_index + start_sector, fat_partition_offset_in_512_byte_sectors))
						{        
							for (; file_counter < 16; file_counter++)
							{
								if (Read_File_Record_At_Offset(file_counter*32, &file_record, cluster, sector_index))
								{
									if ((file_record.name[0] != 0) && (file_record.name[0] != 0xe5))
									{
										/* Update search structure. */
										search_data.current_cluster_index = cluster;
										search_data.file_index_in_sector = file_counter;
										search_data.current_sector_in_cluster = sector_index;

										/* Copy file name.*/
										copy_file_record_name_to_string(&file_record, file_name);
										return 0;
									}
								}
							}
							file_counter = 0;
						}
						else
						{
							break;
						}
					}
					// If this is the end of the cluster and the file has not been found, then see if there is another cluster
					// that holds data for the current directory.
					if (sector_index >= boot_sector_data.sectors_per_cluster)
					{
						unsigned short int new_cluster;

						if (get_cluster_flag(cluster, &new_cluster))
						{
							if ((new_cluster & 0x0000fff8) == 0x0000fff8)
							{
								result = -1;
								search_data.valid = false;
							}
							cluster = ((new_cluster) & 0x0000fff8);
						}
						else
						{
							// Error encountered.                 
							result = -1;
						}
					}              
				} while (cluster < 0x0000fff8);
			}
		}
		else
		{
			// Call Find_First first.
			result = 3;
		}
	}
	return result;
}


short int alt_up_sd_card_fopen(char *name, bool create)
/* This function reads the SD card data in an effort to determine if the card is formated as a FAT16
 * volume. Please note that FAT12 has a similar format, but will not be supported by this driver.
 * 
 * Inputs:
 *      name - a file name including a directory, relative to the root directory
 *      create - a flag set to true to create a file if it does not already exist
 * Output:
 *      An index to the file record assigned to the specified file. -1 is returned if the file could not be opened.
 *		Return -2 if the specified file has already been opened previously.
 */
{
	short int file_record_index = -1;

	if ((alt_up_sd_card_is_Present()) && (is_sd_card_formated_as_FAT16))
	{
        unsigned int home_directory_cluster = 0;
        t_file_record home_dir;
        
        /* First check the file name format. It should not be longer than 12 characters, including a period and the extension.
         * Rules:
         *  - no spaces
         *  - at most 12 chatacters per name, with a period in 9th position.
         *  - a / or a \ every at most 12 characters.
         */
        filename_to_upper_case(name);
        if (check_file_name_for_FAT16_compliance(name))
        {
			int index;

            /* Get home directory cluster location for the specified file. 0 means root directory. */
            if (!get_home_directory_cluster_for_file(name, (int *) &home_directory_cluster, &home_dir))
            {
                return file_record_index;
            }
            
    		/* Find a free file slot to store file specs in. */
    		for (index = 0; index < MAX_FILES_OPENED; index++)
    		{
    			if (active_files[index].in_use == false)
    			{
    				file_record_index = index;
    				break;
    			}
    		}
    		if (file_record_index >= 0)
    		{
    			/* If file record is found, then look for the specified file. If the create flag is set to true 
    			 * and the file is not found, then it should be created in the current directory. 
    			 */
                
                if (find_file_in_directory(home_directory_cluster, name, &(active_files[file_record_index])))
                {
                    if (create)
                    {
                        /* Do not allow overwriting existing files for now. */
                        return -1;
                    }
                    active_files[file_record_index].current_cluster_index = active_files[file_record_index].start_cluster_index;
                    active_files[file_record_index].current_sector_in_cluster = 0;
                    active_files[file_record_index].current_byte_position = 0;
                    active_files[file_record_index].in_use = true;
    				active_files[file_record_index].modified = false;

					/* Check if the file has already been opened. */
					for (index = 0; index < MAX_FILES_OPENED; index++)
					{
						if ((file_record_index != index) && (active_files[index].in_use == true))
						{
							if ((active_files[file_record_index].file_record_cluster == active_files[index].file_record_cluster) &&
								(active_files[file_record_index].file_record_sector_in_cluster == active_files[index].file_record_sector_in_cluster) &&
								(active_files[file_record_index].file_record_offset == active_files[index].file_record_offset))
							{
								// file already in use.
								file_record_index = -2;
								break;
							}
						}
					}

                }
                else if (create)
                {
                    /* Create file if needed. */
                    if (create_file(name, &(active_files[file_record_index]), &home_dir))
                    {
                        active_files[file_record_index].in_use = true;
    					active_files[file_record_index].modified = true;
                    }
                    else
                    {
                        /* If file creation fails then return an invalid file handle. */
                        file_record_index = -1;
                    }                
                }
                else
                {
                    /* Otherwise the file could not be opened.*/
                    file_record_index = -1;
                }
    		}
        }
	}

	return file_record_index;
}


void alt_up_sd_card_set_attributes(short int file_handle, short int attributes)
/* Return file attributes, or -1 if the file_handle is invalid.
 */
{
    if ((file_handle >= 0) && (file_handle < MAX_FILES_OPENED))
    {
        if (active_files[file_handle].in_use)
        {
            active_files[file_handle].attributes = ((char)(attributes & 0x00ff));
        }
    }
}


short int alt_up_sd_card_get_attributes(short int file_handle)
/* Return file attributes, or -1 if the file_handle is invalid.
 */
{
	short int result = -1;
    if ((file_handle >= 0) && (file_handle < MAX_FILES_OPENED))
    {
        if (active_files[file_handle].in_use)
		{
			result = ((active_files[file_handle].attributes) & 0x00ff);
		}
	}
	return result;
}

short int alt_up_sd_card_read(short int file_handle)
/* Read a single character from a given file. Return -1 if at the end of a file. Any other negative number
 * means that the file could not be read. A number between 0 and 255 is an ASCII character read from the SD Card. */
{
    short int ch = -1;
    
    if ((file_handle >= 0) && (file_handle < MAX_FILES_OPENED))
    {
        if (active_files[file_handle].in_use)
        {
            if (active_files[file_handle].current_byte_position < active_files[file_handle].file_size_in_bytes)
            {
                int data_sector = boot_sector_data.data_sector_offset + (active_files[file_handle].current_cluster_index - 2)*boot_sector_data.sectors_per_cluster +
                                  active_files[file_handle].current_sector_in_cluster;
                
                if ((active_files[file_handle].current_byte_position > 0) && ((active_files[file_handle].current_byte_position % 512) == 0))
                {
                    // Read in a new sector of data.
                    if (active_files[file_handle].current_sector_in_cluster == boot_sector_data.sectors_per_cluster - 1)
                    {
                        // Go to the next cluster.
                        unsigned short int next_cluster;
                        if (get_cluster_flag(active_files[file_handle].current_cluster_index, &next_cluster))
                        {
                            if ((next_cluster & 0x0000fff8) == 0x0000fff8)
                            {
                                /* End of file */
                                return -1;
                            } 
                            else
                            {
                                active_files[file_handle].current_cluster_index = next_cluster;
								active_files[file_handle].current_sector_in_cluster = 0;
                                data_sector = boot_sector_data.data_sector_offset + (active_files[file_handle].current_cluster_index - 2)*boot_sector_data.sectors_per_cluster +
                                  active_files[file_handle].current_sector_in_cluster;                                
                            }
                        }
                        else
                        {
                            return -2;
                        }
                    }
                    else
                    {
                        active_files[file_handle].current_sector_in_cluster = active_files[file_handle].current_sector_in_cluster + 1;
                        data_sector = data_sector + 1;
                    }
                }
                // Reading te first byte of the file.
                if (current_sector_index != (data_sector + fat_partition_offset_in_512_byte_sectors))
                {
                    if (!Read_Sector_Data(data_sector, fat_partition_offset_in_512_byte_sectors))
                    {
						return -2;
                    }
                }

                ch = (unsigned char) IORD_8DIRECT(buffer_memory, (active_files[file_handle].current_byte_position % 512));
                active_files[file_handle].current_byte_position = active_files[file_handle].current_byte_position + 1;
            }
        }
    }
    
    return ch;
}


bool alt_up_sd_card_write(short int file_handle, char byte_of_data)
/* Write a single character to a given file. Return true if successful, and false otherwise. */
{
    bool result = false;
    
    if ((file_handle >= 0) && (file_handle < MAX_FILES_OPENED))
    {
        if (active_files[file_handle].in_use)
        {
            int data_sector = boot_sector_data.data_sector_offset + (active_files[file_handle].current_cluster_index - 2)*boot_sector_data.sectors_per_cluster +
                              active_files[file_handle].current_sector_in_cluster;
			short int buffer_offset = active_files[file_handle].current_byte_position % boot_sector_data.sector_size_in_bytes;

			if (active_files[file_handle].current_byte_position < active_files[file_handle].file_size_in_bytes)
            {
                if ((active_files[file_handle].current_byte_position > 0) && (buffer_offset == 0))
                {
                    // Read in a new sector of data.
                    if (active_files[file_handle].current_sector_in_cluster == boot_sector_data.sectors_per_cluster - 1)
                    {
                        // Go to the next cluster.
                        unsigned short int next_cluster;
                        if (get_cluster_flag(active_files[file_handle].current_cluster_index, &next_cluster))
                        {
                            if (next_cluster < 0x0000fff8)
                            {
                                active_files[file_handle].current_cluster_index = next_cluster;
								active_files[file_handle].current_sector_in_cluster = 0;
                                data_sector = boot_sector_data.data_sector_offset + (active_files[file_handle].current_cluster_index - 2)*boot_sector_data.sectors_per_cluster +
                                  active_files[file_handle].current_sector_in_cluster;                                
                            }
                        }
                        else
                        {
                            return false;
                        }
                    }
                    else
                    {
                        active_files[file_handle].current_sector_in_cluster = active_files[file_handle].current_sector_in_cluster + 1;
                        data_sector = data_sector + 1;
                    }
                }
            }
			else
			{
				/* You are adding data to the end of the file, so increment its size and look for an additional data cluster if needed. */
				if ((active_files[file_handle].current_byte_position > 0) && (buffer_offset == 0))
				{
					if (active_files[file_handle].current_sector_in_cluster == boot_sector_data.sectors_per_cluster - 1)
					{
						/* Find a new cluster if possible. */
						unsigned int cluster_number;

						if (find_first_empty_cluster(&cluster_number))
						{
							// mark clusters in both File Allocation Tables.
							mark_cluster(active_files[file_handle].current_cluster_index, ((unsigned short int) (cluster_number & 0x0000ffff)), true);
							mark_cluster(cluster_number, 0xffff, true);
							mark_cluster(active_files[file_handle].current_cluster_index, ((unsigned short int) (cluster_number & 0x0000ffff)), false);
							mark_cluster(cluster_number, 0xffff, false);
							// Change cluster index and sector index to compute a new data sector.
							active_files[file_handle].current_cluster_index = cluster_number;
							active_files[file_handle].current_sector_in_cluster = 0;
						}
						else
						{
							return false;
						}
					}
					else
					{
						/* Read the next sector in the cluster and modify it. We only need to change the data_sector value. The actual read happens a few lines below. */
						active_files[file_handle].current_sector_in_cluster = active_files[file_handle].current_byte_position / boot_sector_data.sector_size_in_bytes;
					}
					data_sector = boot_sector_data.data_sector_offset + (active_files[file_handle].current_cluster_index - 2)*boot_sector_data.sectors_per_cluster +
                          active_files[file_handle].current_sector_in_cluster;
				}
			}
            // Reading a data sector into the buffer. Note that changes to the most recently modified sector will be saved before
			// a new sector is read from the SD Card.
            if (current_sector_index != data_sector + fat_partition_offset_in_512_byte_sectors)
            {
                if (!Read_Sector_Data(data_sector, fat_partition_offset_in_512_byte_sectors))
                {
					return false;
                }
            }
            // Write a byte of data to the buffer.
			IOWR_8DIRECT(buffer_memory, buffer_offset, byte_of_data);
			active_files[file_handle].current_byte_position = active_files[file_handle].current_byte_position + 1;

			// Modify the file record only when necessary.
			if (active_files[file_handle].current_byte_position >= active_files[file_handle].file_size_in_bytes)
			{
				active_files[file_handle].file_size_in_bytes = active_files[file_handle].file_size_in_bytes + 1;
				active_files[file_handle].modified = true;
			}
            // Invaldiate the buffer to ensure that the buffer contents are written to the SD card whe nthe file is closed.
            current_sector_modified = true;
			result = true;
		}
    }
    
    return result;
}


bool alt_up_sd_card_fclose(short int file_handle)
// This function closes an opened file and saves data to SD Card if necessary.
{
    bool result = false;
    if ((alt_up_sd_card_is_Present()) && (is_sd_card_formated_as_FAT16))
    {
        if (active_files[file_handle].in_use) 
        {
			if (active_files[file_handle].modified)
			{
				unsigned int record_sector = active_files[file_handle].file_record_sector_in_cluster;
				if (active_files[file_handle].file_record_cluster == 0)
				{
					record_sector = record_sector + boot_sector_data.root_directory_sector_offset;
				}
				else
				{
					record_sector = record_sector + boot_sector_data.data_sector_offset + 
									(active_files[file_handle].file_record_cluster - 2)*boot_sector_data.sectors_per_cluster;
				}
				if (Read_Sector_Data(record_sector, fat_partition_offset_in_512_byte_sectors))
				{
					if (Write_File_Record_At_Offset(active_files[file_handle].file_record_offset, &(active_files[file_handle])))
					{
						// Make sure that the Data has been saved to the SD Card.
						result = Save_Modified_Sector();
					}
				}
			}
			active_files[file_handle].in_use = false;
			result = true;
        }
    }
    
    return result;
}

#endif //SD_RAW_IFACE
