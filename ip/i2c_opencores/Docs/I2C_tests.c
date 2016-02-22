/* these test were created to show how to use the opencores I2C along with a driver found in
 * the I2C_opencores component to talk to various components. 
 * This test example uses a littel daughter board from microtronix
 * it has a I2c to parallel chip (PCA9554A) a EEPORM and real time clock. 
 * I chose not to impliment the real time clock. 
 * But you can see how the calls work
 * There are only 4 functions associalted with the I2C driver
 * I2C start  -  send start bit and address of the chip
 * I2C_read - read data
 * I2C_write. - write data
 * how and when each of these get used is based on the device you
 * are talking to. 
 * See the driver code for details of each function. 
 * */

#include <stdio.h>
#include "system.h"
#include "i2c_opencores.h"
int main()
{
    int data;
    int i;
    // testing the PCA9554A paralle interface
    // this writes a 5 to the leds and read the position of the dip switches.
 printf(" tesing the PCA9554A interface the\n the LEDS should be at a 5 \n");  
 // address 0x38 
 // set the fequesncy that you want to run at 
 // most devices work at 100Khz  some faster
 I2C_init(I2CA_BASE,ALT_CPU_FREQ,100000);
 I2C_init(I2CA_BASE,ALT_CPU_FREQ,100000);
 // for the parallel io only the first 4 are output s
 
 // the PCA9554A   uses a command word right after the chip address word ( the start work)
 I2C_start(I2CA_BASE,0x38,0);// chip address in write mode
 I2C_write(I2CA_BASE,3,0);  // write to register 3 command
 I2C_write(I2CA_BASE,0xf0,1);  // set the bottom 4 bits to outputs for the LEDs set the stop
 
 // now right to the leds
  I2C_start(I2CA_BASE,0x38,0); // address the chip in write mode
 I2C_write(I2CA_BASE,1,0);  // set command to the pca9554 to be output register
 I2C_write(I2CA_BASE,5,1);  // write the data to the output register and set the stop

//now read the dip switches
// first set the command to 0
 I2C_start(I2CA_BASE,0x38,0); //address the chip in write mode
data =  I2C_write(I2CA_BASE,0,0);  // set command to read input register.
 I2C_start(I2CA_BASE,0x38,1); //send start again but this time in read mode
data =  I2C_read(I2CA_BASE,1);  // read the input register and send stop
data = 0xff & (data >>4);   
printf("dip switch 0x%x\n",data);

printf("\nNow writing and reading from the EEPROM\n");
//address 0x50-57
I2C_start(I2CA_BASE,0x50,0); // chip address in write mode
I2C_write(I2CA_BASE,0,0);  // write to starting address of 0
// now write the data 
for (i=0;i<7;i++)           // can only write 8 bites at a time
{   
 I2C_write(I2CA_BASE,i,0);  // writ the data 
}
 I2C_write(I2CA_BASE,i,1);  // write last one with last flag
 
 while ( I2C_start(I2CA_BASE,0x50,0)); // make sure the write is done be fore continuing.
 // can ony burst 8 at a time.

I2C_write(I2CA_BASE,8,0);  // write to starting address of 8
// now write the data 
for (i=0;i<7;i++)   // write the next 8 bytes
{
 I2C_write(I2CA_BASE,i+8,0);  // 
}
 I2C_write(I2CA_BASE,i+8,1);  // write last one with last flag
 
 while ( I2C_start(I2CA_BASE,0x50,0)); // make sure the write is done be fore continuing.
 
 //now read the values
// first set the command to 0
 I2C_start(I2CA_BASE,0x50,0); //set chip address and set to write/
 I2C_write(I2CA_BASE,0,0);  // set address to 0.
I2C_start(I2CA_BASE,0x50,1); //set chip address in read mode
for (i=0;i<15;i++)
{
 data =  I2C_read(I2CA_BASE,0);  // memory array
 printf("\tdata = 0x%x\n",data);
}

data =  I2C_read(I2CA_BASE,1);  // last memory read
 printf("\tdata = 0x%x\n",data);







  printf("Hello from Nios II!\n");

  return 0;
}
