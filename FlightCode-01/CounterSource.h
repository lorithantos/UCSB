#pragma once

#include "DataSource.h"
#include "internalTime.h"

#include <string>

namespace UCSB_CounterSource
{

using nsDataSource::string;
using nsDataSource::strings;
using nsDataSource::FieldIter;

// Dictionary data

class CounterSource : public nsDataSource::DataSource, public nsDataSource::AutoRegister<CounterSource>
{
public:
    CounterSource(void);
    virtual ~CounterSource(void);

public :
    virtual bool Start()
    {
        GetDeviceHolder().RegisterWriter(*this, GetClassGUID());
        return true;
    }

    virtual bool AddDeviceTypes()
    {
        GetDeviceHolder().AddDeviceType(*this);
        return nsDataSource::DataSource::AddDeviceTypes();
    }

    virtual bool TickImpl(InternalTime::internalTime const& now)
    {
        ++m_Data.ticks;
        m_Data.ones = m_Data.ticks % 10;
        m_Data.tens = m_Data.ticks / 10 % 10;
        m_Data.hundreds = m_Data.ticks / 100 % 10;
        m_Data.thousands = m_Data.ticks / 1000 % 10;
        GetDeviceHolder().WriteDataWithCache(now, *this, m_Data);
        return true;
    }

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

        //// Read the index from the configuration
        //std::string index = *beg++;

        SetFrequency(static_cast<DWORD>(atoi(frequency.c_str())));
        //m_Data.index = static_cast<DWORD>(atoi(index.c_str()));

        return beg;
    }

public :
    static std::string GetName()
    {
        return "Digital Counter";
    }

    static GUID GetClassGUID()
    {
        // {D6FE3751-EB74-44e0-AA98-5BD70D42A3E7}
        static const GUID classGUID = 
        { 0xd6fe3751, 0xeb74, 0x44e0, { 0xaa, 0x98, 0x5b, 0xd7, 0xd, 0x42, 0xa3, 0xe7 } };
        
        return classGUID;
    }

    static std::vector<nsDataSource::ChannelDefinition>  GetClassDescription()
    {
        return localData::GetClassDescription();
    }
    
private :
    struct localData
    {
        DWORD   ticks;
        DWORD   ones;
        DWORD   tens;
        DWORD   hundreds;
        DWORD   thousands;

        static std::vector<nsDataSource::ChannelDefinition>  GetClassDescription()
        {
            static nsDataSource::ChannelDefinition definition[] =
            {
                {"ULONG_I", "ticks"},
                {"ULONG_I", "ones"},
                {"ULONG_I", "tens"},
                {"ULONG_I", "hundreds"},
                {"ULONG_I", "thousands"},
            };

            return UCSBUtility::ConvertToVector(definition);
        }
    };

    localData  m_Data;
};

}

