// Test_CCITT0.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "lib_crc.h"

unsigned char buffer[] =
{
	0x6E, 
	0x00, 
	0x00, 
	0x0C, 
	0x00, 
	0x00, 
	0xAA, 
	0xDA, 
	0x00, 
	0x00,
};

unsigned short UpdateCRC(unsigned short crcIn, unsigned char const* pBuffer, unsigned int len)
{
    unsigned short crcOut = crcIn;
    for(unsigned int i=0; i < len; ++i, ++pBuffer)
    {
        crcOut = update_crc_ccitt(crcOut, *pBuffer);
    }

    return crcOut;
}

int _tmain(int argc, _TCHAR* argv[])
{
    unsigned short crc = UpdateCRC(0, buffer, 6);

	return 0;
}

