#include "spi_io.h"

#define SD_SLAVE_ID 0

void SPI_Init (void) {
    return;
}

BYTE SPI_RW (BYTE d) {
    BYTE rdata;

    alt_avalon_spi_command(SPI_0_BASE, SD_SLAVE_ID, 1, &d, 1, &rdata, 0);
    return rdata;
}

void SPI_Release (void) {
    return;
}

inline void SPI_CS_Low (void) {
    return;
}

inline void SPI_CS_High (void){
    return;
}

inline void SPI_Freq_High (void) {
    return;
}

inline void SPI_Freq_Low (void) {
    return;
}

void SPI_Timer_On (WORD ms) {

}

inline BOOL SPI_Timer_Status (void) {

}

inline void SPI_Timer_Off (void) {

}
