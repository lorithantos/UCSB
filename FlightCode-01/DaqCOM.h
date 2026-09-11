#pragma once

#include "stdafx.h"

#include "DataSource.h"
#include "internalTime.h"

#include <string>
#include <vector>

// #import "DaqCOM2.tlb" rename("LoadString", "DC2LoadString")
#import "libid:ABD18F2E-4C21-4433-B230-349C835B6BAC" rename("LoadString", "DC2LoadString")

namespace UCSB_DAQCOM
{
using namespace DAQCOMLib;

using nsDataSource::string;
using nsDataSource::strings;
using nsDataSource::FieldIter;

static const int SCANCOUNT  = 100;
static const long aquisitionChannels = 32;
//static const float SCANRATE = 100;

#define SHOWPROGRESS printf("Hit Line " __FUNCSIG__ " %d\n", __LINE__);

//#define SHOWPROGRESS printf("Hit Line %d\n", __LINE__);

class DaqCOMSource : public nsDataSource::DataSource, public nsDataSource::AutoRegister<DaqCOMSource>
{
#pragma pack(push)
#pragma pack(1)
    struct localData
    {
        float   channel00;
        float   channel01;
        float   channel02;
        float   channel03;
        float   channel04;
        float   channel05;
        float   channel06;
        float   channel07;
        float   channel08;
        float   channel09;
        float   channel10;
        float   channel11;
        float   channel12;
        float   channel13;
        float   channel14;
        float   channel15;
        
        float   channel16;
        float   channel17;
        float   channel18;
        float   channel19;
        float   channel20;
        float   channel21;
        float   channel22;
        float   channel23;
        float   channel24;
        float   channel25;
        float   channel26;
        float   channel27;
        float   channel28;
        float   channel29;
        float   channel30;
        float   channel31;

        static std::vector<nsDataSource::ChannelDefinition>  GetClassDescription()
        {
            static nsDataSource::ChannelDefinition definition[] =
            {
                {"SPFLOAT_I", "Channel 00"},
                {"SPFLOAT_I", "Channel 01"},
                {"SPFLOAT_I", "Channel 02"},
                {"SPFLOAT_I", "Channel 03"},
                {"SPFLOAT_I", "Channel 04"},
                {"SPFLOAT_I", "Channel 05"},
                {"SPFLOAT_I", "Channel 06"},
                {"SPFLOAT_I", "Channel 07"},
                {"SPFLOAT_I", "Channel 08"},
                {"SPFLOAT_I", "Channel 09"},
                {"SPFLOAT_I", "Channel 10"},
                {"SPFLOAT_I", "Channel 11"},
                {"SPFLOAT_I", "Channel 12"},
                {"SPFLOAT_I", "Channel 13"},
                {"SPFLOAT_I", "Channel 14"},
                {"SPFLOAT_I", "Channel 15"},

                {"SPFLOAT_I", "Channel 16"},
                {"SPFLOAT_I", "Channel 17"},
                {"SPFLOAT_I", "Channel 18"},
                {"SPFLOAT_I", "Channel 19"},
                {"SPFLOAT_I", "Channel 20"},
                {"SPFLOAT_I", "Channel 21"},
                {"SPFLOAT_I", "Channel 22"},
                {"SPFLOAT_I", "Channel 23"},
                {"SPFLOAT_I", "Channel 24"},
                {"SPFLOAT_I", "Channel 25"},
                {"SPFLOAT_I", "Channel 26"},
                {"SPFLOAT_I", "Channel 27"},
                {"SPFLOAT_I", "Channel 28"},
                {"SPFLOAT_I", "Channel 29"},
                {"SPFLOAT_I", "Channel 30"},
                {"SPFLOAT_I", "Channel 31"},
            };

            return UCSBUtility::ConvertToVector(definition);
        }
    };
#pragma pack(pop)

    class DaqAcquisition
    {
    public :
        DaqAcquisition();
        ~DaqAcquisition();

        void Setup(DWORD scanRate)
        {
            // SHOWPROGRESS;

            if (!isValid())
            {
                //printf("Can't do Setup on invalid object\n");
                return;
            }

            // SHOWPROGRESS;

            // Setup
            //printf("Doing Setup\n");
            m_pAI = m_pAIs->Add(aitDAQBRD3kUSBInputs,dbcDaqChannel0,(DeviceModulePosition)0);

            //m_pDIOs->Add(aitDAQBOOK3kUSBInputs, dbcD

            // SHOWPROGRESS;

            // Set all of the channels to differential mode
            UCSBUtility::LogError(__FUNCTION__, __FILE__, __LINE__, 
                "%d channels\n", m_pAI->Channels->Count);

            long channelCount = 0;
            for (long i=0; i < m_pAI->Channels->Count; ++i)
            {
                try
                {
                    IAnalogChannelPtr pAnalogChannel(m_pAI->Channels->GetItem(i + 1));
                    if (pAnalogChannel)
                    {
                        pAnalogChannel->DifferentialMode = true;
                        UCSBUtility::LogError(__FUNCTION__, __FILE__, __LINE__, "Differential set %d (%ls) (%d)\n", i, 
                            m_pAI->Channels->GetItem(i + 1)->Name.GetBSTR(), 
                            m_pAI->Channels->GetItem(i + 1)->GetObjectType());
                        
                        if (channelCount < aquisitionChannels)
                        {
                            UCSBUtility::LogError(__FUNCTION__, __FILE__, __LINE__, "Adding to Scan List %d (%ls) (%d)\n", i, 
                                m_pAI->Channels->GetItem(i + 1)->Name.GetBSTR(), 
                                m_pAI->Channels->GetItem(i + 1)->GetObjectType());
                            m_pAI->Channels->GetItem(i + 1)->AddToScanList();
                            ++channelCount;
                        }
                    }
                    else
                    {
                        //printf("Channel %d does not support the IDaqBoard3kUSBChannelPtr interface\n", i);
                    }
                }
                catch(_com_error &)
                {
                    //throw "Error in " __FUNCSIG__;
                    //printf("Channel %d (%ls) does not support the IAnalogChannelPtr interface\n (%d)\n", i, 
                    //    m_pAI->Channels->GetItem(i + 1)->Name.GetBSTR(), 
                    //    m_pAI->Channels->GetItem(i + 1)->GetObjectType());
                }	
            }

            //Set the number of scans to collect.
            m_pConfig->ScanCount = SCANCOUNT;

            // SHOWPROGRESS;

            //Set the rate a which to collect the above scans.
            m_pConfig->ScanRate = static_cast<float>(scanRate);

            // SHOWPROGRESS;

            //Specify the start/stop conditions.
            //Lets use a manual start and stop on a scan count.
            m_pAcq->Starts->GetItemByType((StartType)sttManual)->UseAsAcqStart();
            m_pAcq->Stops->GetItemByType((StopType)sptManual)->UseAsAcqStop();

            // SHOWPROGRESS;

        }

        void Arm()
        {

            // SHOWPROGRESS;

            if (!isValid())
            {
                //printf("Can't do Arm on invalid object\n");
                return;
            }

            // Arm
            //printf("Arming\n");
            //DaqCOM code that arms the acquisition and makes the device active.
            m_pScanList = m_pConfig->GetScanList();
            //printf("AcqState pre: %d\n", m_pAcq->GetAcqState());
            m_pAcq->Arm();	
            //printf("AcqState post: %d\n", m_pAcq->GetAcqState());

            // SHOWPROGRESS;

        }

        void Start()
        {

            // SHOWPROGRESS;

            if (!isValid() || m_pScanList == NULL)
            {
                //printf("Can't do Start on invalid object\n");
                return;
            }

            // Starting
            //printf("Starting\n");
            m_pAcq->Start();

            // SHOWPROGRESS;

        }

        void Stop()
        {

            // SHOWPROGRESS;

            m_pAcq->Stop();

            // SHOWPROGRESS;

        }

        void GetData()
        {
            // SHOWPROGRESS;

            if (!isValid())
            {
                //printf("Can't do GetData on invalid object\n");
                return;
            }

            //printf("Getting Data\n");

            //Use the Scan List pointer to determine how many channel in the list
            long NumOfChans = m_pScanList->Count;
            if(NumOfChans == 0){
                //printf("Error: No channel in the scan list!\n");
                return;
            } 

            // SHOWPROGRESS;

            if (m_pAcq->AcquiredScans == m_totalScans)
            {
                // Do nothing
                return;
            }

            // SHOWPROGRESS;

            long Scans = SCANCOUNT;
            long ScansRet;

            //Use the Acquisition pointer to get data
            try
            {
                ScansRet = m_pAcq->DataStore->FetchData(&m_psa, Scans);
                if (ScansRet == 0)
                {
                    return;
                }

                m_totalScans += ScansRet;
            }
            catch(_com_error &)
            {
                throw "Error in FetchData";
            }

            // SHOWPROGRESS;

            if (m_psa){
                if(ScansRet < 1){
                    //printf("Empty array!\n");
                    return;
                }


                // SHOWPROGRESS;

                SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), COORD());

                //char str[32];
                long index = 0;
                //printf("Scans %f %d total\n", m_pAcq->AcquiredScans, m_totalScans);
                std::string screen;
                screen.reserve(80 * 50);
                for (long ScanCounter = 0; ScanCounter < ScansRet;)
                {
                    localData* pLD = reinterpret_cast<localData*>(m_rgElems + index);
                    //sprintf(str, "%5d ", ScanCounter);
                    //screen += str;
                    m_channelData.push_back(*pLD);
                    index += NumOfChans;
                    //for(long chans = 0; chans < NumOfChans; chans++){
                    //    index++;
                    //    //sprintf(str, "%+2.5f ",m_rgElems[index++]);
                    //    //screen += str;
                    //}
                    ScanCounter++;
                    //screen += "\n";
                }
                //screen.append(80, ' ');

                //printf("%s\n", screen.c_str());
            }	
        }

        bool isValid() const
        {
            return m_pAIs != NULL;
        }

        std::vector<localData> const& GetChannelData() const { return m_channelData; }
        void ClearChannel0() { m_channelData.clear(); }

    private :
        IDaqSystemPtr           m_pSys;
        IAvailableDevicesPtr    m_pSysDevs;
        IDevicePtr              m_pDev;
        IAcqPtr                 m_pAcq;
        IConfigPtr              m_pConfig;
        IAnalogInputsPtr        m_pAIs;
        IAnalogInputPtr         m_pAI;
        //IDigitalIOsPtr          m_pDIOs;
        ISetPointPtr            m_SetPt;
        IDigitalIOPtr           m_Stat,m_DIO;
        IDaq3xxxPtr				m_Daq;
        IScanListPtr			m_pScanList;
        SAFEARRAY*              m_psa;
        DWORD                   m_totalScans;
        float*                  m_rgElems;
        std::vector<localData>  m_channelData;
    };

public:
    DaqCOMSource(void) : AutoRegister<DaqCOMSource>(0)
    {
    }
    virtual ~DaqCOMSource(void)
    {
    }

public :
    virtual bool Start()
    {
        // SHOWPROGRESS;

        m_daq.Arm();
        m_daq.Start();
        if (!m_daq.isValid())
        {
            return false;
        }

        GetDeviceHolder().RegisterWriter(*this, GetClassGUID());

        // SHOWPROGRESS;

        return true;        
    }

    virtual bool Stop()
    {
        m_daq.Stop();
        // Default behavior is to do nothing
        return true;
    }

    virtual bool AddDeviceTypes()
    {
        GetDeviceHolder().AddDeviceType(*this);
        return nsDataSource::DataSource::AddDeviceTypes();
    }

    virtual bool TickImpl(InternalTime::internalTime const& now)
    {
        // SHOWPROGRESS;

        m_daq.GetData();

        // SHOWPROGRESS;

        std::vector<localData> data = m_daq.GetChannelData();
        size_t index = 0;
        for (std::vector<localData>::const_iterator cit = data.begin();
            cit != data.end(); ++cit, ++index)
        {
            GetDeviceHolder().WriteDataWithCache(now + InternalTime::internalTime(index), *this, *cit);
            //printf ("%g, ", *cit);
        }

        //printf ("\n");

        m_daq.ClearChannel0();

        // SHOWPROGRESS;

        return true;
    }

    virtual FieldIter Configure(FieldIter beg, FieldIter end)
    {
        const DWORD paramCount = 2;
        // read in the parameters for DaqCOMSource
        if ((end - beg) < paramCount)
        {
            UCSBUtility::LogError(__FUNCTION__, __FILE__, __LINE__, "%d parameters expected"
                ", %d parameters found\n", paramCount, (end - beg));
            return end;
        }

        // Read the frequency from the configuration
        SetFrequency(UCSBUtility::ToINT<DWORD>(*beg++));

        m_daq.Setup(UCSBUtility::ToINT<DWORD>(*beg++));

        return beg;
    }

public :
    static std::string GetName()
    {
        return "DaqCOM";
    }

    static GUID GetClassGUID()
    {
        // {751CC155-0B96-460d-9792-40D2401D348B}
        static const GUID classGUID = 
        { 0x751cc155, 0xb96, 0x460d, { 0x97, 0x92, 0x40, 0xd2, 0x40, 0x1d, 0x34, 0x8b } };

        return classGUID;
    }

    static std::vector<nsDataSource::ChannelDefinition>  GetClassDescription()
    {
        return localData::GetClassDescription();
    }

private :

    localData       m_Data;
    DaqAcquisition  m_daq;
};



};