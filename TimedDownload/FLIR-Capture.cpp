// FLIR-Capture.cpp 
//

#include "stdafx.h"
#include "flir-capture.h"

#include <string>
#include <vector>

#include <windows.h>

#include <conio.h>
#include <comutil.h>

#pragma comment(lib, "comsuppw.lib")

#include "lib_crc.h"
#include "utility.h"

// Badly behaved header - requires windows.h before including it
// Make sure your include path points to the "AccessoriesSDK\\include"
// from "FLIR IR camera with Pleora GigE"
#include <ISC_Camera.h>
// Also make sure the ISC_Camera.lib is available
#pragma comment (lib, "ISC_Camera")

using _com_util::ConvertBSTRToString;
const DWORD FLIR_Baud = 57600;
using std::vector;
using std::string;
using std::wstring;

// Signals the camera that this is a command
const BYTE FLIR_ProcessCode = 0x6E;

// We always use channel zero
const int channelID = 0;

// FITS record size
const int FITS_RECORD_SIZE = 2880;

// Commands we use
namespace FLIR_Command
{
const BYTE FLAT_FIELD_CORRECTION    = 0x0C;
const BYTE UNKNOWN_GET_TEMP_COMMAND = 0x2A;
const BYTE READ_TEMP_SENSOR         = 0x20;
};

// Structure of data sent
struct FLIR_CommandHeader
{
    BYTE    processCode;
    BYTE    status;
    BYTE    reserved;
    BYTE    command;
    BYTE    byteCountMSB;
    BYTE    byteCountLSB;
};

//struct FLIR_CommandHeaderCRC : public FLIR_CommandHeader
//{
//    BYTE    crcCCITT_0_MSB;
//    BYTE    crcCCITT_0_LSB;
//};
unsigned short UpdateCRC(unsigned short crcIn, unsigned char const* pBuffer, unsigned int len)
{
    unsigned short crcOut = crcIn;
    for(unsigned int i=0; i < len; ++i, ++pBuffer)
    {
        crcOut = update_crc_ccitt(crcOut, *pBuffer);
    }

    return crcOut;
}

template<typename T>
unsigned short UpdateCRC(unsigned short crcIn, T const& dataStructure)
{
    return UpdateCRC(crcIn, reinterpret_cast<BYTE const*>(&dataStructure), sizeof(dataStructure));
}

vector<BYTE> CreateCommandNoData(BYTE command)
{
    FLIR_CommandHeader header = { FLIR_ProcessCode };
    header.command = command;

    vector<BYTE> retv;
    retv.reserve(10);   // We know that this results in 10 bytes (harmless if wrong)
    retv.insert(retv.end(), reinterpret_cast<BYTE*>(&header), reinterpret_cast<BYTE*>(&header + 1));
    
    // Now append the CRC, most significant byte, then least
    unsigned short crc =  UpdateCRC(0, header);
    retv.push_back(static_cast<BYTE>((crc >> 8) & 0xff));
    retv.push_back(static_cast<BYTE>(crc & 0xff));

    // BUGBUG: I don't know why there are two additional zeros here!!
    retv.push_back(0);
    retv.push_back(0);
    return retv;
}

isc_Error WriteToCamera(int deviceID, vector<BYTE> const& dataIn, vector<BYTE>& dataOut, int dataBytes = 0)
{
    int count = dataIn.size();
    isc_Error lastError = WriteControl(deviceID, const_cast<BYTE*>(&dataIn[0]), &count);
    if (lastError != eOK)
    {
        UCSBUtility::LogError(__FUNCTION__, __FILE__, __LINE__, "WriteControl Failed (%d)\\n", lastError);
        return lastError;
    }

    // Data is 10 bytes (6 bytes header, 2 bytes CRC + 2 'magic' bytes - probably CRC2)

    dataOut.resize(10 + dataBytes);
    count = dataOut.size();
    lastError = ReadControl(deviceID, &dataOut[0], &count);
    if (lastError != eOK)
    {
        UCSBUtility::LogError(__FUNCTION__, __FILE__, __LINE__, "ReadControl Failed (%d)\\n", lastError);
        return lastError;
    }

    return lastError;
}

// Just sends the command to the camera
// Reads back the response
// if the caller has passed a vector, the data is returned to them
// otherwise the response is simply dropped on teh ground
isc_Error SendSimpleCameraCommand(int deviceID, BYTE command, vector<BYTE>* pReturn = NULL, int responseBytes = 0)
{
    vector<BYTE> dataOut;
    if (pReturn == NULL)
    {
        pReturn = &dataOut;
    }
    
    return WriteToCamera(deviceID, CreateCommandNoData(command), 
        *pReturn, responseBytes);
}

isc_Error ReadShortFromCamera(int deviceID, BYTE command, unsigned short& retv)
{
    vector<BYTE> data;
    isc_Error lastError = SendSimpleCameraCommand(deviceID, 
        command, &data, 2);
    if (lastError != eOK)
    {
        UCSBUtility::LogError(__FUNCTION__, __FILE__, __LINE__, "Failed to read command");
        return lastError;
    }
    
    retv = (static_cast<unsigned short>(data[8]) << 8) + data[9];
    return lastError;
}

inline short Swap(short in)
{
    USHORT temp = static_cast<USHORT>(in);
    return ((temp & 0xff) << 8) + (temp >> 8);
}

void PrepDataForFITSOutput(vector<short>& frame, unsigned rows, unsigned cols)
{
    // If the data isn't the right size, nothing can be done
    if (frame.size() != rows * cols)
    {
        UCSBUtility::LogError(__FUNCTION__, __FILE__, __LINE__, "Invalid size passed to '%s' nothing will be done\\n", __FUNCTION__);
        return;
    }

    // Transformation is as follows
    // rows changed from top down to bottom up
    // each USHORT is byte swapped
    for (unsigned r=0; r < rows / 2; ++r)
    {
        int offset1 = r * cols;
        int offset2 = (rows-1-r) * cols;
        for (unsigned c=0; c < cols; ++c)
        {
            USHORT temp = Swap(frame[c + offset1]);
            frame[c + offset1] = Swap(frame[c + offset2]);
            frame[c + offset2] = temp;
        }
    }

    // If there is an odd number of rows
    // do the swap within that row
    if (rows & 1)
    {
        char* pData = reinterpret_cast<char*>(&frame[rows/2*cols]);
        _swab(pData, pData, cols);
    }

    // PAD out to FITS approved 2880 bytes (36 cards of 80 bytes)
    // Remember, 2880 BYTES, not 2880 shorts
    unsigned padding = (FITS_RECORD_SIZE - frame.size() * sizeof(frame[0]) % FITS_RECORD_SIZE) / sizeof(frame[0]);
    if (padding != FITS_RECORD_SIZE / sizeof(frame[0]))
    {
        frame.resize(frame.size() + padding);
    }
}

// Card is a keyword up to 8 chars in length
// (if a longer keyword is requested an empty card will be returned
// then a "= "
// We are using a 30 byte right hand alignment
// if a comment is provided, then add '/ ' at byte 31 then the comment
// Comments that are too long will be truncated
// finally, pad with spaces to 80 bytes
// NOTE: This does not handle COMMENT cards well
vector<char>
CreateFITS_HeaderCard(string const& keyword, string const& value = "", string const& comment = "")
{
    vector<char> empty;

    if (keyword.size() > 8 || keyword.size() == 0)
    {
        return empty;
    }

    string card;
    card.reserve(80);
    card = keyword;
    
    if (value.length() > 0)
    {
        // pad to 8 bytes
        card.append(8 - card.length(), ' ');
        card += "= ";
        
        // right align the value to 30 bytes (20 bytes from here)
        if (value.length() < 20)
        {
            card.append(20 - value.length(), ' ');
        }
    
        card += value;

        if (comment.length() > 0)
        {
            card += " / " + comment;
        }
    }
    
    // pad with spaces
    if (card.length() < 80)
    {
        card.append(80 - card.length(), ' ');
    }

    // Return 80 characters exactly
    return vector<char>(card.begin(), card.begin() + 80);
}

vector<char>
CreateFITS_HeaderCard(string const& keyword, int value, string const& comment = "")
{
    char valueString[80];
    sprintf_s(valueString, "%d", value);
    return CreateFITS_HeaderCard(keyword, valueString, comment);
}

void AppendCard(vector<char>& header, vector<char> const& newCard)
{
    header.insert(header.end(), newCard.begin(), newCard.end());
}

vector<char>
CreateFITSHeader(unsigned cols, unsigned rows, short temperature)
{
    vector<char> header;
    header.reserve(FITS_RECORD_SIZE);

    AppendCard(header, CreateFITS_HeaderCard("SIMPLE", "T", "File Generated by UCSB IRCAM"));
    AppendCard(header, CreateFITS_HeaderCard("BITPIX", "16", "Data is actually 14 bits"));
    AppendCard(header, CreateFITS_HeaderCard("NAXIS", "2"));
    AppendCard(header, CreateFITS_HeaderCard("NAXIS1", cols));
    AppendCard(header, CreateFITS_HeaderCard("NAXIS2", rows));
    AppendCard(header, CreateFITS_HeaderCard("TEMP", temperature));
    AppendCard(header, CreateFITS_HeaderCard("BZERO", "0"));
    AppendCard(header, CreateFITS_HeaderCard("END"));

    // Pad the header out to the appropriate length
    // in most cases, FITS_RECORD_SIZE
    // But this code is robust in the face of changes
    int padding = (FITS_RECORD_SIZE - header.size() % 
        FITS_RECORD_SIZE) % FITS_RECORD_SIZE;
    header.insert(header.end(), padding, ' ');
    return header;
}

isc_Error PrepareCameraAndGetTemp(LPWSTR vifName, LPWSTR vifDevice, short& temperature, bool doFlatField)
{
    isc_Error lastError = eOK;
    int deviceID = 0;
    lastError = OpenControl(vifName, vifDevice, &deviceID);
    if (lastError != eOK)
    {
        UCSBUtility::LogError(__FUNCTION__, __FILE__, __LINE__, "OpenControl failed, aborting (error %d)\\n", lastError);
        return lastError;
    }

	lastError = SetBaudRate(deviceID, FLIR_Baud);
    if (lastError != eOK)
    {
        UCSBUtility::LogError(__FUNCTION__, __FILE__, __LINE__, "Unable to set BaudRate, aborting (error %d)\\n", lastError);
        return lastError;
    }

    if (doFlatField)
    {
        lastError = SendSimpleCameraCommand(deviceID, FLIR_Command::FLAT_FIELD_CORRECTION);
        if (lastError != eOK)
        {
            UCSBUtility::LogError(__FUNCTION__, __FILE__, __LINE__, "Unable to Flat Field, aborting (error %d)\\n", lastError);
            return lastError;
        }
    }

    unsigned short usTemperature;
    lastError = ReadShortFromCamera(deviceID, FLIR_Command::UNKNOWN_GET_TEMP_COMMAND, usTemperature);
    if (lastError != eOK)
    {
        UCSBUtility::LogError(__FUNCTION__, __FILE__, __LINE__, "Unable to Get Temperature, aborting (error %d)\\n", lastError);
        return lastError;
    }
    if (usTemperature > 32767) 
    {
        usTemperature = static_cast<USHORT>(usTemperature - 65536);
    }
    // now that it is signed, assigned it to a signed value
    
    temperature = static_cast<short>(usTemperature);


    lastError = CloseControl(deviceID);
    if (lastError != eOK)
    {
        UCSBUtility::LogError(__FUNCTION__, __FILE__, __LINE__, "CloseControl failed, aborting (error %d)\\n", lastError);
        return lastError;
    }

    return lastError;
}

isc_Error ReadCameraImage(LPWSTR vifName, LPWSTR vifDevice, short& cols, short& rows, vector<short>& frame)
{
    isc_Error lastError = eOK;
    int deviceID = 0;

    lastError = OpenVideo(vifName, vifDevice, &deviceID);
    if (lastError != eOK)
    {
        UCSBUtility::LogError(__FUNCTION__, __FILE__, __LINE__, "Unable to open the video, aborting (error %d)\\n", lastError);
        return lastError;
    }

    lastError = GetFrameSize(deviceID, channelID, &rows, &cols);
    if (lastError != eOK)
    {
        UCSBUtility::LogError(__FUNCTION__, __FILE__, __LINE__, "Unable to get the frame size, aborting (error %d)\\n", lastError);
        return lastError;
    }

	lastError = SetFrameSize(deviceID, channelID, rows, cols);
    if (lastError != eOK)
    {
        UCSBUtility::LogError(__FUNCTION__, __FILE__, __LINE__, "Unable to set the frame size, aborting (error %d)\\n", lastError);
        return lastError;
    }

	lastError = SetPixelDepth(deviceID, channelID, eISC_16BIT);
    if (lastError != eOK)
    {
        UCSBUtility::LogError(__FUNCTION__, __FILE__, __LINE__, "Unable to set the pixel depth, aborting (error %d)\\n", lastError);
        return lastError;
    }

    // Allocate space to capture the data
    frame.resize(rows * cols);
    lastError = GrabFrame(deviceID, channelID, &frame[0]);
    if (lastError != eOK)
    {
        UCSBUtility::LogError(__FUNCTION__, __FILE__, __LINE__, "Unable to set the pixel depth, aborting (error %d)\\n", lastError);
        return lastError;
    }

    lastError = CloseVideo(deviceID);
    if (lastError != eOK)
    {
        UCSBUtility::LogError(__FUNCTION__, __FILE__, __LINE__, "CloseVideo failed (error %d)\\n", lastError);
        return lastError;
    }


    return lastError;
}

int ReadCamera(wstring const& filename, bool doFlatField)
{
    // SETUP Portion
    // Assume a single camera
    isc_Error lastError = eOK;

    // Assume a single interface
    _bstr_t vifName;
    lastError = GetVideoIF(vifName.GetAddress(),0);
    if (lastError != eOK)
    {
        UCSBUtility::LogError(__FUNCTION__, __FILE__, __LINE__, "No camera interface found (error %d)\\n", lastError);
        return 1;
    }

    // Assume a single camera
    _bstr_t vifDevice;
    lastError = GetIFDevice(vifName, vifDevice.GetAddress(), 0);
    if (lastError != eOK)
    {
        UCSBUtility::LogError(__FUNCTION__, __FILE__, __LINE__, "No camera device found (error %d)\\n", lastError);
        return 1;
    }

    short temperature = 0;                                                      
    lastError = PrepareCameraAndGetTemp(vifName, vifDevice, temperature, doFlatField);
    if (lastError != eOK)
    {
        UCSBUtility::LogError(__FUNCTION__, __FILE__, __LINE__, "Failed to prepare camera (error %d)\\n", lastError);
        return 1;
    }

    short cols = 0;
    short rows = 0;
    vector<short> frame;
    lastError = ReadCameraImage(vifName, vifDevice, cols, rows, frame);
    if (lastError != eOK)
    {
        UCSBUtility::LogError(__FUNCTION__, __FILE__, __LINE__, "Failed to get the image (error %d)\\n", lastError);
        return 1;
    }

    PrepDataForFITSOutput(frame, rows, cols);
    vector<char> header = CreateFITSHeader(cols, rows, temperature);

    // Write the header, then the data to the filename given above
    HANDLE hFile = CreateFile(filename.c_str(), GENERIC_WRITE, 0, NULL, 
        CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
    
    if (hFile == INVALID_HANDLE_VALUE)
    {
        UCSBUtility::LogError(__FUNCTION__, __FILE__, __LINE__, "Unable to open a new file with the name %ls\\n", filename.c_str());
        return 1;
    }
    
    // Write the header
    DWORD count = 0;
    WriteFile(hFile, &header[0], header.size(), &count, NULL);

    // Write the data
    count = frame.size() * sizeof(frame[0]);
    WriteFile(hFile, &frame[0], count, &count, NULL);

    // Done
    CloseHandle(hFile);

    return 0;
}

