#pragma once

#include "DataSource.h"
#include "internalTime.h"

#include <string>

namespace UCSB_ClockSync
{

using nsDataSource::string;
using nsDataSource::strings;
using nsDataSource::FieldIter;

// Dictionary data

class ClockSync : public nsDataSource::DataSource, public nsDataSource::AutoRegister<ClockSync>
{
public:
    ClockSync(void);
    virtual ~ClockSync(void);

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
        GetDeviceHolder().WriteDataWithCache(now, *this, m_Data);
        return true;
    }

    virtual FieldIter Configure(FieldIter beg, FieldIter end)
    {
        const DWORD paramCount = 1;
        // read in the parameters for ClockSync
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
        return "ClockSync";
    }

    static GUID GetClassGUID()
    {
        // {E247A5C7-4CCA-403c-BA60-089334450943}
        static const GUID classGUID = 
        { 0xe247a5c7, 0x4cca, 0x403c, { 0xba, 0x60, 0x8, 0x93, 0x34, 0x45, 0x9, 0x43 } };

        return classGUID;
    }

    static std::vector<nsDataSource::ChannelDefinition>  GetClassDescription()
    {
        return localData::GetClassDescription();
    }
    
private :
    struct localData
    {
        UINT64  cpu;
        UINT64  filetime;

        static std::vector<nsDataSource::ChannelDefinition>  GetClassDescription()
        {
            static nsDataSource::ChannelDefinition definition[] =
            {
                {"UINT64_I", "cpu"},
                {"UINT64_I", "filetime"},
            };

            return UCSBUtility::ConvertToVector(definition);
        }
    };

    localData  m_Data;
};

}

