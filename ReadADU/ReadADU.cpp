// ReadADU.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

#include <string>

    struct localData
    {
        float GPSTime;      // GPS Receive Time Seconds of Week
        float heading;      // Heading Degrees
        float pitch;        // Pitch Degrees
        float roll;         // Roll Degrees
        float baseline;     // Baseline (rms error) Meters
        char  reset;        // Reset Attitude reset flag
        float latitude;     // Latitude Degrees
        float longitude;    // Longitude Degrees
        float altitude;     // Altitude Meters
    };

using std::string;

string m_buffer;
localData m_Data;

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


    //sscanf_s(pStart, "%f,%g,%g,%g,%d,%g,%g,%g,%g", 
    //    &, &m_Data.heading, &m_Data.pitch, &m_Data.roll, 
    //    &m_Data.baseline, &reset, &m_Data.latitude, &m_Data.longitude, 
    //    &m_Data.altitude);

    //m_Data.reset = static_cast<char>(reset);

    //GetDeviceHolder().WriteDataWithCache(m_current, *this, m_Data);
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


int _tmain(int argc, _TCHAR* argv[])
{
    FILE* pFile;
    fopen_s(&pFile, "C:\\Users\\Lori\\Documents\\Visual Studio 2008\\Projects\\UCSB\\Release\\adu.txt", "rb");
    if (!pFile)
    {
        return 0;
    }

    char buffer[20];
    unsigned long read = 0xffff;
    for (;read != 0;)
    {
        read = fread(buffer, 1, 20, pFile);
        m_buffer.insert(m_buffer.end(), buffer, buffer + read);
        ProcessBuffer();
    }

    fclose(pFile);

	return 0;
}

