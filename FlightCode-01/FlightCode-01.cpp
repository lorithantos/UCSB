// FlightCode-01.cpp : The startup code for the Flight System
//

#include "stdafx.h"

#include "ConfigFileReader.h"
#include "DataSource.h"
#include "Periodic.h"

#include "CommandInterface.h"

#include <string>
#include <vector>

#include <conio.h>

using std::string;
using std::wstring;
using std::vector;

using ConfigFileReader::InterpretConfigurationFields;

using ConfigFileReader::InterpretConfigurationFileFields;
using ConfigFileReader::InterpretConfigurationResourceFields;

using ConfigFileReader::ReadConfigurationFields;
using InternalTime::internalTime;
using InternalTime::FindNextTimeInTheFuture;
using InternalTime::GetTimeBoundary;
using InternalTime::oneSecond;
using nsDataSource::DataSource;
using nsDataSource::AutoRegister;
using nsDataSource::CreateDevices;
using nsDataSource::FieldIter;
using Periodic::CreateTimedThreadRef;
using UCSB_Datastream::DeviceHolder;
using UCSB_Datastream::Device;

typedef vector<DataSource*>::const_iterator DeviceIter;

HANDLE g_hFile = INVALID_HANDLE_VALUE;
bool g_quit = false;
extern DeviceHolder g_devices;
std::vector<DataSource*> g_DataSources;

const GUID GUID_InternalTimer = UCSBUtility::ConvertToGUID("{7DD3D860-4F19-4a5a-B318-3D9A0278D693}");

// Global Constants
wstring DevicesConfiguration = L"devices.cfg";

// Global variables
DeviceHolder g_devices;

//string  g_boardName = "";
//float   g_readRate;
DWORD   g_secondsBoundary = 600;
wstring g_rootDirectory = L".\\";

void UpdateSharedFile(internalTime& boundary, UINT64 fileTimeLength, HANDLE& /* sharedFile */)
{
    // If enough time hasn't passed, do nothing
    if (internalTime::Now() < boundary)
    {
        return;
    }

    // Get the new directory, and create a new directory for the date if needed
    wstring filename;
    UCSBUtility::CreateDirectoryGetFilename(g_rootDirectory, L".spaceball", filename);

    HANDLE hFile = CreateFile(filename.c_str(), GENERIC_WRITE, 
        FILE_SHARE_READ, NULL, CREATE_ALWAYS, 0, NULL);

    if (hFile == INVALID_HANDLE_VALUE)
    {
        UCSBUtility::LogError(__FUNCTION__, __FILE__, __LINE__, "Failed to create a new file (%ls)\nExisting file "
            "(if any) will continue to be used\n", filename.c_str());
        return;
    }

    g_devices.CreateFileHeader(hFile);

    // Switch the file
    HANDLE hTemp = g_hFile;
    g_hFile = hFile;

    // Give up thread slice if any other thread needs it
    // This will minimize the likelihood of a file being closed while
    // it is still needed
    Sleep(0);
    CloseHandle(hTemp);
    FindNextTimeInTheFuture(*boundary.UINT64Ptr(), fileTimeLength);
}

unsigned CreateDataSources(vector<string> const& fields)
{
    std::vector<DataSource*> newDevices = CreateDevices(fields.begin(), 
        fields.end(), g_devices);
    
    g_DataSources.insert(g_DataSources.end(), newDevices.begin(), 
        newDevices.end());

    return 0;
}

int _tmain(int /*argc*/, _TCHAR* /*argv */[])
{
    UCSBUtility::Logging logging ("FlightCode-01.err");
    try
    {
        InterpretConfigurationFields(L"Devices", CreateDataSources);

        // Ensure that all of the devices are registered before writing the first file
        vector<HANDLE> hAll;
        for (DeviceIter beg = g_DataSources.begin(); 
            beg < g_DataSources.end(); ++beg)
        {
            (*beg)->AddDeviceTypes();
        }

        // Create Directory and file based on current date and time
        // This needs to be done before starting the threads!
        internalTime boundary = GetTimeBoundary(g_secondsBoundary);
        UpdateSharedFile(boundary, g_secondsBoundary * oneSecond, g_hFile);

        for (DeviceIter beg = g_DataSources.begin(); 
            beg < g_DataSources.end(); ++beg)
        {
            hAll.push_back(CreateTimedThreadRef(**beg, 
                (*beg)->GetFrequency()));
        }

        SetConsoleTitleA("press q or space key to stop data collection");

        while (!g_quit)
        {
            if (_kbhit())
            {
                char key = static_cast<char>(_getch());
                if (key == 'T')
                {
                    UCSB_CommandInterface::CallFunction("Digital Counter", "IncrementThousands");
                    continue;
                }

                if (key == 't')
                {
                    UCSB_CommandInterface::CallFunction("Digital Counter", "IncrementThousands", 1);
                    continue;
                }

                if (key == 'h')
                {
                    UCSB_CommandInterface::CallFunction("Digital Counter", "IncrementHundreds", "5", 0);
                    continue;
                }

                if (tolower(key) == 'd')
                {
                    std::string names = UCSB_CommandInterface::GetFunctionList();
                    MessageBoxA(NULL, names.c_str(), "Test", MB_OK);
                    continue;
                }

                if (tolower(key) == 'q' || key == ' ')
                {
                    g_quit = true;
                    break;
                }
            }

            // Write the current data to the file
            g_devices.WriteCurrentData(g_hFile);

            // If we need a new file, open it and write the header to it
            // Once that is done, transfer all the data to collect to that one
            UpdateSharedFile(boundary, g_secondsBoundary * oneSecond, g_hFile);

            Sleep (10);
        }

        // Check to see if any threads have already quit
        DWORD wait = WaitForMultipleObjects(hAll.size(), &hAll[0], true, 1000);
        if (wait == WAIT_FAILED)
        {
            printf("Wait failed for some reason %d\n", GetLastError());
            //return 0;
        }

        // Write out the remaining data
        g_devices.WriteCurrentData(g_hFile);

        for (DeviceIter beg = g_DataSources.begin(); 
            beg < g_DataSources.end(); ++beg)
        {
            delete *beg;
        }

        CloseHandle(g_hFile);
    }
    catch(LPCSTR error)
    {
        printf("exiting due to error:\n%s\n", error);
    }
    catch(...)
    {
        printf("Exiting due to thrown error\n");
    }

    _CrtSetDbgFlag(_CRTDBG_LEAK_CHECK_DF);

    return 0;
}

