#pragma once

#include "internalTime.h"
#include "utility.h"

#include <windows.h>
#include <daqx.h>

#include <string>
#include <vector>



namespace ioTech
{
static const DaqAdcTriggerSource STARTSOURCE= DatsImmediate;
static const DaqAdcTriggerSource STOPSOURCE	= DatsScanCount;
static const DWORD PREWRITE	                = 0;
static const DWORD CHANCOUNT	            = 6;
//static const DWORD SCANS		            = 5000;
//static const float RATE		            = 50.0f;	//you should use a low rate to allow for collection of pulses

class IOTech : public nsDataSource::DataSource, public nsDataSource::AutoRegister<IOTech>
{
public :
#pragma pack (push)
#pragma pack (1)
    struct Encoder
    {
        //WORD    analog[16];
        DWORD   digital[3];
        //WORD    DIO[3];
    };
#pragma pack (pop)
    typedef InternalTime::internalTime internalTime;

public :
    IOTech() : handle(-1), m_init(false), dataCount(0), 
        m_hMutex(INVALID_HANDLE_VALUE), m_rate(0.f), AutoRegister<IOTech>(0)
    {
        //Encoder empty = {};
        //lastValue = empty;
    }
    ~IOTech()
    {
        CloseHandle(m_hMutex);
    }

    bool Setup(std::string const& deviceName, float rate, DWORD scans, bool clearOnZ);
    bool Read();
    bool Shutdown();
    //Encoder GetEncoder() const
    //{
    //    Encoder retv = {};
    //    STATIC_ASSERT(sizeof(retv) == 12);
    //    
    //    WaitForSingleObject(GetMutex(), INFINITE);
    //    retv = lastValue;
    //    ReleaseMutex(GetMutex());

    //    return lastValue;
    //}

    template<typename ProcessFnType>
    bool ReadAndProcess(ProcessFnType& processFn)
    {
        if (!Read())
        {
            UCSBUtility::LogError(__FUNCTION__, __FILE__, __LINE__, "Failed to Read board\n");
            return false;
        }
     
        for(DWORD i=0;i<dataCount;++i)
        {
            processFn(buffer[i]);
        }

        return true;
    }

private :
    HANDLE GetMutex() const
    {
        if (m_hMutex == INVALID_HANDLE_VALUE)
        {
            m_hMutex = CreateMutex(NULL, false, NULL);
        }

        return m_hMutex;
    }
private :
    //Encoder                 lastValue;
    DaqHandleT              handle;
    std::vector<Encoder>    buffer;
    bool                    m_init;
    float                   m_rate;
    DWORD                   dataCount;
    mutable HANDLE          m_hMutex;
};

}; // End IOTech namespace
