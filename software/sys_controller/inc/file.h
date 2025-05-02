#ifndef FILE_H_
#define FILE_H_

#include "ff.h"
#include "sysconfig.h"

FRESULT file_mount();
FRESULT file_open(FIL* fil, char* path);
FRESULT file_close(FIL* fil);
TCHAR* file_get_string(FIL* fil, char* buff, int len);
FRESULT scan_files (char* path);
int find_files_exec(char* path, char* pat, FILINFO *fno, int efirst, int elast, void (*func)(FILINFO *fno));

#endif
