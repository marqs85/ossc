#ifndef IT6613_H_
#define IT6613_H_

#include "sysconfig.h"

#define IT6613_VENDORID		0xCA
#define IT6613_DEVICEID		0x13

#define IT_BASE (0x98>>1)
#define IT_VENDORID 		0x01
#define IT_DEVICEID 		0x02
#define IT_RESET	 		0x04
#define IT_CURBANK			0x0F
#define IT_DRIVECTRL		0x61
#define IT_HDMIMODE			0xC0
#define IT_OUTCOLOR			0x158

#define REG_TX_SW_RST       0x04
    #define B_ENTEST    (1<<7)
    #define B_REF_RST (1<<5)
    #define B_AREF_RST (1<<4)
    #define B_VID_RST (1<<3)
    #define B_AUD_RST (1<<2)
    #define B_HDMI_RST (1<<1)
    #define B_HDCP_RST (1<<0)

#endif /* IT6613_H_ */
