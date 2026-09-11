#pragma once

#include <DataSource.h>
#include "internalTime.h"
#include "cbw.h"

#include <string>

class MMCSource : public nsDataSource::DataSource, public nsDataSource::AutoRegister<MMCSource>
{
public:
    MMCSource(void);
    ~MMCSource(void);

    virtual bool TickImpl(InternalTime::internalTime const& now);

    virtual bool AddDeviceTypes()
    {
        GetDeviceHolder().AddDeviceType(*this);
        return nsDataSource::DataSource::AddDeviceTypes();
    }
    
    static std::string GetName()
    {
        return "MMC Device";
    }

    virtual bool Start()
    {
        GetDeviceHolder().RegisterWriter(*this, GetClassGUID());
        return true;
    }

    static std::vector<nsDataSource::ChannelDefinition>  GetClassDescription()
    {
        return localData::GetClassDescription();
    }

    static GUID GetClassGUID()
    {
        return localData::GetClassGUID();
    }

    struct localData
    {
        BYTE   bits[8];

        static GUID GetClassGUID()
        {
            // {83DE51AC-7648-466a-9ED4-6B140977C4AC}
            static const GUID classGUID = 
            { 0x83de51ac, 0x7648, 0x466a, { 0x9e, 0xd4, 0x6b, 0x14, 0x9, 0x77, 0xc4, 0xac } };

            return classGUID;
        }

        static std::vector<nsDataSource::ChannelDefinition>  GetClassDescription()
        {
            static nsDataSource::ChannelDefinition definition[] =
            {
                {"UCHAR", "bit0"},
                {"UCHAR", "bit1"},
                {"UCHAR", "bit2"},
                {"UCHAR", "bit3"},
                {"UCHAR", "bit4"},
                {"UCHAR", "bit5"},
                {"UCHAR", "bit6"},
                {"UCHAR", "bit7"},
            };

            return UCSBUtility::ConvertToVector(definition);
        }
    };

private :
    localData   ld;
};
