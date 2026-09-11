// TestWaitableTimers.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

#include "DataSource.h"
#include "internalTime.h"
#include "Periodic.h"
#include "UCSB-Datastream.h"
#include "utility.h"

#include <map>
#include <string>
#include <vector>

#include <conio.h>
#include <process.h>
#include <windows.h>


using namespace InternalTime;
using Periodic::CreateTimedThreadRef;
using std::string;

typedef UCSBUtility::FileHandle TimerHandle;
typedef UCSBUtility::FileHandle ThreadHandle;
using   UCSBUtility::FileHandle;
using   UCSB_Datastream::DeviceHolder;

bool g_quit = false;
HANDLE  g_hFile;

struct threadData
{
    string              name;
    DWORD               frequency;
    FILETIME            first;
    HANDLE              hTimer;
    PTIMERAPCROUTINE    action;
};

struct threadDataAlt 
{
    string              name;
    FILETIME            first;

    void Tick(internalTime const& trigger)
    {
        DWORD delta = static_cast<DWORD>((trigger - first) * 1000 / oneSecond);
        
        char buffer[80];
        sprintf_s(buffer, "%s: delta = %8d.%03d\n", name.c_str(), 
            delta / 1000, delta % 1000);

        DWORD written = 0;
        WriteFile(g_hFile, buffer, strlen(buffer), &written, NULL);
    }

    void StartTime(internalTime const& start)
    {
        first = start;
    }
};

//void TestData(internalTime const& trigger)
//{
//    static internalTime first = trigger;
//
//    DWORD delta = static_cast<DWORD>((trigger - first) * 1000 / oneSecond);
//    
//    char buffer[80];
//    sprintf_s(buffer, "%s: delta = %8d.%03d\n", __FUNCTION__, 
//        delta / 1000, delta % 1000);
//
//    DWORD written = 0;
//    WriteFile(GetStdHandle(STD_OUTPUT_HANDLE), buffer, strlen(buffer), &written, NULL);
//}
//

using nsDataSource::DataSource;
using nsDataSource::CreateDevices;
using std::vector;

typedef vector<DataSource*>::const_iterator DeviceIter;

int _tmain(int /*argc*/, _TCHAR* /*argv[]*/)
{
    std::vector<string> test;
    test.push_back("TestDevice");


    // BUGBUG: DO NOT LEAVE THIS IN
    DeviceHolder* pNULL = NULL;
    
    vector<DataSource*> devices = CreateDevices(test.begin(), test.end(), *pNULL);
    vector<HANDLE> hAll;
    for (DeviceIter beg = devices.begin(); beg < devices.end(); ++beg)
    {
        hAll.push_back(CreateTimedThreadRef(**beg, (*beg)->GetFrequency()));
    }
    
    FileHandle hFileHandle = CreateFile(L"test.log", 
        GENERIC_WRITE, FILE_SHARE_READ, NULL, 
        CREATE_ALWAYS, 0, NULL);
    g_hFile = hFileHandle;

    do
    {
        _getch();
        g_quit = true;

    }
    while (!g_quit);

    WaitForMultipleObjects(hAll.size(), &hAll[0], true, INFINITE);
    
	return 0;
}

