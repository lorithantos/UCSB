// Spaceball2FITS.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <windows.h>
#include "ConversionTool.h"
#pragma comment (lib, "..\\lib\\spaceballconversion.lib")


int _tmain(int argc, _TCHAR* argv[])
{
    if (argc < 2)
    {
        printf("Spaceball2FITS requires the name of a spaceball file to work\n");
        return 1;
    }

    for(int i = 1; i < argc; ++i)
    {
        // if this is a real file, pass it on to the conversion tool
        HANDLE hFile = CreateFile(argv[i], GENERIC_READ, FILE_SHARE_READ, NULL, 
            OPEN_EXISTING, 0, NULL);
        
        if (hFile == INVALID_HANDLE_VALUE)
        {
            printf("Bad Filename passed:\n\t%s\n", argv[i]);
        }
        else
        {
            SpaceballFileToFITS(argv[i]);
            CloseHandle(hFile);
        }
    }

	return 0;
}

