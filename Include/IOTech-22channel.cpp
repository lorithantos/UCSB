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

bool IOTech::Setup(string const& deviceName)
{
    Shutdown();

    //used to connect to device
    char const* devName = deviceName.c_str();

    buffer.resize(SCANS);

    // Scan List Setup
	DWORD    channels[CHANCOUNT] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 0, 0, 1, 1, 2, 2, 0, 1, 2};

    DaqAdcGain  gains[CHANCOUNT] = {DgainDbd3kX1, DgainDbd3kX1, 
        DgainDbd3kX1, DgainDbd3kX1,
        DgainDbd3kX1, DgainDbd3kX1,
        DgainDbd3kX1, DgainDbd3kX1,
        DgainDbd3kX1, DgainDbd3kX1,
        DgainDbd3kX1, DgainDbd3kX1, 
        DgainDbd3kX1, DgainDbd3kX1,
        DgainDbd3kX1, DgainDbd3kX1,
        DgainDbd3kX1, DgainDbd3kX1,
        DgainDbd3kX1, DgainDbd3kX1,
        DgainDbd3kX1, DgainDbd3kX1,
        DgainDbd3kX1, DgainDbd3kX1,
        DgainDbd3kX1};


    DWORD       flags[CHANCOUNT] = {DafBipolar, DafBipolar,
        DafBipolar, DafBipolar,
        DafBipolar, DafBipolar,
        DafBipolar, DafBipolar,
        DafBipolar, DafBipolar,
        DafBipolar, DafBipolar,
        DafBipolar, DafBipolar,
        DafBipolar, DafBipolar,
        DafCtr32EnhLow, DafCtr32EnhHigh,
        DafCtr32EnhLow, DafCtr32EnhHigh,
        DafCtr32EnhLow, DafCtr32EnhHigh,
        DafP2Local8, DafP2Local8, 
        DafP2Local8};


    if (deviceName.size() == 0)
    {
        fprintf(stderr, "No device name specified\n");
        return m_init;
    }

    fprintf(stderr,  "Attempting to Connect with %s\n", deviceName.c_str() );		
    //attempt to open device
    handle = daqOpen(const_cast<LPSTR>(devName));

    if (handle == -1)//a return of -1 indicates failure
    {
        fprintf(stderr, "Could not connect to device %s\n", deviceName.c_str());
        return m_init;
    }
    //fprintf(stderr, "Setting up scan...\n");

    // set # of scans to perform and scan mode
    daqAdcSetAcq(handle, DaamInfinitePost, 0, SCANS);	

    //Scan settings
    daqAdcSetScan(handle, channels, gains, flags, CHANCOUNT);  

    //set scan rate
    daqAdcSetFreq(handle, RATE);

    /*
    //Encoder Setup
    */	

    //set clock source
    daqAdcSetClockSource(handle, DacsExternalTTL);

    daqSetOption(handle, channels[16], DcofChannel, DcotCounterEnhMapChannel, DcovCounterEnhMap_Channel_2);
    daqSetOption(handle, channels[18], DcofChannel, DcotCounterEnhMapChannel, DcovCounterEnhMap_Channel_2);

    //Setup Channel 16
    daqSetOption(handle, channels[16], DcofChannel, DcotCounterEnhMeasurementMode,
        DcovCounterEnhMode_Encoder | DcovCounterEnhModeMask_32Bit | DcovCounterEnhEncoder_X4 | DcovCounterEnhRisingEdge | DcovCounterEnhEncoder_ClearOnZ_On | DcovCounterEnhTriggerAfterStable);	

    //Setup Channel 18	
    daqSetOption(handle, channels[18], DcofChannel, DcotCounterEnhMeasurementMode,
        DcovCounterEnhMode_Encoder | DcovCounterEnhModeMask_32Bit | DcovCounterEnhEncoder_X4 | DcovCounterEnhDebounceNone | DcovCounterEnhRisingEdge | DcovCounterEnhEncoder_ClearOnZ_Off | DcovCounterEnhTriggerAfterStable);	

    //Setup Channel 20			
    daqSetOption(handle, channels[20], DcofChannel, DcotCounterEnhMeasurementMode,
        DcovCounterEnhMode_Encoder | DcovCounterEnhModeMask_32Bit | DcovCounterEnhEncoder_X4 | DcovCounterEnhDebounceNone | DcovCounterEnhRisingEdge | DcovCounterEnhEncoder_ClearOnZ_Off | DcovCounterEnhTriggerAfterStable);	

    //Set buffer location, size and flag settings
    daqAdcTransferSetBuffer(handle, reinterpret_cast<WORD*>(&buffer[0]), 
        SCANS, DatmDriverBuf|DatmUpdateBlock|DatmCycleOn|DatmIgnoreOverruns);	

    //Set to Trigger on software trigger
    daqSetTriggerEvent(handle, STARTSOURCE, DetsRisingEdge, channels[16], gains[16], flags[16],
        DaqTypeAnalogLocal, 0, 0, DaqStartEvent);

    //Set to Stop when the requested number of scans is completed
    daqSetTriggerEvent(handle, STOPSOURCE, DetsRisingEdge, channels[16], gains[16], flags[16],
        DaqTypeAnalogLocal, 0, 0, DaqStopEvent);

    //set port A, B, and C as inputs for digital I/O
    DWORD config = 0;
    daqIOGet8255Conf(handle, 1 ,1, 1, 1, &config);

    //write settings to internal register
    daqIOWrite(handle, DiodtLocal8255, Diodp8255IR, 0, DioepP2, config);

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
            SCANS, DabtmRetAvail, &newDataCount);
        dataCount = newDataCount;

        WaitForSingleObject(GetMutex(), INFINITE);
        lastValue = buffer[dataCount - 1];
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

    //fprintf(stderr, "\nScan Completed\n\n");
    //Disarm when completed
    daqAdcDisarm(handle);

    //close device connections
    daqClose(handle);

    m_init = false;

    return true;
}