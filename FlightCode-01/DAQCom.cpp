// DAQComTest-1.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

#include "DaqCOM.h"

using namespace UCSB_DAQCOM;


DaqCOMSource::DaqAcquisition::DaqAcquisition()
            : m_totalScans(0)
            , m_psa(NULL)
            , m_rgElems(NULL)
{
    // SHOWPROGRESS;

    ::CoInitialize(NULL);
    //Create DaqCOM objects.
    try
    {
        IDaqSystemPtr pSys(__uuidof(DaqSystem));

        m_pSys = pSys;
        m_pAcq = m_pSys->Add();
        m_pAcq->DataStore->AutoSizeBuffers = FALSE;
        m_pAcq->DataStore->BufferSizeInScans = 100000;
        m_pAcq->DataStore->IgnoreDataStoreOverruns = TRUE;


        m_pSysDevs = m_pAcq->AvailableDevices;
        m_pConfig = m_pAcq->Config;


        long numDevicesAvail = m_pAcq->AvailableDevices->Count;
        for (long i = 1; i <= numDevicesAvail; i++)
        {
            IAvailableDevicePtr pDevAvail = m_pAcq->AvailableDevices->Item[i];

            //DeviceType devType = pDevAvail->GetDeviceType();
            UCSBUtility::LogError(__FUNCTION__, __FILE__, __LINE__, 
                "Found Device %ls\n", pDevAvail->Name.GetBSTR());
        }

        // For now, just use the first device
        if (numDevicesAvail < 1)
        {
            //printf("No Devices Found\n");
            return;
        }

        // Select
        //printf("Doing Select\n");
        m_pDev = m_pSysDevs->CreateFromIndex(1);
        m_Daq = m_pDev;
        m_pDev->Open();
        m_pAIs = m_pDev->AnalogInputs;
        //m_pDIOs = m_pDev->DigitalIOs;

        // allocate the memory for the descriptor and the array data
        m_psa = SafeArrayCreateVector(VT_R4, 0, SCANCOUNT * 64);
        //	Fetch bounds
        long lLowerBound, lUpperBound;

        HRESULT hr = SafeArrayGetLBound(m_psa, 1, &lLowerBound);
        hr = SafeArrayGetUBound(m_psa, 1, &lUpperBound);
        hr = SafeArrayAccessData(m_psa, (void**)&m_rgElems);

    }
    catch(_com_error &)
    {
        //throw "Error in " __FUNCSIG__;
        UCSBUtility::LogError(__FUNCTION__, __FILE__, __LINE__, 
            "Unable to intantiate DaqAcquisition\n");
        ::CoInitialize(NULL);
    }	
}

DaqCOMSource::DaqAcquisition::~DaqAcquisition() 
{
    // release lock on array state
    if (m_psa)
    {
        SafeArrayUnaccessData(m_psa);  
        SafeArrayDestroy(m_psa);
    }

    if (m_pAIs)
    {
        m_pAIs->RemoveAll();
    }
    if (m_pDev)
    {
        m_pDev->DigitalIOs->RemoveAll();
    }
    if (m_Daq)
    {
        m_Daq->SetPoints->RemoveAll();
    }
    ::CoUninitialize();
}

// Ensure all of the COM pointers are cleaned up inside the CoInitialize/CoUninitialize calls
//void Test()
//{
//    DaqAcquisition daq;
//    daq.Setup();
//    daq.Arm();
//    daq.Start();
//
//    SetConsoleTitle(L"Press any key to stop data acquisition");
//    ClearScreen();
//
//    while(!_kbhit())
//    {
//        try
//        {
//            daq.GetData();
//        }
//        catch(_com_error &)
//        {
//            //printf("Error: %ls\n0x%x\nPress a key to exit", e.ErrorMessage(), e.Error());
//            //printf("Channel %d (%ls) does not support the IAnalogChannelPtr interface\n (%d)\n", i, 
//            //    m_pAI->Channels->GetItem(i + 1)->Name.GetBSTR(), 
//            //    m_pAI->Channels->GetItem(i + 1)->GetObjectType());
//            _getch();
//            return;
//        }
//        catch(LPCSTR msg)
//        {
//            //printf("%s\n", msg);
//            return;
//        }
//    }
//
//    std::vector<float> const& channel0 = daq.GetChannel0();
//    FILE* pFile = NULL;
//    fopen_s(&pFile, "output.txt", "wt");
//    if (pFile)
//    {
//        for(std::vector<float>::const_iterator cit = channel0.begin();
//            cit != channel0.end(); ++cit)
//        {
//            fprintf(pFile, "%f\n", *cit);
//        }
//        fclose(pFile);
//    }
//
//}
//
//int _tmain(int argc, _TCHAR* argv[])
//{
//    ::CoInitialize(NULL);
//
//    Test();
//
//    ::CoUninitialize();
//
//    if (!_kbhit())
//    {
//        //printf ("\n\nPress a key to exit");
//    }
//    _getch();
//
//    return 0;
//}
//
