// TestDaqBoard3000USB.cpp : Defines the entry point for the console application.
//
//
//  For DaqBoard3000USB Series
//
//	*** If your DaqBoard3000USB Series Device
//	*** has a name other than DaqBoard3000USB - Set the 'devName'
//  *** variable below to the apropriate name
//

/*
0-15 > ADC channels - not currently assigned
16-21 > encoders (32 bit values packed in 2 16 bit values)
22-24 > DIO
*/
#include "stdafx.h"

#include <windows.h>
#include <stdio.h>
#include <conio.h>
#include <stdlib.h>

#include "internalTime.h"
#include "ioTech.h"

#include <string>

#include "daqx.h"
#pragma comment (lib, "DAQX")

using std::string;
using namespace ioTech;


void CALLBACK IOTECH_ErrorHandlerFT(DaqHandleT handle, DaqError errCode)
{
    handle;
    char msg[512];

    daqFormatError(errCode, msg);
    UCSBUtility::LogError(__FUNCTION__, __FILE__, __LINE__, "%s\n", msg);
}

bool IOTech::Setup(string const& deviceName, float rate, DWORD scans, bool clearOnZ)
{
    daqSetDefaultErrorHandler(IOTECH_ErrorHandlerFT);

    Shutdown();

    //used to connect to device
    char const* devName = deviceName.c_str();

    m_rate = rate;

    buffer.resize(scans);

    // Scan List Setup
    // Scan List Setup
    DWORD    channels[CHANCOUNT] = {0, 0,	// Daq3000 Series Counters are 32 bits wide
        1, 1,	// to accomidate this the channel must be entered
        2, 2};  // into the scanlist twice

    DaqAdcGain  gains[CHANCOUNT] = {DgainDbd3kX1, DgainDbd3kX1, 
        DgainDbd3kX1, DgainDbd3kX1,
        DgainDbd3kX1, DgainDbd3kX1};

    DWORD       flags[CHANCOUNT] = {DafCtr32EnhLow, DafCtr32EnhHigh, 
        DafCtr32EnhLow, DafCtr32EnhHigh,
        DafCtr32EnhLow, DafCtr32EnhHigh};


    if (deviceName.size() == 0)
    {
        UCSBUtility::LogError(__FUNCTION__, __FILE__, __LINE__, "No device name specified\n");
        return m_init;
    }

    UCSBUtility::LogError(__FUNCTION__, __FILE__, __LINE__,  "Attempting to Connect with %s\n", deviceName.c_str() );		
    //attempt to open device
    handle = daqOpen(const_cast<LPSTR>(devName));

    if (handle == -1)//a return of -1 indicates failure
    {
        UCSBUtility::LogError(__FUNCTION__, __FILE__, __LINE__, "Could not connect to device %s - no data will be collected from it\n", deviceName.c_str());
        return m_init;
    }
    UCSBUtility::LogError(__FUNCTION__, __FILE__, __LINE__, "Connected with %s\n", deviceName.c_str());

    // set # of scans to perform and scan mode
    daqAdcSetAcq(handle, DaamInfinitePost, 0, scans);	

    //Scan settings
    daqAdcSetScan(handle, channels, gains, flags, CHANCOUNT);  

    //set scan rate
    daqAdcSetFreq(handle, m_rate);

    /*
    //Encoder Setup
    */	

    daqSetOption(handle, channels[0], DcofChannel, DcotCounterEnhMapChannel, DcovCounterEnhMap_Channel_2);
    daqSetOption(handle, channels[2], DcofChannel, DcotCounterEnhMapChannel, DcovCounterEnhMap_Channel_2);

    // Strange compiler behavior here
    // assigning a FLOAT from the enum works fine
    // assigning a FLOAT from |'d together enums works fine
    // assigning a FLOAT using a ternary operator - complains
    // FLOAT clearZFlag = clearOnZ ? DcovCounterEnhEncoder_ClearOnZ_On : DcovCounterEnhEncoder_ClearOnZ_Off;
    // This happens because the ? : operation has to have a type...simple assignment doesn't until
    // everything is resolved
    DWORD clearZFlag = clearOnZ ? DcovCounterEnhEncoder_ClearOnZ_On : DcovCounterEnhEncoder_ClearOnZ_Off;

    daqSetOption(handle, channels[0], DcofChannel, DcotCounterEnhMeasurementMode,
        static_cast<float>(DcovCounterEnhMode_Encoder | DcovCounterEnhModeMask_32Bit | 
        DcovCounterEnhEncoder_X1 |DcovCounterEnhDebounce500ns | clearZFlag));
        //DcovCounterEnhEncoder_ClearOnZ_On);	

    //Setup Channel 2	
    daqSetOption(handle, channels[2], DcofChannel, DcotCounterEnhMeasurementMode,
        DcovCounterEnhMode_Encoder | DcovCounterEnhModeMask_32Bit | DcovCounterEnhEncoder_X1 |DcovCounterEnhDebounce500ns );	


    //Setup Channel 4			
    daqSetOption(handle, channels[4], DcofChannel, DcotCounterEnhMeasurementMode,
        DcovCounterEnhMode_Encoder | DcovCounterEnhModeMask_32Bit| DcovCounterEnhEncoder_X1 |DcovCounterEnhDebounce500ns);			

    //Set buffer location, size and flag settings
    daqAdcTransferSetBuffer(handle, reinterpret_cast<PWORD>(&buffer[0]), scans, DatmDriverBuf|DatmUpdateSingle|DatmCycleOn);	

    //Set to Trigger on software trigger
    daqSetTriggerEvent(handle, STARTSOURCE, (DaqEnhTrigSensT)0, channels[0], gains[0], flags[0],
        DaqTypeAnalogLocal, 0, 0, DaqStartEvent);
    //Set to Stop when the requested number of scans is completed
    daqSetTriggerEvent(handle, STOPSOURCE, (DaqEnhTrigSensT)0, channels[0], gains[0], flags[0],
        DaqTypeAnalogLocal, 0, 0, DaqStopEvent);

    //begin data acquisition
    daqAdcTransferStart(handle);
    daqAdcArm(handle);

    m_init = true;
    return m_init;;
}


bool IOTech::Read()
{
    if (!m_init)
    {
        return false;
    }

    //DWORD bufPos = 0;
    DWORD active = 0;
    //FLOAT tickTime = 20.83E-9f; // Used for Period scaling.	
    //DWORD combinedResult = 0;	

    DWORD newDataCount = 0;

    daqAdcTransferGetStat(handle, &active, &newDataCount);
    if (newDataCount > 0)
    {
        daqAdcTransferBufData(handle, reinterpret_cast<WORD*>(&buffer[0]), 
            buffer.size(), DabtmRetAvail, &newDataCount);
        dataCount = newDataCount;

        WaitForSingleObject(GetMutex(), INFINITE);
        //lastValue = buffer[dataCount - 1];
        ReleaseMutex(GetMutex());
    }

    return active & DaafAcqActive;
}

bool IOTech::Shutdown()
{
    if (!m_init)
    {
        return true;
    }

    //UCSBUtility::LogError(__FUNCTION__, __FILE__, __LINE__, "\nScan Completed\n\n");
    //Disarm when completed
    daqAdcDisarm(handle);

    //close device connections
    daqClose(handle);

    m_init = false;

    return true;
}
