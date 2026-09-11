#pragma once

#include <string>
#include <vector>
#include <io.h>

#include <windows.h>

namespace UCSBUtility
{

class Logging
{
    typedef std::string string;

public :
    Logging(string const& logfileName)
    {
        GetHandle() = CreateFile(L"\\\\.\\mailslot\\ErrorMailSlot", 
            GENERIC_WRITE, 
            FILE_SHARE_READ,
            NULL, 
            OPEN_EXISTING, 
            FILE_ATTRIBUTE_NORMAL, 
            NULL); 

        SYSTEMTIME localTime;
        GetLocalTime(&localTime);

        char buffer[1024];
        sprintf_s(buffer, "%04d.%02d.%02d-%02d.%02d.%02d-", localTime.wYear, localTime.wMonth, localTime.wDay, 
            localTime.wHour, localTime.wMinute, localTime.wSecond);

        string realFilename = std::string(buffer) + logfileName;

        FILE* pFile;
        freopen_s(&pFile, realFilename.c_str(), "wt", stderr);

        LogError("------------NEW INSTANCE------------\n");
        LogError("Date (yyyy/mm/dd): %04d/%02d/%02d\n", localTime.wYear, localTime.wMonth, localTime.wDay);
        LogError("Time   (hh:mm:ss):   %02d:%02d:%02d\n", localTime.wHour, localTime.wMinute, localTime.wSecond);
        LogError("------------------------------------\n");

    }

    static void LogErrorVA(char const* format, va_list args)
    {
        int nChars = vfprintf_s(stderr, format, args);

        //// We need to display this as well
        if (nChars < 0)
        {
            // We should never get here!
            return;
        }

        std::string buffer;
        buffer.resize(nChars + 1);
        vsprintf_s(&buffer[0], buffer.size(), format, args);
        OutputDebugStringA(&buffer[0]);
        DWORD dummy = 0;

        if (GetHandle() != INVALID_HANDLE_VALUE)
        {
            WriteFile(GetHandle(), &buffer[0], nChars, &dummy, NULL);
        }
    }

    static void LogError(char const* format, ...)
    {
       va_list args;
       va_start(args, format);      /* Initialize variable arguments. */

       LogErrorVA(format, args);

       va_end(args);                /* Reset variable arguments.      */
    }

    ~Logging()
    {
        SYSTEMTIME localTime;
        GetLocalTime(&localTime);

        LogError("------------------------------------\n");
        LogError("Date (yyyy/mm/dd): %04d/%02d/%02d\n", localTime.wYear, localTime.wMonth, localTime.wDay);
        LogError("Time   (hh:mm:ss):   %02d:%02d:%02d\n", localTime.wHour, localTime.wMinute, localTime.wSecond);
        LogError("------------NORMAL EXIT-------------\n");
        CloseHandle(GetHandle());
    }

private :
    static HANDLE& GetHandle()
    {
        static HANDLE m_hLoggingMailSlot;

        return m_hLoggingMailSlot;
    }
};

};
