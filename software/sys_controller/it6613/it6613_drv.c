/////////////////////////////////////////////////////////////////////
// IT6613.C
// Driver code for platform independent
/////////////////////////////////////////////////////////////////////
#include "hdmitx.h"
//#include "dss_sha.h"

#define MSCOUNT 1000
#define LOADING_UPDATE_TIMEOUT (3000/32)    // 3sec
// USHORT u8msTimer = 0 ;
// USHORT TimerServF = TRUE ;



//////////////////////////////////////////////////////////////////////
// Authentication status
//////////////////////////////////////////////////////////////////////

// #define TIMEOUT_WAIT_AUTH MS(2000)

#define Switch_HDMITX_Bank(x)   HDMITX_WriteI2C_Byte(0x0f,(x)&1)

#define HDMITX_OrREG_Byte(reg,ormask) HDMITX_WriteI2C_Byte(reg,(HDMITX_ReadI2C_Byte(reg) | (ormask)))
#define HDMITX_AndREG_Byte(reg,andmask) HDMITX_WriteI2C_Byte(reg,(HDMITX_ReadI2C_Byte(reg) & (andmask)))
#define HDMITX_SetREG_Byte(reg,andmask,ormask) HDMITX_WriteI2C_Byte(reg,((HDMITX_ReadI2C_Byte(reg) & (andmask))|(ormask)))

//////////////////////////////////////////////////////////////////////
// General global variables
//////////////////////////////////////////////////////////////////////
// static _IDATA TXVideo_State_Type VState ;
// static _IDATA TXAudio_State_Type AState ;
// static _XDATA MODE_DESCRIPTION ModeID = MODE_InvalidMode;

// BYTE I2C_DEV ;
// BYTE I2C_ADDR ;
//////////////////////////////////////////////////////////////////////
// /////////////////////////////////////////////////
// // Interrupt Type
// /////////////////////////////////////////////////
// BYTE bIntType = 0 ;
// /////////////////////////////////////////////////
// // Video Property
// /////////////////////////////////////////////////
// BYTE bInputVideoMode ;
// BYTE bOutputVideoMode ;
// BYTE Instance[0].bInputVideoSignalType = 0 /* | T_MODE_CCIR656 | T_MODE_SYNCEMB | T_MODE_INDDR */ ; // for Sync Embedded,CCIR656,InputDDR
// BOOL Instance[0].bAuthenticated = FALSE ;
// /////////////////////////////////////////////////
// // Video Property
// /////////////////////////////////////////////////
// BYTE bOutputAudioMode = 0 ;
// BYTE bAudioChannelSwap = 0 ;
//////////////////////////////////////////////////////////////////////
// BOOL Instance[0].bHDMIMode = FALSE;
// BOOL Instance[0].bIntPOL = FALSE ; // 0 = Low Active
// BOOL bHPD ;

INSTANCE Instance[HDMITX_INSTANCE_MAX] ;

//////////////////////////////////////////////////////////////////////
// Function Prototype
//////////////////////////////////////////////////////////////////////

// static BOOL IsRxSense() ;

static void SetInputMode(BYTE InputMode,BYTE bInputSignalType) ;
static void SetCSCScale(BYTE bInputMode,BYTE bOutputMode) ;
// static void SetupAFE(BYTE ucFreqInMHz) ;
static void SetupAFE(VIDEOPCLKLEVEL PCLKLevel) ;
static void FireAFE() ;


static SYS_STATUS SetAudioFormat(BYTE NumChannel,BYTE AudioEnable,BYTE bSampleFreq,BYTE AudSWL,BYTE AudioCatCode) ;
static SYS_STATUS SetNCTS(ULONG PCLK,ULONG Fs) ;

static void AutoAdjustAudio() ;
static void SetupAudioChannel() ;

static SYS_STATUS SetAVIInfoFrame(AVI_InfoFrame *pAVIInfoFrame) ;
static SYS_STATUS SetAudioInfoFrame(Audio_InfoFrame *pAudioInfoFrame) ;
static SYS_STATUS SetSPDInfoFrame(SPD_InfoFrame *pSPDInfoFrame) ;
static SYS_STATUS SetMPEGInfoFrame(MPEG_InfoFrame *pMPGInfoFrame) ;
static SYS_STATUS SetHDRInfoFrame(HDR_InfoFrame *pHDRInfoFrame) ;
static SYS_STATUS ReadEDID(BYTE *pData,BYTE bSegment,BYTE offset,SHORT Count) ;
static void AbortDDC() ;
static void ClearDDCFIFO() ;
static void ClearDDCFIFO() ;
static void GenerateDDCSCLK() ;
static SYS_STATUS HDCP_EnableEncryption() ;
static void HDCP_ResetAuth() ;
static void HDCP_Auth_Fire() ;
static void HDCP_StartAnCipher() ;
static void HDCP_StopAnCipher() ;
static void HDCP_GenerateAn() ;
// RICHARD static SYS_STATUS HDCP_GetVr(ULONG *pVr) ;
static SYS_STATUS HDCP_GetBCaps(PBYTE pBCaps ,PUSHORT pBStatus) ;
static SYS_STATUS HDCP_GetBKSV(BYTE *pBKSV) ;
static SYS_STATUS HDCP_Authenticate() ;
static SYS_STATUS HDCP_Authenticate_Repeater() ;
static SYS_STATUS HDCP_VerifyIntegration() ;
static SYS_STATUS HDCP_GetKSVList(BYTE *pKSVList,BYTE cDownStream) ;
static SYS_STATUS HDCP_CheckSHA(BYTE M0[],USHORT BStatus,BYTE KSVList[],int devno,BYTE Vr[]) ;
static void HDCP_ResumeAuthentication() ;
static void HDCP_Reset() ;


static void ENABLE_NULL_PKT() ;
static void ENABLE_ACP_PKT() ;
static void ENABLE_ISRC1_PKT() ;
static void ENABLE_ISRC2_PKT() ;
static void ENABLE_AVI_INFOFRM_PKT() ;
static void ENABLE_AUD_INFOFRM_PKT() ;
static void ENABLE_SPD_INFOFRM_PKT() ;
static void ENABLE_MPG_INFOFRM_PKT() ;

static void DISABLE_NULL_PKT() ;
static void DISABLE_ACP_PKT() ;
static void DISABLE_ISRC1_PKT() ;
static void DISABLE_ISRC2_PKT() ;
static void DISABLE_AVI_INFOFRM_PKT() ;
static void DISABLE_AUD_INFOFRM_PKT() ;
static void DISABLE_SPD_INFOFRM_PKT() ;
static void DISABLE_MPG_INFOFRM_PKT() ;
static BYTE countbit(BYTE b) ;

#ifdef HDMITX_REG_DEBUG
static void DumpCatHDMITXReg() ;
#endif // DEBUG





//////////////////////////////////////////////////////////////////////
// Function Body.
//////////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////
// utility function for main..
//////////////////////////////////////////////////////////////////////



// int aiNonCEAVIC[] = { 2 } ;

// Y,C,RGB offset
// for register 73~75
static _CODE BYTE bCSCOffset_16_235[] =
{
    0x00,0x80,0x00
};

static _CODE BYTE bCSCOffset_0_255[] =
{
    0x10,0x80,0x10
};

#ifdef SUPPORT_INPUTRGB
    static _CODE BYTE bCSCMtx_RGB2YUV_ITU601_16_235[] =
    {
        0xB2,0x04,0x64,0x02,0xE9,0x00,
        0x93,0x3C,0x16,0x04,0x56,0x3F,
        0x49,0x3D,0x9F,0x3E,0x16,0x04
    } ;

    static _CODE BYTE bCSCMtx_RGB2YUV_ITU601_0_255[] =
    {
        0x09,0x04,0x0E,0x02,0xC8,0x00,
        0x0E,0x3D,0x83,0x03,0x6E,0x3F,
        0xAC,0x3D,0xD0,0x3E,0x83,0x03
    } ;

    static _CODE BYTE bCSCMtx_RGB2YUV_ITU709_16_235[] =
    {
        0xB8,0x05,0xB4,0x01,0x93,0x00,
        0x49,0x3C,0x16,0x04,0x9F,0x3F,
        0xD9,0x3C,0x10,0x3F,0x16,0x04
    } ;

    static _CODE BYTE bCSCMtx_RGB2YUV_ITU709_0_255[] =
    {
        0xE5,0x04,0x78,0x01,0x81,0x00,
        0xCE,0x3C,0x83,0x03,0xAE,0x3F,
        0x49,0x3D,0x33,0x3F,0x83,0x03
    } ;
#endif
/*
#ifdef SUPPORT_INPUTYUV

    static _CODE BYTE bCSCMtx_YUV2RGB_ITU601_16_235[] =
    {
        0x00,0x08,0x6A,0x3A,0x4F,0x3D,
        0x00,0x08,0xF7,0x0A,0x00,0x00,
        0x00,0x08,0x00,0x00,0xDB,0x0D
    } ;

    static _CODE BYTE bCSCMtx_YUV2RGB_ITU601_0_255[] =
    {
        0x4F,0x09,0x81,0x39,0xDF,0x3C,
        0x4F,0x09,0xC2,0x0C,0x00,0x00,
        0x4F,0x09,0x00,0x00,0x1E,0x10
    } ;

    static _CODE BYTE bCSCMtx_YUV2RGB_ITU709_16_235[] =
    {
        0x00,0x08,0x53,0x3C,0x89,0x3E,
        0x00,0x08,0x51,0x0C,0x00,0x00,
        0x00,0x08,0x00,0x00,0x87,0x0E
    } ;

    static _CODE BYTE bCSCMtx_YUV2RGB_ITU709_0_255[] =
    {
        0x4F,0x09,0xBA,0x3B,0x4B,0x3E,
        0x4F,0x09,0x56,0x0E,0x00,0x00,
        0x4F,0x09,0x00,0x00,0xE7,0x10
    } ;
#endif*/

#ifdef SUPPORT_INPUTYUV

     BYTE bCSCMtx_YUV2RGB_ITU601_16_235[] =
    {
        0x00,0x08,0x6A,0x3A,0x4F,0x3D,
        0x00,0x08,0xF7,0x0A,0x00,0x00,
        0x00,0x08,0x00,0x00,0xDB,0x0D
    } ;

     BYTE bCSCMtx_YUV2RGB_ITU601_0_255[] =
    {
        0x4F,0x09,0x81,0x39,0xDF,0x3C,
        0x4F,0x09,0xC2,0x0C,0x00,0x00,
        0x4F,0x09,0x00,0x00,0x1E,0x10
    } ;

     BYTE bCSCMtx_YUV2RGB_ITU709_16_235[] =
    {
        0x00,0x08,0x53,0x3C,0x89,0x3E,
        0x00,0x08,0x51,0x0C,0x00,0x00,
        0x00,0x08,0x00,0x00,0x87,0x0E
    } ;

     BYTE bCSCMtx_YUV2RGB_ITU709_0_255[] =
    {
        0x4F,0x09,0xBA,0x3B,0x4B,0x3E,
        0x4F,0x09,0x56,0x0E,0x00,0x00,
        0x4F,0x09,0x00,0x00,0xE7,0x10
    } ;
#endif



//////////////////////////////////////////////////////////////////////
// external Interface                                                         //
//////////////////////////////////////////////////////////////////////

void
HDMITX_InitInstance(INSTANCE *pInstance)
{
	if(pInstance && 0 < HDMITX_INSTANCE_MAX)
	{
		Instance[0] = *pInstance ;
	}
}

static BYTE InitIT6613_HDCPROM()
{
    BYTE uc[5]  ;
    Switch_HDMITX_Bank(0) ;
    HDMITX_WriteI2C_Byte(0xF8,0xC3) ;	//password
    HDMITX_WriteI2C_Byte(0xF8,0xA5) ;	// password
    HDMITX_WriteI2C_Byte(REG_TX_LISTCTRL,0x60) ; // Richard, ????
    I2C_Read_ByteN(0xE0,0x00,uc,5) ;  // richard note. internal rom is used

    if(uc[0] == 1 &&
        uc[1] == 1 &&
        uc[2] == 1 &&
        uc[3] == 1 &&
        uc[4] == 1)
    {
        // with internal eMem
        HDMITX_WriteI2C_Byte(REG_TX_ROM_HEADER,0xE0) ;
        HDMITX_WriteI2C_Byte(REG_TX_LISTCTRL,0x48) ;
    }
    else
    {
        // with external ROM
        HDMITX_WriteI2C_Byte(REG_TX_ROM_HEADER,0xA0) ;  // ROMHeader
        HDMITX_WriteI2C_Byte(REG_TX_LISTCTRL,0x00) ; // Richard, ????
    }
    HDMITX_WriteI2C_Byte(0xF8,0xFF) ;  // password

    // richard add
    return ER_SUCCESS;
}

void InitIT6613()
{
	// config interrupt
    HDMITX_WriteI2C_Byte(REG_TX_INT_CTRL,Instance[0].bIntType) ;
    Instance[0].bIntPOL = (Instance[0].bIntType&B_INTPOL_ACTH)?TRUE:FALSE ;

	// Reset
    HDMITX_WriteI2C_Byte(REG_TX_SW_RST,B_REF_RST|B_VID_RST|B_AUD_RST|B_AREF_RST|B_HDCP_RST) ;
    DelayMS(1) ;
    HDMITX_WriteI2C_Byte(REG_TX_SW_RST,B_VID_RST|B_AUD_RST|B_AREF_RST|B_HDCP_RST) ;

#if 0
    // Enable clock ring (richard add according toe programming guide)
    HDMITX_WriteI2C_Byte(REG_TX_AFE_DRV_CTRL, 0x10);

    // Set default DVI mode (richard add according toe programming guide)
//    HDMITX_WriteI2C_Byte(REG_TX_HDMI_MODE, 0x01);  // set HDMI mode
    HDMITX_WriteI2C_Byte(REG_TX_HDMI_MODE, 0x00);  // set DVI mode
#endif

    // Avoid power loading in un play status.
    HDMITX_WriteI2C_Byte(REG_TX_AFE_DRV_CTRL,B_AFE_DRV_RST|B_AFE_DRV_PWD) ;

    // set interrupt mask,mask value 0 is interrupt available.
// richard    HDMITX_WriteI2C_Byte(REG_TX_INT_MASK1,0xB2) ;  // enable interrupt: HPD, DDCBusHangMask,
    HDMITX_WriteI2C_Byte(REG_TX_INT_MASK1,0xB2) ;  // enable interrupt: HPD, DDCBusHangMask,
    HDMITX_WriteI2C_Byte(REG_TX_INT_MASK2,0xF8) ;  // enable interrupt: AuthFailMask, AUthDoneMask, KSVListChkMask
    HDMITX_WriteI2C_Byte(REG_TX_INT_MASK3,0x37) ; //  enable interrupt: PktAudMask, PktDBDMask, PkMpgMask, AUdCTSMask, HDCPSynDetMask

    Switch_HDMITX_Bank(0) ;
    DISABLE_NULL_PKT() ;
    DISABLE_ACP_PKT() ;
    DISABLE_ISRC1_PKT() ;
    DISABLE_ISRC2_PKT() ;
    DISABLE_AVI_INFOFRM_PKT() ;
    DISABLE_AUD_INFOFRM_PKT() ;
    DISABLE_SPD_INFOFRM_PKT() ;
    DISABLE_MPG_INFOFRM_PKT();


	//////////////////////////////////////////////////////////////////
	// Setup Output Audio format.
	//////////////////////////////////////////////////////////////////
    HDMITX_WriteI2C_Byte(REG_TX_AUDIO_CTRL1,Instance[0].bOutputAudioMode) ; // regE1 bOutputAudioMode should be loaded from ROM image.

	//////////////////////////////////////////////////////////////////
	// Setup HDCP ROM
	//////////////////////////////////////////////////////////////////
#ifdef SUPPORT_HDCP
	InitIT6613_HDCPROM() ;
#endif
// #ifdef EXTERN_HDCPROM
// #pragma message("EXTERN ROM CODED") ;
// 	HDMITX_WriteI2C_Byte(REG_TX_ROM_HEADER,0xA0) ;
// #endif


}

//////////////////////////////////////////////////////////////////////
// export this for dynamic change input signal
//////////////////////////////////////////////////////////////////////
BOOL SetupVideoInputSignal(BYTE inputSignalType)
{
	Instance[0].bInputVideoSignalType = inputSignalType ;
    // SetInputMode(inputColorMode,Instance[0].bInputVideoSignalType) ;
    return TRUE ;
}

BOOL EnableVideoOutput(VIDEOPCLKLEVEL level,BYTE inputColorMode,BYTE outputColorMode,BYTE bHDMI)
{
    // bInputVideoMode,bOutputVideoMode,Instance[0].bInputVideoSignalType,bAudioInputType,should be configured by upper F/W or loaded from EEPROM.
    // should be configured by initsys.c
    WORD i ;
    BYTE uc ;
    // VIDEOPCLKLEVEL level ;

    HDMITX_WriteI2C_Byte(REG_TX_SW_RST,B_VID_RST|B_AUD_RST|B_AREF_RST|B_HDCP_RST) ;

    Instance[0].bHDMIMode = (BYTE)bHDMI ;

    if(Instance[0].bHDMIMode)
    {
        SetAVMute(TRUE) ;
    }

    SetInputMode(inputColorMode,Instance[0].bInputVideoSignalType) ;

    SetCSCScale(inputColorMode,outputColorMode) ;

    if(Instance[0].bHDMIMode)
    {
        HDMITX_WriteI2C_Byte(REG_TX_HDMI_MODE,B_TX_HDMI_MODE) ;
    }
    else
    {
        HDMITX_WriteI2C_Byte(REG_TX_HDMI_MODE,B_TX_DVI_MODE) ;
    }

#ifdef INVERT_VID_LATCHEDGE
    uc = HDMITX_ReadI2C_Byte(REG_TX_CLK_CTRL1) ;
    uc |= B_VDO_LATCH_EDGE ;
    HDMITX_WriteI2C_Byte(REG_TX_CLK_CTRL1, uc) ;
#endif

    HDMITX_WriteI2C_Byte(REG_TX_SW_RST,          B_AUD_RST|B_AREF_RST|B_HDCP_RST) ;

    // if (pVTiming->VideoPixelClock>80000000)
    // {
    //     level = PCLK_HIGH ;
    // }
    // else if (pVTiming->VideoPixelClock>20000000)
    // {
    //     level = PCLK_MEDIUM ;
    // }
    // else
    // {
    //     level = PCLK_LOW ;
    // }

    SetupAFE(level) ; // pass if High Freq request

    for(i = 0 ; i < 100 ; i++)
    {
        if(HDMITX_ReadI2C_Byte(REG_TX_SYS_STATUS) & B_TXVIDSTABLE)
        {
            break ;

        }
        DelayMS(1) ;
    }
    // Clive suggestion.
    // clear int3 video stable interrupt.
    HDMITX_WriteI2C_Byte(REG_TX_INT_CLR0,0) ;
    HDMITX_WriteI2C_Byte(REG_TX_INT_CLR1,B_CLR_VIDSTABLE) ;
    HDMITX_WriteI2C_Byte(REG_TX_SYS_STATUS,B_INTACTDONE) ;
    HDMITX_WriteI2C_Byte(REG_TX_SYS_STATUS,0) ;

    FireAFE() ;
	return TRUE ;
}

BOOL EnableAudioOutput(ULONG VideoPixelClock,BYTE bAudioSampleFreq,BYTE ChannelNumber,BYTE bAudSWL,BYTE bSPDIF)
{
    BYTE bAudioChannelEnable ;
  // richard   unsigned long N ;

    Instance[0].TMDSClock = VideoPixelClock ;
    Instance[0].bAudFs = bAudioSampleFreq ;

    ErrorF("EnableAudioOutput(%d,%ld,%x,%d,%d,%d);\n",0,VideoPixelClock,bAudioSampleFreq,ChannelNumber,bAudSWL,bSPDIF) ;

    switch(ChannelNumber)
    {
    case 7:
    case 8:
        bAudioChannelEnable = 0xF ;
        break ;
    case 6:
    case 5:
        bAudioChannelEnable = 0x7 ;
        break ;
    case 4:
    case 3:
        bAudioChannelEnable = 0x3 ;
        break ;
    case 2:
    case 1:
    default:
        bAudioChannelEnable = 0x1 ;
        break ;
    }

    if(bSPDIF) bAudioChannelEnable |= B_AUD_SPDIF ;

    if( bSPDIF )
    {
        Switch_HDMITX_Bank(1) ;
        HDMITX_WriteI2C_Byte(REGPktAudCTS0,0x50) ;
        HDMITX_WriteI2C_Byte(REGPktAudCTS1,0x73) ;
        HDMITX_WriteI2C_Byte(REGPktAudCTS2,0x00) ;

        HDMITX_WriteI2C_Byte(REGPktAudN0,0) ;
        HDMITX_WriteI2C_Byte(REGPktAudN1,0x18) ;
        HDMITX_WriteI2C_Byte(REGPktAudN2,0) ;
        Switch_HDMITX_Bank(0) ;

        HDMITX_WriteI2C_Byte(0xC5, 2) ; // D[1] = 0, HW auto count CTS
    }
    else
    {
        SetNCTS(VideoPixelClock,bAudioSampleFreq) ;
    }

    /*
    if(VideoPixelClock != 0)
    {
        SetNCTS(VideoPixelClock,bAudioSampleFreq) ;
    }
    else
    {
        switch(bAudioSampleFreq)
        {
		case AUDFS_32KHz: N = 4096; break;
		case AUDFS_44p1KHz: N = 6272; break;
		case AUDFS_48KHz: N = 6144; break;
		case AUDFS_88p2KHz: N = 12544; break;
		case AUDFS_96KHz: N = 12288; break;
		case AUDFS_176p4KHz: N = 25088; break;
		case AUDFS_192KHz: N = 24576; break;
		default: N = 6144;
        }
        Switch_HDMITX_Bank(1) ;
        HDMITX_WriteI2C_Byte(REGPktAudN0,(BYTE)((N)&0xFF)) ;
        HDMITX_WriteI2C_Byte(REGPktAudN1,(BYTE)((N>>8)&0xFF)) ;
        HDMITX_WriteI2C_Byte(REGPktAudN2,(BYTE)((N>>16)&0xF)) ;
        Switch_HDMITX_Bank(0) ;
        HDMITX_WriteI2C_Byte(REG_TX_PKT_SINGLE_CTRL,0) ; // D[1] = 0,HW auto count CTS
    }
    */

	//HDMITX_AndREG_Byte(REG_TX_SW_RST,~(B_AUD_RST|B_AREF_RST)) ;
    SetAudioFormat(ChannelNumber,bAudioChannelEnable,bAudioSampleFreq,bAudSWL,bSPDIF) ;

    #ifdef HDMITX_REG_DEBUG
    DumpCatHDMITXReg() ;
    #endif // HDMITX_REG_DEBUG
    return TRUE ;
}

BOOL EnableAudioOutput4OSSC(ULONG VideoPixelClock,BYTE bAudioDwSampl,BYTE bAudioSwapLR)
{
    // set N and CTS
    ULONG n = 12288;
    ULONG cts = VideoPixelClock/1000;

    if (bAudioDwSampl == 0x1)
        n = n>>1;

    //program N
    Switch_HDMITX_Bank(1);
    HDMITX_WriteI2C_Byte(REGPktAudN0,(BYTE)((n)&0xFF));
    HDMITX_WriteI2C_Byte(REGPktAudN1,(BYTE)((n>>8)&0xFF));
    HDMITX_WriteI2C_Byte(REGPktAudN2,(BYTE)((n>>16)&0xF));
    //program CTS
    HDMITX_WriteI2C_Byte(REGPktAudCTS0, cts & 0xff);
    HDMITX_WriteI2C_Byte(REGPktAudCTS1,(cts>>8) & 0xff);
    HDMITX_WriteI2C_Byte(REGPktAudCTS2,(cts>>16) & 0xff);
    Switch_HDMITX_Bank(0);

#ifdef MANUAL_CTS
    HDMITX_WriteI2C_Byte(0xF8, 0xC3);
    HDMITX_WriteI2C_Byte(0xF8, 0xA5);
    HDMITX_WriteI2C_Byte(REG_TX_PKT_SINGLE_CTRL,B_SW_CTS);
    HDMITX_WriteI2C_Byte(0xF8, 0xFF);
#else
    HDMITX_WriteI2C_Byte(REG_TX_PKT_SINGLE_CTRL,0); // D[1] = 0,HW auto count CTS
#endif


    // set audio format
    Instance[0].TMDSClock = VideoPixelClock;
    BYTE fs = bAudioDwSampl == 0x1 ? AUDFS_48KHz : AUDFS_96KHz;
    Instance[0].bAudFs = fs;
    Instance[0].bOutputAudioMode = B_AUDFMT_32BIT_I2S;
    Instance[0].bAudioChannelSwap = bAudioSwapLR == 0x1 ? 0xf : 0x0; // swap channels
    BYTE AudioEnable = (0x1 & ~(M_AUD_SWL|B_SPDIFTC)) | M_AUD_24BIT;

    HDMITX_WriteI2C_Byte(REG_TX_AUDIO_CTRL0,AudioEnable & 0xF0);

    if (bAudioDwSampl == 0x1)
        HDMITX_SetREG_Byte(REG_TX_CLK_CTRL1,~M_AUD_DIV,B_AUD_DIV2);
    else
        HDMITX_SetREG_Byte(REG_TX_CLK_CTRL1,~M_AUD_DIV,B_AUD_NODIV);

    HDMITX_AndREG_Byte(REG_TX_SW_RST,~(B_AUD_RST|B_AREF_RST));
    HDMITX_WriteI2C_Byte(REG_TX_AUDIO_CTRL1,Instance[0].bOutputAudioMode);
    HDMITX_WriteI2C_Byte(REG_TX_AUDIO_FIFOMAP,0xE4); // default mapping.
    HDMITX_WriteI2C_Byte(REG_TX_AUDIO_CTRL3,(Instance[0].bAudioChannelSwap&0xF)|(AudioEnable&B_AUD_SPDIF));
    HDMITX_WriteI2C_Byte(REG_TX_AUD_SRCVALID_FLAT,B_AUD_ERR2FLAT); // only two channels

    Switch_HDMITX_Bank(1) ;
    HDMITX_WriteI2C_Byte(REG_TX_AUDCHST_MODE,(1<<3)); // 2 audio channel without pre-emphasis, no copyright
    HDMITX_WriteI2C_Byte(REG_TX_AUDCHST_CAT,0);
    HDMITX_WriteI2C_Byte(REG_TX_AUDCHST_SRCNUM,1);
    HDMITX_WriteI2C_Byte(REG_TX_AUD0CHST_CHTNUM,0);
    HDMITX_WriteI2C_Byte(REG_TX_AUDCHST_CA_FS,0x00|fs); // choose clock
    fs = ~fs; // OFS is the one's complement of FS
    HDMITX_WriteI2C_Byte(REG_TX_AUDCHST_OFS_WL,(fs<<4)|AUD_SWL_24); // 24 bit Audio

    Switch_HDMITX_Bank(0);
    HDMITX_WriteI2C_Byte(REG_TX_AUDIO_CTRL0,AudioEnable);

    Instance[0].bAudioChannelEnable = AudioEnable;

    return TRUE;
}

BOOL
GetEDIDData(int EDIDBlockID,BYTE *pEDIDData)
{
	if(!pEDIDData)
	{
		return FALSE ;
	}

    if(ReadEDID(pEDIDData,EDIDBlockID/2,(EDIDBlockID%2)*128,128) == ER_FAIL)
    {
        return FALSE ;
    }

    return TRUE ;
}


BOOL
EnableHDCP(BYTE bEnable)
{
    if(bEnable)
    {
        if(ER_FAIL == HDCP_Authenticate())
        {

            HDCP_ResetAuth() ;
			return FALSE ;
        }

    }
    else
    {
        HDCP_ResetAuth() ;
    }
    return TRUE ;
}


BOOL
CheckHDMITX(BYTE *pHPD,BYTE *pHPDChange)
{
    BYTE intdata1,intdata2,intdata3,sysstat;
    BYTE  intclr3 = 0 ;
    BOOL PrevHPD = Instance[0].bHPD ;
    BOOL HPD ;

    sysstat = HDMITX_ReadI2C_Byte(REG_TX_SYS_STATUS) ;  // read system status register

  //  OS_PRINTF("sysstat(REG[0x0E])=%02Xh\r\n", sysstat);

    HPD = ((sysstat & (B_HPDETECT|B_RXSENDETECT)) == (B_HPDETECT|B_RXSENDETECT))?TRUE:FALSE ;

    // 2007/06/20 added by jj_tseng@chipadvanced.com
    if(pHPDChange)
    {
    	*pHPDChange = FALSE ;

    }
    //~jj_tseng@chipadvanced.com 2007/06/20

    if(!HPD)
    {
        Instance[0].bAuthenticated = FALSE ;
    }

    if(sysstat & B_INT_ACTIVE)  // interrupt is activce
    {

        intdata1 = HDMITX_ReadI2C_Byte(REG_TX_INT_STAT1) ;  // reg 0x06
        //ErrorF("INT_Handler: reg%02x = %02x\n",REG_TX_INT_STAT1,intdata1) ;

		if(intdata1 & B_INT_DDCFIFO_ERR)
		{
		    //ErrorF("DDC FIFO Error.\n") ;
		    ClearDDCFIFO() ;
		}


		if(intdata1 & B_INT_DDC_BUS_HANG)
		{
		    ErrorF("DDC BUS HANG.\n") ;
            AbortDDC() ;

            if(Instance[0].bAuthenticated)
            {
                ErrorF("when DDC hang,and aborted DDC,the HDCP authentication need to restart.\n") ;
                HDCP_ResumeAuthentication() ;
            }
		}


		if(intdata1 & (B_INT_HPD_PLUG|B_INT_RX_SENSE))
		{

            if(pHPDChange) *pHPDChange = TRUE ;

            if(!HPD)
            {
                // reset
                HDMITX_WriteI2C_Byte(REG_TX_SW_RST,B_AREF_RST|B_VID_RST|B_AUD_RST|B_HDCP_RST) ;
                DelayMS(1) ;
                HDMITX_WriteI2C_Byte(REG_TX_AFE_DRV_CTRL,B_AFE_DRV_RST|B_AFE_DRV_PWD) ;
                //ErrorF("Unplug,%x %x\n",HDMITX_ReadI2C_Byte(REG_TX_SW_RST),HDMITX_ReadI2C_Byte(REG_TX_AFE_DRV_CTRL)) ;
                // VState = TXVSTATE_Unplug ;
            }
		}


        intdata2 = HDMITX_ReadI2C_Byte(REG_TX_INT_STAT2) ;  // reg 0x07
        //ErrorF("INT_Handler: reg%02x = %02x\n",REG_TX_INT_STAT2,intdata2) ;



		#ifdef SUPPORT_HDCP
		if(intdata2 & B_INT_AUTH_DONE)
		{
            ErrorF("interrupt Authenticate Done.\n") ;
            HDMITX_OrREG_Byte(REG_TX_INT_MASK2,B_T_AUTH_DONE_MASK) ;
            Instance[0].bAuthenticated = TRUE ;
            SetAVMute(FALSE) ;
		}

		if(intdata2 & B_INT_AUTH_FAIL)
		{
            ErrorF("interrupt Authenticate Fail.\n") ;
			AbortDDC();   // @emily add
            HDCP_ResumeAuthentication() ;
        }
        #endif // SUPPORT_HDCP

		intdata3 = HDMITX_ReadI2C_Byte(REG_TX_INT_STAT3) ;  // reg 0x08
		if(intdata3 & B_INT_VIDSTABLE)
		{
			sysstat = HDMITX_ReadI2C_Byte(REG_TX_SYS_STATUS) ;
			if(sysstat & B_TXVIDSTABLE)
			{
				FireAFE() ;
			}
		}
        HDMITX_WriteI2C_Byte(REG_TX_INT_CLR0,0xFF) ;
        HDMITX_WriteI2C_Byte(REG_TX_INT_CLR1,0xFF) ;
        intclr3 = (HDMITX_ReadI2C_Byte(REG_TX_SYS_STATUS))|B_CLR_AUD_CTS | B_INTACTDONE ;
        HDMITX_WriteI2C_Byte(REG_TX_SYS_STATUS,intclr3) ; // clear interrupt.
        intclr3 &= ~(B_INTACTDONE) ;
        HDMITX_WriteI2C_Byte(REG_TX_SYS_STATUS,intclr3) ; // INTACTDONE reset to zero.
    }
    else
    {
        if(pHPDChange)
        {
            *pHPDChange = (HPD != PrevHPD)?TRUE:FALSE ;

            if(*pHPDChange &&(!HPD))
            {
                HDMITX_WriteI2C_Byte(REG_TX_AFE_DRV_CTRL,B_AFE_DRV_RST|B_AFE_DRV_PWD) ;
            }
        }
    }

    SetupAudioChannel() ; // 2007/12/12 added by jj_tseng

    if(pHPD)
    {
        *pHPD = HPD ? TRUE:FALSE ;
    }

    Instance[0].bHPD  = (BYTE)HPD ;
    return HPD ;
}

void
DisableIT6613()
{
    HDMITX_WriteI2C_Byte(REG_TX_SW_RST,B_AREF_RST|B_VID_RST|B_AUD_RST|B_HDCP_RST) ;
    DelayMS(1) ;
    HDMITX_WriteI2C_Byte(REG_TX_AFE_DRV_CTRL,B_AFE_DRV_RST|B_AFE_DRV_PWD) ;
}

void
DisableVideoOutput()
{
	BYTE uc = HDMITX_ReadI2C_Byte(REG_TX_SW_RST) | B_VID_RST ;
	HDMITX_WriteI2C_Byte(REG_TX_SW_RST,uc) ;
    HDMITX_WriteI2C_Byte(REG_TX_AFE_DRV_CTRL,B_AFE_DRV_RST|B_AFE_DRV_PWD) ;
}


void
DisableAudioOutput()
{
	BYTE uc = HDMITX_ReadI2C_Byte(REG_TX_SW_RST) | B_AUD_RST ;
	HDMITX_WriteI2C_Byte(REG_TX_SW_RST,uc) ;
}



BOOL
EnableAVIInfoFrame(BYTE bEnable,BYTE *pAVIInfoFrame)
{
    if(!bEnable)
    {
        DISABLE_AVI_INFOFRM_PKT() ;
        return TRUE ;
    }

    if(SetAVIInfoFrame((AVI_InfoFrame *)pAVIInfoFrame) == ER_SUCCESS)
    {
        return TRUE ;
    }

    return FALSE ;
}

BOOL
EnableAudioInfoFrame(BYTE bEnable,BYTE *pAudioInfoFrame)
{
    if(!bEnable)
    {
        // richard modify, DISABLE_AVI_INFOFRM_PKT() ;
        DISABLE_AUD_INFOFRM_PKT();
        return TRUE ;
    }


    if(SetAudioInfoFrame((Audio_InfoFrame *)pAudioInfoFrame) == ER_SUCCESS)
    {
        return TRUE ;
    }

    return FALSE ;
}

BOOL
EnableHDRInfoFrame(BYTE bEnable, BYTE *pHDRInfoFrame)
{
    if (!bEnable) {
        DISABLE_NULL_PKT();
        return TRUE;
    }

    if(SetHDRInfoFrame((HDR_InfoFrame *)pHDRInfoFrame) == ER_SUCCESS)
    {
        return TRUE;
    }

    return FALSE ;
}

void
SetAVMute(BYTE bEnable)
{
    BYTE uc ;

    Switch_HDMITX_Bank(0) ;
    uc = HDMITX_ReadI2C_Byte(REG_TX_GCP) ;
    uc &= ~B_TX_SETAVMUTE ;
    uc |= bEnable?B_TX_SETAVMUTE:0 ;
    HDMITX_WriteI2C_Byte(REG_TX_GCP,uc) ;
    HDMITX_WriteI2C_Byte(REG_TX_PKT_GENERAL_CTRL,B_ENABLE_PKT|B_REPEAT_PKT) ;
}

void
SetOutputColorDepthPhase(BYTE ColorDepth,BYTE bPhase)
{
    BYTE uc ;
    BYTE bColorDepth ;

    if(ColorDepth == 30)
    {
        bColorDepth = B_CD_30 ;
    }
    else if (ColorDepth == 36)
    {
        bColorDepth = B_CD_36 ;
    }
    else if (ColorDepth == 24)
    {
        bColorDepth = B_CD_24 ;
    }
    else
    {
        bColorDepth = 0 ; // not indicated
    }

    Switch_HDMITX_Bank(0) ;
    uc = HDMITX_ReadI2C_Byte(REG_TX_GCP) ;
    uc &= ~B_COLOR_DEPTH_MASK ;
    uc |= bColorDepth&B_COLOR_DEPTH_MASK;
    HDMITX_WriteI2C_Byte(REG_TX_GCP,uc) ;
}

void
Get6613Reg(BYTE *pReg)
{
	int i ;
	BYTE reg ;
    Switch_HDMITX_Bank(0) ;
	for(i = 0 ; i < 0x100 ; i++)
	{
		reg = i & 0xFF ;
		pReg[i] = HDMITX_ReadI2C_Byte(reg) ;
	}
    Switch_HDMITX_Bank(1) ;
	for(reg = 0x30 ; reg < 0xB0 ; i++,reg++)
	{
		pReg[i] = HDMITX_ReadI2C_Byte(reg) ;
	}
    Switch_HDMITX_Bank(0) ;

}
//////////////////////////////////////////////////////////////////////
// SubProcedure process                                                       //
//////////////////////////////////////////////////////////////////////
#ifdef SUPPORT_DEGEN

typedef struct {
    MODE_ID id ;
    BYTE Reg90;
    BYTE Reg92;
    BYTE Reg93;
    BYTE Reg94;
    BYTE Reg9A;
    BYTE Reg9B;
    BYTE Reg9C;
    BYTE Reg9D;
    BYTE Reg9E;
    BYTE Reg9F;
} DEGEN_Setting ;


static _CODE DEGEN_Setting DeGen_Table[] = {
    {CEA_640x480p60      ,0x01,0x8E,0x0E,0x30,0x22,0x02,0x20,0xFF,0xFF,0xFF},
    // HDES = 142, HDEE = 782, VDES = 34, VDEE = 514
    {CEA_720x480p60      ,0x01,0x78,0x48,0x30,0x23,0x03,0x20,0xFF,0xFF,0xFF},
    // HDES = 120, HDEE = 840, VDES = 35, VDEE = 515
    {CEA_1280x720p60     ,0x07,0x02,0x02,0x61,0x18,0xE8,0x20,0xFF,0xFF,0xFF},
    // HDES = 258, HDEE = 1538, VDES = 24, VDEE = 744
//    {CEA_1920x1080i60    ,0x07,0xBE,0x3E,0x80,0x13,0x2F,0x20,0x45,0x61,0x42},
//    // HDES = 190, HDEE = 2110, VDES = 19, VDEE = 559, VDS2 = 581, VDE2 = 1121
    {CEA_1920x1080i60    ,0x07,0xBE,0x3E,0x80,0x13,0x2F,0x20,0x46,0x62,0x42},
    // HDES = 190, HDEE = 2110, VDES = 19, VDEE = 559, VDS2 = 582, VDE2 = 1122

    {CEA_720x480i60      ,0x01,0x75,0x45,0x30,0x11,0x01,0x10,0x17,0x07,0x21},
    // HDES = 117, HDEE = 837, VDES = 17, VDEE = 257, VDS2 = 279, VDE2 = 519
    {CEA_720x240p60      ,0x01,0x75,0x45,0x30,0x11,0x01,0x10,0xFF,0xFF,0xFF},
    // HDES = 117, HDEE = 837, VDES = 17, VDEE = 257
    {CEA_1440x480i60     ,0x01,0xEC,0x8C,0x60,0x11,0x01,0x10,0x17,0x07,0x21},
    // HDES = 236, HDEE = 1676, VDES = 17, VDEE = 257, VDS2 = 279, VDE2 = 519
    {CEA_1440x240p60     ,0x01,0xEC,0x8C,0x60,0x11,0x01,0x10,0xFF,0xFF,0xFF},
    // HDES = 236, HDEE = 1676, VDES = 17, VDEE = 257
    {CEA_2880x480i60     ,0x01,0x16,0x56,0xD2,0x11,0x01,0x10,0x17,0x07,0x21},
    // HDES = 534, HDEE = 3414, VDES = 17, VDEE = 257, VDS2 = 279, VDE2 = 519
    {CEA_2880x240p60     ,0x01,0x16,0x56,0xD2,0x11,0x01,0x10,0xFF,0xFF,0xFF},
    // HDES = 534, HDEE = 3414, VDES = 17, VDEE = 257
    {CEA_1440x480p60     ,0x01,0xF2,0x92,0x60,0x23,0x03,0x20,0xFF,0xFF,0xFF},
    // HDES = 242, HDEE = 1682, VDES = 35, VDEE = 515
    {CEA_1920x1080p60    ,0x07,0xBE,0x3E,0x80,0x28,0x60,0x40,0xFF,0xFF,0xFF},
    // HDES = 190, HDEE = 2110, VDES = 40, VDEE = 1120
    {CEA_720x576p50      ,0x01,0x82,0x52,0x30,0x2b,0x6b,0x20,0xFF,0xFF,0xFF},
    // HDES = 130, HDEE = 850, VDES = 43, VDEE = 619
    {CEA_1280x720p50     ,0x07,0x02,0x02,0x61,0x18,0xE8,0x20,0xFF,0xFF,0xFF},
    // HDES = 258, HDEE = 1538, VDES = 24, VDEE = 744
    {CEA_1920x1080i50    ,0x07,0xBE,0x3E,0x80,0x13,0x2F,0x20,0x46,0x62,0x42},
    // HDES = 190, HDEE = 2110, VDES = 19, VDEE = 559, VDS2 = 582, VDE2 = 1122
    {CEA_720x576i50      ,0x01,0x82,0x52,0x30,0x15,0x35,0x10,0x4D,0x6D,0x21},
    // HDES = 130, HDEE = 850, VDES = 21, VDEE = 309, VDS2 = 333, VDE2 = 621
    {CEA_1440x576i50     ,0x01,0x06,0xA6,0x61,0x15,0x35,0x10,0x4D,0x6D,0x21},
    // HDES = 262, HDEE = 1702, VDES = 21, VDEE = 309, VDS2 = 333, VDE2 = 621
    {CEA_720x288p50      ,0x01,0x82,0x52,0x30,0x15,0x35,0x10,0xFF,0xFF,0xFF},
    // HDES = 130, HDEE = 850, VDES = 21, VDEE = 309
    {CEA_1440x288p50     ,0x01,0x06,0xA6,0x61,0x15,0x35,0x10,0xFF,0xFF,0xFF},
    // HDES = 262, HDEE = 1702, VDES = 21, VDEE = 309
    {CEA_2880x576i50     ,0x01,0x0E,0x4E,0xD2,0x15,0x35,0x10,0x4D,0x6D,0x21},
    // HDES = 526, HDEE = 3406, VDES = 21, VDEE = 309, VDS2 = 333, VDE2 = 621
    {CEA_2880x288p50     ,0x01,0x0E,0x4E,0xD2,0x15,0x35,0x10,0xFF,0xFF,0xFF},
    // HDES = 526, HDEE = 3406, VDES = 21, VDEE = 309
    {CEA_1440x576p50     ,0x05,0x06,0xA6,0x61,0x2B,0x6B,0x20,0xFF,0xFF,0xFF},
    // HDES = 262, HDEE = 1702, VDES = 43, VDEE = 619
    {CEA_1920x1080p50    ,0x07,0xBE,0x3E,0x80,0x28,0x60,0x40,0xFF,0xFF,0xFF},
    // HDES = 190, HDEE = 2110, VDES = 40, VDEE = 1120
    {CEA_1920x1080p24    ,0x07,0xBE,0x3E,0x80,0x28,0x60,0x40,0xFF,0xFF,0xFF},
    // HDES = 190, HDEE = 2110, VDES = 40, VDEE = 1120
    {CEA_1920x1080p25    ,0x07,0xBE,0x3E,0x80,0x28,0x60,0x40,0xFF,0xFF,0xFF},
    // HDES = 190, HDEE = 2110, VDES = 40, VDEE = 1120
    {CEA_1920x1080p30    ,0x07,0xBE,0x3E,0x80,0x28,0x60,0x40,0xFF,0xFF,0xFF},
    // HDES = 190, HDEE = 2110, VDES = 40, VDEE = 1120
    {VESA_640x350p85     ,0x03,0x9E,0x1E,0x30,0x3E,0x9C,0x10,0xFF,0xFF,0xFF},
    // HDES = 158, HDEE = 798, VDES = 62, VDEE = 412
    {VESA_640x400p85     ,0x05,0x9E,0x1E,0x30,0x2B,0xBB,0x10,0xFF,0xFF,0xFF},
    // HDES = 158, HDEE = 798, VDES = 43, VDEE = 443
    {VESA_720x400p85     ,0x05,0xB2,0x82,0x30,0x2C,0xBC,0x10,0xFF,0xFF,0xFF},
    // HDES = 178, HDEE = 898, VDES = 44, VDEE = 444
    {VESA_640x480p60     ,0x01,0x8E,0x0E,0x30,0x22,0x02,0x20,0xFF,0xFF,0xFF},
    // HDES = 142, HDEE = 782, VDES = 34, VDEE = 514
    {VESA_640x480p72     ,0x01,0xA6,0x26,0x30,0x1E,0xFE,0x10,0xFF,0xFF,0xFF},
    // HDES = 166, HDEE = 806, VDES = 30, VDEE = 510
    {VESA_640x480p75     ,0x01,0xB6,0x36,0x30,0x12,0xF2,0x10,0xFF,0xFF,0xFF},
    // HDES = 182, HDEE = 822, VDES = 18, VDEE = 498
    {VESA_640x480p85     ,0x01,0x86,0x06,0x30,0x1B,0xFB,0x10,0xFF,0xFF,0xFF},
    // HDES = 134, HDEE = 774, VDES = 27, VDEE = 507
    {VESA_800x600p56     ,0x07,0xC6,0xE6,0x30,0x17,0x6F,0x20,0xFF,0xFF,0xFF},
    // HDES = 198, HDEE = 998, VDES = 23, VDEE = 623
    {VESA_800x600p60     ,0x07,0xD6,0xF6,0x30,0x1A,0x72,0x20,0xFF,0xFF,0xFF},
    // HDES = 214, HDEE = 1014, VDES = 26, VDEE = 626
    {VESA_800x600p72     ,0x07,0xB6,0xD6,0x30,0x1C,0x74,0x20,0xFF,0xFF,0xFF},
    // HDES = 182, HDEE = 982, VDES = 28, VDEE = 628
    {VESA_800x600p75     ,0x07,0xEE,0x0E,0x40,0x17,0x6F,0x20,0xFF,0xFF,0xFF},
    // HDES = 238, HDEE = 1038, VDES = 23, VDEE = 623
    {VESA_800X600p85     ,0x07,0xD6,0xF6,0x30,0x1D,0x75,0x20,0xFF,0xFF,0xFF},
    // HDES = 214, HDEE = 1014, VDES = 29, VDEE = 629
    {VESA_840X480p60     ,0x07,0xDE,0x2E,0x40,0x1E,0xFE,0x10,0xFF,0xFF,0xFF},
    // HDES = 222, HDEE = 1070, VDES = 30, VDEE = 510
    {VESA_1024x768p60    ,0x01,0x26,0x26,0x51,0x22,0x22,0x30,0xFF,0xFF,0xFF},
    // HDES = 294, HDEE = 1318, VDES = 34, VDEE = 802
    {VESA_1024x768p70    ,0x01,0x16,0x16,0x51,0x22,0x22,0x30,0xFF,0xFF,0xFF},
    // HDES = 278, HDEE = 1302, VDES = 34, VDEE = 802
    {VESA_1024x768p75    ,0x07,0x0E,0x0E,0x51,0x1E,0x1E,0x30,0xFF,0xFF,0xFF},
    // HDES = 270, HDEE = 1294, VDES = 30, VDEE = 798
    {VESA_1024x768p85    ,0x07,0x2E,0x2E,0x51,0x26,0x26,0x30,0xFF,0xFF,0xFF},
    // HDES = 302, HDEE = 1326, VDES = 38, VDEE = 806
    {VESA_1152x864p75    ,0x07,0x7E,0xFE,0x51,0x22,0x82,0x30,0xFF,0xFF,0xFF},
    // HDES = 382, HDEE = 1534, VDES = 34, VDEE = 898
    {VESA_1280x768p60R   ,0x03,0x6E,0x6E,0x50,0x12,0x12,0x30,0xFF,0xFF,0xFF},
    // HDES = 110, HDEE = 1390, VDES = 18, VDEE = 786
    {VESA_1280x768p60    ,0x05,0x3E,0x3E,0x61,0x1A,0x1A,0x30,0xFF,0xFF,0xFF},
    // HDES = 318, HDEE = 1598, VDES = 26, VDEE = 794
    {VESA_1280x768p75    ,0x05,0x4E,0x4E,0x61,0x21,0x21,0x30,0xFF,0xFF,0xFF},
    // HDES = 334, HDEE = 1614, VDES = 33, VDEE = 801
    {VESA_1280x768p85    ,0x05,0x5E,0x5E,0x61,0x25,0x25,0x30,0xFF,0xFF,0xFF},
    // HDES = 350, HDEE = 1630, VDES = 37, VDEE = 805
    {VESA_1280x960p60    ,0x07,0xA6,0xA6,0x61,0x26,0xE6,0x30,0xFF,0xFF,0xFF},
    // HDES = 422, HDEE = 1702, VDES = 38, VDEE = 998
    {VESA_1280x960p85    ,0x07,0x7E,0x7E,0x61,0x31,0xF1,0x30,0xFF,0xFF,0xFF},
    // HDES = 382, HDEE = 1662, VDES = 49, VDEE = 1009
    {VESA_1280x1024p60   ,0x07,0x66,0x66,0x61,0x28,0x28,0x40,0xFF,0xFF,0xFF},
    // HDES = 358, HDEE = 1638, VDES = 40, VDEE = 1064
    {VESA_1280x1024p75   ,0x07,0x86,0x86,0x61,0x28,0x28,0x40,0xFF,0xFF,0xFF},
    // HDES = 390, HDEE = 1670, VDES = 40, VDEE = 1064
    {VESA_1280X1024p85   ,0x07,0x7E,0x7E,0x61,0x2E,0x2E,0x40,0xFF,0xFF,0xFF},
    // HDES = 382, HDEE = 1662, VDES = 46, VDEE = 1070
    {VESA_1360X768p60    ,0x07,0x6E,0xBE,0x61,0x17,0x17,0x30,0xFF,0xFF,0xFF},
    // HDES = 366, HDEE = 1726, VDES = 23, VDEE = 791
    {VESA_1400x768p60R   ,0x03,0x6E,0xE6,0x50,0x1A,0x34,0x40,0xFF,0xFF,0xFF},
    // HDES = 110, HDEE = 1510, VDES = 26, VDEE = 1076
    {VESA_1400x768p60    ,0x05,0x76,0xEE,0x61,0x23,0x3D,0x40,0xFF,0xFF,0xFF},
    // HDES = 374, HDEE = 1774, VDES = 35, VDEE = 1085
    {VESA_1400x1050p75   ,0x05,0x86,0xFE,0x61,0x2D,0x47,0x40,0xFF,0xFF,0xFF},
    // HDES = 390, HDEE = 1790, VDES = 45, VDEE = 1095
    {VESA_1400x1050p85   ,0x05,0x96,0x0E,0x71,0x33,0x4D,0x40,0xFF,0xFF,0xFF},
    // HDES = 406, HDEE = 1806, VDES = 51, VDEE = 1101
    {VESA_1440x900p60R   ,0x03,0x6E,0x0E,0x60,0x16,0x9A,0x30,0xFF,0xFF,0xFF},
    // HDES = 110, HDEE = 1550, VDES = 22, VDEE = 922
    {VESA_1440x900p60    ,0x05,0x7E,0x1E,0x71,0x1E,0xA2,0x30,0xFF,0xFF,0xFF},
    // HDES = 382, HDEE = 1822, VDES = 30, VDEE = 930
    {VESA_1440x900p75    ,0x05,0x8E,0x2E,0x71,0x26,0xAA,0x30,0xFF,0xFF,0xFF},
    // HDES = 398, HDEE = 1838, VDES = 38, VDEE = 938
    {VESA_1440x900p85    ,0x05,0x96,0x36,0x71,0x2C,0xB0,0x30,0xFF,0xFF,0xFF},
    // HDES = 406, HDEE = 1846, VDES = 44, VDEE = 944
    {VESA_1600x1200p60   ,0x07,0xEE,0x2E,0x81,0x30,0xE0,0x40,0xFF,0xFF,0xFF},
    // HDES = 494, HDEE = 2094, VDES = 48, VDEE = 1248
    {VESA_1600x1200p65   ,0x07,0xEE,0x2E,0x81,0x30,0xE0,0x40,0xFF,0xFF,0xFF},
    // HDES = 494, HDEE = 2094, VDES = 48, VDEE = 1248
    {VESA_1600x1200p70   ,0x07,0xEE,0x2E,0x81,0x30,0xE0,0x40,0xFF,0xFF,0xFF},
    // HDES = 494, HDEE = 2094, VDES = 48, VDEE = 1248
    {VESA_1600x1200p75   ,0x07,0xEE,0x2E,0x81,0x30,0xE0,0x40,0xFF,0xFF,0xFF},
    // HDES = 494, HDEE = 2094, VDES = 48, VDEE = 1248
    {VESA_1600x1200p85   ,0x07,0xEE,0x2E,0x81,0x30,0xE0,0x40,0xFF,0xFF,0xFF},
    // HDES = 494, HDEE = 2094, VDES = 48, VDEE = 1248
    {VESA_1680x1050p60R  ,0x03,0x6E,0xFE,0x60,0x1A,0x34,0x40,0xFF,0xFF,0xFF},
    // HDES = 110, HDEE = 1790, VDES = 26, VDEE = 1076
    {VESA_1680x1050p60   ,0x05,0xC6,0x56,0x81,0x23,0x3D,0x40,0xFF,0xFF,0xFF},
    // HDES = 454, HDEE = 2134, VDES = 35, VDEE = 1085
    {VESA_1680x1050p75   ,0x05,0xD6,0x66,0x81,0x2D,0x47,0x40,0xFF,0xFF,0xFF},
    // HDES = 470, HDEE = 2150, VDES = 45, VDEE = 1095
    {VESA_1680x1050p85   ,0x05,0xDE,0x6E,0x81,0x33,0x4D,0x40,0xFF,0xFF,0xFF},
    // HDES = 478, HDEE = 2158, VDES = 51, VDEE = 1101
    {VESA_1792x1344p60   ,0x05,0x0E,0x0E,0x92,0x30,0x70,0x50,0xFF,0xFF,0xFF},
    // HDES = 526, HDEE = 2318, VDES = 48, VDEE = 1392
    {VESA_1792x1344p75   ,0x05,0x36,0x36,0x92,0x47,0x87,0x50,0xFF,0xFF,0xFF},
    // HDES = 566, HDEE = 2358, VDES = 71, VDEE = 1415
    {VESA_1856x1392p60   ,0x05,0x3E,0x7E,0x92,0x2D,0x9D,0x50,0xFF,0xFF,0xFF},
    // HDES = 574, HDEE = 2430, VDES = 45, VDEE = 1437
    {VESA_1856x1392p75   ,0x05,0x3E,0x7E,0x92,0x6A,0xDA,0x50,0xFF,0xFF,0xFF},
    // HDES = 574, HDEE = 2430, VDES = 106, VDEE = 1498
    {VESA_1920x1200p60R  ,0x03,0x6E,0xEE,0x70,0x1F,0xCF,0x40,0xFF,0xFF,0xFF},
    // HDES = 110, HDEE = 2030, VDES = 31, VDEE = 1231
    {VESA_1920x1200p60   ,0x05,0x16,0x96,0x92,0x29,0xD9,0x40,0xFF,0xFF,0xFF},
    // HDES = 534, HDEE = 2454, VDES = 41, VDEE = 1241
    {VESA_1920x1200p75   ,0x05,0x26,0xA6,0x92,0x33,0xE3,0x40,0xFF,0xFF,0xFF},
    // HDES = 550, HDEE = 2470, VDES = 51, VDEE = 1251
    {VESA_1920x1200p85   ,0x05,0x2E,0xAE,0x92,0x3A,0xEA,0x40,0xFF,0xFF,0xFF},
    // HDES = 558, HDEE = 2478, VDES = 58, VDEE = 1258
    {VESA_1920x1440p60   ,0x05,0x26,0xA6,0x92,0x3A,0xDA,0x50,0xFF,0xFF,0xFF},
    // HDES = 550, HDEE = 2470, VDES = 58, VDEE = 1498
    {VESA_1920x1440p75   ,0x05,0x3E,0xBE,0x92,0x3A,0xDA,0x50,0xFF,0xFF,0xFF},
    // HDES = 574, HDEE = 2494, VDES = 58, VDEE = 1498
    {UNKNOWN_MODE,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF}
} ;

BOOL ProgramDEGenModeByID(MODE_ID id,BYTE bInputSignalType)
{
    int i ;
    if( (bInputSignalType & (T_MODE_DEGEN|T_MODE_SYNCGEN|T_MODE_SYNCEMB) )==(T_MODE_DEGEN))
    {
        for( i = 0 ; DeGen_Table[i].id != UNKNOWN_MODE ; i++ )
        {
            if( id == DeGen_Table[i].id ) break ;
        }
        if( DeGen_Table[i].id == UNKNOWN_MODE )
        {
            return FALSE ;
        }

        Switch_HDMITX_Bank(0) ;
        HDMITX_WriteI2C_Byte(0x90,DeGen_Table[i].Reg90) ;
        HDMITX_WriteI2C_Byte(0x92,DeGen_Table[i].Reg92) ;
        HDMITX_WriteI2C_Byte(0x93,DeGen_Table[i].Reg93) ;
        HDMITX_WriteI2C_Byte(0x94,DeGen_Table[i].Reg94) ;
        HDMITX_WriteI2C_Byte(0x9A,DeGen_Table[i].Reg9A) ;
        HDMITX_WriteI2C_Byte(0x9B,DeGen_Table[i].Reg9B) ;
        HDMITX_WriteI2C_Byte(0x9C,DeGen_Table[i].Reg9C) ;
        HDMITX_WriteI2C_Byte(0x9D,DeGen_Table[i].Reg9D) ;
        HDMITX_WriteI2C_Byte(0x9E,DeGen_Table[i].Reg9E) ;
        HDMITX_WriteI2C_Byte(0x9F,DeGen_Table[i].Reg9F) ;
        return TRUE ;

    }
    return FALSE ;
}

#endif

#ifdef SUPPORT_SYNCEMBEDDED
/* ****************************************************** */
// sync embedded table setting,defined as comment.
/* ****************************************************** */
struct SyncEmbeddedSetting {
    BYTE fmt ;
    BYTE RegHVPol ; // Reg90
	BYTE RegHfPixel ; // Reg91
    BYTE RegHSSL ; // Reg95
    BYTE RegHSEL ; // Reg96
    BYTE RegHSH ; // Reg97
    BYTE RegVSS1 ; // RegA0
    BYTE RegVSE1 ; // RegA1
    BYTE RegVSS2 ; // RegA2
    BYTE RegVSE2 ; // RegA3

    ULONG PCLK ;
    BYTE VFreq ;
} ;

static _CODE struct SyncEmbeddedSetting SyncEmbTable[] = {
 // {FMT,0x90,0x91,
 //                 0x95,0x96,0x97,0xA0,0xA1,0xA2,0xA3,PCLK,VFREQ},
    {   1,0xF0,0x31,0x0E,0x6E,0x00,0x0A,0xC0,0xFF,0xFF,25175000,60},
    {   2,0xF0,0x31,0x0E,0x4c,0x00,0x09,0xF0,0xFF,0xFF,27000000,60},
    {   3,0xF0,0x31,0x0E,0x4c,0x00,0x09,0xF0,0xFF,0xFF,27000000,60},
    {   4,0x76,0x33,0x6c,0x94,0x00,0x05,0xA0,0xFF,0xFF,74175000,60},
    {   5,0x26,0x4A,0x56,0x82,0x00,0x02,0x70,0x34,0x92,74175000,60},
    {   6,0xE0,0x1B,0x11,0x4F,0x00,0x04,0x70,0x0A,0xD1,27000000,60},
    {   7,0xE0,0x1B,0x11,0x4F,0x00,0x04,0x70,0x0A,0xD1,27000000,60},
    {   8,0x00,0xff,0x11,0x4F,0x00,0x04,0x70,0xFF,0xFF,27000000,60},
    {   9,0x00,0xff,0x11,0x4F,0x00,0x04,0x70,0xFF,0xFF,27000000,60},
    {  10,0xe0,0x1b,0x11,0x4F,0x00,0x04,0x70,0x0A,0xD1,54000000,60},
    {  11,0xe0,0x1b,0x11,0x4F,0x00,0x04,0x70,0x0A,0xD1,54000000,60},
    {  12,0x00,0xff,0x11,0x4F,0x00,0x04,0x70,0xFF,0xFF,54000000,60},
    {  13,0x00,0xff,0x11,0x4F,0x00,0x04,0x70,0xFF,0xFF,54000000,60},
    {  14,0x00,0xff,0x1e,0x9A,0x00,0x09,0xF0,0xFF,0xFF,54000000,60},
    {  15,0x00,0xff,0x1e,0x9A,0x00,0x09,0xF0,0xFF,0xFF,54000000,60},
    {  16,0x06,0xff,0x56,0x82,0x00,0x04,0x90,0xFF,0xFF,148350000,60},
    {  17,0x00,0xff,0x0a,0x4A,0x00,0x05,0xA0,0xFF,0xFF,27000000,50},
    {  18,0x00,0xff,0x0a,0x4A,0x00,0x05,0xA0,0xFF,0xFF,27000000,50},
    {  19,0x06,0xff,0xB6,0xDE,0x11,0x05,0xA0,0xFF,0xFF,74250000,50},
    {  20,0x66,0x73,0x0e,0x3A,0x22,0x02,0x70,0x34,0x92,74250000,50},
    {  21,0xA0,0x1B,0x0a,0x49,0x00,0x02,0x50,0x3A,0xD1,27000000,50},
    {  22,0xA0,0x1B,0x0a,0x49,0x00,0x02,0x50,0x3A,0xD1,27000000,50},
    {  23,0x00,0xff,0x0a,0x49,0x00,0x02,0x50,0xFF,0xFF,27000000,50},
    {  24,0x00,0xff,0x0a,0x49,0x00,0x02,0x50,0xFF,0xFF,27000000,50},
    {  25,0xA0,0x1B,0x0a,0x49,0x00,0x02,0x50,0x3A,0xD1,54000000,50},
    {  26,0xA0,0x1B,0x0a,0x49,0x00,0x02,0x50,0x3A,0xD1,54000000,50},
    {  27,0x00,0xff,0x0a,0x49,0x00,0x02,0x50,0xFF,0xFF,54000000,50},
    {  28,0x00,0xff,0x0a,0x49,0x00,0x02,0x50,0xFF,0xFF,54000000,50},
    {  29,0x04,0xff,0x16,0x96,0x00,0x05,0xA0,0xFF,0xFF,54000000,50},
    {  30,0x04,0xff,0x16,0x96,0x00,0x05,0xA0,0xFF,0xFF,54000000,50},
    {  31,0x06,0xff,0x0e,0x3a,0x22,0x04,0x90,0xFF,0xFF,148500000,50},
    {0xFF,0xFF,0xff,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0,0}
} ;

BOOL
ProgramSyncEmbeddedVideoMode(BYTE VIC,BYTE bInputSignalType)
{
    int i ;
    // if Embedded Video,need to generate timing with pattern register

    ErrorF("ProgramSyncEmbeddedVideoMode(%d,%x)\n",VIC,bInputSignalType) ;

    if( bInputSignalType & T_MODE_SYNCEMB )
    {
        for(i = 0 ; SyncEmbTable[i].fmt != 0xFF ; i++)
        {
            if(VIC == SyncEmbTable[i].fmt)
            {
                break ;
            }
        }

        if(SyncEmbTable[i].fmt == 0xFF)
        {
            return FALSE ;
        }

        HDMITX_WriteI2C_Byte(REG_TX_HVPol,SyncEmbTable[i].RegHVPol) ; // Reg90
        HDMITX_WriteI2C_Byte(REG_TX_HfPixel,SyncEmbTable[i].RegHfPixel) ; // Reg91

    	HDMITX_WriteI2C_Byte(REG_TX_HSSL,SyncEmbTable[i].RegHSSL) ; // Reg95
    	HDMITX_WriteI2C_Byte(REG_TX_HSEL,SyncEmbTable[i].RegHSEL) ; // Reg96
    	HDMITX_WriteI2C_Byte(REG_TX_HSH,SyncEmbTable[i].RegHSH) ; // Reg97
        HDMITX_WriteI2C_Byte(REG_TX_VSS1,SyncEmbTable[i].RegVSS1) ; // RegA0
        HDMITX_WriteI2C_Byte(REG_TX_VSE1,SyncEmbTable[i].RegVSE1) ; // RegA1

        HDMITX_WriteI2C_Byte(REG_TX_VSS2,SyncEmbTable[i].RegVSS2) ; // RegA2
        HDMITX_WriteI2C_Byte(REG_TX_VSE2,SyncEmbTable[i].RegVSE2) ; // RegA3
    }

    return TRUE ;
}
#endif // SUPPORT_SYNCEMBEDDED

//~jj_tseng@chipadvanced.com 2007/01/02


//////////////////////////////////////////////////////////////////////
// Function: SetInputMode
// Parameter: InputMode,bInputSignalType
//      InputMode - use [1:0] to identify the color space for reg70[7:6],
//                  definition:
//                     #define F_MODE_RGB444  0
//                     #define F_MODE_YUV422 1
//                     #define F_MODE_YUV444 2
//                     #define F_MODE_CLRMOD_MASK 3
//      bInputSignalType - defined the CCIR656 D[0],SYNC Embedded D[1],and
//                     DDR input in D[2].
// Return: N/A
// Remark: program Reg70 with the input value.
// Side-Effect: Reg70.
//////////////////////////////////////////////////////////////////////

static void
SetInputMode(BYTE InputMode,BYTE bInputSignalType)
{
    BYTE ucData ;

    ErrorF("SetInputMode(%02X,%02X)\n",InputMode,bInputSignalType) ;

    ucData = HDMITX_ReadI2C_Byte(REG_TX_INPUT_MODE) ;

    ucData &= ~(M_INCOLMOD|B_2X656CLK|B_SYNCEMB|B_INDDR|B_PCLKDIV2) ;

    switch(InputMode & F_MODE_CLRMOD_MASK)
    {
    case F_MODE_YUV422:
        ucData |= B_IN_YUV422 ;
        break ;
    case F_MODE_YUV444:
        ucData |= B_IN_YUV444 ;
        break ;
    case F_MODE_RGB444:
    default:
        ucData |= B_IN_RGB ;
        break ;
    }

    if(bInputSignalType & T_MODE_PCLKDIV2)
    {
        ucData |= B_PCLKDIV2 ; ErrorF("PCLK Divided by 2 mode\n") ;
    }
    if(bInputSignalType & T_MODE_CCIR656)
    {
        ucData |= B_2X656CLK ; ErrorF("CCIR656 mode\n") ;
    }

    if(bInputSignalType & T_MODE_SYNCEMB)
    {
        ucData |= B_SYNCEMB ; ErrorF("Sync Embedded mode\n") ;
    }

    if(bInputSignalType & T_MODE_INDDR)
    {
        ucData |= B_INDDR ; ErrorF("Input DDR mode\n") ;
    }

    HDMITX_WriteI2C_Byte(REG_TX_INPUT_MODE,ucData) ;
}

//////////////////////////////////////////////////////////////////////
// Function: SetCSCScale
// Parameter: bInputMode -
//             D[1:0] - Color Mode
//             D[4] - Colorimetry 0: ITU_BT601 1: ITU_BT709
//             D[5] - Quantization 0: 0_255 1: 16_235
//             D[6] - Up/Dn Filter 'Required'
//                    0: no up/down filter
//                    1: enable up/down filter when csc need.
//             D[7] - Dither Filter 'Required'
//                    0: no dither enabled.
//                    1: enable dither and dither free go "when required".
//            bOutputMode -
//             D[1:0] - Color mode.
// Return: N/A
// Remark: reg72~reg8D will be programmed depended the input with table.
// Side-Effect:
//////////////////////////////////////////////////////////////////////

static void
SetCSCScale(BYTE bInputMode,BYTE bOutputMode)
{
    BYTE ucData,csc=0 ;
    BYTE filter = 0 ; // filter is for Video CTRL DN_FREE_GO,EN_DITHER,and ENUDFILT


	// (1) YUV422 in,RGB/YUV444 output (Output is 8-bit,input is 12-bit)
	// (2) YUV444/422  in,RGB output (CSC enable,and output is not YUV422)
	// (3) RGB in,YUV444 output   (CSC enable,and output is not YUV422)
    //
    // YUV444/RGB24 <-> YUV422 need set up/down filter.

    switch(bInputMode&F_MODE_CLRMOD_MASK)
    {
    #ifdef SUPPORT_INPUTYUV444
    case F_MODE_YUV444:
        ErrorF("Input mode is YUV444 ") ;
        switch(bOutputMode&F_MODE_CLRMOD_MASK)
        {
        case F_MODE_YUV444:
            ErrorF("Output mode is YUV444\n") ;
            csc = B_CSC_BYPASS ;
            break ;

        case F_MODE_YUV422:
            ErrorF("Output mode is YUV422\n") ;
            if(bInputMode & F_MODE_EN_UDFILT) // YUV444 to YUV422 need up/down filter for processing.
            {
                filter |= B_TX_EN_UDFILTER ;
            }
            csc = B_CSC_BYPASS ;
            break ;
        case F_MODE_RGB444:
            ErrorF("Output mode is RGB24\n") ;
            csc = B_CSC_YUV2RGB ;
            if(bInputMode & F_MODE_EN_DITHER) // YUV444 to RGB24 need dither
            {
                filter |= B_TX_EN_DITHER | B_TX_DNFREE_GO ;
            }

            break ;
        }
        break ;
    #endif

    #ifdef SUPPORT_INPUTYUV422
    case F_MODE_YUV422:
        ErrorF("Input mode is YUV422\n") ;
        switch(bOutputMode&F_MODE_CLRMOD_MASK)
        {
        case F_MODE_YUV444:
            ErrorF("Output mode is YUV444\n") ;
            csc = B_CSC_BYPASS ;
            if(bInputMode & F_MODE_EN_UDFILT) // YUV422 to YUV444 need up filter
            {
                filter |= B_TX_EN_UDFILTER ;
            }

            if(bInputMode & F_MODE_EN_DITHER) // YUV422 to YUV444 need dither
            {
                filter |= B_TX_EN_DITHER | B_TX_DNFREE_GO ;
            }

            break ;
        case F_MODE_YUV422:
            ErrorF("Output mode is YUV422\n") ;
            csc = B_CSC_BYPASS ;

            break ;

        case F_MODE_RGB444:
            ErrorF("Output mode is RGB24\n") ;
            csc = B_CSC_YUV2RGB ;
            if(bInputMode & F_MODE_EN_UDFILT) // YUV422 to RGB24 need up/dn filter.
            {
                filter |= B_TX_EN_UDFILTER ;
            }

            if(bInputMode & F_MODE_EN_DITHER) // YUV422 to RGB24 need dither
            {
                filter |= B_TX_EN_DITHER | B_TX_DNFREE_GO ;
            }

            break ;
        }
        break ;
    #endif

    #ifdef SUPPORT_INPUTRGB
    case F_MODE_RGB444:
        ErrorF("Input mode is RGB24\n") ;
        switch(bOutputMode&F_MODE_CLRMOD_MASK)
        {
        case F_MODE_YUV444:
            ErrorF("Output mode is YUV444\n") ;
            csc = B_CSC_RGB2YUV ;

            if(bInputMode & F_MODE_EN_DITHER) // RGB24 to YUV444 need dither
            {
                filter |= B_TX_EN_DITHER | B_TX_DNFREE_GO ;
            }
            break ;

        case F_MODE_YUV422:
            ErrorF("Output mode is YUV422\n") ;
            if(bInputMode & F_MODE_EN_UDFILT) // RGB24 to YUV422 need down filter.
            {
                filter |= B_TX_EN_UDFILTER ;
            }

            if(bInputMode & F_MODE_EN_DITHER) // RGB24 to YUV422 need dither
            {
                filter |= B_TX_EN_DITHER | B_TX_DNFREE_GO ;
            }
            csc = B_CSC_RGB2YUV ;
            break ;

        case F_MODE_RGB444:
            ErrorF("Output mode is RGB24\n") ;
            csc = B_CSC_BYPASS ;
            break ;
        }
        break ;
    #endif
    }

    #ifdef SUPPORT_INPUTRGB
    // set the CSC metrix registers by colorimetry and quantization
    if(csc == B_CSC_RGB2YUV)
    {
        ErrorF("CSC = RGB2YUV %x ",csc) ;
        switch(bInputMode&(F_MODE_ITU709|F_MODE_16_235))
        {
        case F_MODE_ITU709|F_MODE_16_235:
            ErrorF("ITU709 16-235 ") ;
            HDMITX_WriteI2C_ByteN(REG_TX_CSC_YOFF,bCSCOffset_16_235,SIZEOF_CSCOFFSET) ;
            HDMITX_WriteI2C_ByteN(REG_TX_CSC_MTX11_L,bCSCMtx_RGB2YUV_ITU709_16_235,SIZEOF_CSCMTX) ;
            break ;
        case F_MODE_ITU709|F_MODE_0_255:
            ErrorF("ITU709 0-255 ") ;
            HDMITX_WriteI2C_ByteN(REG_TX_CSC_YOFF,bCSCOffset_0_255,SIZEOF_CSCOFFSET) ;
            HDMITX_WriteI2C_ByteN(REG_TX_CSC_MTX11_L,bCSCMtx_RGB2YUV_ITU709_0_255,SIZEOF_CSCMTX) ;
            break ;
        case F_MODE_ITU601|F_MODE_16_235:
            ErrorF("ITU601 16-235 ") ;
            HDMITX_WriteI2C_ByteN(REG_TX_CSC_YOFF,bCSCOffset_16_235,SIZEOF_CSCOFFSET) ;
            HDMITX_WriteI2C_ByteN(REG_TX_CSC_MTX11_L,bCSCMtx_RGB2YUV_ITU601_16_235,SIZEOF_CSCMTX) ;
            break ;
        case F_MODE_ITU601|F_MODE_0_255:
        default:
            ErrorF("ITU601 0-255 ") ;

            HDMITX_WriteI2C_ByteN(REG_TX_CSC_YOFF,bCSCOffset_0_255,SIZEOF_CSCOFFSET) ;
            HDMITX_WriteI2C_ByteN(REG_TX_CSC_MTX11_L,bCSCMtx_RGB2YUV_ITU601_0_255,SIZEOF_CSCMTX) ;
            break ;
        }

    }
    #endif

    #ifdef SUPPORT_INPUTYUV
    if (csc == B_CSC_YUV2RGB)
    {
        int i;
        ErrorF("CSC = YUV2RGB %x ",csc) ;

        switch(bInputMode&(F_MODE_ITU709|F_MODE_16_235))
        {
        case F_MODE_ITU709|F_MODE_16_235:
            ErrorF("ITU709 16-235 ") ;
            HDMITX_WriteI2C_ByteN(REG_TX_CSC_YOFF,bCSCOffset_16_235,SIZEOF_CSCOFFSET) ;
            HDMITX_WriteI2C_ByteN(REG_TX_CSC_MTX11_L,bCSCMtx_YUV2RGB_ITU709_16_235,SIZEOF_CSCMTX) ;
            break ;
        case F_MODE_ITU709|F_MODE_0_255:
            ErrorF("ITU709 0-255 ") ;
            HDMITX_WriteI2C_ByteN(REG_TX_CSC_YOFF,bCSCOffset_0_255,SIZEOF_CSCOFFSET) ;
            HDMITX_WriteI2C_ByteN(REG_TX_CSC_MTX11_L,bCSCMtx_YUV2RGB_ITU709_0_255,SIZEOF_CSCMTX) ;
            break ;
        case F_MODE_ITU601|F_MODE_16_235:
            ErrorF("ITU601 16-235 ") ;
            HDMITX_WriteI2C_ByteN(REG_TX_CSC_YOFF,bCSCOffset_16_235,SIZEOF_CSCOFFSET) ;
            HDMITX_WriteI2C_ByteN(REG_TX_CSC_MTX11_L,bCSCMtx_YUV2RGB_ITU601_16_235,SIZEOF_CSCMTX) ;
            break ;
        case F_MODE_ITU601|F_MODE_0_255:
        default:
            //????? debug
            ErrorF("ITU601 0-255 ") ;
            HDMITX_WriteI2C_ByteN(REG_TX_CSC_YOFF,bCSCOffset_0_255,SIZEOF_CSCOFFSET) ;
            HDMITX_WriteI2C_ByteN(REG_TX_CSC_MTX11_L,bCSCMtx_YUV2RGB_ITU601_0_255,SIZEOF_CSCMTX) ;
            break ;
        }
    }
    #endif

    ucData = HDMITX_ReadI2C_Byte(REG_TX_CSC_CTRL) & ~(M_CSC_SEL|B_TX_DNFREE_GO|B_TX_EN_DITHER|B_TX_EN_UDFILTER) ;
    ucData |= filter|csc ;
    HDMITX_WriteI2C_Byte(REG_TX_CSC_CTRL,ucData) ;
    // set output Up/Down Filter,Dither control

}


//////////////////////////////////////////////////////////////////////
// Function: SetupAFE
// Parameter: VIDEOPCLKLEVEL level
//            PCLK_LOW - for 13.5MHz (for mode less than 1080p)
//            PCLK MEDIUM - for 25MHz~74MHz
//            PCLK HIGH - PCLK > 80Hz (for 1080p mode or above)
// Return: N/A
// Remark: set reg62~reg65 depended on HighFreqMode
//         reg61 have to be programmed at last and after video stable input.
// Side-Effect:
//////////////////////////////////////////////////////////////////////

static void
// SetupAFE(BYTE ucFreqInMHz)
SetupAFE(VIDEOPCLKLEVEL level)
{
    // @emily turn off reg61 before SetupAFE parameters.
    HDMITX_WriteI2C_Byte(REG_TX_AFE_DRV_CTRL,B_AFE_DRV_RST) ;/* 0x10 */
    // HDMITX_WriteI2C_Byte(REG_TX_AFE_DRV_CTRL,0x3) ;
    ErrorF("SetupAFE()\n") ;

    //TMDS Clock < 80MHz	TMDS Clock > 80MHz
    //Reg61	0x03	0x03

    //Reg62	0x18	0x88
    //Reg63	Default	Default
    //Reg64	0x08	0x80
    //Reg65	Default	Default
    //Reg66	Default	Default
    //Reg67	Default	Default

    switch(level)
    {
    case PCLK_HIGH:
        HDMITX_WriteI2C_Byte(REG_TX_AFE_XP_CTRL,0x88) ; // reg62
        HDMITX_WriteI2C_Byte(REG_TX_AFE_ISW_CTRL, 0x10) ; // reg63
        HDMITX_WriteI2C_Byte(REG_TX_AFE_IP_CTRL,0x84) ; // reg64
        break ;
    default:
        HDMITX_WriteI2C_Byte(REG_TX_AFE_XP_CTRL,0x18) ; // reg62
        HDMITX_WriteI2C_Byte(REG_TX_AFE_ISW_CTRL, 0x10) ; // reg63
        HDMITX_WriteI2C_Byte(REG_TX_AFE_IP_CTRL,0x0C) ; // reg64
        break ;
    }
    //HDMITX_AndREG_Byte(REG_TX_SW_RST,~(B_REF_RST|B_VID_RST|B_AREF_RST|B_HDMI_RST)) ;
    DelayMS(1) ;
    HDMITX_AndREG_Byte(REG_TX_SW_RST,B_VID_RST|B_AREF_RST|B_AUD_RST|B_HDCP_RST) ;
    DelayMS(100) ;
    HDMITX_AndREG_Byte(REG_TX_SW_RST,          B_AREF_RST|B_AUD_RST|B_HDCP_RST) ;
    // REG_TX_AFE_DRV_CTRL have to be set at the last step of setup .
}


//////////////////////////////////////////////////////////////////////
// Function: FireAFE
// Parameter: N/A
// Return: N/A
// Remark: write reg61 with 0x04
//         When program reg61 with 0x04,then audio and video circuit work.
// Side-Effect: N/A
//////////////////////////////////////////////////////////////////////
static void
FireAFE()
{
    BYTE reg;
    Switch_HDMITX_Bank(0) ;

    HDMITX_WriteI2C_Byte(REG_TX_AFE_DRV_CTRL,0) ;

    for(reg = 0x61 ; reg <= 0x67 ; reg++)
    {
        ErrorF("Reg[%02X] = %02X\n",reg,HDMITX_ReadI2C_Byte(reg)) ;
    }
}

//////////////////////////////////////////////////////////////////////
// Audio Output
//////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////
// Function: SetAudioFormat
// Parameter:
//    NumChannel - number of channel,from 1 to 8
//    AudioEnable - Audio source and type bit field,value of bit field are
//        ENABLE_SPDIF    (1<<4)
//        ENABLE_I2S_SRC3  (1<<3)
//        ENABLE_I2S_SRC2  (1<<2)
//        ENABLE_I2S_SRC1  (1<<1)
//        ENABLE_I2S_SRC0  (1<<0)
//    SampleFreq - the audio sample frequence in Hz
//    AudSWL - Audio sample width,only support 16,18,20,or 24.
//    AudioCatCode - The audio channel catalogy code defined in IEC 60958-3
// Return: ER_SUCCESS if done,ER_FAIL for otherwise.
// Remark: program audio channel control register and audio channel registers
//         to enable audio by input.
// Side-Effect: register bank will keep in bank zero.
//////////////////////////////////////////////////////////////////////


static SYS_STATUS
SetAudioFormat(BYTE NumChannel,BYTE AudioEnable,BYTE bSampleFreq,BYTE AudSWL,BYTE AudioCatCode)
{
    BYTE fs = bSampleFreq ;
    BYTE SWL ;

    BYTE SourceValid ;
    BYTE SoruceNum ;


    ErrorF("SetAudioFormat(%d channel,%02X,SampleFreq %d,AudSWL %d,%02X)\n",NumChannel,AudioEnable,bSampleFreq,AudSWL,AudioCatCode) ;


//richard remove    Instance[0].bOutputAudioMode |= 0x41 ;
    if(NumChannel > 6)
    {
        SourceValid = B_AUD_ERR2FLAT | B_AUD_S3VALID | B_AUD_S2VALID | B_AUD_S1VALID ;
        SoruceNum = 4 ;
    }
    else if (NumChannel > 4)
    {
        SourceValid = B_AUD_ERR2FLAT | B_AUD_S2VALID | B_AUD_S1VALID ;
        SoruceNum = 3 ;
    }
    else if (NumChannel > 2)
    {
        SourceValid = B_AUD_ERR2FLAT | B_AUD_S1VALID ;
        SoruceNum = 2 ;
    }
    else
    {
        SourceValid = B_AUD_ERR2FLAT ; // only two channel.
        SoruceNum = 1 ;
        Instance[0].bOutputAudioMode &= ~0x40 ;
    }

    AudioEnable &= ~ (M_AUD_SWL|B_SPDIFTC) ;

    switch(AudSWL)
    {
    case 16:
        SWL = AUD_SWL_16 ;
        AudioEnable |= M_AUD_16BIT ;
        break ;
    case 18:
        SWL = AUD_SWL_18 ;
        AudioEnable |= M_AUD_18BIT ;
        break ;
    case 20:
        SWL = AUD_SWL_20 ;
        AudioEnable |= M_AUD_20BIT ;
        break ;
    case 24:
        SWL = AUD_SWL_24 ;
        AudioEnable |= M_AUD_24BIT ;
        break ;
    default:
        return ER_FAIL ;
    }


    Switch_HDMITX_Bank(0) ;
    HDMITX_WriteI2C_Byte(REG_TX_AUDIO_CTRL0,AudioEnable&0xF0) ;

    HDMITX_AndREG_Byte(REG_TX_SW_RST,~(B_AUD_RST|B_AREF_RST)) ;
    HDMITX_WriteI2C_Byte(REG_TX_AUDIO_CTRL1,Instance[0].bOutputAudioMode) ; // regE1 bOutputAudioMode should be loaded from ROM image.
    HDMITX_WriteI2C_Byte(REG_TX_AUDIO_FIFOMAP,0xE4) ; // default mapping.
    HDMITX_WriteI2C_Byte(REG_TX_AUDIO_CTRL3,(Instance[0].bAudioChannelSwap&0xF)|(AudioEnable&B_AUD_SPDIF)) ;
    HDMITX_WriteI2C_Byte(REG_TX_AUD_SRCVALID_FLAT,SourceValid) ;

    // suggested to be 0x41

//     Switch_HDMITX_Bank(1) ;
//     HDMITX_WriteI2C_Byte(REG_TX_AUDCHST_MODE,0 |((NumChannel == 1)?1:0)) ; // 2 audio channel without pre-emphasis,if NumChannel set it as 1.
//     HDMITX_WriteI2C_Byte(REG_TX_AUDCHST_CAT,AudioCatCode) ;
//     HDMITX_WriteI2C_Byte(REG_TX_AUDCHST_SRCNUM,SoruceNum) ;
//     HDMITX_WriteI2C_Byte(REG_TX_AUD0CHST_CHTNUM,0x21) ;
//     HDMITX_WriteI2C_Byte(REG_TX_AUD1CHST_CHTNUM,0x43) ;
//     HDMITX_WriteI2C_Byte(REG_TX_AUD2CHST_CHTNUM,0x65) ;
//     HDMITX_WriteI2C_Byte(REG_TX_AUD3CHST_CHTNUM,0x87) ;
//     HDMITX_WriteI2C_Byte(REG_TX_AUDCHST_CA_FS,0x00|fs) ; // choose clock
//     fs = ~fs ; // OFS is the one's complement of FS
//     HDMITX_WriteI2C_Byte(REG_TX_AUDCHST_OFS_WL,(fs<<4)|SWL) ;
//     Switch_HDMITX_Bank(0) ;

    Switch_HDMITX_Bank(1) ;
    HDMITX_WriteI2C_Byte(REG_TX_AUDCHST_MODE,0 |((NumChannel == 1)?1:0)) ; // 2 audio channel without pre-emphasis,if NumChannel set it as 1.
    HDMITX_WriteI2C_Byte(REG_TX_AUDCHST_CAT,AudioCatCode) ;
    HDMITX_WriteI2C_Byte(REG_TX_AUDCHST_SRCNUM,SoruceNum) ;
    HDMITX_WriteI2C_Byte(REG_TX_AUD0CHST_CHTNUM,0) ;
    HDMITX_WriteI2C_Byte(REG_TX_AUDCHST_CA_FS,0x00|fs) ; // choose clock
    fs = ~fs ; // OFS is the one's complement of FS
    HDMITX_WriteI2C_Byte(REG_TX_AUDCHST_OFS_WL,(fs<<4)|SWL) ;
    Switch_HDMITX_Bank(0) ;

    // richard modify (could be bug), if(!(AudioEnable | B_AUD_SPDIF))
    if(!(AudioEnable & B_AUD_SPDIF))
    {
        HDMITX_WriteI2C_Byte(REG_TX_AUDIO_CTRL0,AudioEnable) ;
    }

    Instance[0].bAudioChannelEnable = AudioEnable ;

    // HDMITX_AndREG_Byte(REG_TX_SW_RST,B_AUD_RST) ;    // enable Audio
    return ER_SUCCESS;
}



static void
AutoAdjustAudio()
{
    unsigned long SampleFreq ;
    unsigned long N ;
    unsigned long CTS ;
    BYTE fs, uc ;

//    bPendingAdjustAudioFreq = TRUE ;

//     if( CAT6611_AudioChannelEnable & B_AUD_SPDIF )
//     {
//         if(!(HDMITX_ReadI2C_Byte(REG_TX_CLK_STATUS2) & B_OSF_LOCK))
//         {
//             return ;
//         }
//     }

    Switch_HDMITX_Bank(1) ;

    N = ((unsigned long)HDMITX_ReadI2C_Byte(REGPktAudN2)&0xF) << 16 ;
    N |= ((unsigned long)HDMITX_ReadI2C_Byte(REGPktAudN1)) <<8 ;
    N |= ((unsigned long)HDMITX_ReadI2C_Byte(REGPktAudN0)) ;

    CTS = ((unsigned long)HDMITX_ReadI2C_Byte(REGPktAudCTSCnt2)&0xF) << 16 ;
    CTS |= ((unsigned long)HDMITX_ReadI2C_Byte(REGPktAudCTSCnt1)) <<8 ;
    CTS |= ((unsigned long)HDMITX_ReadI2C_Byte(REGPktAudCTSCnt0)) ;
    Switch_HDMITX_Bank(0) ;

    // CTS = TMDSCLK * N / ( 128 * SampleFreq )
    // SampleFreq = TMDSCLK * N / (128*CTS)

    if( CTS == 0 )
    {
        return  ;
    }

    SampleFreq = Instance[0].TMDSClock/CTS ;
    SampleFreq *= N ;
    SampleFreq /= 128 ;

    if( SampleFreq>31000 && SampleFreq<=38050 )
    {
        Instance[0].bAudFs = AUDFS_32KHz ;
        fs = AUDFS_32KHz ;;
    }
    else if (SampleFreq < 46050 ) // 44.1KHz
    {
        Instance[0].bAudFs = AUDFS_44p1KHz ;
        fs = AUDFS_44p1KHz ;;
    }
    else if (SampleFreq < 68100 ) // 48KHz
    {
        Instance[0].bAudFs = AUDFS_48KHz ;
        fs = AUDFS_48KHz ;;
    }
    else if (SampleFreq < 92100 ) // 88.2 KHz
    {
        Instance[0].bAudFs = AUDFS_88p2KHz ;
        fs = AUDFS_88p2KHz ;;
    }
    else if (SampleFreq < 136200 ) // 96KHz
    {
        Instance[0].bAudFs = AUDFS_96KHz ;
        fs = AUDFS_96KHz ;;
    }
    else if (SampleFreq < 184200 ) // 176.4KHz
    {
        Instance[0].bAudFs = AUDFS_176p4KHz ;
        fs = AUDFS_176p4KHz ;;
    }
    else if (SampleFreq < 240200 ) // 192KHz
    {
        Instance[0].bAudFs = AUDFS_192KHz ;
        fs = AUDFS_192KHz ;;
    }
    else
    {
        Instance[0].bAudFs = AUDFS_OTHER;
        fs = AUDFS_OTHER;;
    }

//    bPendingAdjustAudioFreq = FALSE ;

    SetNCTS(Instance[0].TMDSClock, Instance[0].bAudFs) ; // set N, CTS by new generated clock.

    Switch_HDMITX_Bank(1) ; // adjust the new fs in channel status registers
    HDMITX_WriteI2C_Byte(REG_TX_AUDCHST_CA_FS,0x00|fs) ; // choose clock
    fs = ~fs ; // OFS is the one's complement of FS
    uc = HDMITX_ReadI2C_Byte(REG_TX_AUDCHST_OFS_WL) ;
    uc &= 0xF ;
    uc |= fs << 4 ;
    HDMITX_WriteI2C_Byte(REG_TX_AUDCHST_OFS_WL,uc) ;

    Switch_HDMITX_Bank(0) ;

}

static void
SetupAudioChannel()
{
    static BYTE bEnableAudioChannel=FALSE ;
    if( (HDMITX_ReadI2C_Byte(REG_TX_SW_RST) & (B_AUD_RST|B_AREF_RST)) == 0) // audio enabled
    {
        Switch_HDMITX_Bank(0) ;

        if((HDMITX_ReadI2C_Byte(REG_TX_AUDIO_CTRL0) & 0xf) == 0)
        {
            if(HDMITX_ReadI2C_Byte(REG_TX_CLK_STATUS2) & B_OSF_LOCK)
            {
                SetNCTS(Instance[0].TMDSClock, Instance[0].bAudFs) ; // to enable automatic progress setting for N/CTS
                DelayMS(5);
                AutoAdjustAudio() ;
                Switch_HDMITX_Bank(0) ;
                HDMITX_WriteI2C_Byte(REG_TX_AUDIO_CTRL0, Instance[0].bAudioChannelEnable) ;
                bEnableAudioChannel=TRUE ;
            }
        }
        else
        {
            if((HDMITX_ReadI2C_Byte(REG_TX_CLK_STATUS2) & B_OSF_LOCK)==0)
            {
                // AutoAdjustAudio() ;
                // ForceSetNCTS(CurrentPCLK, CurrentSampleFreq) ;
                if( bEnableAudioChannel == TRUE )
                {
                    Switch_HDMITX_Bank(0) ;
                    HDMITX_WriteI2C_Byte(REG_TX_AUDIO_CTRL0, Instance[0].bAudioChannelEnable&0xF0) ;
                }
                bEnableAudioChannel=FALSE ;
            }
        }
    }
}
//////////////////////////////////////////////////////////////////////
// Function: SetNCTS
// Parameter: PCLK - video clock in Hz.
//            Fs - audio sample frequency in Hz
// Return: ER_SUCCESS if success
// Remark: set N value,the CTS will be auto generated by HW.
// Side-Effect: register bank will reset to bank 0.
//////////////////////////////////////////////////////////////////////

static SYS_STATUS
SetNCTS(ULONG PCLK,ULONG Fs)
{
    ULONG n,MCLK ;

    MCLK = Fs * 256 ; // MCLK = fs * 256 ;

    ErrorF("SetNCTS(%ld,%ld): MCLK = %ld\n",PCLK,Fs,MCLK) ;

    if( PCLK )
    {
    	switch (Fs) {
    		case AUDFS_32KHz:
    			switch (PCLK) {
    				case 74175000: n = 11648; break;
    				case 14835000: n = 11648; break;
    				default: n = 4096;
    			}
    			break;
    		case AUDFS_44p1KHz:
    			switch (PCLK) {
    				case 74175000: n = 17836; break;
    				case 14835000: n = 8918; break;
    				default: n = 6272;
    			}
    			break;
    		case AUDFS_48KHz:
    			switch (PCLK) {
    				case 74175000: n = 11648; break;
    				case 14835000: n = 5824; break;
    				default: n = 6144;
    			}
    			break;
    		case AUDFS_88p2KHz:
    			switch (PCLK) {
    				case 74175000: n = 35672; break;
    				case 14835000: n = 17836; break;
    				default: n = 12544;
    			}
    			break;
    		case AUDFS_96KHz:
    			switch (PCLK) {
    				case 74175000: n = 23296; break;
    				case 14835000: n = 11648; break;
    				default: n = 12288;
    			}
    			break;
    		case AUDFS_176p4KHz:
    			switch (PCLK) {
    				case 74175000: n = 71344; break;
    				case 14835000: n = 35672; break;
    				default: n = 25088;
    			}
    			break;
    		case AUDFS_192KHz:
    			switch (PCLK) {
    				case 74175000: n = 46592; break;
    				case 14835000: n = 23296; break;
    				default: n = 24576;
    			}
    			break;
    		default: n = MCLK / 2000;
    	}
    }
    else
    {
        switch(Fs)
        {
		case AUDFS_32KHz: n = 4096; break;
		case AUDFS_44p1KHz: n = 6272; break;
		case AUDFS_48KHz: n = 6144; break;
		case AUDFS_88p2KHz: n = 12544; break;
		case AUDFS_96KHz: n = 12288; break;
		case AUDFS_176p4KHz: n = 25088; break;
		case AUDFS_192KHz: n = 24576; break;
		default: n = 6144;
        }

    }


    ErrorF("N = %ld\n",n) ;
    Switch_HDMITX_Bank(1) ;
    HDMITX_WriteI2C_Byte(REGPktAudN0,(BYTE)((n)&0xFF)) ;
    HDMITX_WriteI2C_Byte(REGPktAudN1,(BYTE)((n>>8)&0xFF)) ;
    HDMITX_WriteI2C_Byte(REGPktAudN2,(BYTE)((n>>16)&0xF)) ;
    Switch_HDMITX_Bank(0) ;

    HDMITX_WriteI2C_Byte(REG_TX_PKT_SINGLE_CTRL,0) ; // D[1] = 0,HW auto count CTS

    HDMITX_SetREG_Byte(REG_TX_CLK_CTRL0,~M_EXT_MCLK_SEL,B_EXT_256FS) ;
    return ER_SUCCESS ;
}

//////////////////////////////////////////////////////////////////////
// DDC Function.
//////////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////
// Function: ClearDDCFIFO
// Parameter: N/A
// Return: N/A
// Remark: clear the DDC FIFO.
// Side-Effect: DDC master will set to be HOST.
//////////////////////////////////////////////////////////////////////

static void
ClearDDCFIFO()
{
    HDMITX_WriteI2C_Byte(REG_TX_DDC_MASTER_CTRL,B_MASTERDDC|B_MASTERHOST) ;
    HDMITX_WriteI2C_Byte(REG_TX_DDC_CMD,CMD_FIFO_CLR) ;
}

static void
GenerateDDCSCLK()
{
    HDMITX_WriteI2C_Byte(REG_TX_DDC_MASTER_CTRL,B_MASTERDDC|B_MASTERHOST) ;
    HDMITX_WriteI2C_Byte(REG_TX_DDC_CMD,CMD_GEN_SCLCLK) ;
}
//////////////////////////////////////////////////////////////////////
// Function: AbortDDC
// Parameter: N/A
// Return: N/A
// Remark: Force abort DDC and reset DDC bus.
// Side-Effect:
//////////////////////////////////////////////////////////////////////

static void
AbortDDC()
{
    BYTE CPDesire,SWReset,DDCMaster ;
    BYTE uc, timeout ;
    // save the SW reset,DDC master,and CP Desire setting.
    SWReset = HDMITX_ReadI2C_Byte(REG_TX_SW_RST) ;
    CPDesire = HDMITX_ReadI2C_Byte(REG_TX_HDCP_DESIRE) ;
    DDCMaster = HDMITX_ReadI2C_Byte(REG_TX_DDC_MASTER_CTRL) ;


    HDMITX_WriteI2C_Byte(REG_TX_HDCP_DESIRE,CPDesire&(~B_CPDESIRE)) ; // @emily change order
    HDMITX_WriteI2C_Byte(REG_TX_SW_RST,SWReset|B_HDCP_RST) ;		 // @emily change order
    HDMITX_WriteI2C_Byte(REG_TX_DDC_MASTER_CTRL,B_MASTERDDC|B_MASTERHOST) ;
    HDMITX_WriteI2C_Byte(REG_TX_DDC_CMD,CMD_DDC_ABORT) ;

    for( timeout = 0 ; timeout < 200 ; timeout++ )
    {
        uc = HDMITX_ReadI2C_Byte(REG_TX_DDC_STATUS) ;
        if (uc&B_DDC_DONE)
        {
            break ; // success
        }

        if( uc & (B_DDC_NOACK|B_DDC_WAITBUS|B_DDC_ARBILOSE) )
        {
            ErrorF("AbortDDC Fail by reg16=%02X\n",uc) ;
            break ;
        }
        DelayMS(1) ; // delay 1 ms to stable.
    }

    // restore the SW reset,DDC master,and CP Desire setting.
    HDMITX_WriteI2C_Byte(REG_TX_SW_RST,SWReset) ;
    HDMITX_WriteI2C_Byte(REG_TX_HDCP_DESIRE,CPDesire) ;
    HDMITX_WriteI2C_Byte(REG_TX_DDC_MASTER_CTRL,DDCMaster) ;
}

//////////////////////////////////////////////////////////////////////
// Packet and InfoFrame
//////////////////////////////////////////////////////////////////////

// ////////////////////////////////////////////////////////////////////////////////
// // Function: SetAVMute()
// // Parameter: N/A
// // Return: N/A
// // Remark: set AVMute as TRUE and enable GCP sending.
// // Side-Effect: N/A
// ////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
// void
// SetAVMute()
// {
//     Switch_HDMITX_Bank(0) ;
//     HDMITX_WriteI2C_Byte(REG_TX_GCP,B_SET_AVMUTE) ;
//     HDMITX_WriteI2C_Byte(REG_TX_PKT_GENERAL_CTRL,B_ENABLE_PKT|B_REPEAT_PKT) ;
// }

// ////////////////////////////////////////////////////////////////////////////////
// // Function: SetAVMute(FALSE)
// // Parameter: N/A
// // Return: N/A
// // Remark: clear AVMute as TRUE and enable GCP sending.
// // Side-Effect: N/A
// ////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////
// void
// SetAVMute(FALSE)
// {
//     Switch_HDMITX_Bank(0) ;
//     HDMITX_WriteI2C_Byte(REG_TX_GCP,B_CLR_AVMUTE) ;
//     HDMITX_WriteI2C_Byte(REG_TX_PKT_GENERAL_CTRL,B_ENABLE_PKT|B_REPEAT_PKT) ;
// }



//////////////////////////////////////////////////////////////////////
// Function: ReadEDID
// Parameter: pData - the pointer of buffer to receive EDID ucdata.
//            bSegment - the segment of EDID readback.
//            offset - the offset of EDID ucdata in the segment. in byte.
//            count - the read back bytes count,cannot exceed 32
// Return: ER_SUCCESS if successfully getting EDID. ER_FAIL otherwise.
// Remark: function for read EDID ucdata from reciever.
// Side-Effect: DDC master will set to be HOST. DDC FIFO will be used and dirty.
//////////////////////////////////////////////////////////////////////

static SYS_STATUS
ReadEDID(BYTE *pData,BYTE bSegment,BYTE offset,SHORT Count)
{
    SHORT RemainedCount,ReqCount ;
    BYTE bCurrOffset ;
    SHORT TimeOut ;
    BYTE *pBuff = pData ;
    BYTE ucdata ;

    // ErrorF("ReadEDID(%08lX,%d,%d,%d)\n",(ULONG)pData,bSegment,offset,Count) ;
    if(!pData)
    {
        ErrorF("ReadEDID(): Invallid pData pointer %08lX\n",(ULONG)pData) ;
        return ER_FAIL ;
    }

    if(HDMITX_ReadI2C_Byte(REG_TX_INT_STAT1) & B_INT_DDC_BUS_HANG)
    {
    	ErrorF("Called AboutDDC()\n") ;
        AbortDDC() ;

    }

    ClearDDCFIFO() ;

    RemainedCount = Count ;
    bCurrOffset = offset ;

    Switch_HDMITX_Bank(0) ;

    while(RemainedCount > 0)
    {

        ReqCount = (RemainedCount > DDC_FIFO_MAXREQ)?DDC_FIFO_MAXREQ:RemainedCount ;
        ErrorF("ReadEDID(): ReqCount = %d,bCurrOffset = %d\n",ReqCount,bCurrOffset) ;

        HDMITX_WriteI2C_Byte(REG_TX_DDC_MASTER_CTRL,B_MASTERDDC|B_MASTERHOST) ;
        HDMITX_WriteI2C_Byte(REG_TX_DDC_CMD,CMD_FIFO_CLR) ;

        for(TimeOut = 0 ; TimeOut < 200 ; TimeOut++)
    	{
		    ucdata = HDMITX_ReadI2C_Byte(REG_TX_DDC_STATUS) ;

		    if(ucdata&B_DDC_DONE)
		    {
		        break ;
		    }

		    if((ucdata & B_DDC_ERROR)||(HDMITX_ReadI2C_Byte(REG_TX_INT_STAT1) & B_INT_DDC_BUS_HANG))
		    {
		    	ErrorF("Called AboutDDC()\n") ;
		        AbortDDC() ;
				return ER_FAIL ;
		    }
    	}

        HDMITX_WriteI2C_Byte(REG_TX_DDC_MASTER_CTRL,B_MASTERDDC|B_MASTERHOST) ;
        HDMITX_WriteI2C_Byte(REG_TX_DDC_HEADER,DDC_EDID_ADDRESS) ; // for EDID ucdata get
        HDMITX_WriteI2C_Byte(REG_TX_DDC_REQOFF,bCurrOffset) ;
        HDMITX_WriteI2C_Byte(REG_TX_DDC_REQCOUNT,(BYTE)ReqCount) ;
        HDMITX_WriteI2C_Byte(REG_TX_DDC_EDIDSEG,bSegment) ;
        HDMITX_WriteI2C_Byte(REG_TX_DDC_CMD,CMD_EDID_READ) ;

        bCurrOffset += ReqCount ;
        RemainedCount -= ReqCount ;

        for(TimeOut = 250 ; TimeOut > 0 ; TimeOut --)
        {
            DelayMS(1) ;
            ucdata = HDMITX_ReadI2C_Byte(REG_TX_DDC_STATUS) ;
            if(ucdata & B_DDC_DONE)
            {
                break ;
            }

            if(ucdata & B_DDC_ERROR)
            {
                ErrorF("ReadEDID(): DDC_STATUS = %02X,fail.\n",ucdata) ;
                return ER_FAIL ;
            }
        }

        if(TimeOut == 0)
        {
            ErrorF("ReadEDID(): DDC TimeOut (DDC_STATUS = %02X). \n",ucdata) ;
            return ER_FAIL ;
        }

        do
        {
            *(pBuff++) = HDMITX_ReadI2C_Byte(REG_TX_DDC_READFIFO) ;
            ReqCount -- ;
        }while(ReqCount > 0) ;

    }

    return ER_SUCCESS ;
}



#ifdef SUPPORT_HDCP
//////////////////////////////////////////////////////////////////////
// Authentication
//////////////////////////////////////////////////////////////////////
static void
HDCP_ClearAuthInterrupt()
{
    BYTE uc ;
    uc = HDMITX_ReadI2C_Byte(REG_TX_INT_MASK2) & (~(B_KSVLISTCHK_MASK|B_T_AUTH_DONE_MASK|B_AUTH_FAIL_MASK));
    HDMITX_WriteI2C_Byte(REG_TX_INT_CLR0,B_CLR_AUTH_FAIL|B_CLR_AUTH_DONE|B_CLR_KSVLISTCHK) ;
    HDMITX_WriteI2C_Byte(REG_TX_INT_CLR1,0) ;
    HDMITX_WriteI2C_Byte(REG_TX_SYS_STATUS,B_INTACTDONE) ;
}

static void
HDCP_ResetAuth()
{
    HDMITX_WriteI2C_Byte(REG_TX_LISTCTRL,0) ;
    HDMITX_WriteI2C_Byte(REG_TX_HDCP_DESIRE,0) ;
    HDMITX_OrREG_Byte(REG_TX_SW_RST,B_HDCP_RST) ;
    HDMITX_WriteI2C_Byte(REG_TX_DDC_MASTER_CTRL,B_MASTERDDC|B_MASTERHOST) ;
    HDCP_ClearAuthInterrupt() ;
    AbortDDC() ;
}
//////////////////////////////////////////////////////////////////////
// Function: HDCP_EnableEncryption
// Parameter: N/A
// Return: ER_SUCCESS if done.
// Remark: Set regC1 as zero to enable continue authentication.
// Side-Effect: register bank will reset to zero.
//////////////////////////////////////////////////////////////////////

static SYS_STATUS
HDCP_EnableEncryption()
{
    Switch_HDMITX_Bank(0) ;
	return HDMITX_WriteI2C_Byte(REG_TX_ENCRYPTION,B_ENABLE_ENCRYPTION);
}


//////////////////////////////////////////////////////////////////////
// Function: HDCP_Auth_Fire()
// Parameter: N/A
// Return: N/A
// Remark: write anything to reg21 to enable HDCP authentication by HW
// Side-Effect: N/A
//////////////////////////////////////////////////////////////////////

static void
HDCP_Auth_Fire()
{
    // ErrorF("HDCP_Auth_Fire():\n") ;
    HDMITX_WriteI2C_Byte(REG_TX_DDC_MASTER_CTRL,B_MASTERDDC|B_MASTERHDCP) ; // MASTERHDCP,no need command but fire.
	HDMITX_WriteI2C_Byte(REG_TX_AUTHFIRE,1);
}

//////////////////////////////////////////////////////////////////////
// Function: HDCP_StartAnCipher
// Parameter: N/A
// Return: N/A
// Remark: Start the Cipher to free run for random number. When stop,An is
//         ready in Reg30.
// Side-Effect: N/A
//////////////////////////////////////////////////////////////////////

static void
HDCP_StartAnCipher()
{
    HDMITX_WriteI2C_Byte(REG_TX_AN_GENERATE,B_START_CIPHER_GEN) ;
    DelayMS(1) ; // delay 1 ms
}

//////////////////////////////////////////////////////////////////////
// Function: HDCP_StopAnCipher
// Parameter: N/A
// Return: N/A
// Remark: Stop the Cipher,and An is ready in Reg30.
// Side-Effect: N/A
//////////////////////////////////////////////////////////////////////

static void
HDCP_StopAnCipher()
{
    HDMITX_WriteI2C_Byte(REG_TX_AN_GENERATE,B_STOP_CIPHER_GEN) ;
}

//////////////////////////////////////////////////////////////////////
// Function: HDCP_GenerateAn
// Parameter: N/A
// Return: N/A
// Remark: start An ciper random run at first,then stop it. Software can get
//         an in reg30~reg38,the write to reg28~2F
// Side-Effect:
//////////////////////////////////////////////////////////////////////

static void
HDCP_GenerateAn()
{
    BYTE Data[8] ;

    HDCP_StartAnCipher() ;
    // HDMITX_WriteI2C_Byte(REG_TX_AN_GENERATE,B_START_CIPHER_GEN) ;
    // DelayMS(1) ; // delay 1 ms
    // HDMITX_WriteI2C_Byte(REG_TX_AN_GENERATE,B_STOP_CIPHER_GEN) ;

    HDCP_StopAnCipher() ;

    Switch_HDMITX_Bank(0) ;
    // new An is ready in reg30
    HDMITX_ReadI2C_ByteN(REG_TX_AN_GEN,Data,8) ;
    HDMITX_WriteI2C_ByteN(REG_TX_AN,Data,8) ;

}


//////////////////////////////////////////////////////////////////////
// Function: HDCP_GetBCaps
// Parameter: pBCaps - pointer of byte to get BCaps.
//            pBStatus - pointer of two bytes to get BStatus
// Return: ER_SUCCESS if successfully got BCaps and BStatus.
// Remark: get B status and capability from HDCP reciever via DDC bus.
// Side-Effect:
//////////////////////////////////////////////////////////////////////

static SYS_STATUS
HDCP_GetBCaps(PBYTE pBCaps ,PUSHORT pBStatus)
{
    BYTE ucdata ;
    BYTE TimeOut ;

    Switch_HDMITX_Bank(0) ;
    HDMITX_WriteI2C_Byte(REG_TX_DDC_MASTER_CTRL,B_MASTERDDC|B_MASTERHOST) ;
    HDMITX_WriteI2C_Byte(REG_TX_DDC_HEADER,DDC_HDCP_ADDRESS) ;
    HDMITX_WriteI2C_Byte(REG_TX_DDC_REQOFF,0x40) ; // BCaps offset
    HDMITX_WriteI2C_Byte(REG_TX_DDC_REQCOUNT,3) ;
    HDMITX_WriteI2C_Byte(REG_TX_DDC_CMD,CMD_DDC_SEQ_BURSTREAD) ;

    for(TimeOut = 200 ; TimeOut > 0 ; TimeOut --)
    {
        DelayMS(1) ;

        ucdata = HDMITX_ReadI2C_Byte(REG_TX_DDC_STATUS) ;
        if(ucdata & B_DDC_DONE)
        {
            //ErrorF("HDCP_GetBCaps(): DDC Done.\n") ;
            break ;
        }

        if(ucdata & B_DDC_ERROR)
        {
            ErrorF("HDCP_GetBCaps(): DDC fail by reg16=%02X.\n",ucdata) ;
            return ER_FAIL ;
        }
    }

    if(TimeOut == 0)
    {
        return ER_FAIL ;
    }

    HDMITX_ReadI2C_ByteN(REG_TX_BSTAT,(PBYTE)pBStatus,2) ;
    *pBCaps = HDMITX_ReadI2C_Byte(REG_TX_BCAP) ;
    return ER_SUCCESS ;

}


//////////////////////////////////////////////////////////////////////
// Function: HDCP_GetBKSV
// Parameter: pBKSV - pointer of 5 bytes buffer for getting BKSV
// Return: ER_SUCCESS if successfuly got BKSV from Rx.
// Remark: Get BKSV from HDCP reciever.
// Side-Effect: N/A
//////////////////////////////////////////////////////////////////////

static SYS_STATUS
HDCP_GetBKSV(BYTE *pBKSV)
{
    BYTE ucdata ;
    BYTE TimeOut ;

    Switch_HDMITX_Bank(0) ;
    HDMITX_WriteI2C_Byte(REG_TX_DDC_MASTER_CTRL,B_MASTERDDC|B_MASTERHOST) ;
    HDMITX_WriteI2C_Byte(REG_TX_DDC_HEADER,DDC_HDCP_ADDRESS) ;
    HDMITX_WriteI2C_Byte(REG_TX_DDC_REQOFF,0x00) ; // BKSV offset
    HDMITX_WriteI2C_Byte(REG_TX_DDC_REQCOUNT,5) ;
    HDMITX_WriteI2C_Byte(REG_TX_DDC_CMD,CMD_DDC_SEQ_BURSTREAD) ;

    for(TimeOut = 200 ; TimeOut > 0 ; TimeOut --)
    {
        DelayMS(1) ;

        ucdata = HDMITX_ReadI2C_Byte(REG_TX_DDC_STATUS) ;
        if(ucdata & B_DDC_DONE)
        {
            ErrorF("HDCP_GetBCaps(): DDC Done.\n") ;
            break ;
        }

        if(ucdata & B_DDC_ERROR)
        {
            ErrorF("HDCP_GetBCaps(): DDC No ack or arbilose,%x,maybe cable did not connected. Fail.\n",ucdata) ;
            return ER_FAIL ;
        }
    }

    if(TimeOut == 0)
    {
        return ER_FAIL ;
    }

    HDMITX_ReadI2C_ByteN(REG_TX_BKSV,(PBYTE)pBKSV,5) ;

    return ER_SUCCESS ;
}

//////////////////////////////////////////////////////////////////////
// Function:HDCP_Authenticate
// Parameter: N/A
// Return: ER_SUCCESS if Authenticated without error.
// Remark: do Authentication with Rx
// Side-Effect:
//  1. Instance[0].bAuthenticated global variable will be TRUE when authenticated.
//  2. Auth_done interrupt and AUTH_FAIL interrupt will be enabled.
//////////////////////////////////////////////////////////////////////
static BYTE
countbit(BYTE b)
{
    BYTE i,count ;
    for( i = 0, count = 0 ; i < 8 ; i++ )
    {
        if( b & (1<<i) )
        {
            count++ ;
        }
    }
    return count ;
}

static void
HDCP_Reset()
{
    BYTE uc ;
    uc = HDMITX_ReadI2C_Byte(REG_TX_SW_RST) | B_HDCP_RST ;
    HDMITX_WriteI2C_Byte(REG_TX_SW_RST,uc) ;
    HDMITX_WriteI2C_Byte(REG_TX_HDCP_DESIRE,0) ;
    HDMITX_WriteI2C_Byte(REG_TX_LISTCTRL,0) ;
    HDMITX_WriteI2C_Byte(REG_TX_DDC_MASTER_CTRL,B_MASTERHOST) ;
    ClearDDCFIFO() ;
    AbortDDC() ;
}

static SYS_STATUS
HDCP_Authenticate()
{
    BYTE ucdata ;
    BYTE BCaps ;
    USHORT BStatus ;
    USHORT TimeOut ;

  // richard   BYTE revoked = FALSE ;
    BYTE BKSV[5] ;

    Instance[0].bAuthenticated = FALSE ;

    // Authenticate should be called after AFE setup up.

    ErrorF("HDCP_Authenticate():\n") ;
	HDCP_Reset() ;
    // ClearDDCFIFO() ;
    // AbortDDC() ;

    if(HDCP_GetBCaps(&BCaps,&BStatus) != ER_SUCCESS)
    {
        ErrorF("HDCP_GetBCaps fail.\n") ;
        return ER_FAIL ;
    }


	if(Instance[0].bHDMIMode)
	{
		if((BStatus & B_CAP_HDMI_MODE)==0)
		{
			ErrorF("Not a HDMI mode,do not authenticate and encryption. BCaps = %x BStatus = %x\n",BCaps,BStatus) ;
			return ER_FAIL ;
		}
	}

	ErrorF("BCaps = %02X BStatus = %04X\n",BCaps,BStatus) ;
    /*
    if((BStatus & M_DOWNSTREAM_COUNT)> 6)
    {
        ErrorF("Down Stream Count %d is over maximum supported number 6,fail.\n",(BStatus & M_DOWNSTREAM_COUNT)) ;
        return ER_FAIL ;
    }
    */

    HDCP_GetBKSV(BKSV) ;
	ErrorF("BKSV %02X %02X %02X %02X %02X\n",BKSV[0],BKSV[1],BKSV[2],BKSV[3],BKSV[4]) ;

	for(TimeOut = 0, ucdata = 0 ; TimeOut < 5 ; TimeOut ++)
	{
	    ucdata += countbit(BKSV[TimeOut]) ;
	}
	if( ucdata != 20 ) return ER_FAIL ;


    #ifdef SUPPORT_REVOKE_KSV
    HDCP_VerifyRevocationList(SRM1,BKSV,&revoked) ;
    if(revoked)
    {
        ErrorF("BKSV is revoked\n") ; return ER_FAIL ;
    }
    ErrorF("BKSV %02X %02X %02X %02X %02X is NOT %srevoked\n",BKSV[0],BKSV[1],BKSV[2],BKSV[3],BKSV[4],revoked?"not ":"") ;
    #endif // SUPPORT_DSSSHA

    Switch_HDMITX_Bank(0) ; // switch bank action should start on direct register writting of each function.

    // 2006/08/11 added by jjtseng
    // enable HDCP on CPDired enabled.
    HDMITX_AndREG_Byte(REG_TX_SW_RST,~(B_HDCP_RST)) ;
    //~jjtseng 2006/08/11

//    if(BCaps & B_CAP_HDCP_1p1)
//    {
//        ErrorF("RX support HDCP 1.1\n") ;
//        HDMITX_WriteI2C_Byte(REG_TX_HDCP_DESIRE,B_ENABLE_HDPC11|B_CPDESIRE) ;
//    }
//    else
//    {
//        ErrorF("RX not support HDCP 1.1\n") ;
    HDMITX_WriteI2C_Byte(REG_TX_HDCP_DESIRE,B_CPDESIRE) ;
//    }


    // HDMITX_WriteI2C_Byte(REG_TX_INT_CLR0,B_CLR_AUTH_DONE|B_CLR_AUTH_FAIL|B_CLR_KSVLISTCHK) ;
    // HDMITX_WriteI2C_Byte(REG_TX_INT_CLR1,0) ; // don't clear other settings.
    // ucdata = HDMITX_ReadI2C_Byte(REG_TX_SYS_STATUS) ;
    // ucdata = (ucdata & M_CTSINTSTEP) | B_INTACTDONE ;
    // HDMITX_WriteI2C_Byte(REG_TX_SYS_STATUS,ucdata) ; // clear action.

    // HDMITX_AndREG_Byte(REG_TX_INT_MASK2,~(B_AUTH_FAIL_MASK|B_T_AUTH_DONE_MASK)) ;    // enable GetBCaps Interrupt
    HDCP_ClearAuthInterrupt() ;
    ErrorF("int2 = %02X DDC_Status = %02X\n",HDMITX_ReadI2C_Byte(REG_TX_INT_STAT2),HDMITX_ReadI2C_Byte(REG_TX_DDC_STATUS)) ;


    HDCP_GenerateAn() ;
    HDMITX_WriteI2C_Byte(REG_TX_LISTCTRL,0) ;
    Instance[0].bAuthenticated = FALSE ;

    if((BCaps & B_CAP_HDMI_REPEATER) == 0)
    {
        HDCP_Auth_Fire();
        // wait for status ;

        for(TimeOut = 250 ; TimeOut > 0 ; TimeOut --)
        {
            DelayMS(5) ; // delay 1ms
            ucdata = HDMITX_ReadI2C_Byte(REG_TX_AUTH_STAT) ;
            ErrorF("reg46 = %02x reg16 = %02x\n",ucdata,HDMITX_ReadI2C_Byte(0x16)) ;

            if(ucdata & B_T_AUTH_DONE)
            {
                Instance[0].bAuthenticated = TRUE ;
                break ;
            }

            ucdata = HDMITX_ReadI2C_Byte(REG_TX_INT_STAT2) ;
            if(ucdata & B_INT_AUTH_FAIL)
            {
                /*
                HDMITX_WriteI2C_Byte(REG_TX_INT_CLR0,B_CLR_AUTH_FAIL) ;
                HDMITX_WriteI2C_Byte(REG_TX_INT_CLR1,0) ;
                HDMITX_WriteI2C_Byte(REG_TX_SYS_STATUS,B_INTACTDONE) ;
                HDMITX_WriteI2C_Byte(REG_TX_SYS_STATUS,0) ;
                */
                ErrorF("HDCP_Authenticate(): Authenticate fail\n") ;
                Instance[0].bAuthenticated = FALSE ;
                return ER_FAIL ;
            }
        }

        if(TimeOut == 0)
        {
             ErrorF("HDCP_Authenticate(): Time out. return fail\n") ;
             Instance[0].bAuthenticated = FALSE ;
             return ER_FAIL ;
        }
        return ER_SUCCESS ;
    }

    return HDCP_Authenticate_Repeater() ;
}

//////////////////////////////////////////////////////////////////////
// Function: HDCP_VerifyIntegration
// Parameter: N/A
// Return: ER_SUCCESS if success,if AUTH_FAIL interrupt status,return fail.
// Remark: no used now.
// Side-Effect:
//////////////////////////////////////////////////////////////////////

static SYS_STATUS
HDCP_VerifyIntegration()
{
 // richard   BYTE ucdata ;
    // if any interrupt issued a Auth fail,returned the Verify Integration fail.

    if(HDMITX_ReadI2C_Byte(REG_TX_INT_STAT1) & B_INT_AUTH_FAIL)
    {
        HDCP_ClearAuthInterrupt() ;
        Instance[0].bAuthenticated = FALSE ;
        return ER_FAIL ;
    }

    if(Instance[0].bAuthenticated == TRUE)
    {
        return ER_SUCCESS ;
    }

    return ER_FAIL ;
}

//////////////////////////////////////////////////////////////////////
// Function: HDCP_Authenticate_Repeater
// Parameter: BCaps and BStatus
// Return: ER_SUCCESS if success,if AUTH_FAIL interrupt status,return fail.
// Remark:
// Side-Effect: as Authentication
//////////////////////////////////////////////////////////////////////
static _XDATA BYTE KSVList[32] ;
static _XDATA BYTE Vr[20] ;
static _XDATA BYTE M0[8] ;

static void
HDCP_CancelRepeaterAuthenticate()
{
    ErrorF("HDCP_CancelRepeaterAuthenticate") ;
    HDMITX_WriteI2C_Byte(REG_TX_DDC_MASTER_CTRL,B_MASTERDDC|B_MASTERHOST) ;
    AbortDDC() ;
    HDMITX_WriteI2C_Byte(REG_TX_LISTCTRL,B_LISTFAIL|B_LISTDONE) ;
    HDCP_ClearAuthInterrupt() ;
}

static void
HDCP_ResumeRepeaterAuthenticate()
{
    HDMITX_WriteI2C_Byte(REG_TX_LISTCTRL,B_LISTDONE) ;
	HDMITX_WriteI2C_Byte(REG_TX_DDC_MASTER_CTRL,B_MASTERHDCP) ;
}


static SYS_STATUS
HDCP_GetKSVList(BYTE *pKSVList,BYTE cDownStream)
{
    BYTE TimeOut = 100 ;
	BYTE ucdata ;

	if(cDownStream == 0 || pKSVList == NULL)
	{
	    return ER_FAIL ;
	}

    HDMITX_WriteI2C_Byte(REG_TX_DDC_MASTER_CTRL,B_MASTERHOST) ;
    HDMITX_WriteI2C_Byte(REG_TX_DDC_HEADER,0x74) ;
    HDMITX_WriteI2C_Byte(REG_TX_DDC_REQOFF,0x43) ;
    HDMITX_WriteI2C_Byte(REG_TX_DDC_REQCOUNT,cDownStream * 5) ;
    HDMITX_WriteI2C_Byte(REG_TX_DDC_CMD,CMD_DDC_SEQ_BURSTREAD) ;


    for(TimeOut = 200 ; TimeOut > 0 ; TimeOut --)
    {

        ucdata = HDMITX_ReadI2C_Byte(REG_TX_DDC_STATUS) ;
        if(ucdata & B_DDC_DONE)
        {
            ErrorF("HDCP_GetKSVList(): DDC Done.\n") ;
            break ;
        }

        if(ucdata & B_DDC_ERROR)
        {
            ErrorF("HDCP_GetKSVList(): DDC Fail by REG_TX_DDC_STATUS = %x.\n",ucdata) ;
            return ER_FAIL ;
        }
        DelayMS(5) ;
    }

    if(TimeOut == 0)
    {
        return ER_FAIL ;
    }

    ErrorF("HDCP_GetKSVList(): KSV") ;
    for(TimeOut = 0 ; TimeOut < cDownStream * 5 ; TimeOut++)
    {
        pKSVList[TimeOut] = HDMITX_ReadI2C_Byte(REG_TX_DDC_READFIFO) ;
        ErrorF(" %02X",pKSVList[TimeOut]) ;
    }
    ErrorF("\n") ;
	return ER_SUCCESS ;
}

static SYS_STATUS
HDCP_GetVr(BYTE *pVr)
{
    BYTE TimeOut  ;
	BYTE ucdata ;

	if(pVr == NULL)
	{
	   // richard  return NULL ;
       return ER_FAIL;
	}

    HDMITX_WriteI2C_Byte(REG_TX_DDC_MASTER_CTRL,B_MASTERHOST) ;
    HDMITX_WriteI2C_Byte(REG_TX_DDC_HEADER,0x74) ;
    HDMITX_WriteI2C_Byte(REG_TX_DDC_REQOFF,0x20) ;
    HDMITX_WriteI2C_Byte(REG_TX_DDC_REQCOUNT,20) ;
    HDMITX_WriteI2C_Byte(REG_TX_DDC_CMD,CMD_DDC_SEQ_BURSTREAD) ;


    for(TimeOut = 200 ; TimeOut > 0 ; TimeOut --)
    {
        ucdata = HDMITX_ReadI2C_Byte(REG_TX_DDC_STATUS) ;
        if(ucdata & B_DDC_DONE)
        {
            ErrorF("HDCP_GetVr(): DDC Done.\n") ;
            break ;
        }

        if(ucdata & B_DDC_ERROR)
        {
            ErrorF("HDCP_GetVr(): DDC fail by REG_TX_DDC_STATUS = %x.\n",ucdata) ;
            return ER_FAIL ;
        }
        DelayMS(5) ;
    }

    if(TimeOut == 0)
    {
        ErrorF("HDCP_GetVr(): DDC fail by timeout.\n",ucdata) ;
        return ER_FAIL ;
    }

    Switch_HDMITX_Bank(0) ;

    for(TimeOut = 0 ; TimeOut < 5 ; TimeOut++)
    {
        HDMITX_WriteI2C_Byte(REG_TX_SHA_SEL ,TimeOut) ;
        pVr[TimeOut*4+3]  = (ULONG)HDMITX_ReadI2C_Byte(REG_TX_SHA_RD_BYTE1) ;
        pVr[TimeOut*4+2] = (ULONG)HDMITX_ReadI2C_Byte(REG_TX_SHA_RD_BYTE2) ;
        pVr[TimeOut*4+1] = (ULONG)HDMITX_ReadI2C_Byte(REG_TX_SHA_RD_BYTE3) ;
        pVr[TimeOut*4] = (ULONG)HDMITX_ReadI2C_Byte(REG_TX_SHA_RD_BYTE4) ;
		ErrorF("V' = %02X %02X %02X %02X\n",pVr[TimeOut*4],pVr[TimeOut*4+1],pVr[TimeOut*4+2],pVr[TimeOut*4+3]) ;
    }

    return ER_SUCCESS ;
}

static SYS_STATUS
HDCP_GetM0(BYTE *pM0)
{
	int i ;

    if(!pM0)
    {
        return ER_FAIL ;
    }

    HDMITX_WriteI2C_Byte(REG_TX_SHA_SEL,5) ; // read m0[31:0] from reg51~reg54
    pM0[0] = HDMITX_ReadI2C_Byte(REG_TX_SHA_RD_BYTE1) ;
    pM0[1] = HDMITX_ReadI2C_Byte(REG_TX_SHA_RD_BYTE2) ;
    pM0[2] = HDMITX_ReadI2C_Byte(REG_TX_SHA_RD_BYTE3) ;
    pM0[3] = HDMITX_ReadI2C_Byte(REG_TX_SHA_RD_BYTE4) ;
    HDMITX_WriteI2C_Byte(REG_TX_SHA_SEL,0) ; // read m0[39:32] from reg55
    pM0[4] = HDMITX_ReadI2C_Byte(REG_TX_AKSV_RD_BYTE5) ;
    HDMITX_WriteI2C_Byte(REG_TX_SHA_SEL,1) ; // read m0[47:40] from reg55
    pM0[5] = HDMITX_ReadI2C_Byte(REG_TX_AKSV_RD_BYTE5) ;
    HDMITX_WriteI2C_Byte(REG_TX_SHA_SEL,2) ; // read m0[55:48] from reg55
    pM0[6] = HDMITX_ReadI2C_Byte(REG_TX_AKSV_RD_BYTE5) ;
    HDMITX_WriteI2C_Byte(REG_TX_SHA_SEL,3) ; // read m0[63:56] from reg55
    pM0[7] = HDMITX_ReadI2C_Byte(REG_TX_AKSV_RD_BYTE5) ;

    ErrorF("M[] =") ;
	for(i = 0 ; i < 8 ; i++){
		ErrorF("0x%02x,",pM0[i]) ;
	}
	ErrorF("\n") ;
    return ER_SUCCESS ;
}

static _XDATA BYTE SHABuff[64] ;
static _XDATA BYTE V[20] ;

static _XDATA ULONG w[80];
static _XDATA ULONG sha[5] ;

#define rol(x,y) (((x) << (y)) | (((ULONG)x) >> (32-y)))

static void SHATransform(ULONG * h); // richard add
void SHATransform(ULONG * h)
{
	LONG t;


	for (t = 16; t < 80; t++) {
		ULONG tmp = w[t - 3] ^ w[t - 8] ^ w[t - 14] ^ w[t - 16];
		w[t] = rol(tmp,1);
		printf("w[%2d] = %08lX\n",t,w[t]) ;
	}

	h[0] = 0x67452301 ;
	h[1] = 0xefcdab89;
	h[2] = 0x98badcfe;
	h[3] = 0x10325476;
	h[4] = 0xc3d2e1f0;

	for (t = 0; t < 20; t++) {
		ULONG tmp =
			rol(h[0],5) + ((h[1] & h[2]) | (h[3] & ~h[1])) + h[4] + w[t] + 0x5a827999;
		printf("%08lX %08lX %08lX %08lX %08lX\n",h[0],h[1],h[2],h[3],h[4]) ;

		h[4] = h[3];
		h[3] = h[2];
		h[2] = rol(h[1],30);
		h[1] = h[0];
		h[0] = tmp;

	}
	for (t = 20; t < 40; t++) {
		ULONG tmp = rol(h[0],5) + (h[1] ^ h[2] ^ h[3]) + h[4] + w[t] + 0x6ed9eba1;
		printf("%08lX %08lX %08lX %08lX %08lX\n",h[0],h[1],h[2],h[3],h[4]) ;
		h[4] = h[3];
		h[3] = h[2];
		h[2] = rol(h[1],30);
		h[1] = h[0];
		h[0] = tmp;
	}
	for (t = 40; t < 60; t++) {
		ULONG tmp = rol(h[0],
						 5) + ((h[1] & h[2]) | (h[1] & h[3]) | (h[2] & h[3])) + h[4] + w[t] +
			0x8f1bbcdc;
		printf("%08lX %08lX %08lX %08lX %08lX\n",h[0],h[1],h[2],h[3],h[4]) ;
		h[4] = h[3];
		h[3] = h[2];
		h[2] = rol(h[1],30);
		h[1] = h[0];
		h[0] = tmp;
	}
	for (t = 60; t < 80; t++) {
		ULONG tmp = rol(h[0],5) + (h[1] ^ h[2] ^ h[3]) + h[4] + w[t] + 0xca62c1d6;
		printf("%08lX %08lX %08lX %08lX %08lX\n",h[0],h[1],h[2],h[3],h[4]) ;
		h[4] = h[3];
		h[3] = h[2];
		h[2] = rol(h[1],30);
		h[1] = h[0];
		h[0] = tmp;
	}
	printf("%08lX %08lX %08lX %08lX %08lX\n",h[0],h[1],h[2],h[3],h[4]) ;

	h[0] += 0x67452301 ;
	h[1] += 0xefcdab89;
	h[2] += 0x98badcfe;
	h[3] += 0x10325476;
	h[4] += 0xc3d2e1f0;
	printf("%08lX %08lX %08lX %08lX %08lX\n",h[0],h[1],h[2],h[3],h[4]) ;
}

/* ----------------------------------------------------------------------
 * Outer SHA algorithm: take an arbitrary length byte string,
 * convert it into 16-word blocks with the prescribed padding at
 * the end,and pass those blocks to the core SHA algorithm.
 */


void SHA_Simple(void *p,LONG len,BYTE *output)
{
	// SHA_State s;
    int i, t ;
    ULONG c ;
    char *pBuff = p ;


    for( i = 0 ; i < len ; i++ )
    {
        t = i/4 ;
        if( i%4 == 0 )
        {
            w[t] = 0 ;
        }
        c = pBuff[i] ;
        c <<= (3-(i%4))*8 ;
        w[t] |= c ;
        printf("pBuff[%d] = %02x, c = %08lX, w[%d] = %08lX\n",i,pBuff[i],c,t,w[t]) ;
    }
    t = i/4 ;
    if( i%4 == 0 )
    {
        w[t] = 0 ;
    }
    c = 0x80 << ((3-i%4)*24) ;
    w[t]|= c ; t++ ;
    for( ; t < 15 ; t++ )
    {
        w[t] = 0 ;
    }
    w[15] = len*8  ;

    for( t = 0 ; t< 16 ; t++ )
    {
        printf("w[%2d] = %08lX\n",t,w[t]) ;
    }

    SHATransform(sha) ;

    for( i = 0 ; i < 5 ; i++ )
    {
        output[i*4]   = (BYTE)((sha[i]>>24)&0xFF) ;
        output[i*4+1] = (BYTE)((sha[i]>>16)&0xFF) ;
        output[i*4+2] = (BYTE)((sha[i]>>8)&0xFF) ;
        output[i*4+3] = (BYTE)(sha[i]&0xFF) ;
    }
}

static SYS_STATUS
HDCP_CheckSHA(BYTE pM0[],USHORT BStatus,BYTE pKSVList[],int cDownStream,BYTE Vr[])
{
    int i,n ;

    for(i = 0 ; i < cDownStream*5 ; i++)
    {
        SHABuff[i] = pKSVList[i] ;
    }
    SHABuff[i++] = BStatus & 0xFF ;
    SHABuff[i++] = (BStatus>>8) & 0xFF ;
    for(n = 0 ; n < 8 ; n++,i++)
    {
        SHABuff[i] = pM0[n] ;
    }
    n = i ;
    // SHABuff[i++] = 0x80 ; // end mask
    for(; i < 64 ; i++)
    {
        SHABuff[i] = 0 ;
    }
    // n = cDownStream * 5 + 2 /* for BStatus */ + 8 /* for M0 */ ;
    // n *= 8 ;
    // SHABuff[62] = (n>>8) & 0xff ;
    // SHABuff[63] = (n>>8) & 0xff ;
    for(i = 0 ; i < 64 ; i++)
	{
		if(i % 16 == 0) printf("SHA[]: ") ;
		printf(" %02X",SHABuff[i]) ;
		if((i%16)==15) printf("\n") ;
	}
    SHA_Simple(SHABuff,n,V) ;
    printf("V[] =") ;
    for(i = 0 ; i < 20 ; i++)
    {
        printf(" %02X",V[i]) ;
    }
    printf("\nVr[] =") ;
    for(i = 0 ; i < 20 ; i++)
    {
        printf(" %02X",Vr[i]) ;
    }

    for(i = 0 ; i < 20 ; i++)
    {
        if(V[i] != Vr[i])
        {
            return ER_FAIL ;
        }
    }
    return ER_SUCCESS ;
}

static SYS_STATUS
HDCP_Authenticate_Repeater()
{
    BYTE uc ;
    #ifdef SUPPORT_DSSSHA
    BYTE revoked ;
    int i ;
    #else
    int i; // richard add
    BYTE revoked; // richard add
    #endif // _DSS_SHA_
	// BYTE test;
	// BYTE test06;
	// BYTE test07;
	// BYTE test08;
    BYTE cDownStream ;

    BYTE BCaps;
    USHORT BStatus ;
    USHORT TimeOut ;

	ErrorF("Authentication for repeater\n") ;
    // emily add for test,abort HDCP
    // 2007/10/01 marked by jj_tseng@chipadvanced.com
    // HDMITX_WriteI2C_Byte(0x20,0x00) ;
    // HDMITX_WriteI2C_Byte(0x04,0x01) ;
	// HDMITX_WriteI2C_Byte(0x10,0x01) ;
	// HDMITX_WriteI2C_Byte(0x15,0x0F) ;
	// DelayMS(100);
    // HDMITX_WriteI2C_Byte(0x04,0x00) ;
	// HDMITX_WriteI2C_Byte(0x10,0x00) ;
	// HDMITX_WriteI2C_Byte(0x20,0x01) ;
	// DelayMS(100);
	// test07 = HDMITX_ReadI2C_Byte(0x7)  ;
	// test06 = HDMITX_ReadI2C_Byte(0x6);
	// test08 = HDMITX_ReadI2C_Byte(0x8);
	//~jj_tseng@chipadvanced.com
	// end emily add for test
    //////////////////////////////////////
    // Authenticate Fired
    //////////////////////////////////////

    HDCP_GetBCaps(&BCaps,&BStatus) ;
	DelayMS(2);
    HDCP_Auth_Fire();
	DelayMS(550); // emily add for test

    for(TimeOut = 250*6 ; TimeOut > 0 ; TimeOut --)
    {

        uc = HDMITX_ReadI2C_Byte(REG_TX_INT_STAT1) ;
        if(uc & B_INT_DDC_BUS_HANG)
        {
            ErrorF("DDC Bus hang\n") ;
            goto HDCP_Repeater_Fail ;
        }

        uc = HDMITX_ReadI2C_Byte(REG_TX_INT_STAT2) ;

        if(uc & B_INT_AUTH_FAIL)
        {
			/*
            HDMITX_WriteI2C_Byte(REG_TX_INT_CLR0,B_CLR_AUTH_FAIL) ;
            HDMITX_WriteI2C_Byte(REG_TX_INT_CLR1,0) ;
            HDMITX_WriteI2C_Byte(REG_TX_SYS_STATUS,B_INTACTDONE) ;
            HDMITX_WriteI2C_Byte(REG_TX_SYS_STATUS,0) ;
			*/
            ErrorF("HDCP_Authenticate_Repeater(): B_INT_AUTH_FAIL.\n") ;
            goto HDCP_Repeater_Fail ;
        }
        // emily add for test
		// test =(HDMITX_ReadI2C_Byte(0x7)&0x4)>>2 ;
        if(uc & B_INT_KSVLIST_CHK)
        {
            HDMITX_WriteI2C_Byte(REG_TX_INT_CLR0,B_CLR_KSVLISTCHK) ;
            HDMITX_WriteI2C_Byte(REG_TX_INT_CLR1,0) ;
            HDMITX_WriteI2C_Byte(REG_TX_SYS_STATUS,B_INTACTDONE) ;
            HDMITX_WriteI2C_Byte(REG_TX_SYS_STATUS,0) ;
            ErrorF("B_INT_KSVLIST_CHK\n") ;
            break ;
        }

        DelayMS(5) ;
    }

    if(TimeOut == 0)
    {
        ErrorF("Time out for wait KSV List checking interrupt\n") ;
        goto HDCP_Repeater_Fail ;
    }

    ///////////////////////////////////////
    // clear KSVList check interrupt.
    ///////////////////////////////////////

    for(TimeOut = 500 ; TimeOut > 0 ; TimeOut --)
    {
		if((TimeOut % 100) == 0)
		{
		    ErrorF("Wait KSV FIFO Ready %d\n",TimeOut) ;
		}

        if(HDCP_GetBCaps(&BCaps,&BStatus) == ER_FAIL)
        {
            ErrorF("Get BCaps fail\n") ;
            goto HDCP_Repeater_Fail ;
        }

        if(BCaps & B_CAP_KSV_FIFO_RDY)
        {
			 ErrorF("FIFO Ready\n") ;
			 break ;
        }
        DelayMS(5) ;

    }

    if(TimeOut == 0)
    {
        ErrorF("Get KSV FIFO ready TimeOut\n") ;
        goto HDCP_Repeater_Fail ;
    }

	ErrorF("Wait timeout = %d\n",TimeOut) ;

    ClearDDCFIFO() ;
    GenerateDDCSCLK() ;
    cDownStream =  (BStatus & M_DOWNSTREAM_COUNT) ;

    if(cDownStream == 0 || cDownStream > 6 || BStatus & (B_MAX_CASCADE_EXCEEDED|B_DOWNSTREAM_OVER))
    {
        ErrorF("Invalid Down stream count,fail\n") ;
        goto HDCP_Repeater_Fail ;
    }


    if(HDCP_GetKSVList(KSVList,cDownStream) == ER_FAIL)
    {
        goto HDCP_Repeater_Fail ;
    }

    for(i = 0 ; i < cDownStream ; i++)
    {
		revoked=FALSE ; uc = 0 ;
		for( TimeOut = 0 ; TimeOut < 5 ; TimeOut++ )
		{
		    // check bit count
		    uc += countbit(KSVList[i*5+TimeOut]) ;
		}
		if( uc != 20 ) revoked = TRUE ;
    #ifdef SUPPORT_REVOKE_KSV
        HDCP_VerifyRevocationList(SRM1,&KSVList[i*5],&revoked) ;
    #endif
        if(revoked)
        {
            ErrorF("KSVFIFO[%d] = %02X %02X %02X %02X %02X is revoked\n",i,KSVList[i*5],KSVList[i*5+1],KSVList[i*5+2],KSVList[i*5+3],KSVList[i*5+4]) ;
			 goto HDCP_Repeater_Fail ;
        }
    }


    if(HDCP_GetVr(Vr) == ER_FAIL)
    {
        goto HDCP_Repeater_Fail ;
    }

    if(HDCP_GetM0(M0) == ER_FAIL)
    {
        goto HDCP_Repeater_Fail ;
    }

    // do check SHA
    if(HDCP_CheckSHA(M0,BStatus,KSVList,cDownStream,Vr) == ER_FAIL)
    {
        goto HDCP_Repeater_Fail ;
    }


    HDCP_ResumeRepeaterAuthenticate() ;
	Instance[0].bAuthenticated = TRUE ;
    return ER_SUCCESS ;

HDCP_Repeater_Fail:
    HDCP_CancelRepeaterAuthenticate() ;
    return ER_FAIL ;
}

//////////////////////////////////////////////////////////////////////
// Function: HDCP_ResumeAuthentication
// Parameter: N/A
// Return: N/A
// Remark: called by interrupt handler to restart Authentication and Encryption.
// Side-Effect: as Authentication and Encryption.
//////////////////////////////////////////////////////////////////////

static void
HDCP_ResumeAuthentication()
{
    SetAVMute(TRUE) ;
    if(HDCP_Authenticate() == ER_SUCCESS)
	{
		HDCP_EnableEncryption() ;
	}
	SetAVMute(FALSE) ;
}



#endif // SUPPORT_HDCP


static void
ENABLE_NULL_PKT()
{

    HDMITX_WriteI2C_Byte(REG_TX_NULL_CTRL,B_ENABLE_PKT|B_REPEAT_PKT);
}


static void
ENABLE_ACP_PKT()
{

    HDMITX_WriteI2C_Byte(REG_TX_ACP_CTRL,B_ENABLE_PKT|B_REPEAT_PKT);
}


static void
ENABLE_ISRC1_PKT()
{

    HDMITX_WriteI2C_Byte(REG_TX_ISRC1_CTRL,B_ENABLE_PKT|B_REPEAT_PKT);
}


static void
ENABLE_ISRC2_PKT()
{

    HDMITX_WriteI2C_Byte(REG_TX_ISRC2_CTRL,B_ENABLE_PKT|B_REPEAT_PKT);
}


static void
ENABLE_AVI_INFOFRM_PKT()
{

    HDMITX_WriteI2C_Byte(REG_TX_AVI_INFOFRM_CTRL,B_ENABLE_PKT|B_REPEAT_PKT);
}


static void
ENABLE_AUD_INFOFRM_PKT()
{

    HDMITX_WriteI2C_Byte(REG_TX_AUD_INFOFRM_CTRL,B_ENABLE_PKT|B_REPEAT_PKT);
}


static void
ENABLE_SPD_INFOFRM_PKT()
{

    HDMITX_WriteI2C_Byte(REG_TX_SPD_INFOFRM_CTRL,B_ENABLE_PKT|B_REPEAT_PKT);
}


static void
ENABLE_MPG_INFOFRM_PKT()
{

    HDMITX_WriteI2C_Byte(REG_TX_MPG_INFOFRM_CTRL,B_ENABLE_PKT|B_REPEAT_PKT);
}

static void
DISABLE_NULL_PKT()
{

    HDMITX_WriteI2C_Byte(REG_TX_NULL_CTRL,0);
}


static void
DISABLE_ACP_PKT()
{

    HDMITX_WriteI2C_Byte(REG_TX_ACP_CTRL,0);
}


static void
DISABLE_ISRC1_PKT()
{

    HDMITX_WriteI2C_Byte(REG_TX_ISRC1_CTRL,0);
}


static void
DISABLE_ISRC2_PKT()
{

    HDMITX_WriteI2C_Byte(REG_TX_ISRC2_CTRL,0);
}


static void
DISABLE_AVI_INFOFRM_PKT()
{

    HDMITX_WriteI2C_Byte(REG_TX_AVI_INFOFRM_CTRL,0);
}


static void
DISABLE_AUD_INFOFRM_PKT()
{

    HDMITX_WriteI2C_Byte(REG_TX_AUD_INFOFRM_CTRL,0);
}


static void
DISABLE_SPD_INFOFRM_PKT()
{

    HDMITX_WriteI2C_Byte(REG_TX_SPD_INFOFRM_CTRL,0);
}


static void
DISABLE_MPG_INFOFRM_PKT()
{

    HDMITX_WriteI2C_Byte(REG_TX_MPG_INFOFRM_CTRL,0);
}

void TX_SetPixelRepetition(BYTE pixelrep, BYTE via_infoframe) {
    BYTE pllpr;

    Switch_HDMITX_Bank(0);
    pllpr = HDMITX_ReadI2C_Byte(REG_TX_CLK_CTRL1) & 0x2F;

    if (!via_infoframe)
        pllpr |= (1<<4)|((pixelrep&0x3)<<6);

    HDMITX_WriteI2C_Byte(REG_TX_CLK_CTRL1, pllpr);
}

//////////////////////////////////////////////////////////////////////
// Function: SetAVIInfoFrame()
// Parameter: pAVIInfoFrame - the pointer to HDMI AVI Infoframe ucData
// Return: N/A
// Remark: Fill the AVI InfoFrame ucData,and count checksum,then fill into
//         AVI InfoFrame registers.
// Side-Effect: N/A
//////////////////////////////////////////////////////////////////////

static SYS_STATUS
SetAVIInfoFrame(AVI_InfoFrame *pAVIInfoFrame)
{
    int i ;
    byte ucData ;

    if(!pAVIInfoFrame)
    {
        return ER_FAIL ;
    }

    Switch_HDMITX_Bank(1) ;
    for(i = 0,ucData = 0; i < AVI_INFOFRAME_LEN ; i++)
    {
        HDMITX_WriteI2C_Byte(REG_TX_AVIINFO_DB1+i+(i>=5),pAVIInfoFrame->pktbyte.AVI_DB[i]);
        ucData -= pAVIInfoFrame->pktbyte.AVI_DB[i] ;
    }
	ErrorF("SetAVIInfo(): ") ;
    //ErrorF("%02X ",HDMITX_ReadI2C_Byte(REG_TX_AVIINFO_DB1)) ;
    //ErrorF("%02X ",HDMITX_ReadI2C_Byte(REG_TX_AVIINFO_DB2)) ;
    //ErrorF("%02X ",HDMITX_ReadI2C_Byte(REG_TX_AVIINFO_DB3)) ;
    //ErrorF("%02X ",HDMITX_ReadI2C_Byte(REG_TX_AVIINFO_DB4)) ;
    //ErrorF("%02X ",HDMITX_ReadI2C_Byte(REG_TX_AVIINFO_DB5)) ;
    //ErrorF("%02X ",HDMITX_ReadI2C_Byte(REG_TX_AVIINFO_DB6)) ;
    //ErrorF("%02X ",HDMITX_ReadI2C_Byte(REG_TX_AVIINFO_DB7)) ;
    //ErrorF("%02X ",HDMITX_ReadI2C_Byte(REG_TX_AVIINFO_DB8)) ;
    //ErrorF("%02X ",HDMITX_ReadI2C_Byte(REG_TX_AVIINFO_DB9)) ;
    //ErrorF("%02X ",HDMITX_ReadI2C_Byte(REG_TX_AVIINFO_DB10)) ;
    //ErrorF("%02X ",HDMITX_ReadI2C_Byte(REG_TX_AVIINFO_DB11)) ;
    //ErrorF("%02X ",HDMITX_ReadI2C_Byte(REG_TX_AVIINFO_DB12)) ;
    //ErrorF("%02X ",HDMITX_ReadI2C_Byte(REG_TX_AVIINFO_DB13)) ;
	ErrorF("\n") ;
    ucData -= 0x80+AVI_INFOFRAME_VER+AVI_INFOFRAME_TYPE+AVI_INFOFRAME_LEN ;
	HDMITX_WriteI2C_Byte(REG_TX_AVIINFO_SUM,ucData);


    Switch_HDMITX_Bank(0) ;
    ENABLE_AVI_INFOFRM_PKT();
    return ER_SUCCESS ;
}

//////////////////////////////////////////////////////////////////////
// Function: SetAudioInfoFrame()
// Parameter: pAudioInfoFrame - the pointer to HDMI Audio Infoframe ucData
// Return: N/A
// Remark: Fill the Audio InfoFrame ucData,and count checksum,then fill into
//         Audio InfoFrame registers.
// Side-Effect: N/A
//////////////////////////////////////////////////////////////////////

static SYS_STATUS
SetAudioInfoFrame(Audio_InfoFrame *pAudioInfoFrame)
{
    int i ;
    BYTE ucData ;

    if(!pAudioInfoFrame)
    {
        return ER_FAIL ;
    }

    Switch_HDMITX_Bank(1) ;
    for(i = 0,ucData = 0 ; i< 5 ; i++)
    {
        HDMITX_WriteI2C_Byte(REG_TX_PKT_AUDINFO_CC+i,pAudioInfoFrame->pktbyte.AUD_DB[i]);
        ucData -= pAudioInfoFrame->pktbyte.AUD_DB[i] ;
    }
    ucData -= 0x80+AUDIO_INFOFRAME_VER+AUDIO_INFOFRAME_TYPE+AUDIO_INFOFRAME_LEN ;

    HDMITX_WriteI2C_Byte(REG_TX_PKT_AUDINFO_SUM,ucData) ;


    Switch_HDMITX_Bank(0) ;
    ENABLE_AUD_INFOFRM_PKT();
    return ER_SUCCESS ;
}

static SYS_STATUS
SetHDRInfoFrame(HDR_InfoFrame *pHDRInfoFrame)
{
    int i ;
    BYTE ucData ;

    if(!pHDRInfoFrame)
    {
        return ER_FAIL ;
    }

    Switch_HDMITX_Bank(1) ;

    HDMITX_WriteI2C_Byte(REG_TX_PKT_HB00, (0x80+pHDRInfoFrame->info.Type));
    HDMITX_WriteI2C_Byte(REG_TX_PKT_HB01, pHDRInfoFrame->info.Ver);
    HDMITX_WriteI2C_Byte(REG_TX_PKT_HB02, pHDRInfoFrame->info.Len);

    for(i = 0,ucData = 0 ; i< HDR_INFOFRAME_LEN ; i++)
    {
        HDMITX_WriteI2C_Byte(REG_TX_PKT_PB01+i, pHDRInfoFrame->pktbyte.HDR_DB[i]);
        ucData -= pHDRInfoFrame->pktbyte.HDR_DB[i] ;
    }
    ucData -= 0x80+HDR_INFOFRAME_VER+HDR_INFOFRAME_TYPE+HDR_INFOFRAME_LEN ;
    HDMITX_WriteI2C_Byte(REG_TX_PKT_PB00, ucData);

    Switch_HDMITX_Bank(0) ;
    ENABLE_NULL_PKT();
    return ER_SUCCESS ;
}

//////////////////////////////////////////////////////////////////////
// Function: SetSPDInfoFrame()
// Parameter: pSPDInfoFrame - the pointer to HDMI SPD Infoframe ucData
// Return: N/A
// Remark: Fill the SPD InfoFrame ucData,and count checksum,then fill into
//         SPD InfoFrame registers.
// Side-Effect: N/A
//////////////////////////////////////////////////////////////////////

static SYS_STATUS
SetSPDInfoFrame(SPD_InfoFrame *pSPDInfoFrame)
{
    int i ;
    BYTE ucData ;

    if(!pSPDInfoFrame)
    {
        return ER_FAIL ;
    }

    Switch_HDMITX_Bank(1) ;
    for(i = 0,ucData = 0 ; i < SPD_INFOFRAME_LEN ; i++)
    {
        ucData -= pSPDInfoFrame->pktbyte.SPD_DB[i] ;
        HDMITX_WriteI2C_Byte(REG_TX_PKT_SPDINFO_PB1+i,pSPDInfoFrame->pktbyte.SPD_DB[i]) ;
    }
    ucData -= 0x80+SPD_INFOFRAME_VER+SPD_INFOFRAME_TYPE+SPD_INFOFRAME_LEN ;
    HDMITX_WriteI2C_Byte(REG_TX_PKT_SPDINFO_SUM,ucData) ; // checksum
    Switch_HDMITX_Bank(0) ;
    ENABLE_SPD_INFOFRM_PKT();
    return ER_SUCCESS ;
}

//////////////////////////////////////////////////////////////////////
// Function: SetMPEGInfoFrame()
// Parameter: pMPEGInfoFrame - the pointer to HDMI MPEG Infoframe ucData
// Return: N/A
// Remark: Fill the MPEG InfoFrame ucData,and count checksum,then fill into
//         MPEG InfoFrame registers.
// Side-Effect: N/A
//////////////////////////////////////////////////////////////////////

static SYS_STATUS
SetMPEGInfoFrame(MPEG_InfoFrame *pMPGInfoFrame)
{
    int i ;
    BYTE ucData ;

    if(!pMPGInfoFrame)
    {
        return ER_FAIL ;
    }

    Switch_HDMITX_Bank(1) ;

    HDMITX_WriteI2C_Byte(REG_TX_PKT_MPGINFO_FMT,pMPGInfoFrame->info.FieldRepeat|(pMPGInfoFrame->info.MpegFrame<<1)) ;
    HDMITX_WriteI2C_Byte(REG_TX_PKG_MPGINFO_DB0,pMPGInfoFrame->pktbyte.MPG_DB[0]) ;
    HDMITX_WriteI2C_Byte(REG_TX_PKG_MPGINFO_DB1,pMPGInfoFrame->pktbyte.MPG_DB[1]) ;
    HDMITX_WriteI2C_Byte(REG_TX_PKG_MPGINFO_DB2,pMPGInfoFrame->pktbyte.MPG_DB[2]) ;
    HDMITX_WriteI2C_Byte(REG_TX_PKG_MPGINFO_DB3,pMPGInfoFrame->pktbyte.MPG_DB[3]) ;

    for(ucData = 0,i = 0 ; i < 5 ; i++)
    {
        ucData -= pMPGInfoFrame->pktbyte.MPG_DB[i] ;
    }
    ucData -= 0x80+MPEG_INFOFRAME_VER+MPEG_INFOFRAME_TYPE+MPEG_INFOFRAME_LEN ;

    HDMITX_WriteI2C_Byte(REG_TX_PKG_MPGINFO_SUM,ucData) ;

    Switch_HDMITX_Bank(0) ;
    ENABLE_SPD_INFOFRM_PKT() ;

    return ER_SUCCESS ;
}


//////////////////////////////////////////////////////////////////////
// Function: DumpCatHDMITXReg()
// Parameter: N/A
// Return: N/A
// Remark: Debug function,dumps the registers of CAT6611.
// Side-Effect: N/A
//////////////////////////////////////////////////////////////////////

#ifdef HDMITX_REG_DEBUG
static void
DumpCatHDMITXReg()
{
    int i,j ;
    BYTE reg ;
    BYTE bank ;
    BYTE ucData ;

    ErrorF("       ") ;
    for(j = 0 ; j < 16 ; j++)
    {
        ErrorF(" %02X",j) ;
        if((j == 3)||(j==7)||(j==11))
        {
            ErrorF("  ") ;
        }
    }
    ErrorF("\n        -----------------------------------------------------\n") ;

    Switch_HDMITX_Bank(0) ;

    for(i = 0 ; i < 0x100 ; i+=16)
    {
        ErrorF("[%3X]  ",i) ;
        for(j = 0 ; j < 16 ; j++)
        {
            ucData = HDMITX_ReadI2C_Byte((BYTE)((i+j)&0xFF)) ;
            ErrorF(" %02X",ucData) ;
            if((j == 3)||(j==7)||(j==11))
            {
                ErrorF(" -") ;
            }
        }
        ErrorF("\n") ;
        if((i % 0x40) == 0x30)
        {
            ErrorF("        -----------------------------------------------------\n") ;
        }
    }

    Switch_HDMITX_Bank(1) ;
    for(i = 0x130; i < 0x1B0 ; i+=16)
    {
        ErrorF("[%3X]  ",i) ;
        for(j = 0 ; j < 16 ; j++)
        {
            ucData = HDMITX_ReadI2C_Byte((BYTE)((i+j)&0xFF)) ;
            ErrorF(" %02X",ucData) ;
            if((j == 3)||(j==7)||(j==11))
            {
                ErrorF(" -") ;
            }
        }
        ErrorF("\n") ;
        if(i == 0x160)
        {
            ErrorF("        -----------------------------------------------------\n") ;
        }

    }
    Switch_HDMITX_Bank(0) ;
}
#endif
