#pragma once

#include "ComPort.h"
#include "DataSource.h"
#include "internalTime.h"

#include <string>
#include <iostream>

//#define test

namespace UCSB_Telescope
{

using nsDataSource::string;
using nsDataSource::strings;
using nsDataSource::FieldIter;

// Dictionary Data

class Telescope : public nsDataSource::DataSource, public nsDataSource::AutoRegister<Telescope>
{
public:
	Telescope(void);
	virtual ~Telescope(void);

public :
	virtual bool Start()
	{
		GetDeviceHolder().RegisterWriter(*this, GetClassGUID());

#ifndef test
        return m_port.Start();
#else
        m_RawInput = CreateFile(L".\\Telescope.dat", GENERIC_READ, 0, NULL, OPEN_ALWAYS, 0, NULL);
        return true;
#endif
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
		
#ifndef test
		if (m_port.Read(buffer, &read))
#else
        ReadFile(m_RawInput, buffer, _countof(buffer), &read, NULL);
#endif
        {
			UCSBUtility::AddData(m_buffer, buffer, read);
            InterpretBuffer();
		}
		
		return true;
	}
	
	void InterpretBuffer()
	{
        STATIC_ASSERT(sizeof(localData) == 196);
        static const int frameLength = sizeof(localData) + 2;

        // Look for the start of the data
        if (m_bufferOffset >= m_buffer.size())
        {
            // WE SHOULD NEVER GET HERE
            m_bufferOffset = 0;
        };

        bool done = false;
        while (!done)
        {
            bool found = 0;
            for (;m_bufferOffset != m_buffer.size(); ++m_bufferOffset)
            {
                if (m_buffer[m_bufferOffset] == '$')
                {
                    found = true;
                    break;
                }
            }

            // We have reached the end of the buffer
            if (!found)
            {
                m_bufferOffset = 0;
                m_buffer.clear();
                return;
            }

            // We need at least a full frame before proceeding
            if (m_buffer.size() - m_bufferOffset < frameLength)
            {
                // Remove the already processed data
                m_buffer = std::vector<char> (m_buffer.begin() + m_bufferOffset, m_buffer.end());

                // Allow the buffer to continue to fill                
                m_bufferOffset = 0;
                return;
            }

            std::vector<char>::const_iterator cit = m_buffer.begin() + m_bufferOffset;

            // *cit == '$'
            // for a valid frame *(cit + frameLength -1) == '*'
            if (*(cit + frameLength - 1) != '*')
            {
                m_bufferOffset += 1;

                // We dont have a valid frame, look for the next '*'
                continue;
            }

            // We have a valid frame 
            memcpy(&m_Data, &m_buffer[m_bufferOffset+1], sizeof(m_Data));
            GetDeviceHolder().WriteDataWithCache(m_current, *this, m_Data);
            m_bufferOffset += frameLength;
        }
	}
	
	virtual FieldIter Configure(FieldIter beg, FieldIter end)
	{
		const DWORD paramCount = 1;
		// read in the parameters for Telescope
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
		return "Telescope";
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
	/*union btod
	{
		double value;
		char buffer[sizeof(double)];
	};*/

	struct localData
	{
		float	rev;
	    float   channel00t;
        float   channel00q;
        float   channel00u;
	    float   channel01t;
        float   channel01q;
        float   channel01u;
		float   channel02t;
        float   channel02q;
        float   channel02u;
		float   channel03t;
        float   channel03q;
        float   channel03u;
		float   channel04t;
        float   channel04q;
        float   channel04u;
		float   channel05t;
        float   channel05q;
        float   channel05u;
		float   channel06t;
        float   channel06q;
        float   channel06u;
		float   channel07t;
        float   channel07q;
        float   channel07u;
		float   channel08t;
        float   channel08q;
        float   channel08u;
		float   channel09t;
        float   channel09q;
        float   channel09u;

		float   channel10t;
        float   channel10q;
        float   channel10u;
		float   channel11t;
        float   channel11q;
        float   channel11u;
		float   channel12t;
        float   channel12q;
        float   channel12u;
		float   channel13t;
        float   channel13q;
        float   channel13u;
		float   channel14t;
        float   channel14q;
        float   channel14u;
		float   channel15t;
        float   channel15q;
        float   channel15u;

		static GUID GetClassGUID()
		{
			// {A38A47EE-D74E-479a-A2C1-FE7EFDC2C4CF}
			static const GUID classGUID = 
			{ 0xa38a47ee, 0xd74e, 0x479a, { 0xa2, 0xc1, 0xfe, 0x7e, 0xfd, 0xc2, 0xc4, 0xcf } };

			return classGUID;
		}
		
		static std::vector<nsDataSource::ChannelDefinition>  GetClassDescription()
		{
			static nsDataSource::ChannelDefinition definition[] = 
			{
				{"SPFLOAT_I", "Rev"},
                {"SPFLOAT_I", "Channel 00T"},
				{"SPFLOAT_I", "Channel 00Q"},
				{"SPFLOAT_I", "Channel 00U"},
				{"SPFLOAT_I", "Channel 01T"},
				{"SPFLOAT_I", "Channel 01Q"},
				{"SPFLOAT_I", "Channel 01U"},
				{"SPFLOAT_I", "Channel 02T"},
				{"SPFLOAT_I", "Channel 02Q"},
				{"SPFLOAT_I", "Channel 02U"},
				{"SPFLOAT_I", "Channel 03T"},
				{"SPFLOAT_I", "Channel 03Q"},
				{"SPFLOAT_I", "Channel 03U"},
				{"SPFLOAT_I", "Channel 04T"},
				{"SPFLOAT_I", "Channel 04Q"},
				{"SPFLOAT_I", "Channel 04U"},
				{"SPFLOAT_I", "Channel 05T"},
				{"SPFLOAT_I", "Channel 05Q"},
				{"SPFLOAT_I", "Channel 05U"},
				{"SPFLOAT_I", "Channel 06T"},
				{"SPFLOAT_I", "Channel 06Q"},
				{"SPFLOAT_I", "Channel 06U"},
				{"SPFLOAT_I", "Channel 07T"},
				{"SPFLOAT_I", "Channel 07Q"},
				{"SPFLOAT_I", "Channel 07U"},
				{"SPFLOAT_I", "Channel 08T"},
				{"SPFLOAT_I", "Channel 08Q"},
				{"SPFLOAT_I", "Channel 08U"},
				{"SPFLOAT_I", "Channel 09T"},
				{"SPFLOAT_I", "Channel 09Q"},
				{"SPFLOAT_I", "Channel 09U"},
				{"SPFLOAT_I", "Channel 10T"},
				{"SPFLOAT_I", "Channel 10Q"},
				{"SPFLOAT_I", "Channel 10U"},
				{"SPFLOAT_I", "Channel 11T"},
				{"SPFLOAT_I", "Channel 11Q"},
				{"SPFLOAT_I", "Channel 11U"},
				{"SPFLOAT_I", "Channel 12T"},
				{"SPFLOAT_I", "Channel 12Q"},
				{"SPFLOAT_I", "Channel 12U"},
				{"SPFLOAT_I", "Channel 13T"},
				{"SPFLOAT_I", "Channel 13Q"},
				{"SPFLOAT_I", "Channel 13U"},
				{"SPFLOAT_I", "Channel 14T"},
				{"SPFLOAT_I", "Channel 14Q"},
				{"SPFLOAT_I", "Channel 14U"},
				{"SPFLOAT_I", "Channel 15T"},
				{"SPFLOAT_I", "Channel 15Q"},
				{"SPFLOAT_I", "Channel 15U"},
            };

            return UCSBUtility::ConvertToVector(definition);
        }
	};

	localData					    m_Data;
	ComPort						    m_port;
	std::vector<char>               m_buffer;
    std::vector<char>::size_type    m_bufferOffset;

	InternalTime::internalTime	    m_current;

#ifdef test
    HANDLE                          m_RawInput;
#endif
};

}