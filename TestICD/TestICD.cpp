// TestICD.cpp : A code to test the LN-250
//

#include "stdafx.h"

// STL headers
#include <string>
#include <vector>

#include "ioTech.h"

// Local headers
#include "ConfigFileReader.h"
#include "InternalTime.h"
#include "LN250.h"
#include "utility.h"

// System headers
#include <math.h>
#include <process.h>
#include <windows.h>

using namespace InternalTime;
using namespace UCSBUtility;
using namespace ioTech;

using LN250::HighSpeedPortMessageHeader;
using LN250::HybridInertialData;
using LN250::MessageControl;
using LN250::StartOfHeaderConst;
using LN250::CreateChecksum;
using LN250::ChunkInputData;

using std::string;
using std::wstring;
using std::vector;

// Constants
DWORD const expectedFileSize = 50*1024*1024;
            
// Types used
typedef AutoCloseHandle<HANDLE, INVALID_HANDLE_VALUE, BOOL(WINAPI *)(HANDLE), CloseHandle>  MutexHandle;

// global variables
//HANDLE g_mutexHID;
bool   g_quit = false;
DWORD  g_displaySleep = 0;
IOTech g_IOTech;
LN250::systemTimeToScreen g_toScreen;

struct WriteHybridInertialDataToBin
{
    WriteHybridInertialDataToBin(string const& filename)
    {
        errno_t err = fopen_s(&pFile, filename.c_str(), "wb");
        if (!pFile)
        {
            UCSBUtility::LogError("Unable to open output file - reason %d\n", err);
            return;
        }
    }

    ~WriteHybridInertialDataToBin()
    {
        if (pFile)
        {
            fclose(pFile);
        }
    }

    void operator() (BYTE messageID, BYTE byteCount, BYTE const* pPayload) const
    {
        if (!pFile)
        {
            return;
        }

        if (messageID != HybridInertialData::MESSAGE_ID || byteCount != sizeof(HybridInertialData))
        {
            return;
        }
        
        HybridInertialData hid = HybridInertialData::Reorder(*reinterpret_cast<HybridInertialData const*>(pPayload));

        WriteToFile(hid.SystemTimer);
        WriteToFile(hid.GPSTime);
        WriteToFile(hid.OutputDataValidity);
        WriteToFile(hid.HybridLatitude);
        WriteToFile(hid.HybridLongitude);
        WriteToFile(hid.HybridAltitude);
        WriteToFile(hid.HybridNorthVelocity);
        WriteToFile(hid.HybridEastVelocity);
        WriteToFile(hid.HybridVerticalVelocity);
        WriteToFile(hid.HybridHeadingAngle);
        WriteToFile(hid.HybridPitchAngle);
        WriteToFile(hid.HybridRollAngle);
        WriteToFile(hid.HybridYawAngle);
        WriteToFile(hid.HybridFOM);
    }

    void WriteToFile(double value) const
    {
        fwrite(&value, sizeof(double), 1, pFile);
    }

private :
    FILE*   pFile;

    WriteHybridInertialDataToBin(WriteHybridInertialDataToBin const&);
    WriteHybridInertialDataToBin& operator =(WriteHybridInertialDataToBin const&);
};

//// Consumer is safe
//// producer must be interlocked
//struct ProducerConsumer
//{
//    ProducerConsumer() : m_producerPtr(NULL), m_consumerPtr(NULL) {}
//
//    void SetProducer(BYTE* pProducer)
//    {
//        InterlockedExchangePointer(m_producerPtr, pProducer);
//    }
//
//    void SetConsumer(BYTE* pConsumer)
//    {
//        m_consumerPtr = pConsumer;
//    };
//
//    BYTE* GetConsumer()
//    {
//        return m_consumerPtr;
//    }
//
//private :
//    BYTE* m_producerPtr;
//    BYTE* m_consumerPtr;
//};

struct WriteHybridInertialDataToCSV
{
    WriteHybridInertialDataToCSV(FILE* pFile) : 
        m_pFile (pFile), m_closeFile(false)
    {
        WriteHeader();
    }

    WriteHybridInertialDataToCSV(string const& filename)
        : m_closeFile(true)
    {
        errno_t err = fopen_s(&m_pFile, filename.c_str(), "wt");
        if (!m_pFile)
        {
            UCSBUtility::LogError("Unable to open output file - reason %d\n", err);
            return;
        }

        WriteHeader();
    }

    ~WriteHybridInertialDataToCSV()
    {
        if (m_pFile && m_closeFile)
        {
            fclose(m_pFile);
        }
    }

    void operator() (BYTE messageID, BYTE byteCount, BYTE const* pPayload) const
    {
        if (!m_pFile)
        {
            return;
        }

        if (messageID != HybridInertialData::MESSAGE_ID || byteCount != sizeof(HybridInertialData))
        {
            return;
        }
        
        HybridInertialData hid = HybridInertialData::Reorder(*reinterpret_cast<HybridInertialData const*>(pPayload));
        fprintf(m_pFile, "%f, ", hid.SystemTimer);
        fprintf(m_pFile, "%f, ", hid.GPSTime);
        fprintf(m_pFile, "%d, ", hid.OutputDataValidity);
        fprintf(m_pFile, "%f, ", hid.HybridLatitude);
        fprintf(m_pFile, "%f, ", hid.HybridLongitude);
        fprintf(m_pFile, "%f, ", hid.HybridAltitude);
        fprintf(m_pFile, "%f, ", hid.HybridNorthVelocity);
        fprintf(m_pFile, "%f, ", hid.HybridEastVelocity);
        fprintf(m_pFile, "%f, ", hid.HybridVerticalVelocity);
        fprintf(m_pFile, "%f, ", hid.HybridHeadingAngle);
        fprintf(m_pFile, "%f, ", hid.HybridPitchAngle);
        fprintf(m_pFile, "%f, ", hid.HybridRollAngle);
        fprintf(m_pFile, "%f, ", hid.HybridYawAngle);
        fprintf(m_pFile, "%d\n", hid.HybridFOM);
    }
private :
    void WriteHeader()
    {
        if (!m_pFile)
        {
            return;
        }

        // Write the names of each field
        fprintf(m_pFile, "SystemTimer, ");
        fprintf(m_pFile, "GPSTime, ");
        fprintf(m_pFile, "OutputDataValidity, ");
        fprintf(m_pFile, "HybridLatitude, ");
        fprintf(m_pFile, "HybridLongitude, ");
        fprintf(m_pFile, "HybridAltitude, ");
        fprintf(m_pFile, "HybridNorthVelocity, ");
        fprintf(m_pFile, "HybridEastVelocity, ");
        fprintf(m_pFile, "HybridVerticalVelocity, ");
        fprintf(m_pFile, "HybridHeadingAngle, ");
        fprintf(m_pFile, "HybridPitchAngle, ");
        fprintf(m_pFile, "HybridRollAngle, ");
        fprintf(m_pFile, "HybridYawAngle, ");
        fprintf(m_pFile, "HybridFOM\n");
    }

private :
    FILE*   m_pFile;
    bool    m_closeFile;

    WriteHybridInertialDataToCSV(WriteHybridInertialDataToCSV const&);
    WriteHybridInertialDataToCSV& operator =(WriteHybridInertialDataToCSV const&);
};

enum OUTPUTTYPE
{
    BINARY_OUTPUT,
    CSV_OUTPUT,
};

// testing data stream
int InterpretData(wstring const& inputName, wstring const& outputName, OUTPUTTYPE outType)
{
    HANDLE hFile = CreateFile(inputName.c_str(), 
        GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL);
    
    if (hFile == INVALID_HANDLE_VALUE)
    {
        UCSBUtility::LogError("Unable to open data file\n");
        return 1;
    }

    vector<BYTE> data;
    data.resize(GetFileSize (hFile, NULL));
    DWORD readSize = 0;
    ReadFile(hFile, &data[0], data.size(), &readSize, NULL);
    CloseHandle(hFile);

    string outputFilename = StupidConvertToString(outputName);

    if (outType == BINARY_OUTPUT)
    {
        WriteHybridInertialDataToBin toBin (outputFilename);
        ChunkInputData(&data[0], &data[0] + data.size(), toBin);
    }
    else if (outType == CSV_OUTPUT)
    {
        WriteHybridInertialDataToCSV toCSV(outputFilename);
        ChunkInputData(&data[0], &data[0] + data.size(), toCSV);
    }

    return 0;
}

void __cdecl IOTechThread(void* /*pThreadData*/)
{
    while (!g_quit)
    {
        g_IOTech.Read();

        //IOTech::Encoder encoder = g_IOTech.GetEncoder();
        Sleep(5);
    }
}

void cls( HANDLE hConsole )
{
    COORD coordScreen = { 0, 0 };    // home for the cursor 
    DWORD cCharsWritten;
    CONSOLE_SCREEN_BUFFER_INFO csbi; 
    DWORD dwConSize;

    // Get the number of character cells in the current buffer. 

    if( !GetConsoleScreenBufferInfo( hConsole, &csbi ))
        return;
    dwConSize = csbi.dwSize.X * csbi.dwSize.Y;

    // Fill the entire screen with blanks.

    if( !FillConsoleOutputCharacter( hConsole, (TCHAR) ' ',
        dwConSize, coordScreen, &cCharsWritten ))
        return;

    // Get the current text attribute.

    if( !GetConsoleScreenBufferInfo( hConsole, &csbi ))
        return;

    // Set the buffer's attributes accordingly.

    if( !FillConsoleOutputAttribute( hConsole, csbi.wAttributes,
        dwConSize, coordScreen, &cCharsWritten ))
        return;

    // Put the cursor at its home coordinates.

    SetConsoleCursorPosition( hConsole, coordScreen );
}

void __cdecl DisplayThread(void* /*pThreadData*/)
{
    //HybridInertialData const* pLast = NULL;
    //double diff = 0;
    //double startSystem = 0;
    //double startGPS = 0;

    internalTime nextStop = internalTime::Now() + 
        g_displaySleep * oneSecond / 1000;

    printf ("system time"
        ", GPS time"
        ", HybridHeadingAngle"
        ", HybridYawAngle"
        ", HybridPitchAngle"
        ", HybridRollAngle"
        ", EndcoderValue 1"
        ", EndcoderValue 2"
        ", EndcoderValue 3"
        "\n");
    bool started = false;
    //UINT64 startTime = internalTime::Now();
    HybridInertialData localCopy = {};
    double lastSysTime = 0;
    double lastSkip = 0;
    string outputData;
    outputData.reserve(expectedFileSize);
    bool skip = false;
    while (!g_quit)
    {
        started |= g_toScreen.isInit();
        if (!started)
        {
            continue;
        }

        localCopy = g_toScreen.GetHybridInertialData();
        if (lastSysTime == localCopy.SystemTimer)
        {
            continue;
        }
        DWORD skipValue = static_cast<DWORD> (floor(
            localCopy.SystemTimer * 100) - floor(lastSysTime * 100));
        if (skipValue > 1)
        {
            lastSkip = lastSysTime;
            skip = true;
        }
        else
        {
            skip = false;
        }
        lastSysTime = localCopy.SystemTimer;

        //internalTime clock1 = internalTime::Now();

        IOTech::Encoder encoder = g_IOTech.GetEncoder();
        //internalTime clock2 = internalTime::Now();

        char outputBuffer[1024];

        sprintf_s(outputBuffer, "%10f, %10f, %10f, %f, %f, %f, %d, %d, %d\n",
            localCopy.SystemTimer, 
            localCopy.GPSTime, 
            localCopy.HybridHeadingAngle,
            localCopy.HybridYawAngle,
            localCopy.HybridPitchAngle,
            localCopy.HybridRollAngle,
            encoder.digital[0],
            encoder.digital[1],
            encoder.digital[2]
        );
        if (skip)
        {
            outputData += "\n";
        }
        outputData += outputBuffer;

        //printf("GetHID %12I64i, IOTech %12I64i, Reorder %12I64i\n", clock1 - clock0, clock2 - clock1, clock3 - clock2);
        //if (g_displaySleep)
        //{
        //    if(internalTime::Now() < nextStop)
        //    {
        //        DWORD sleepTime = static_cast<DWORD>(1000 * (nextStop - internalTime::Now()) / 
        //            static_cast<double>(oneSecond));
        //        Sleep(sleepTime);
        //    }

        //    while(nextStop <= static_cast<UINT64>(internalTime::Now()))
        //    {
        //        nextStop = nextStop + g_displaySleep * oneSecond / 1000;
        //    }
        //}
    }

    printf("%s\n", outputData.c_str());
    printf("Last skipped %f\n", lastSkip);

    //printf ("g_quit = %d\n", static_cast<int>(g_quit));
}

int ReadDataBlock(HANDLE hPort, 
                  wstring const& outputFileName, 
                  internalTime const& endTime, 
                  vector<BYTE>& existingData)
{
    internalTime captureSeconds = (endTime - internalTime::Now()) / oneSecond;

    UCSBUtility::LogError("Starting capture for %d seconds\n", captureSeconds);
    UCSBUtility::LogError("Will write to file %ls\n", outputFileName.c_str());

    internalTime endOfTest = internalTime::Now() + captureSeconds * oneSecond;
    internalTime nextSecond = internalTime::Now() + oneSecond;
    //unsigned secondsPassed = 0;

    SetCommMask(hPort, EV_RXCHAR);

    //ProcessHybridInertialData processing;
    DWORD processedBytes = 0;


    BYTE buffer[10240] = {};

    for (;;)
    {
        DWORD eventMask = 0;
        WaitCommEvent(hPort, &eventMask, NULL);

        DWORD bytesRead = 0;
        BOOL readSuccess = ReadFile(hPort, buffer, sizeof(buffer), &bytesRead, NULL);
        if (readSuccess && bytesRead > 0)
        {
            //fprintf(stdout, "Bytes Read %d\n", bytesRead);
            vector<BYTE>::size_type capacity = existingData.capacity();
            vector<BYTE>::size_type existingSize = existingData.size();
            // Check for expansion
            if (bytesRead + existingSize > capacity)
            {
                // double the size
                existingData.reserve(2 * max(capacity, bytesRead));
            }

            existingData.insert(existingData.end(), &buffer[0], &buffer[bytesRead]);

            ChunkInputData(static_cast<const BYTE *>(&existingData[processedBytes]), 
                static_cast<const BYTE *>(&existingData[existingData.size()-1] + 1), 
                g_toScreen);
            //printf("bytes read %d\n", bytesRead);
            processedBytes = g_toScreen.GetNextRead() - &existingData[0];
            processedBytes = min(processedBytes, existingData.size());
            //Sleep(8);
        }

        if (static_cast<UINT64>(internalTime::Now()) >= endOfTest)
        {
            break;
        }
    }

    if (existingData.size() > 0)
    {
        HANDLE hFile = CreateFile(outputFileName.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
        if (hFile != INVALID_HANDLE_VALUE)
        {
            DWORD bytesWritten = 0;
            WriteFile(hFile, &existingData[0], existingData.size(), &bytesWritten, 0);
            CloseHandle(hFile);
        }
    }
    else
    {
        UCSBUtility::LogError("No Data Read!\n");
    }

    g_quit = true;

    return 0;
}

// 
// 
int ReadData(wstring const& portName, 
             DWORD baud, 
             wstring const& outputFileName, 
             unsigned captureSeconds, 
             string const& ioTechBoardName,
             float readRate)
{
    HANDLE hPort = CreateFile(portName.c_str(), GENERIC_READ | GENERIC_WRITE, 0, NULL, 
        OPEN_EXISTING, 0, NULL);
    if (hPort == INVALID_HANDLE_VALUE)
    {
        UCSBUtility::LogError("Unable to open the COM Port %ls\n", portName.c_str());
        return 1;
    }

    if (!g_IOTech.Setup(ioTechBoardName, readRate, 5000, true))
    {
        UCSBUtility::LogError("Unable to setup the IOTech board %s\n", ioTechBoardName.c_str());
        return 1;
    }

    UCSBUtility::LogError("COM Port Open (%ls)\n", portName.c_str());

    SetupPort(hPort, baud);

    CreateWaitableTimer(NULL, false, NULL);



    SendMessage(hPort, MessageControl::MessageOn(0x32));

    vector<BYTE> data;
    data.reserve(expectedFileSize); // expectedFileSize
    _beginthread(DisplayThread, 0, &data);
    _beginthread(IOTechThread, 0, NULL);
    internalTime endTime = internalTime::Now() + captureSeconds * oneSecond;
    ReadDataBlock(hPort, outputFileName, endTime, data);

    SendMessage(hPort, MessageControl::MessageOff(0x32));

    printf("\n");

    UCSBUtility::LogError("Closing Port\n");
    CloseHandle(hPort);

    g_IOTech.Shutdown();

    return 0;
}

struct InterpretConfigFile
{
    InterpretConfigFile() : fn(NOTHING) {}

    unsigned operator ()(vector<string> const& fields)
    {
        //vector<string> fields;

        //for(vector<string>::size_type i=0; i < input.size(); ++i)
        //{
        //    vector<string> subFields = ConfigFileReader::ConvertLineToFields(input[i]);
        //    fields.insert(fields.end(), subFields.begin(), subFields.end());
        //}

        // Look for the first instance of [generate] or [convert]
        for(vector<string>::size_type i=0; i < fields.size(); ++i)
        {
            if (_stricmp(fields[i].c_str(), "generate") == 0)
            {
                // State is generate
                // Do generation code
                GenerateData(vector<string>(fields.begin()+i+1, fields.end()));
                break;
            }

            if (_stricmp(fields[i].c_str(), "convert") == 0)
            {
                // State is convert
                // Do convert code
                ConvertData(vector<string>(fields.begin()+i+1, fields.end()));
                break;
            }
        }

        return 0;
    }

    void Execute()
    {
        switch(fn)
        {
        case  NOTHING : 
            UCSBUtility::LogError("Nothing to do, exiting\n");
            break;
            
        case CONVERT :
            InterpretData(convert.inFile, convert.outFile, convert.type);
            break;

        case GENERATE :
            g_displaySleep = generate.outputDelay;
            ReadData(generate.port, generate.baud, generate.filename, 
                generate.time, generate.ioTechBoard, generate.ioReadRate);
            break;
        }
    }

private :
    void GenerateData(vector<string> const& config)
    {
        // Get the type of data to be read, interpret that
        if (config.size() < 1)
        {
            UCSBUtility::LogError("No parameters found for generating data\n");
            return;
        }
        vector<string>::const_iterator begin = config.begin();
        vector<string>::const_iterator end = config.end();
        fn = GENERATE;

        while (begin < end)
        {
            // First, find the type
            string typeMarker = *begin++;
            InterpretType(typeMarker, begin, end);
        }

        UCSBUtility::LogError("Parameters used:\n"
        "\tLN-250 port %ls\n" 
        "\tLN-250 baud %d\n"
        "\tLN-250 output filename %ls\n" 
        "\tLN-250 seconds to read %d\n"
        "\tIOTech board name %s\n"
        "\tIOTech read rate %f\n"
        "\tDisplay frequency %fHz\n",
            generate.port.c_str(),
            generate.baud,
            generate.filename.c_str(),
            generate.time,
            generate.ioTechBoard.c_str(),
            generate.ioReadRate,
            generate.outputDelay ? (1000 / generate.outputDelay) : -1.f);
    };

    void InterpretType(string const& type, 
        vector<string>::const_iterator& begin,
        vector<string>::const_iterator end)
    {
        if (_stricmp(type.c_str(), "LN-250") == 0)
        {
            ReadLN250Config(begin, end);
            return;
        }

        if (_stricmp(type.c_str(), "IOTech") == 0)
        {
            ReadIOTechConfig(begin, end);
        }

        if (_stricmp(type.c_str(), "Display") == 0)
        {
            ReadDisplayConfig(begin, end);
        }
    }

    bool ReadLN250Config(vector<string>::const_iterator& begin, vector<string>::const_iterator end)
    {
        const int numParams = 5;
        const string deviceName = "LN-250";

        if (end - begin < numParams)
        {
            UCSBUtility::LogError("%d of %d parameters found for %s\n", end - begin, numParams, deviceName.c_str());
            begin = end;
            fn = NOTHING;
            return false;
        }

        generate.port = StupidConvertToWString(*begin++);
        generate.baud = atol(begin->c_str()); ++begin;
        generate.filename = StupidConvertToWString(*begin++);
        generate.time = atol(begin->c_str()); ++begin;
        return true;
    }

    bool ReadIOTechConfig(vector<string>::const_iterator& begin, vector<string>::const_iterator end)
    {
        const int numParams = 2;
        const string deviceName = "IOTech";

        if (end - begin < numParams)
        {
            UCSBUtility::LogError("%d of %d parameters found for %s\n", end - begin, numParams, deviceName.c_str());
            begin = end;
            fn = NOTHING;
            return false;
        }

        generate.ioTechBoard = *begin++;
        generate.ioReadRate = static_cast<float>(atof(begin->c_str())); ++begin;

        return true;
    }

    bool ReadDisplayConfig(vector<string>::const_iterator& begin, vector<string>::const_iterator end)
    {
        const int numParams = 1;
        const string deviceName = "Display";

        if (end - begin < numParams)
        {
            UCSBUtility::LogError("%d of %d parameters found for %s\n", end - begin, numParams, deviceName.c_str());
            begin = end;
            fn = NOTHING;
            return false;
        }

        double frequency = fabs(atof(begin->c_str()));
        ++begin;
        if (frequency != 0)
        {
            generate.outputDelay = static_cast<DWORD>(1000. / frequency);
        }
        else
        {
            generate.outputDelay = 0;
        }

        return true;
    }

    void ConvertData(vector<string> const& config)
    {
        // Wrapper to ensure data is sent to conversion code correctly
        // There need to be at least four parameters
        if (config.size() < 3)
        {
            UCSBUtility::LogError("Converting data requires more parameters\n");
            return;
        }
        
        convert.inFile = StupidConvertToWString(config[0]);
        convert.outFile = StupidConvertToWString(config[1]);
        if (_stricmp(config[2].c_str(), "BIN-DOUBLE") == 0)
        {
            convert.type = BINARY_OUTPUT;
            fn = CONVERT;
        }
        else if (_stricmp(config[2].c_str(), "CSV") == 0)
        {
            convert.type = CSV_OUTPUT;
            fn = CONVERT;
        }

        if (fn == NOTHING)
        {
            UCSBUtility::LogError("Unable to determine configuration type\n");
        }
    };

private :
    enum FUNCTION { NOTHING, CONVERT, GENERATE };
    enum DEVICE { Disk, Display, LN_250, IOTech };
    //static const LPCSTR devices[] = {"disk", "display", "iotech", "ln-250"};

    FUNCTION    fn;
    struct
    {
        wstring port;
        DWORD   baud;
        wstring filename;
        DWORD   time;
        DWORD   outputDelay;
        string  ioTechBoard;
        float   ioReadRate;
    } generate;

    struct
    {
        wstring     inFile;
        wstring     outFile;
        OUTPUTTYPE  type;
    } convert;
};

int _tmain(int argc, _TCHAR* argv[])
{

    InterpretConfigFile icf;
    ConfigFileReader::ReadConfigurationFields(argc, argv, icf);

    // Do whatever we've been configured to do
    icf.Execute();

    return 0;
}
