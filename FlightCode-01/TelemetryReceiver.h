#pragma once

#include "DataSource.h"
#include "comPort.h"
#include "DictionaryConfigFileInterpreter.h"

class TelemetryReceiver : public nsDataSource::DataSource, public nsDataSource::AutoRegister<TelemetryReceiver>
{
    typedef nsDataSource::FieldIter     FieldIter;
    typedef std::string     string;
    typedef std::wstring    wstring;
    typedef UCSB_Datastream::DeviceHolder DeviceHolder;
    struct  ReceiverPrint;
    typedef DeviceHolder::FileParser<ReceiverPrint> FileParser;
    typedef LN250::InputBuffer<FileParser>
        FileParserBuffer;

public:
    TelemetryReceiver(void);
    virtual ~TelemetryReceiver(void);

public :
    virtual bool Start();

    virtual bool AddDeviceTypes()
    {
        return nsDataSource::DataSource::AddDeviceTypes();
    }
    
    virtual bool TickImpl(InternalTime::internalTime const& now);

    virtual FieldIter Configure(FieldIter beg, FieldIter end)
    {
        const DWORD paramCount = 2;
        // read in the parameters for CounterSource
        if ((end - beg) < paramCount)
        {
            UCSBUtility::LogError(__FUNCTION__, __FILE__, __LINE__, "%d parameters expected"
                ", %d parameters found\n", paramCount, (end - beg));
            return end;
        }

        // Read the frequency from the configuration
        SetFrequency(UCSBUtility::ToINT<DWORD>(*beg++));

        string  directory = *beg++;

        wstring filename;
        UCSBUtility::CreateDirectoryGetFilename(UCSBUtility::
            StupidConvertToWString(directory), L".telemetry", filename);
        
        m_hOutputFile = CreateFile(filename.c_str(), GENERIC_WRITE, FILE_SHARE_READ, 
            NULL, CREATE_NEW, 0, NULL);
        if (m_hOutputFile == INVALID_HANDLE_VALUE)
        {
            UCSBUtility::LogError(__FUNCTION__, __FILE__, __LINE__, 
                "Telemetry Output file %ls not created in direcotry %s",
                filename.c_str(), directory.c_str());
        }

        m_port.SetParity(0);
        m_port.SetStop(0);
        return m_port.Configure(beg, end);
    }

public :
    static std::string GetName()
    {
        return "TelemetryReceiver";
    }

private :
    ComPort                     m_port;
    HANDLE                      m_hOutputFile;
    CONSOLE_SCREEN_BUFFER_INFO  m_screenBufferInfo;
    DeviceHolder                m_receiveDicionary;
    ReceiverPrint*              m_pPrint;
    FileParser                  m_parser;
    FileParserBuffer            m_parserBuffer;
};
