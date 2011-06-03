
#include "stdafx.h"
#include "25xFxx.h"
#include "..\USB\USBDevice.h"
#include <boost\algorithm\string\predicate.hpp>

C25xFxx::C25xFxx(CUSB& usb, const memory::memory_id& id) 
    : CSerialFlash(usb, id)
{
    m_codeRDSR = RDSR; 
    m_codeWRSR = WRSR; 

    m_codeChipErase = CHIP_ERASE;
    m_codeRead      = BULK_NORM_READ;

    // determine AAI command according to chip name
    // B type : AAI word
    if(boost::icontains(m_uniqueId.TypeName,L"B") || boost::icontains(m_uniqueId.TypeName,L"W"))
    {
        m_codeProgram = AAI_2_BYTE ;
    }
    else
    {
        m_codeProgram = AAI_1_BYTE ;
    }


} 

bool C25xFxx::doWRSR(unsigned char cSR) //  virtual to override
{
    if(! m_usb.is_open())
        return false ;

    // wait until WIP = 0
    waitForWIP() ;

    // wait until WEL = 1
    DoEWSR() ;

    // send request
    CNTRPIPE_RQ rq ;
    vector<unsigned char> vInstruction ;

    vInstruction.push_back(m_codeWRSR) ;
    vInstruction.push_back(cSR) ;

    rq.Function = URB_FUNCTION_VENDOR_ENDPOINT ;
    rq.Direction = VENDOR_DIRECTION_OUT ;
    rq.Request = TRANSCEIVE ;
    rq.Value = RFU ;
    rq.Index = NO_RESULT_IN ;
    rq.Length = static_cast<unsigned long>(vInstruction.size()) ;

    if(! m_usb.OutCtrlRequest(rq, vInstruction))
        return false ;

    return true ;
}
    

// conjunct with WRSR command to make both effect
bool C25xFxx::DoEWSR()
{
    if(! m_usb.is_open())
        return false ;

    // send request
    CNTRPIPE_RQ rq ;
    vector<unsigned char> vInstruction(1) ;

    vInstruction[0] = EWSR ;

    rq.Function = URB_FUNCTION_VENDOR_ENDPOINT ;
    rq.Direction = VENDOR_DIRECTION_OUT ;
    rq.Request = TRANSCEIVE ;                
    rq.Value = RFU ;
    rq.Index = NO_RESULT_IN ;
    rq.Length = static_cast<unsigned long>(vInstruction.size()) ;

    if(! m_usb.OutCtrlRequest(rq, vInstruction))
        return false ;

    return true ;
}


tribool C25xFxx::protectBlock(tribool bProtect)
{
    // 7    6   5   4   3   2   1   0
    // BPL AAI RES RES BP1 BP0 WEL WIP


    if((is_protectbits_set() == bProtect) || boost::logic::indeterminate(bProtect)) return do_nothing;


    bool result = false ;
    unsigned char tmpSRVal ;
    unsigned char dstSRVal ;

    // un-protect block ,set BP1 BP0 to 0
    dstSRVal = 0x00 ;
    // protect block ,set BP1 BP0 to 1
    if(bProtect){
        dstSRVal |= 0x8C ;   
    }

    int numOfRetry = 1000 ;
    result = doWRSR(dstSRVal) ;
    result = doRDSR(tmpSRVal) ;
    while( (tmpSRVal & 0x01) &&  numOfRetry > 0) // WIP = TRUE;
    {
        // read SR
        result = doRDSR(tmpSRVal) ;

        if(! result) return false;

        numOfRetry -- ;

    }; 

    return ((tmpSRVal ^ dstSRVal)& 0x0C ) ? false:true;
}

