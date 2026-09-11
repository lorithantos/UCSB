// UCSB_ErrorDisplay.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <windows.h>
#include <conio.h>


int _tmain(int argc, _TCHAR* argv[])
{
    HANDLE hPipe = CreateFileA("\\\\.\\pipe\\ucsbLogging", GENERIC_READ, 
        FILE_SHARE_READ, 
        NULL, 
        OPEN_EXISTING, 
        FILE_ATTRIBUTE_NORMAL, NULL);
    
    if(hPipe == INVALID_HANDLE_VALUE)
    {
        fprintf(stderr, "Unable to open named pipe exiting\n");
        _getch();
        return 1;
    }

    char buffer[1024];

    while (!_kbhit())
    {
        DWORD read = 0;
        ReadFile(hPipe, buffer, _countof(buffer), &read, NULL);
        if (read > 0)
        {
            fprintf(stderr, "%s", buffer);
        }
    }

    CloseHandle(hPipe);
	return 0;
}

