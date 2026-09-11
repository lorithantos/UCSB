// TestDaqBoard3000USB.cpp : Defines the entry point for the console application.
//
//
//  For DaqBoard3000USB Series
//
//	*** If your DaqBoard3000USB Series Device
//	*** has a name other than DaqBoard3000USB - Set the 'devName'
//  *** variable below to the apropriate name
//

#include "stdafx.h"

#include <windows.h>
#include <stdio.h>
#include <conio.h>
#include <stdlib.h>

#include "internalTime.h"

#include <string>


#include "daqx.h"
#pragma comment (lib, "DAQX")

using std::string;


const string g_DeviceName = "DaqBoard3031USB{325188}";


const DaqAdcTriggerSource STARTSOURCE	= DatsImmediate;
const DaqAdcTriggerSource STOPSOURCE	= DatsScanCount;
const DWORD CHANCOUNT	= 6;
const DWORD SCANS		= 25;
const float RATE		= 5;	//you should use a low rate to allow for collection of pulses

int _tmain(int /*argc*/, _TCHAR* /*argv[]*/)
{
    //used to connect to device
    char const* devName = g_DeviceName.c_str();

    WORD	buffer[SCANS*CHANCOUNT];

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


    //used to monitor scan
    DWORD active, retCount;
    DWORD i;
    DWORD bufPos = 0;
    //FLOAT tickTime = 20.83E-9f; // Used for Period scaling.	
    DWORD combinedResult = 0;	

    if (*devName == NULL)
    {
        printf("No device name specified\n");
        return 1;
    }

    printf( "Attempting to Connect with %s\n", devName );		
    //attempt to open device
    DaqHandleT handle = daqOpen(const_cast<LPSTR>(devName));

    if (handle == -1)//a return of -1 indicates failure
    {
        printf("Could not connect to device\n");
        return 1;
    }
    printf("Setting up scan...\n");

    // set # of scans to perform and scan mode
    daqAdcSetAcq(handle, DaamInfinitePost, 0, SCANS);	

    //Scan settings
    daqAdcSetScan(handle, channels, gains, flags, CHANCOUNT);  

    //set scan rate
    daqAdcSetFreq(handle, RATE);

    /*
    //Encoder Setup
    */	

    daqSetOption(handle, channels[0], DcofChannel, DcotCounterEnhMapChannel, DcovCounterEnhMap_Channel_2);
    daqSetOption(handle, channels[2], DcofChannel, DcotCounterEnhMapChannel, DcovCounterEnhMap_Channel_2);

    //Setup Channel 0
    daqSetOption(handle, channels[0], DcofChannel, DcotCounterEnhMeasurementMode,
        DcovCounterEnhMode_Encoder | DcovCounterEnhModeMask_32Bit | DcovCounterEnhEncoder_X1 |DcovCounterEnhDebounce500ns | DcovCounterEnhEncoder_ClearOnZ_On);	

    //Setup Channel 2	
    daqSetOption(handle, channels[2], DcofChannel, DcotCounterEnhMeasurementMode,
        DcovCounterEnhMode_Encoder | DcovCounterEnhModeMask_32Bit | DcovCounterEnhEncoder_X1 |DcovCounterEnhDebounce500ns );	


    //Setup Channel 4			
    daqSetOption(handle, channels[4], DcofChannel, DcotCounterEnhMeasurementMode,
        DcovCounterEnhMode_Encoder | DcovCounterEnhModeMask_32Bit| DcovCounterEnhEncoder_X1 |DcovCounterEnhDebounce500ns);			



    //Set buffer location, size and flag settings
    daqAdcTransferSetBuffer(handle, buffer, SCANS, DatmDriverBuf|DatmUpdateSingle|DatmCycleOn);	

    //Set to Trigger on software trigger
    daqSetTriggerEvent(handle, STARTSOURCE, (DaqEnhTrigSensT)0, channels[0], gains[0], flags[0],
        DaqTypeAnalogLocal, 0, 0, DaqStartEvent);
    //Set to Stop when the requested number of scans is completed
    daqSetTriggerEvent(handle, STOPSOURCE, (DaqEnhTrigSensT)0, channels[0], gains[0], flags[0],
        DaqTypeAnalogLocal, 0, 0, DaqStopEvent);

    //begin data acquisition
    printf("\nScanning...   Press any key to Exit\n");
    daqAdcTransferStart(handle);
    daqAdcArm(handle);

    InternalTime::internalTime startTime = InternalTime::internalTime::Now();
    InternalTime::internalTime nextTime = startTime + InternalTime::oneSecond / 10;

    
    printf("\n%-11s %-11s %-11s %-11s %-11s\n\n", "A(POS)", "B(COUNTS)", "Z(COUNTS)", "Time (0.1 m          q   s)", "retCount");
    do 
    {				
        daqAdcTransferGetStat(handle, &active,&retCount);	
        //transfer data (voltage readings) into computer memory and halt acquisition when done
        daqAdcTransferBufData(handle, buffer, SCANS, DabtmRetAvail, &retCount);

        bufPos = 0;
        for(i = 0; i < retCount; i++)
        {
            combinedResult = (DWORD)buffer[bufPos] + (((DWORD)buffer[bufPos+1])<<16);
            printf("%11i ",combinedResult);
            combinedResult = (DWORD)buffer[bufPos+2] + (((DWORD)buffer[bufPos+3])<<16);
            printf("%11i ",combinedResult);
            combinedResult = (DWORD)buffer[bufPos+4] + (((DWORD)buffer[bufPos+5])<<16);
            printf("%11i ",combinedResult);								
            printf("%11I64i ", (InternalTime::internalTime::Now() - startTime) / 100);
            printf("%11i ", retCount);
            printf("\r");

            bufPos += 6;
        }
        UINT64 sleepTime = (nextTime - InternalTime::internalTime::Now());
        sleepTime = sleepTime * 1000 / InternalTime::oneSecond;
        if(sleepTime < 100)
        {
            Sleep(static_cast<DWORD>(100 - sleepTime));
        }

    }
    while (( active & DaafAcqActive ) & !_kbhit()); 

    printf("\nScan Completed\n\n");
    //Disarm when completed
    daqAdcDisarm(handle);

    //close device connections
    daqClose(handle);
    printf("\nPress any key to exit.\n");
    _getch();

    return 0;
}

