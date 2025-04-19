#ifndef _EDID_H_
#define _EDID_H_

#include "hdmitx.h"

#ifdef SUPPORT_EDID

/////////////////////////////////////////
// RX Capability.
/////////////////////////////////////////
typedef struct {
    BYTE b16bit:1 ;
    BYTE b20bit:1 ;
    BYTE b24bit:1 ;
    BYTE Rsrv:5 ;
} LPCM_BitWidth ;

typedef enum {
    AUD_RESERVED_0 = 0 ,
    AUD_LPCM,
    AUD_AC3,
    AUD_MPEG1,
    AUD_MP3,
    AUD_MPEG2,
    AUD_AAC,
    AUD_DTS,
    AUD_ATRAC,
    AUD_ONE_BIT_AUDIO,
    AUD_DOLBY_DIGITAL_PLUS,
    AUD_DTS_HD,
    AUD_MAT_MLP,
    AUD_DST,
    AUD_WMA_PRO,
    AUD_RESERVED_15
} AUDIO_FORMAT_CODE ;

typedef union {
    struct {
        BYTE channel:3 ;
        BYTE AudioFormatCode:4 ;
        BYTE Rsrv1:1 ;

        BYTE b32KHz:1 ;
        BYTE b44_1KHz:1 ;
        BYTE b48KHz:1 ;
        BYTE b88_2KHz:1 ;
        BYTE b96KHz:1 ;
        BYTE b176_4KHz:1 ;
        BYTE b192KHz:1 ;
        BYTE Rsrv2:1 ;
        BYTE ucCode ;
    } s ;
    BYTE uc[3] ;

} AUDDESCRIPTOR ;

typedef union {
    struct {
        BYTE FL_FR:1 ;
        BYTE LFE:1 ;
        BYTE FC:1 ;
        BYTE RL_RR:1 ;
        BYTE RC:1 ;
        BYTE FLC_FRC:1 ;
        BYTE RLC_RRC:1 ;
        BYTE Reserve:1 ;
        BYTE Unuse[2] ;
    } s ;
    BYTE uc[3] ;
} SPK_ALLOC ;

#define CEA_SUPPORT_UNDERSCAN (1<<7)
#define CEA_SUPPORT_AUDIO (1<<6)
#define CEA_SUPPORT_YUV444 (1<<5)
#define CEA_SUPPORT_YUV422 (1<<4)
#define CEA_NATIVE_MASK 0xF

typedef union _tag_DCSUPPORT {
    struct {
        BYTE DVI_Dual:1 ;
        BYTE Rsvd:2 ;
        BYTE DC_Y444:1 ;
        BYTE DC_30Bit:1 ;    
        BYTE DC_36Bit:1 ;    
        BYTE DC_48Bit:1 ;    
        BYTE SUPPORT_AI:1 ;    
    } info ;
    BYTE uc ;
} DCSUPPORT ;   // Richard Note: Color Depth

typedef union _LATENCY_SUPPORT{
    struct {
        BYTE Rsvd:6 ;
        BYTE I_Latency_Present:1 ;
        BYTE Latency_Present:1 ;
    } info ;
    BYTE uc ;
} LATENCY_SUPPORT ;

#define HDMI_IEEEOUI 0x0c03

typedef struct _RX_CAP{
    BYTE Valid; // richard add
    BYTE VideoMode ;
    BYTE VDOModeCount ;
    BYTE idxNativeVDOMode ;
    BYTE VDOMode[128] ;
    BYTE AUDDesCount ;
    AUDDESCRIPTOR AUDDes[32] ;
    ULONG IEEEOUI ;
    DCSUPPORT dc ;
    BYTE MaxTMDSClock ;
    LATENCY_SUPPORT lsupport ;
    BYTE V_Latency ;
    BYTE A_Latency ;
    BYTE V_I_Latency ;
    BYTE A_I_Latency ;
    SPK_ALLOC   SpeakerAllocBlk ;
    BYTE ValidCEA:1 ;
    BYTE ValidHDMI:1 ;
} RX_CAP ;

SYS_STATUS ParseVESAEDID(BYTE *pEDID) ;
SYS_STATUS ParseCEAEDID(BYTE *pCEAEDID, RX_CAP *pRxCap) ;



#endif // SUPPORT_EDID
#endif // _EDID_H_
