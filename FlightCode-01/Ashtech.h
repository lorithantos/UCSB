#pragma once

#include "ComPort.h"
#include "DataSource.h"
#include "internalTime.h"

#include <string>

namespace UCSB_Ashtech
{

using nsDataSource::string;
using nsDataSource::strings;
using nsDataSource::FieldIter;

// Dictionary data

class Ashtech : public nsDataSource::DataSource, public nsDataSource::AutoRegister<Ashtech>
{
    typedef InternalTime::internalTime internalTime;

public:
    Ashtech(void);
    virtual ~Ashtech(void);

public :
    virtual bool Start()
    {
        GetDeviceHolder().RegisterWriter(*this, GetClassGUID());

        m_port.SetParity(0);
        m_port.SetStop(0);

        return m_port.Start();
    }

    virtual bool AddDeviceTypes()
    {
        GetDeviceHolder().AddDeviceType(*this);
        return nsDataSource::DataSource::AddDeviceTypes();
    }

    size_t ReadNextFloat(size_t offset, float& value)
    {
        if (offset == m_buffer.npos)
        {
            value = 0;
            return offset;
        }

        char const* pStart = &m_buffer[offset];
        if (*pStart == '+')
        {
            ++pStart;
        }

        value = static_cast<float>(atof(pStart));
        offset = m_buffer.find(',', offset);
        if (offset == m_buffer.npos)
        {
            return m_buffer.npos;
        }

        return offset + 1;
    }

    void ReadDSO(size_t offset)
    {
        // 518873.5,312.53,-01.75,+01.31,0.0356,0,+34.4139412,-119.8425726,+0004.34
        // localData
        // For DSO, just do a sscanf_s

        offset = ReadNextFloat(offset, m_Data.GPSTime);
        offset = ReadNextFloat(offset, m_Data.heading);
        offset = ReadNextFloat(offset, m_Data.pitch);
        offset = ReadNextFloat(offset, m_Data.roll);
        offset = ReadNextFloat(offset, m_Data.baseline);
        float reset;
        offset = ReadNextFloat(offset, reset);
        m_Data.reset = static_cast<char>(reset);
        offset = ReadNextFloat(offset, m_Data.latitude);
        offset = ReadNextFloat(offset, m_Data.longitude);
        ReadNextFloat(offset, m_Data.altitude);

        GetDeviceHolder().WriteDataWithCache(m_current, *this, m_Data);
    }

    void ProcessBuffer()
    {
        // Look for PASHR,
        size_t pashResponse = m_buffer.find("$PASHR,");

        for(;pashResponse != m_buffer.npos; m_buffer.find("$PASHR,", 
            pashResponse))
        {
            size_t endl = m_buffer.find('\n', pashResponse);

            if (endl == m_buffer.npos)
            {
                // Clear the buffer to this point
                if (pashResponse != 0)
                {
                    m_buffer = string(m_buffer.begin() + pashResponse, 
                        m_buffer.end());
                }

                return;
            }

            ++endl;

            // Check for a response to DSO
            size_t dso = m_buffer.find("DSO,", pashResponse);
            if (dso == pashResponse + 7)
            {
                ReadDSO(dso + 4);
            }

            pashResponse = endl;
        }

        return;
    }


    virtual bool TickImpl(InternalTime::internalTime const& now)
    {
        m_current = now;

        char buffer[512];
        DWORD read = 0;

        if (m_port.Read(buffer, &read))
        {
            UCSBUtility::AddData(m_buffer, buffer, read);
            ProcessBuffer();
        }

        m_port.Write("$PASHQ,DSO\r\n");

        return true;
    }


    virtual FieldIter Configure(FieldIter beg, FieldIter end)
    {
        const DWORD paramCount = 1;
        // read in the parameters for Magnetometer
        if ((end - beg) < paramCount)
        {
            UCSBUtility::LogError(__FUNCTION__, __FILE__, __LINE__, "%d parameters expected"
                ", %d parameters found\n", paramCount, (end - beg));
            return end;
        }

        // Read the frequency
        SetFrequency(UCSBUtility::ToINT<DWORD>(*beg++));

        return m_port.Configure(beg, end);
    }

public :
    static std::string GetName()
    {
        return "Ashtech";
    }

    static std::vector<nsDataSource::ChannelDefinition>  GetClassDescription()
    {
        return localData::GetClassDescription();
    }

    static GUID GetClassGUID()
    {
        return localData::GetClassGUID();
    }

private :
    struct localData
    {
#pragma pack(push)
#pragma pack(1)
        float GPSTime;      // GPS Receive Time Seconds of Week
        float heading;      // Heading Degrees
        float pitch;        // Pitch Degrees
        float roll;         // Roll Degrees
        float baseline;     // Baseline (rms error) Meters
        char  reset;        // Reset Attitude reset flag
        float latitude;     // Latitude Degrees
        float longitude;    // Longitude Degrees
        float altitude;     // Altitude Meters
#pragma pack(pop)

        static GUID GetClassGUID()
        {
            // {188072F4-E523-4c3a-B0D0-4A0045475070}
            static const GUID classGUID = 
            { 0x188072f4, 0xe523, 0x4c3a, { 0xb0, 0xd0, 0x4a, 0x0, 0x45, 0x47, 0x50, 0x70 } };

            return classGUID;
        }

        static std::vector<nsDataSource::ChannelDefinition>  GetClassDescription()
        {
            static nsDataSource::ChannelDefinition definition[] =
            {
                { "SPFLOAT_I",  "GPSTime" },
                { "SPFLOAT_I",  "heading" },
                { "SPFLOAT_I",  "pitch" },
                { "SPFLOAT_I",  "roll" },
                { "SPFLOAT_I",  "baseline" },
                { "SCHAR",      "reset" },
                { "SPFLOAT_I",  "latitude" },
                { "SPFLOAT_I",  "longitude" },
                { "SPFLOAT_I",  "altitude" },
            };

            return UCSBUtility::ConvertToVector(definition);
        }
    };

    FILE*           m_pFileDummy;
    localData       m_Data;
    ComPort         m_port;
    std::string     m_buffer;
    internalTime    m_current;
};

}

