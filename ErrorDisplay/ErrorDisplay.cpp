// ErrorDisplay.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

#include <conio.h>
#include <windows.h>

#include <string>
using std::string;

BOOL ReadSlot(HANDLE hSlot) 
{ 
    DWORD cbMessage, cMessage, cbRead; 
    BOOL fResult; 
    DWORD cAllMessages; 
 
    fResult = GetMailslotInfo( hSlot, // mailslot handle 
        (LPDWORD) NULL,               // no maximum message size 
        &cbMessage,                   // size of next message 
        &cMessage,                    // number of messages 
        (LPDWORD) NULL);              // no read time-out 
 
    if (!fResult) 
    { 
        printf("GetMailslotInfo failed with %d.\n", GetLastError()); 
        return FALSE; 
    } 
 
    if (cbMessage == MAILSLOT_NO_MESSAGE) 
    { 
        //printf("Waiting for a message...\n"); 
        return TRUE; 
    } 
 
    cAllMessages = cMessage; 
 
    while (cMessage != 0)  // retrieve all messages
    { 
        // Create a message-number string. 
 
        // Allocate memory for the message. 
        string message;
        message.resize(cbMessage + 1);
 
        fResult = ReadFile(hSlot, 
            &message[0], 
            cbMessage, 
            &cbRead, 
            NULL); 
 
        if (!fResult) 
        { 
            return FALSE; 
        } 
 
        //// Display the message. 
 
        printf("%s", message.c_str()); 
 
        fResult = GetMailslotInfo(hSlot,  // mailslot handle 
            (LPDWORD) NULL,               // no maximum message size 
            &cbMessage,                   // size of next message 
            &cMessage,                    // number of messages 
            (LPDWORD) NULL);              // no read time-out 
 
        if (!fResult) 
        { 
            printf("GetMailslotInfo failed (%d)\n", GetLastError());
            return FALSE; 
        } 
    } 

    return TRUE; 
}

int _tmain(int argc, _TCHAR* argv[])
{
    HANDLE hMailslot = CreateMailslot(L"\\\\.\\mailslot\\ErrorMailSlot", 
        0,  MAILSLOT_WAIT_FOREVER, NULL);

    if (hMailslot == INVALID_HANDLE_VALUE)
    {
        hMailslot = CreateFile(L"\\\\.\\mailslot\\ErrorMailSlot", 
            GENERIC_READ, 
            FILE_SHARE_READ,
            (LPSECURITY_ATTRIBUTES) NULL, 
            OPEN_EXISTING, 
            FILE_ATTRIBUTE_NORMAL, 
            (HANDLE) NULL); 

        while (!_kbhit())
        {
            DWORD dummy = 0;
            char buffer[512] = {};
            ReadFile(hMailslot, buffer, sizeof(buffer), &dummy, NULL);
            if (dummy > 0)
            {
                printf("%s", buffer);
            }
        }

        CloseHandle(hMailslot);
        return 1;

    }

    while (!_kbhit())
    {
        ReadSlot(hMailslot);
    }

    CloseHandle(hMailslot);

    return 0;
}

