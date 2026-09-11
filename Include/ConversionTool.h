#pragma once

#include <string>

#include <windows.h>
#include <OAIdl.h>

#ifdef SPACEBALLCONVERSION_EXPORTS
#define EXPORTABLE __declspec(dllexport)
#else
#define EXPORTABLE __declspec(dllimport)
#endif

typedef HANDLE SPACEHANDLE;
typedef HANDLE SPACEPROCESSHANDLE;

extern "C"
{
EXPORTABLE SPACEHANDLE _cdecl GetDirectoryInfo(LPCSTR filename, VARIANT* configText);
EXPORTABLE void _cdecl DeleteHandle(SPACEHANDLE hHandle);
EXPORTABLE void _cdecl GetConfigString(SPACEHANDLE hSpaceball, VARIANT* configText);
EXPORTABLE void _cdecl GetDefaultConfigString(SPACEHANDLE hSpaceball, VARIANT* configText);
EXPORTABLE DWORD _cdecl SetConfigString(SPACEHANDLE hSpaceball, VARIANT configText);
EXPORTABLE void _cdecl ProcessDirectory(SPACEHANDLE hSpaceball);
EXPORTABLE BOOL _cdecl SetOutputType(SPACEHANDLE hSpaceball, BOOL fits, BOOL csv);
EXPORTABLE SPACEPROCESSHANDLE _cdecl ProcessDirectoryStart(SPACEHANDLE hSpaceball);
EXPORTABLE SPACEPROCESSHANDLE _cdecl GetProcessStatus(SPACEPROCESSHANDLE hProcess, VARIANT* filename, VARIANT* lastStatus);
EXPORTABLE void _cdecl ProcessStop(SPACEPROCESSHANDLE hProcess);

EXPORTABLE BOOL _cdecl SpaceballFileToFITS(LPCSTR filename);
}
