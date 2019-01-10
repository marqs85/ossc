//#include "terasic_includes.h"
//#include "mcu.h"
#include <unistd.h>
#include "it6613_sys.h"
#include "hdmitx.h"
#include "HDMI_TX.h"
#include "edid.h"


int TX_HDP = FALSE;
extern BYTE bOutputColorMode;
extern BYTE bInputColorMode;
extern BOOL bChangeMode;
extern RX_CAP RxCapability;
extern BOOL bHDMIMode;
extern BOOL bAudioEnable;
BOOL ParseEDID();

INSTANCE InitInstanceData = 
{
        0,0,  //I2C_DEV, I2C_ADDR
        0, //bIntType (TxCLK active, Push-Pull Mode, INT active low)
        0,/* | T_MODE_CCIR656 | T_MODE_SYNCEMB | T_MODE_INDDR */ // bInputVideoSignalType
        
        B_AUDFMT_STD_I2S, // bOutputAudioMode, 0x00, standard i2s, rising edge to sample ws/i2s, not full packet mode   REG[0xE1]
        
        0,// bAudioChannelSwap
        B_AUD_EN_I2S0 | B_AUD_I2S | M_AUD_16BIT, // bAudioChannelEnable, 0x01, REG[0xE0], defined in it6613_drv.h
        AUDFS_48KHz, //0, //bAudFs,
        0,  // TMDSClock
        TRUE,//bAuthenticated  
        TRUE,// bHDMIMode
        FALSE,// bIntPOL
        FALSE // bHPD
} ;

bool HDMITX_ChipVerify(void){
    bool bPass = FALSE;
    alt_u8 szID[4];
    int i;
    
    
    for(i=0;i<4;i++)
        szID[i] = HDMITX_ReadI2C_Byte(i);
        
//    if (szID[0] == 0x00 && szID[1] == 0xCA && szID[1] == 0x13 && szID[1] == 0x06) szID[0] ???
    if ((szID[1] == 0xCA && szID[2] == 0x13 && szID[3] == 0x06) || (szID[1] == 0xCA && szID[2] == 0x13 && szID[3] == 0x16)){
        bPass = TRUE;
        printf("TX Chip Revision ID: %d\n", szID[0]);     
    }else{
        printf("NG, Read TX Chip ID:%02X%02X%02X%02Xh (expected:00CA1306h)\n", szID[0], szID[1], szID[2], szID[3]);     
    }
                    
    return bPass;
}

bool HDMITX_Init(void){
    bool bSuccess = TRUE;
    HDMITX_Reset();
    usleep(500*1000);
    if (!HDMITX_ChipVerify()){
        OS_PRINTF("Failed to find IT6613 HDMI-TX Chip.\n");
        bSuccess = FALSE;
        //return 0;
    }    

    HDMITX_InitInstance(&InitInstanceData) ;
    InitIT6613() ;
    
    
    return bSuccess;  
}        

bool HDMITX_HPD(void){
    if (TX_HDP)
        return TRUE;
    return FALSE;        
}


void HDMITX_SetAVIInfoFrame(alt_u8 VIC, alt_u8 OutputColorMode, bool b16x9, bool ITU709, bool ITC, alt_u8 pixelrep)
{
    AVI_InfoFrame AviInfo;

    OS_PRINTF("HDMITX_SetAVIInfoFrame: VIC=%d, ColorMode=%d, Aspect-Ratio=%s, ITU709=%s, ITC=%s, pixelrep=%u\n",
        VIC, OutputColorMode, b16x9?"16:9":"4:3", ITU709?"Yes":"No", ITC?"Yes":"No", pixelrep);

    AviInfo.pktbyte.AVI_HB[0] = AVI_INFOFRAME_TYPE|0x80 ; 
    AviInfo.pktbyte.AVI_HB[1] = AVI_INFOFRAME_VER ; 
    AviInfo.pktbyte.AVI_HB[2] = AVI_INFOFRAME_LEN ; 
    
    switch(OutputColorMode)
    {
    case F_MODE_YUV444:
        // AviInfo.info.ColorMode = 2 ;
        AviInfo.pktbyte.AVI_DB[0] = (2<<5)|(1<<4) ;
        break ;
    case F_MODE_YUV422:
        // AviInfo.info.ColorMode = 1 ;
        AviInfo.pktbyte.AVI_DB[0] = (1<<5)|(1<<4) ;
        break ;
    case F_MODE_RGB444:
    default:
        // AviInfo.info.ColorMode = 0 ;
        AviInfo.pktbyte.AVI_DB[0] = (0<<5)|(1<<4) ;
        break ;
    }
    //AviInfo.pktbyte.AVI_DB[0] = (0<<5)|(1<<4) ;
    AviInfo.pktbyte.AVI_DB[0] |= 2; // indicate "no overscan"
    AviInfo.pktbyte.AVI_DB[1] = 8 ;
    //AviInfo.pktbyte.AVI_DB[1] |= (!b16x9)?(1<<4):(2<<4) ; // 4:3 or 16:9
    AviInfo.pktbyte.AVI_DB[1] |= (!ITU709)?(1<<6):(2<<6) ; // ITU709 or ITU601
    AviInfo.pktbyte.AVI_DB[2] = ((1<<3) | (ITC<<7)) ; // indicate "full-range RGB", setup ITC bit
    AviInfo.pktbyte.AVI_DB[3] = VIC ;
    AviInfo.pktbyte.AVI_DB[4] = pixelrep & 3 ;
    AviInfo.pktbyte.AVI_DB[5] = 0 ;
    AviInfo.pktbyte.AVI_DB[6] = 0 ;
    AviInfo.pktbyte.AVI_DB[7] = 0 ;
    AviInfo.pktbyte.AVI_DB[8] = 0 ;
    AviInfo.pktbyte.AVI_DB[9] = 0 ;
    AviInfo.pktbyte.AVI_DB[10] = 0 ;
    AviInfo.pktbyte.AVI_DB[11] = 0 ;
    AviInfo.pktbyte.AVI_DB[12] = 0 ;

    EnableAVIInfoFrame(TRUE, (unsigned char *)&AviInfo) ;
}

void HDMITX_ChangeVideoTiming(int VIC){
    int OutputVideoTiming = VIC;
    int HdmiColorMode;
    
    switch(bOutputColorMode)
    {
    case F_MODE_YUV444:
        HdmiColorMode =  HDMI_YUV444;
        break ;
    case F_MODE_YUV422:
        HdmiColorMode =  HDMI_YUV422;
        break ;
    case F_MODE_RGB444:
    default:
        HdmiColorMode =  HDMI_RGB444;
        break ;
    }    
    
    HDMITX_ChangeDisplayOption(OutputVideoTiming, HdmiColorMode); // just modify variable. Take effect when HDMITX_SetOutput is called in HDMITX_DevLoopProc   
}

void HDMITX_ChangeVideoTimingAndColor(int VIC, COLOR_TYPE Color){
    int OutputVideoTiming = VIC;
    int HdmiColorMode;
    
    switch(Color)
    {
    case COLOR_YUV444:
        HdmiColorMode =  HDMI_YUV444;
        break ;
    case COLOR_YUV422:
        HdmiColorMode =  HDMI_YUV422;
        break ;
    case COLOR_RGB444:
    default:
        HdmiColorMode =  HDMI_RGB444;
        break ;
    }     
    HDMITX_ChangeDisplayOption(OutputVideoTiming, HdmiColorMode);    
}

void HDMITX_DisableVideoOutput(void){
    DisableVideoOutput();        
}

void HDMITX_EnableVideoOutput(void){
    HDMITX_SetOutput();
}

void HDMITX_SetColorSpace(COLOR_TYPE InputColor, COLOR_TYPE OutputColor){
   // DisableVideoOutput();
    bInputColorMode = InputColor;
    bOutputColorMode = OutputColor;
   // HDMITX_SetOutput();
}

bool HDMITX_IsSinkSupportYUV444(void){
    bool bSupport = FALSE;
    if (RxCapability.Valid && RxCapability.ValidHDMI && RxCapability.ValidCEA && 
       (RxCapability.VideoMode & CEA_SUPPORT_YUV444))
        bSupport = TRUE;
    return bSupport;
}

bool HDMITX_IsSinkSupportYUV422(void){
    bool bSupport = FALSE;
    if (RxCapability.Valid && RxCapability.ValidHDMI && RxCapability.ValidCEA && 
       (RxCapability.VideoMode & CEA_SUPPORT_YUV422))
        bSupport = TRUE;
    return bSupport;
}

bool HDMITX_IsSinkSupportColorDepth36(void){
    bool bSupport = FALSE;
    if (RxCapability.Valid && RxCapability.ValidHDMI && RxCapability.ValidCEA && 
       RxCapability.dc.info.DC_36Bit)
        bSupport = TRUE;
    return bSupport;    
}


bool HDMITX_IsSinkSupportColorDepth30(void){
    bool bSupport = FALSE;
    if (RxCapability.Valid && RxCapability.ValidHDMI && RxCapability.ValidCEA && 
       RxCapability.dc.info.DC_30Bit)
        bSupport = TRUE;
    return bSupport;     
}

void HDMITX_SetOutputColorDepth(int ColorDepth){
    SetOutputColorDepthPhase(ColorDepth, 0);
}



bool HDMITX_DevLoopProc()
{
    static BYTE PreHPDChange = 0;
    static BYTE PreHPD = 0;
    BYTE HPD, HPDChange ;

    // Richard CheckHDMI(&HPD,&HPDChange) ;
    CheckHDMITX(&HPD,&HPDChange) ;
    
    if (HPD == PreHPD && HPDChange)  // richard add
        return FALSE;
        
    TX_HDP = HPD;    
    PreHPD = HPD;    
    PreHPDChange = HPDChange;    
    
    if( HPDChange )
    {
      
        
        OS_PRINTF("HPDChange\n");
        if( HPD )
        {
            OS_PRINTF("HPD=ON\n");
            RxCapability.Valid = ParseEDID() ;
            //bOutputColorMode = F_MODE_YUV444; //F_MODE_RGB444; // richard node. users can change color space here according to HDMI sink 
            
            if( RxCapability.Valid && RxCapability.ValidHDMI )
            {
                OS_PRINTF("HDMI Display found\n");
                bHDMIMode = TRUE ;
                
                if(RxCapability.VideoMode & (1<<6))
                {
                    bAudioEnable = TRUE ;
                }
                
#if 0    // richard, don't care edid, the output always RGB444                    
                if( RxCapability.VideoMode & (1<<5))
                {
                    bOutputColorMode &= ~F_MODE_CLRMOD_MASK ;
                    bOutputColorMode |= F_MODE_YUV444;
                }
                else if (RxCapability.VideoMode & (1<<4))
                {
                    bOutputColorMode &= ~F_MODE_CLRMOD_MASK ;
                    bOutputColorMode |= F_MODE_YUV422 ;
                }
#endif           
            }
            else if (!RxCapability.Valid)
            {
                OS_PRINTF("Failed to read EDID\n");
                
                // enable it when edid fail
                bHDMIMode = TRUE ;
                bAudioEnable = TRUE ;
            }     
            else    
            {
                OS_PRINTF("Invalid HDMI Display\n");
                bHDMIMode = FALSE ;
                bAudioEnable = FALSE ;
            }

            OS_PRINTF("HDMITX_SetOutput\n");
            //HDMITX_SetOutput() ;
            
        }
        else
        {
            OS_PRINTF("HPD=OFF\n");
            // unplug mode, ...
            OS_PRINTF("DisableVideoOutput\n");
            //DisableVideoOutput() ;
            RxCapability.Valid = FALSE; // richard add
            RxCapability.ValidHDMI = FALSE; // richard add
            RxCapability.ValidCEA = FALSE; // richard add
        }
    }
    else // no stable but need to process mode change procedure
    {
        if(bChangeMode && HPD)
        {
            OS_PRINTF("HDMITX_SetOutput\n");
            HDMITX_SetOutput() ;
        }
    }
    
    return HPDChange;
}

void HDMITX_SetAudioInfoFrame(BYTE bAudioDwSampling)
{
    int i;
    Audio_InfoFrame AudioInfo;

    AudioInfo.info.Type = AUDIO_INFOFRAME_TYPE;
    AudioInfo.info.Ver = AUDIO_INFOFRAME_VER;
    AudioInfo.info.Len = AUDIO_INFOFRAME_LEN;

    for (i=0; i<AUDIO_INFOFRAME_LEN; i++) {
        AudioInfo.pktbyte.AUD_DB[i] = 0;
    }

    AudioInfo.info.AudioChannelCount = 1; // 2 channels
    AudioInfo.info.AudioCodingType = 1; // PCM
    AudioInfo.info.SampleSize = 3; // 24bit
    AudioInfo.info.SampleFreq = bAudioDwSampling ? 3 : 5; //48kHz or 96kHz
    AudioInfo.info.SpeakerPlacement = 0; // Front left and front right
    AudioInfo.info.LevelShiftValue = 0;
    AudioInfo.info.DM_INH = 0; // Down-mix Inhibit Flag; 0=Permitted or no information about any assertion of this

    EnableAudioInfoFrame(TRUE, (BYTE *) &AudioInfo);
}
