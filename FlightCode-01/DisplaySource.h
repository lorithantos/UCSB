#pragma once

#include "DataSource.h"

class DisplaySource : public nsDataSource::DataSource, public nsDataSource::AutoRegister<DisplaySource>
{
    typedef UCSB_Datastream::DeviceHolder::CacheDataType CacheDataType;
    typedef nsDataSource::FieldIter FieldIter;

public:
    DisplaySource(void);
    virtual ~DisplaySource(void);

public :
    virtual bool Start()
    {
        m_start = m_start.Now();
        return true;
    }

    virtual bool AddDeviceTypes()
    {
        return nsDataSource::DataSource::AddDeviceTypes();
    }
    
    virtual bool TickImpl(InternalTime::internalTime const& now);

    virtual FieldIter Configure(FieldIter beg, FieldIter end)
    {
        const DWORD paramCount = 1;
        // read in the parameters for CounterSource
        if ((end - beg) < paramCount)
        {
            UCSBUtility::LogError(__FUNCTION__, __FILE__, __LINE__, "%d parameters expected"
                ", %d parameters found\n", paramCount, (end - beg));
            return end;
        }

        // Read the frequency from the configuration
        std::string frequency = *beg++;

        SetFrequency(static_cast<DWORD>(atoi(frequency.c_str())));

        return beg;
    }

public :
    static std::string GetName()
    {
        return "CommandLineDisplay";
    }

private :
    InternalTime::internalTime  m_start;
    CONSOLE_SCREEN_BUFFER_INFO  m_screenBufferInfo;
};
