#pragma once

#include "DataSource.h"
#include "internalTime.h"
#include "CommandInterface.h"

#include <string>

namespace UCSB_ClockSync
{

using nsDataSource::string;
using nsDataSource::strings;
using nsDataSource::FieldIter;

// Dictionary data

class ClockSync : public nsDataSource::DataSource, public nsDataSource::AutoRegister<ClockSync>, UCSB_CommandInterface::CommandInterface<ClockSync>
{
public:
    ClockSync(void);
    virtual ~ClockSync(void);

public :
    virtual bool Start()
    {
        m_run = true;
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
        m_Data.filetime = InternalTime::internalTime::Now();
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

        SetFrequency(UCSBUtility::ToINT<DWORD>(*beg++));

        return beg;
    }

public  :
    static void RegisterCommandFunctions()
    {
        AddFunction("Pause", &ClockSync::Pause);
        AddFunction("Unpause", &ClockSync::Pause);
    }

private :
    void Pause()
    {
        m_run = false;
    }

    void UnPause()
    {
        m_run = true;
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
        UINT64  filetime;

        static std::vector<nsDataSource::ChannelDefinition>  GetClassDescription()
        {
            static nsDataSource::ChannelDefinition definition[] =
            {
                {"UINT64_I", "filetime"},
            };

            return UCSBUtility::ConvertToVector(definition);
        }
    };

    localData   m_Data;
    bool        m_run;
};

}

