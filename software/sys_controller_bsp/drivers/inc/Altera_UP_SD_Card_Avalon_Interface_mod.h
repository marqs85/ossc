#ifndef __ALTERA_UP_SD_CARD_AVALON_INTERFACE_H__
#define __ALTERA_UP_SD_CARD_AVALON_INTERFACE_H__

#include <stddef.h>
#include <alt_types.h>
#include <sys/alt_dev.h>

#ifdef __cplusplus
extern "C"
{
#endif /* __cplusplus */

#define SD_RAW_IFACE

/*
 * Device structure definition. Each instance of the driver uses one
 * of these structures to hold its associated state.
 */
typedef struct alt_up_sd_card_dev {
	/// @brief character mode device structure 
	/// @sa Developing Device Drivers for the HAL in Nios II Software Developer's Handbook
	alt_dev dev;
	/// @brief the base address of the device
	unsigned int base;

} alt_up_sd_card_dev;

#ifndef bool
    typedef enum e_bool { false = 0, true = 1 } bool;
#endif

//////////////////////////////////////////////////////////////////////////
// HAL system functions

alt_up_sd_card_dev* alt_up_sd_card_open_dev(const char *name);
/* Open an SD Card Interface if it is connected to the system. */


bool alt_up_sd_card_is_Present(void);
/* Check if there is an SD Card insterted into the SD Card socket.
 */

#ifndef SD_RAW_IFACE
bool alt_up_sd_card_is_FAT16(void);
/* This function reads the SD card data in an effort to determine if the card is formated as a FAT16
 * volume. Please note that FAT12 has a similar format, but will not be supported by this driver.
 */


short int alt_up_sd_card_fopen(char *name, bool create);
/* This function reads the SD card data in an effort to determine if the card is formated as a FAT16
 * volume. Please note that FAT12 has a similar format, but will not be supported by this driver.
 * 
 * Inputs:
 *      name - a file name including a directory, relative to the root directory
 *      create - a flag set to true to create a file if it does not already exist
 * Output:
 *      An index to the file record assigned to the specified file. -1 is returned if the file could not be opened.
 */


short int alt_up_sd_card_find_first(char *directory_to_search_through, char *file_name);
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



short int alt_up_sd_card_find_next(char *file_name);
/* This function searches for the next file in a given directory, as specified by the find_first function.
 * Inputs:
 *		file_name - an array to store a name of the file found. Must be 13 bytes long (12 bytes for file name and 1 byte of NULL termination).
 * Outputs:
 *		-1 - end of directory.
 *		0 - success
 *		2 - No card or incorrect card format.
 *		4 - find_first has not been called successfully.
 */

void alt_up_sd_card_set_attributes(short int file_handle, short int attributes);
/* Set file attributes as needed.
 */

short int alt_up_sd_card_get_attributes(short int file_handle);
/* Return file attributes, or -1 if the file_handle is invalid.
 */


short int alt_up_sd_card_read(short int file_handle);
/* Read a single character from the given file. Return -1 if at the end of a file. Any other negative number
 * means that the file could not be read. A number between 0 and 255 is an ASCII character read from the SD Card. */


bool alt_up_sd_card_write(short int file_handle, char byte_of_data);
/* Write a single character to a given file. Return true if successful, and false otherwise. */


bool alt_up_sd_card_fclose(short int file_handle);
// This function closes an opened file and saves data to SD Card if necessary.

#else
bool Write_Sector_Data(int sector_index, int partition_offset);
bool Save_Modified_Sector();
bool Read_Sector_Data(int sector_index, int partition_offset);
#endif //SD_RAW_IFACE

//////////////////////////////////////////////////////////////////////////
// file-like operation functions

//////////////////////////////////////////////////////////////////////////
// direct operation functions


/*
 * Macros used by alt_sys_init 
 */
#define ALTERA_UP_SD_CARD_AVALON_INTERFACE_MOD_INSTANCE(name, device)	\
  static alt_up_sd_card_dev device =		\
  {                                                 	\
    {                                               	\
      ALT_LLIST_ENTRY,                              	\
      name##_NAME,                                  	\
      NULL , /* open */		\
      NULL , /* close */		\
      NULL, /* read */		\
      NULL, /* write */		\
      NULL , /* lseek */		\
      NULL , /* fstat */		\
      NULL , /* ioctl */		\
    },                                              	\
	name##_BASE,                                	\
  }

#define ALTERA_UP_SD_CARD_AVALON_INTERFACE_MOD_INIT(name, device) \
{	\
    alt_dev_reg(&device.dev);                          	\
}



#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __ALTERA_UP_SD_CARD_AVALON_INTERFACE_H__ */


