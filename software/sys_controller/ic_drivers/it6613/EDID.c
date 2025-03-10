#include "hdmitx.h"
#include "edid.h"

#ifdef SUPPORT_EDID
static SYS_STATUS EDIDCheckSum(BYTE *pEDID) ;

static SYS_STATUS
EDIDCheckSum(BYTE *pEDID)
{
    BYTE CheckSum ;
    int i ;

	if( !pEDID )
	{
		return ER_FAIL ;
	}
    for( i = 0, CheckSum = 0 ; i < 128 ; i++ )
    {
        CheckSum += pEDID[i] ; CheckSum &= 0xFF ;
    }
    
    return (CheckSum == 0)?ER_SUCCESS:ER_FAIL ;
}

SYS_STATUS
ParseVESAEDID(BYTE *pEDID)
{
    if( ER_SUCCESS != EDIDCheckSum(pEDID) ) return ER_FAIL ;
    
	if( pEDID[0] != 0 || 
	    pEDID[7] != 0 || 
	    pEDID[1] != 0xFF || 
	    pEDID[2] != 0xFF || 
	    pEDID[3] != 0xFF || 
	    pEDID[4] != 0xFF || 
	    pEDID[5] != 0xFF || 
	    pEDID[6] != 0xFF )
	{
		return ER_FAIL ; // not a EDID 1.3 standard block.
	}

	/////////////////////////////////////////////////////////
	// if need to parse EDID property , put here.
	/////////////////////////////////////////////////////////
	
    return ER_SUCCESS ;

}

SYS_STATUS
ParseCEAEDID(BYTE *pCEAEDID, RX_CAP *pRxCap)
{
    BYTE offset,End ;
    BYTE count ;
    BYTE tag ;
    int i ;

	if( !pCEAEDID || !pRxCap ) return ER_FAIL ;

    pRxCap->ValidCEA = FALSE ;
    
    if( ER_SUCCESS != EDIDCheckSum(pCEAEDID) ) return ER_FAIL ;

    if( pCEAEDID[0] != 0x02 || pCEAEDID[1] != 0x03 ) return ER_SUCCESS ; // not a CEA BLOCK.
    End = pCEAEDID[2]  ; // CEA description.
    pRxCap->VideoMode = pCEAEDID[3] ;
    
    if (pRxCap->VideoMode & CEA_SUPPORT_YUV444)
        OS_PRINTF("Support Color: YUV444\n");
    if (pRxCap->VideoMode & CEA_SUPPORT_YUV422)
        OS_PRINTF("Support Color: YUV422\n");

    for( offset = 0 ; offset < 0x80 ; offset ++ )
    {
        if( (offset % 0x10) == 0 )
        {
            ErrorF("[%02X]", offset ) ;
        }
        else if((offset%0x10)==0x08)
        {
            ErrorF( " -" ) ;
        }
        ErrorF(" %02X",pCEAEDID[offset]) ;
        if((offset%0x10)==0x0f)
        {
            ErrorF("\n") ;
        }
    }
    
	pRxCap->VDOModeCount = 0 ;
    pRxCap->idxNativeVDOMode = 0xff ;
    for( offset = 4 ; offset < End ; )
    {
        tag = pCEAEDID[offset] >> 5 ;
        count = pCEAEDID[offset] & 0x1f ;
        switch( tag )
        {
        case 0x01: // Audio Data Block ;
            pRxCap->AUDDesCount = count/3 ;
            offset++ ;
            for( i = 0 ; i < pRxCap->AUDDesCount ; i++ )
            {
                pRxCap->AUDDes[i].uc[0] = pCEAEDID[offset++] ;
                pRxCap->AUDDes[i].uc[1] = pCEAEDID[offset++] ;
                pRxCap->AUDDes[i].uc[2] = pCEAEDID[offset++] ;

            }

            break ;

        case 0x02: // Video Data Block ;
            //pRxCap->VDOModeCount = 0 ;
            offset ++ ;
            for( i = 0,pRxCap->idxNativeVDOMode = 0xff ; i < count ; i++, offset++ )
            {
            	BYTE VIC ;
            	VIC = pCEAEDID[offset] & (~0x80) ;
            	OS_PRINTF("HDMI Sink VIC(Video Identify Code)=%d\n", VIC);
            	// if( FindModeTableEntryByVIC(VIC) != -1 )
            	{
	                pRxCap->VDOMode[pRxCap->VDOModeCount] = VIC ;
	                if( pCEAEDID[offset] & 0x80 )
	                {
	                    pRxCap->idxNativeVDOMode = (BYTE)pRxCap->VDOModeCount ;
	                    // iVideoModeSelect = pRxCap->VDOModeCount ;
	                }

	                pRxCap->VDOModeCount++ ;
            	}
            }
            break ;
        case 0x03: // Vendor Specific Data Block ;
            offset ++ ;
            pRxCap->IEEEOUI = (ULONG)pCEAEDID[offset+2] ;
            pRxCap->IEEEOUI <<= 8 ;
            pRxCap->IEEEOUI += (ULONG)pCEAEDID[offset+1] ;
            pRxCap->IEEEOUI <<= 8 ;
            pRxCap->IEEEOUI += (ULONG)pCEAEDID[offset] ;

            ///////////////////////////////////////////////////////////
            // For HDMI 1.3 extension handling.
            ///////////////////////////////////////////////////////////
            
            pRxCap->dc.uc = 0 ;
            pRxCap->MaxTMDSClock = 0  ;
            pRxCap->lsupport.uc = 0 ;
            pRxCap->ValidHDMI = (pRxCap->IEEEOUI==HDMI_IEEEOUI)? TRUE:FALSE ;
            if( (pRxCap->ValidHDMI) && (count > 5 ))
            {
                // HDMI 1.3 extension
                pRxCap->dc.uc = pCEAEDID[offset+5] ;
                pRxCap->MaxTMDSClock = pCEAEDID[offset+6] ;
                pRxCap->lsupport.uc = pCEAEDID[offset+7] ;
                
                if(pRxCap->lsupport.info.Latency_Present)
                {
                    pRxCap->V_Latency = pCEAEDID[offset+9] ;
                    pRxCap->A_Latency = pCEAEDID[offset+10] ;
                }
                
                if(pRxCap->lsupport.info.I_Latency_Present)
                {
                    pRxCap->V_I_Latency = pCEAEDID[offset+11] ;
                    pRxCap->A_I_Latency = pCEAEDID[offset+12] ;
                }
                
            }
            
            offset += count ; // ignore the remaind.
            break ;

        case 0x04: // Speaker Data Block ;
            offset ++ ;
            pRxCap->SpeakerAllocBlk.uc[0] = pCEAEDID[offset] ;
            pRxCap->SpeakerAllocBlk.uc[1] = pCEAEDID[offset+1] ;
            pRxCap->SpeakerAllocBlk.uc[2] = pCEAEDID[offset+2] ;
            offset += 3 ;
            break ;
        case 0x05: // VESA Data Block ;
            offset += count+1 ;
            break ;
        case 0x07: // Extended Data Block ;
            offset += count+1 ; //ignore
            break ;
        default:
            offset += count+1 ; // ignore
        }
    }

    pRxCap->ValidCEA = TRUE ;
    return ER_SUCCESS ;
}



#endif // SUPPORT_EDID
