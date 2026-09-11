#pragma once

#include "ComPort.h"
#include "DataSource.h"
#include "internalTime.h"

#include <string>

namespace UCSB_Magnetometer
{

using nsDataSource::string;
using nsDataSource::strings;
using nsDataSource::FieldIter;

// Dictionary data

class Magnetometer : public nsDataSource::DataSource, public nsDataSource::AutoRegister<Magnetometer>
{
public:
    Magnetometer(void);
    virtual ~Magnetometer(void);

public :
    virtual bool Start()
    {
        GetDeviceHolder().RegisterWriter(*this, GetClassGUID());

        return m_port.Start();
    }

    virtual bool AddDeviceTypes()
    {
        GetDeviceHolder().AddDeviceType(*this);
        return nsDataSource::DataSource::AddDeviceTypes();
    }

    virtual bool TickImpl(InternalTime::internalTime const& now)
    {
        m_current = now;

        char buffer[512];
        DWORD read = 0;

        
        if (m_port.Read(buffer, &read))
        {
            UCSBUtility::AddData(m_buffer, buffer, read);
            InterpretBuffer();
        }

        return true;
    }

    std::string::size_type Read(double& destination, std::string::size_type offset)
    {
        std::string::size_type end = m_buffer.find_first_not_of("0123456789-+.", offset);
        if (end == std::string::npos)
        {
            // malformed string, don't parse anything
            return end;
        }

        std::string value(m_buffer.begin() + offset, m_buffer.begin() + end);

        destination = static_cast<double>(atof(value.c_str()));

        return end;
    }

    std::string::size_type ReadMagnetic(std::string::size_type offset)
    {
        switch(m_buffer[offset++])
        {
        case 'x' :
            return Read(m_Data.magX, offset);
            break;

        case 'y' :
            return Read(m_Data.magY, offset);
            break;

        case 'z' :
            return Read(m_Data.magZ, offset);
            break;
        }

        // Adjust for having skipped the first digit
        return Read(m_Data.magLength, offset - 1);
    }

    std::string::size_type Parse(std::string::size_type offset)
    {
        while (offset != std::string::npos)
        {
            double ignore;

            switch (m_buffer[offset++])
            {
            case 'C' :
                offset = Read(m_Data.heading, offset);
                break;

            case 'P' :
                offset = Read(m_Data.pitch, offset);
                break;

            case 'R' :
                offset = Read(m_Data.roll, offset);
                break;

            case 'M' :
                offset = ReadMagnetic(offset);
                break;

            case 'T' :
                offset = Read(ignore, offset);
                break;

            default :
                return offset;
            }
        }

        // assert(offset == std::string::npos);
        return offset;
    }

    void InterpretBuffer()
    {
        std::string::size_type start = 0;

        while (start != std::string::npos)
        {
            // Find the next '$'
            start = m_buffer.find('$', start);

            // If the buffer is invalid, clear it
            if (start == std::string::npos)
            {
                m_buffer.clear();
                return;
            }

            // If we don't have a whole message, return
            std::string::size_type last = m_buffer.find('*', start + 1);
            if (last == std::string::npos)
            {
                // if we have never read any data, no need to adjust the buffer
                if (start == 0)
                {
                    return;
                }

                m_buffer = std::string(m_buffer.begin() + start, m_buffer.end());
                return;
            }

            start = Parse(++start);

            GetDeviceHolder().WriteDataWithCache(m_current, *this, m_Data);
        }
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
        return "Magnetometer";
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
        double heading;
        double pitch;
        double roll;
        double magLength;
        double magX;
        double magY;
        double magZ;

        static GUID GetClassGUID()
        {
            // {7330215D-0399-44fb-982D-4F0F61BC91C6}
            static const GUID classGUID  = 
            { 0x7330215d, 0x399, 0x44fb, { 0x98, 0x2d, 0x4f, 0xf, 0x61, 0xbc, 0x91, 0xc6 } };

            return classGUID;
        }

        static std::vector<nsDataSource::ChannelDefinition>  GetClassDescription()
        {
            static nsDataSource::ChannelDefinition definition[] =
            {
                {"DPFLOAT_I", "heading"},
                {"DPFLOAT_I", "pitch"},
                {"DPFLOAT_I", "roll"},
                {"DPFLOAT_I", "magLength"},
                {"DPFLOAT_I", "magX"},
                {"DPFLOAT_I", "magY"},
                {"DPFLOAT_I", "magZ"},
            };

            return UCSBUtility::ConvertToVector(definition);
        }
    };

    localData                   m_Data;
    ComPort                     m_port;
    std::string                 m_buffer;
    InternalTime::internalTime  m_current;
};

}

