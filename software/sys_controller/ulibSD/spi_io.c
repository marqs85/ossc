#include <alt_types.h>
#include <altera_avalon_pio_regs.h>
#include <sys/alt_timestamp.h>
#include <system.h>
#include <io.h>
#include "i2c_opencores.h"
#include "spi_io.h"
#include "av_controller.h"

extern alt_u16 sys_ctrl;

alt_u32 sd_timer_ts;

void SPI_Init (void) {
    I2C_init(SD_SPI_BASE,ALT_CPU_FREQ,400000);
}

void SPI_W(const BYTE *wd, int len) {
    SPI_write(SD_SPI_BASE, wd, len);
}

void SPI_R(BYTE *rd, int len) {
    SPI_read(SD_SPI_BASE, rd, len);
}

void SPI_WW(BYTE d) {
    SPI_W(&d, 1);
}

BYTE SPI_RW (BYTE d) {
    BYTE w;
    SPI_R(&w, 1);

    return w;
}

void SPI_Release (void) {
    return;
}

inline void SPI_CS_Low (void) {
    sys_ctrl &= ~SD_SPI_SS_N;
    IOWR_ALTERA_AVALON_PIO_DATA(PIO_0_BASE, sys_ctrl);
}

inline void SPI_CS_High (void){
    sys_ctrl |= SD_SPI_SS_N;
    IOWR_ALTERA_AVALON_PIO_DATA(PIO_0_BASE, sys_ctrl);
}

inline void SPI_Freq_High (void) {
    I2C_init(SD_SPI_BASE,ALT_CPU_FREQ,ALT_CPU_FREQ/SCL_MIN_CLKDIV);
}

inline void SPI_Freq_Low (void) {
    I2C_init(SD_SPI_BASE,ALT_CPU_FREQ,400000);
}

int SPI_Timer_On (WORD ms) {
    if (!sd_timer_ts) {
        sd_timer_ts = ms*(ALT_CPU_FREQ/1000);
        alt_timestamp_start();
        return 0;
    }
    return 1;
}

inline BOOL SPI_Timer_Status (void) {
    return alt_timestamp() < sd_timer_ts;
}

inline void SPI_Timer_Off (void) {
    sd_timer_ts = 0;
    return;
}
