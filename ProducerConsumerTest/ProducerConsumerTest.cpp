// ProducerConsumerTest.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

#include "utility.h"

#include <process.h>
#include <windows.h>

#include <string>

using std::string;
using std::wstring;

using UCSBUtility::FileHandle;
typedef UCSBUtility::FileHandle ThreadHandle;

static const wstring LN250FileName  = L".\\\\file.ln250";

static const DWORD sleepThread = 10000;
static const DWORD sleepProducer = 10;
static const DWORD sleepConsumer = 25;

static LONG g_numProducers;

void ProduceData(void* handle)
{
    HANDLE hProducer = (HANDLE) handle;
    DWORD totalBytesWritten = 0;
    LONG whichProducer = InterlockedIncrement(&g_numProducers);

    for(int i=0;i<100;++i)
    {
        char buffer[64] = {};
        sprintf_s(buffer, "Producer %p %d\\n", whichProducer, i);

        DWORD numWritten = 0;
        if (!WriteFile(hProducer, buffer, strlen(buffer), &numWritten, NULL))
        {
            UCSBUtility::LogError(__FUNCTION__, __FILE__, __LINE__, "Failed to write %d\\n", GetLastError());
        }
        totalBytesWritten += numWritten;
        
        Sleep(sleepProducer + whichProducer);
    }

    UCSBUtility::LogError(__FUNCTION__, __FILE__, __LINE__, "Total Bytes Written %d\\n", totalBytesWritten);
};

//void ConsumeData(void*)
//{
//    while (!g_ProducerStarted)
//    {
//        Sleep(10);
//    }
//
//    FileHandle hConsumer = CreateFile(LN250FileName.c_str(), 
//        GENERIC_WRITE,
//        FILE_SHARE_READ | FILE_SHARE_WRITE,
//        NULL,
//        OPEN_EXISTING, 0, NULL);
//
//    if (!hConsumer.isValid())
//    {
//        UCSBUtility::LogError(__FUNCTION__, __FILE__, __LINE__, "Failed to open file %ls %d\\n", LN250FileName.c_str(),
//            GetLastError());
//        return;
//    }
//
//    char buffer[1024];
//    DWORD totalRead = 0;
//    while(g_ProducerStarted)
//    {
//        //DWORD size = GetFileSize(hFile, NULL);
//        DWORD numRead = 0;
//        ReadFile(hConsumer, &buffer, sizeof(buffer), &numRead, NULL);
//        totalRead += numRead;
//        Sleep(sleepConsumer);
//    }
//
//    UCSBUtility::LogError(__FUNCTION__, __FILE__, __LINE__, "Total Bytes Read %d\\n", totalRead);
//};
//
//
int _tmain(int argc, _TCHAR* argv[])
{
    HANDLE hFile = CreateFile(LN250FileName.c_str(), 
        0,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        CREATE_ALWAYS, 0, NULL);
    CloseHandle(hFile);

    FileHandle hProducer = CreateFile(LN250FileName.c_str(), 
        GENERIC_WRITE,
        FILE_SHARE_READ | FILE_SHARE_WRITE,
        NULL,
        OPEN_ALWAYS, 0, NULL);

    if (!hProducer.isValid())
    {
        UCSBUtility::LogError(__FUNCTION__, __FILE__, __LINE__, "Failed to open file %ls %d\\n", LN250FileName.c_str(),
            GetLastError());
        return 1;
    }

    HANDLE hThread0 = (HANDLE) _beginthread(ProduceData, 0, (HANDLE)hProducer);
    HANDLE hThread1 = (HANDLE) _beginthread(ProduceData, 0, (HANDLE)hProducer);
    HANDLE handles[] = { hThread0, hThread1 };

    WaitForMultipleObjects(2, handles, true, INFINITE);

    return 0;
}

