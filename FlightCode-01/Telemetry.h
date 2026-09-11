#pragma once

#include "DataSource.h"
#include "comPort.h"
#include "DictionaryConfigFileInterpreter.h"

class Telemetry : public nsDataSource::DataSource, public nsDataSource::AutoRegister<Telemetry>
{
    typedef UCSB_Datastream::DeviceHolder::CacheDataType CacheDataType;
    typedef nsDataSource::FieldIter     FieldIter;
    typedef InternalTime::internalTime  internalTime;
    typedef DictionaryConfigFileInterpreter::dictionaryData dictionaryData;
    typedef UCSB_Datastream::DeviceHolder DeviceHolder;
    typedef InternalTime::internalTime internalTime;

public:
    Telemetry(void);
    virtual ~Telemetry(void);

public :
    virtual bool Start();

    virtual bool AddDeviceTypes()
    {
        return nsDataSource::DataSource::AddDeviceTypes();
    }
    
    virtual bool TickImpl(InternalTime::internalTime const& now);

    virtual FieldIter Configure(FieldIter beg, FieldIter end)
    {
        const DWORD paramCount = 1;
        // read in the parameters for CounterSource
        if ((end - beg) < paramCount)
        {
            UCSBUtility::LogError(__FUNCTION__, __FILE__, __LINE__, "%d parameters expected"
                ", %d parameters found\n", paramCount, (end - beg));
            return end;
        }

        // Read the frequency from the configuration
        SetFrequency(UCSBUtility::ToINT<DWORD>(*beg++));

        m_port.SetParity(0);
        m_port.SetStop(0);
        return m_port.Configure(beg, end);
    }

    void CreateTelemetryDictionary();
    bool CreateOutputChannels();

public :
    static std::string GetName()
    {
        return "Telemetry";
    }

private :
    bool CanSendMoreData() const;
    void CreateFullConfig() const;

private :
    typedef UCSB_Datastream::ChannelDefinition ChannelDefinition;
    typedef std::vector<ChannelDefinition> ChannelDefinitions;

    internalTime            m_current;
    internalTime            m_lastMinute;
    mutable internalTime    m_lastSecond;
    ComPort                 m_port;
    DWORD                   m_bytesPerSecond;
    dictionaryData          m_dictionaryData;
    bool                    m_sendDictionary;
    bool                    m_createFullConfig;
    mutable DWORD           m_written;
    DeviceHolder            m_telemetryDictionary;
    ChannelDefinitions      m_channels;
};
