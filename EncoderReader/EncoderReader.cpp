// EncoderReader.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

#include "ConfigFileReader.h"
#include "internalTime.h"
#include "IOTech.h"
#include "Periodic.h"
#include "UCSB-Datastream.h"

#include <string>
#include <vector>

#include <conio.h>

using std::string;
using std::wstring;
using std::vector;

using UCSB_Datastream::DeviceHolder;
using UCSB_Datastream::Device;
using ConfigFileReader::InterpretConfigurationResourceFields;
using ConfigFileReader::ReadConfigurationFields;
using InternalTime::internalTime;
using InternalTime::FindNextTimeInTheFuture;
using InternalTime::GetTimeBoundary;
using InternalTime::oneSecond;

LPOLESTR GUID_InternalTimerString = L"{7DD3D860-4F19-4a5a-B318-3D9A0278D693}";
LPOLESTR GUID_IOTechDeviceString = L"{3C452666-DBEA-42ff-93C1-F6D0A183161B}";

GUID GUID_IOTechDevice;
GUID GUID_InternalTimer;

// Global variables
DeviceHolder g_devices;

//GUID IOTechDeviceGUID = { 3C452666-DBEA-42ff-93C1-F6D0A183161B };

// Take care of the two things this needs to do
// Read the data when passed an internalTime
// Write each of the data fields
struct EncoderWrapper
{
    typedef ioTech::IOTech IOTech;
    typedef IOTech::Encoder Encoder;

    EncoderWrapper(HANDLE& hFile, string const& boardName, float rate)
        : m_hFile(hFile)
        , m_current(internalTime::Now())
        , m_rate(rate)
        , m_boardName(boardName)
    {
        Start();
    }
    
    static bool AddDeviceTypes()
    {
        return true;
    }

    bool Start()
    {
        g_devices.RegisterWriter(*this, GUID_IOTechDevice);
        g_devices.RegisterWriter(m_current, GUID_InternalTimer);
        
        return m_ioTech.Setup(m_boardName, m_rate, 5000, true);
    }

    bool Stop()
    {
        return m_ioTech.Shutdown();
    }
    

    void Tick(internalTime const& now)
    {
        m_current = now;
        //g_devices.WriteDataWithCache(m_hFile, m_current, m_current);
        m_ioTech.ReadAndProcess(*this);
    }

    void operator()(Encoder const& data)
    {
        // Write Data to the file
        // include the time
        g_devices.WriteDataWithCache(m_current, *this, data);
    }

    static string GetName()
    {
        return "LN-250";
    }

private :    
    EncoderWrapper(EncoderWrapper const&);
    EncoderWrapper operator = (EncoderWrapper const&);


    internalTime        m_current;
    ioTech::IOTech      m_ioTech;
    HANDLE&             m_hFile;
    //DeviceHolder&       g_devices;
    float               m_rate;
    string              m_boardName;

};

// Global Constants
wstring DevicesConfiguration = L"devices.cfg";

string  boardName = "";
float   readRate;
DWORD   secondsBoundary = 60;
wstring rootDirectory = L".\\";

struct InterpretConfigFields
{
    bool operator ()(vector<string> const& fields)
    {
        if (fields.size() < 5)
        {
            ConfigFileReader::ProcessHelp();
            return false;
        }

        boardName = fields[0];

        readRate = static_cast<float>(atof(fields[1].c_str()));
        rootDirectory = UCSBUtility::StupidConvertToWString(fields[2]);
        secondsBoundary = atoi(fields[3].c_str());

        return true;
    }
};


HANDLE g_hFile = INVALID_HANDLE_VALUE;
bool g_quit = false;
extern DeviceHolder g_devices;

int _tmain(int argc, _TCHAR* argv[])
{
    // Initialize GUIDs
    CLSIDFromString(GUID_IOTechDeviceString, &GUID_IOTechDevice);
    CLSIDFromString(GUID_InternalTimerString, &GUID_InternalTimer);

    // Start by reading the config file into the dicitonary
    InterpretConfigurationResourceFields(L"devicesConfiguration", 
        DeviceHolder::DictionaryBuilder(g_devices));
    InterpretConfigFields icf;
    ReadConfigurationFields(argc, argv, icf);

    internalTime boundary = GetTimeBoundary(secondsBoundary);
    FindNextTimeInTheFuture(*boundary.UINT64Ptr(), secondsBoundary * oneSecond);

    internalTime dummy = boundary;// - (internalTime)internalTime::SystemTimeMinusLocalTime();

    SYSTEMTIME systemTime;
    FileTimeToSystemTime(dummy.FileTimePtr(), &systemTime);

    wstring filename;
    UCSBUtility::CreateDirectoryGetFilename(rootDirectory, L".spaceball", filename);

    g_hFile = CreateFile(filename.c_str(), GENERIC_WRITE, 
        FILE_SHARE_READ, NULL, CREATE_ALWAYS, 0, NULL);

    g_devices.CreateFileHeader(g_hFile);

    EncoderWrapper encoder(g_hFile, boardName, readRate);

    HANDLE hThreads[] = { Periodic::CreateTimedThreadRef(encoder, 100) };

    printf("press any key to stop data collection\n");
    while (!g_quit)
    {
        if (_kbhit())
        {
            _getch();
            g_quit = true;
            break;
        }

        // If we need a new file, open it and write the header to it
        // Once that is done, transfer all the data to collect to that one
        if (internalTime::Now() > boundary)
        {
            wstring filename;
            UCSBUtility::CreateDirectoryGetFilename(rootDirectory, L".spaceball", filename);

            HANDLE hFile = CreateFile(filename.c_str(), GENERIC_WRITE, 
                FILE_SHARE_READ, NULL, CREATE_ALWAYS, 0, NULL);

            g_devices.CreateFileHeader(hFile);

            HANDLE hTemp = g_hFile;
            g_hFile = hFile;
            CloseHandle(hTemp);
            FindNextTimeInTheFuture(*boundary.UINT64Ptr(), secondsBoundary * oneSecond);
        }
        Sleep (50);
    }

    WaitForMultipleObjects(_countof(hThreads), hThreads, true, INFINITE);

	return 0;
}

