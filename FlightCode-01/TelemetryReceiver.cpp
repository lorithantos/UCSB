#include "StdAfx.h"
#include "TelemetryReceiver.h"

#include "ConfigFileReader.h"

#include <algorithm>
#include <map>
#include <string>
#include <vector>

// #define RECOGNIZE_FAILURE

using UCSB_Datastream::Device;
using UCSB_Datastream::DeviceDefinition;
using UCSB_Datastream::DeviceHolder;
using UCSB_Datastream::ChannelDefinition;

using UCSBUtility::ConvertToGUID;
using UCSBUtility::Append;
using UCSBUtility::Write;
using UCSBUtility::Read;

typedef std::vector<DWORD> DWORDS;
typedef std::vector<Device> Devices;
using UCSBUtility::PadToWidth;

using std::vector;

struct TelemetryReceiver::ReceiverPrint : public 
    UCSB_Datastream::DefaultThrowingParserHandler
{
    typedef std::string string;

    ReceiverPrint() : implicitIndex(256), clsFlag(true), hasDictionary(false) {}

    void DictionaryUpdated() 
    {
        hasDictionary = true;
        clsFlag = true;
    };

    void ControlData(BYTE message)
    {
        if (message == UCSB_Datastream::TELEMETRY_EOL)
        {
            SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), COORD());

            // Print all the data
            string output;
            for (std::map<int, string>::const_iterator cit = outputStrings.begin();
                cit != outputStrings.end(); ++cit)
            {
                output += cit->second;
            }

            if (clsFlag)
            {
                clsFlag = false;
                UCSBUtility::ClearScreen();
            }

            if (!hasDictionary)
            {
                output = "Waiting for Dictionary\n";
            }

            CONSOLE_SCREEN_BUFFER_INFO csbi;
            GetConsoleScreenBufferInfo (GetStdHandle(STD_OUTPUT_HANDLE), &csbi);
            if (output.size() > static_cast<size_t>(csbi.dwSize.X * csbi.dwSize.Y))
            {
                output.resize(csbi.dwSize.X * csbi.dwSize.Y);
            }
            printf("%s", output.c_str());

            outputStrings.clear();
        }
    }

    void ProcessData(Device const& device, BYTE const* pPayload)
    {
        string output = device.DisplayName();
        PadToWidth(output, width);
        
        int index = implicitIndex++;

        if (device.GetChannelCount() > 1 &&
            device.GetChannel(1).dataType == UCSB_Datastream::UCHAR &&
            !strcmp(device.GetChannel(1).displayName, "index"))
        {
            index = device.GetChannelVARIANT(1, pPayload).iVal;
        }


        std::vector<string> channels = device.GetChannelNames();
        string nextLine;
        for (size_t i = 0; i < channels.size(); ++i)
        {
            string header = channels[i];

            std::vector<DWORD> field;
            field.push_back(i);
            string data = device.GetDisplayData(pPayload, field);

            if (i < channels.size() - 1)
            {
                data += ", ";
                header += ", ";
            }

            if (header.size() > data.size())
            {
                data += string(header.size() - data.size(), ' ');
            }
            else if (data.size() > header.size())
            {
                header += string(data.size() - header.size(), ' ');
            }

            size_t lines = output.size() / width;
            if (lines > 0)
            {
                size_t newLines = (output.size() + header.size()) / width;
                if (newLines > lines)
                {
                    // Adjust
                    PadToWidth(output, width);

                    // Add Next lien
                    output += nextLine;
                    // Re-adjust
                    PadToWidth(output, width);
                    nextLine.clear();
                }
            }

            output += header;
            nextLine += data;
        }

        PadToWidth(output, width);
        output += nextLine;
        PadToWidth(output, width);

        // Blank line at the end
        output += string(width, ' ');

        //printf ("%s", output.c_str());
        outputStrings[index] = output;
    }

    static void InvalidData(BYTE messageID, Device const& device, 
        BYTE byteCount, BYTE const* pPayload)
    {
        // Silence the compiler warning
        messageID;
        pPayload;


        UCSBUtility::LogError(__FUNCTION__, __FILE__, __LINE__, 
            "%s expected %d bytes received %d\n", 
            device.DisplayName().c_str(), device.GetDataSize(), byteCount);
    }

    void MissingDictionaryEntry(BYTE dictionaryEntry)
    {
        // If we have never received a dictionary there is nothing we can do
        if (!hasDictionary)
        {
            return;
        }
        
        UCSBUtility::LogError(__FUNCTION__, __FILE__, __LINE__, 
            "Dictionary Entry %d is missing\n", dictionaryEntry);
    }


    int width;

    int implicitIndex;

    std::map<int, std::string>  outputStrings;
    bool clsFlag;
    bool hasDictionary;
};


TelemetryReceiver::TelemetryReceiver(void) 
    : nsDataSource::AutoRegister<TelemetryReceiver>(0)
    , m_hOutputFile(INVALID_HANDLE_VALUE)
    , m_pPrint(new ReceiverPrint)
    , m_parser(m_receiveDicionary, *m_pPrint)
    , m_parserBuffer(m_parser)
{
    GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), 
        &m_screenBufferInfo);
}

TelemetryReceiver::~TelemetryReceiver(void)
{
    CloseHandle(m_hOutputFile);
}

bool TelemetryReceiver::Start()
{
#ifdef RECOGNIZE_FAILURE
    bool success = 
#endif        
        m_port.Start();



#ifdef RECOGNIZE_FAILURE
    return success;
#else
    return true;
#endif
}

bool TelemetryReceiver::TickImpl(InternalTime::internalTime const&)
{

    BYTE buffer[4 * 1024];
    DWORD bytes = 0;
    while (m_port.Read(buffer, &bytes))
    {
        // if no bytes are available, we are done
        if (bytes == 0)
        {
            return true;
        }

        Write(m_hOutputFile, buffer, bytes);

        m_pPrint->width = m_screenBufferInfo.dwSize.X;
        m_parserBuffer.AddData(buffer, buffer + bytes);
    }

    return true;
}
