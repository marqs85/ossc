#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#define MEMBLK 1024

int main(int argc, char **argv)
{
	unsigned char block[4];
    unsigned int csum;
	
	int fd_i;
	FILE *fd_o;
	struct stat fileinfo;
	
	unsigned int i;
	
	if (argc != 3) {
		printf("usage: %s binfile hexfile\n", argv[0]);
		return -1;	
	}
	
	if ((fd_i = open(argv[1], O_RDONLY)) == -1 || fstat(fd_i, &fileinfo) == -1) {
		printf("Couldn't open file\n");
		return -1;
	}

    printf("size: %u bytes\n", fileinfo.st_size);
	
	if ((fd_o = fopen(argv[2], "w")) == NULL) {
		printf("invalid outfile\n");
		return -1;
	}
	
	for (i=0; i<fileinfo.st_size; i+=4) {
		read(fd_i, (void*)block, 4);
        csum = 0x04+((i/4)>>8)+((i/4)&0xff)+block[3]+block[2]+block[1]+block[0];
        csum &= 0xff;
        csum = (~csum+1)&0xff;
		fprintf(fd_o, ":04%.4X00%.2X%.2X%.2X%.2X%.2X\n", i/4, block[3],block[2],block[1],block[0],csum);
	}
	
	fprintf(fd_o, ":00000001FF\n");
	
	fclose(fd_o);
	close(fd_i);
	
	return 0;
}
