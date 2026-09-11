#include "StdAfx.h"
#include "DisplaySource.h"

using UCSB_Datastream::Device;
using UCSB_Datastream::DeviceHolder;
using std::string;
using UCSBUtility::PadToWidth;

namespace 
{

struct PrintDevice : UCSB_Datastream::DefaultThrowingParserHandler
{
    PrintDevice() : implicitIndex(256) {}

    void DictionaryUpdated() {};

    void ProcessData(Device const& device, BYTE const* pPayload)
    {
        //char buffer[1024];
        string output = device.DisplayName();
        PadToWidth(output, width);
        
        int index = implicitIndex++;

        if (device.GetChannel(1).dataType == UCSB_Datastream::UCHAR &&
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


        UCSBUtility::LogError(__FUNCTION__, __FILE__, __LINE__, "%s expected %d bytes received %d\n", 
            device.DisplayName().c_str(), device.GetDataSize(), byteCount);
    }

    static void MissingDictionaryEntry(BYTE dictionaryEntry)
    {
        UCSBUtility::LogError(__FUNCTION__, __FILE__, __LINE__, "Dictionary Entry %d is missing\n", dictionaryEntry);
    }


    int width;

    int implicitIndex;

    std::map<int, string>  outputStrings;
};

}

DisplaySource::DisplaySource(void) : nsDataSource::AutoRegister<DisplaySource>(0)
{
    GetConsoleScreenBufferInfo (GetStdHandle(STD_OUTPUT_HANDLE), &m_screenBufferInfo);
    printf ("This should be cleared\n1\n2\n3\n4\n");
    UCSBUtility::ClearScreen();
}

DisplaySource::~DisplaySource(void)
{
}

bool DisplaySource::TickImpl(InternalTime::internalTime const& now)
{
    // Get the data from all of the the sources
    CacheDataType cachedData = GetDeviceHolder().GetCache();

    COORD pos = {};
    SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), pos);

    printf("%I64d %s %d sources  \n%*s", static_cast<UINT64>(now - m_start) / 
        100000, __FUNCTION__, cachedData.size(), m_screenBufferInfo.dwSize.X, " ");

    PrintDevice pPrint;
    pPrint.width = m_screenBufferInfo.dwSize.X;
    DeviceHolder::FileParser<PrintDevice> parser(GetDeviceHolder(), pPrint);
    LN250::InputBuffer<DeviceHolder::FileParser<PrintDevice> > fileBuffer(parser);

    // Display the data
    for (CacheDataType::const_iterator cit = cachedData.begin(); 
        cit != cachedData.end(); ++cit)
    {
        if (cit->second.size() > 0)
        {
            fileBuffer.AddData(&cit->second[0], &cit->second[0] + cit->second.size());
        }
    }

    // Print all the data
    string output;
    for (std::map<int, string>::const_iterator cit = pPrint.outputStrings.begin();
        cit != pPrint.outputStrings.end(); ++cit)
    {
        output += cit->second;
    }

    // Don't overwrite the screen area (prevent scrolling)
    GetConsoleScreenBufferInfo (GetStdHandle(STD_OUTPUT_HANDLE), &m_screenBufferInfo);
    if (output.size() > static_cast<size_t>(m_screenBufferInfo.dwSize.X * m_screenBufferInfo.dwSize.Y))
    {
        output.resize(m_screenBufferInfo.dwSize.X * m_screenBufferInfo.dwSize.Y);
    }

    printf("%s", output.c_str());

    return true;
}
