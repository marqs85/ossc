#ifndef _CAT6611_SYS_H_
#define _CAT6611_SYS_H_
////////////////////////////////////////////////////////////////////////////////
// Internal Data Type
////////////////////////////////////////////////////////////////////////////////

typedef enum tagHDMI_Video_Type {
    HDMI_Unknown = 0 ,
    HDMI_640x480p60 = 1 ,
    HDMI_480p60,
    HDMI_480p60_16x9,
    HDMI_720p60,
    HDMI_1080i60,
    HDMI_480i60,
    HDMI_480i60_16x9,
    HDMI_240p60,
    HDMI_1080p60 = 16,
    HDMI_576p50,
    HDMI_576p50_16x9,
    HDMI_720p50 = 19,
    HDMI_1080i50,
    HDMI_576i50,
    HDMI_576i50_16x9,
    HDMI_288p50,
    HDMI_1080p50 = 31,
    /*HDMI_1080p24,
    HDMI_1080p25,
    HDMI_1080p30,
    HDMI_1080i120 = 46,*/ // richard add
} HDMI_Video_Type ;

typedef enum tagHDMI_Aspec {
    HDMI_4x3 ,
    HDMI_16x9
} HDMI_Aspec;

typedef enum tagHDMI_OutputColorMode {
    HDMI_RGB444,
    HDMI_YUV444,
    HDMI_YUV422
} HDMI_OutputColorMode ;

typedef enum tagHDMI_Colorimetry {
    HDMI_ITU601,
    HDMI_ITU709
} HDMI_Colorimetry ;

///////////////////////////////////////////////////////////////////////
// Output Mode Type
///////////////////////////////////////////////////////////////////////

#define RES_ASPEC_4x3 0
#define RES_ASPEC_16x9 1
#define F_MODE_REPT_NO 0
#define F_MODE_REPT_TWICE 1
#define F_MODE_REPT_QUATRO 3
#define F_MODE_CSC_ITU601 0
#define F_MODE_CSC_ITU709 1

void HDMITX_ChangeDisplayOption(HDMI_Video_Type VideoMode, HDMI_OutputColorMode OutputColorMode);
void HDMITX_SetOutput();



#endif // _CAT6611_SYS_H_
