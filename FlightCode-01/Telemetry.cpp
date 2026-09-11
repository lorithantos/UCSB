#include "StdAfx.h"
#include "Telemetry.h"

#include "ConfigFileReader.h"

#include <algorithm>
#include <map>
#include <string>
#include <vector>

using UCSB_Datastream::Device;
using UCSB_Datastream::DeviceDefinition;
using UCSB_Datastream::DeviceHolder;
using UCSB_Datastream::ChannelDefinition;

using UCSBUtility::ConvertToGUID;
using UCSBUtility::Append;
using UCSBUtility::Write;
using UCSBUtility::GetStringFromCLSID;

typedef std::vector<DWORD> DWORDS;
typedef std::vector<Device> Devices;

using std::string;
using std::vector;

namespace
{

class PacketConverter : public UCSB_Datastream::DefaultThrowingParserHandler
{
    typedef DictionaryConfigFileInterpreter::dictionaryData dictionaryData;
    typedef DictionaryConfigFileInterpreter::DWORDs DWORDs;
    //typedef GUIDToDWORDs::const_iterator

public :
    PacketConverter(dictionaryData const& fields, DeviceHolder const& outputDictionary)
        : m_fields(fields)
        , m_outputDictionary(outputDictionary)
    {
    };

    void DictionaryUpdated() {};
    void ProcessData(Device const& device, BYTE const* pPayload)
    {
        // If we don't support this device, then simply return
        DWORDs fields = m_fields.GetFields(device);

        // We don't need anything from this device
        if (fields.size() == 0)
        {
            return;
        }

        Device outputDevice;
        m_outputDictionary.GetDevice(device.DeviceID(), outputDevice);

        vector<BYTE> outputData;

        // Get the fields from the payload
        for (DWORDs::const_iterator cit=fields.begin(); cit != fields.end(); ++cit)
        {
            UCSBUtility::Append(outputData, device.GetChannelData(*cit, pPayload));
        }

        // Create an appropriate header
        UCSBUtility::Append(m_output, UCSB_Datastream::CreateCacheData(outputData, 
            static_cast<BYTE>(outputDevice.DeviceNumber())));
    }

    static void InvalidData(BYTE messageID, Device const& device, 
        BYTE byteCount, BYTE const* pPayload)
    {
        // Silence the compiler warning
        messageID;
        pPayload;


        UCSBUtility::LogError(__FUNCTION__, __FILE__, __LINE__, "%s expected %d bytes received %d\n", 
            device.DisplayName().c_str(), device.GetDataSize(), byteCount);
    }

    static void MissingDictionaryEntry(BYTE dictionaryEntry)
    {
        UCSBUtility::LogError(__FUNCTION__, __FILE__, __LINE__, "Dictionary Entry %d is missing\n", dictionaryEntry);
    }

    std::vector<BYTE> const& GetOutput() const
    {
        return m_output;
    }

private :
    PacketConverter(PacketConverter const&);
    PacketConverter operator = (PacketConverter const&);

private :
    dictionaryData const&           m_fields;
    DeviceHolder const&             m_outputDictionary;
    std::vector<BYTE>               m_output;
};

};

Telemetry::Telemetry(void) 
    : nsDataSource::AutoRegister<Telemetry>(0)
    , m_sendDictionary(true)
    , m_written(0)
    , m_createFullConfig(true)
{
}

Telemetry::~Telemetry(void)
{
}

vector<ChannelDefinition>
GetChannels(Device const& device, DWORDS const& channels)
{
    vector<ChannelDefinition> retv;

    for (DWORDS::const_iterator cit = channels.begin();
        cit != channels.end(); ++cit)
    {
        if (*cit < device.GetChannelCount())
        {
            retv.push_back(device.GetChannel(*cit));
        }
    }

    return retv;
}

bool Telemetry::CreateOutputChannels()
{
    typedef std::vector<GUID>  GUIDS;
    m_channels.clear();

    // Create a new dictionary from the program dictionary + the data we have
    DeviceHolder const& dictionary = GetDeviceHolder();

    // Any fields not found must be discarded - there is no chance they will be added
    
    // iterate through the devices in m_dictionaryData
    // for each device found, collect the channels into m_telemetryDictionary
    GUIDS deviceGuids = m_dictionaryData.GetDevices();

    for (GUIDS::const_iterator cit = deviceGuids.begin();
        cit != deviceGuids.end(); ++cit)
    {
        UCSB_Datastream::Device device;
        if (dictionary.GetDevice(*cit, device))
        {
            // Get the channels for this device
            Append(m_channels, GetChannels(device, 
                m_dictionaryData.GetFields(device)));
        }
    }

    return m_channels.size() > 0;
}

void Telemetry::CreateTelemetryDictionary()
{
    m_telemetryDictionary.ClearDictionary();

    if (!CreateOutputChannels())
    {
        return;
    }

    //// {4607EAE9-3A8D-4dde-B481-0FAB02ED0548}
    //static const GUID deviceGUID = 
    //{ 0x4607eae9, 0x3a8d, 0x4dde, { 0xb4, 0x81, 0xf, 0xab, 0x2, 0xed, 0x5, 0x48 } };

    // In order to preserve the ability to see which devices are which
    // the new dictionary relies on the old dictionary's device GUIDS
    // It merely adds fewer channels
    
    typedef std::map<GUID, Device, UCSBUtility::GUIDLess> GUID_Devices;
    GUID_Devices guidDevices;

    for (ChannelDefinitions::const_iterator cit = m_channels.begin(); 
        cit != m_channels.end(); ++cit)
    {
        guidDevices[cit->deviceGUID].AddChannel(*cit, false);
    }

    Devices devices;
    // Add each device to the dictionary
    for (GUID_Devices::const_iterator cit = guidDevices.begin();
        cit != guidDevices.end(); ++cit)
    {
        Device originalDevice;
        GetDeviceHolder().GetDevice(cit->first, originalDevice);

        // BUGBUG big cheat here!
        DeviceDefinition* pdd = (DeviceDefinition*)&originalDevice;
        Device newDevice = cit->second;
        DeviceDefinition* pddNew = (DeviceDefinition*)&newDevice;
        pddNew->deviceID = pdd->deviceID;
        pddNew->channelCount = static_cast<BYTE>(cit->second.GetChannelCount());
        memcpy(pddNew->displayName, pdd->displayName, sizeof(pddNew->displayName));

        devices.push_back(newDevice);
    }

    m_telemetryDictionary.BuildDictionary(devices);
}

bool Telemetry::Start()
{
    bool success = m_port.Start();

    m_lastMinute = m_lastMinute.Now();
    m_lastSecond = m_lastMinute;

#if 0
    if (!success)
    {
        return false;
    }
#endif
    // Read the configuration into m_dictionaryData
    DictionaryConfigFileInterpreter interpreter (m_dictionaryData);

    success |= ConfigFileReader::InterpretConfigurationFields
        (L"telemetrySend", interpreter);

#if 0
    if (!success)
    {
        return false;
    }
#endif

    CreateTelemetryDictionary();

    return true;
}

// Governor functionality
bool Telemetry::CanSendMoreData() const
{
    if (m_written == 0)
    {
        return true;
    }

    UINT64 time = m_current - m_lastSecond;
    DWORD elapsedMS = static_cast<DWORD> (time / InternalTime::dw100nsPerMS);
    DWORD supportedRate = m_port.GetBaud() * elapsedMS / 8 / 1000 * 9 / 10;

    // We are over the limit
    // stop writing until we are back on track
    if (supportedRate < m_written)
    {
        return false;
    }
    
    m_lastSecond = m_current;
    m_written = 0;

    return true;
}

bool Telemetry::TickImpl(InternalTime::internalTime const& now)
{
    m_current = now;
    if (now - m_lastMinute > InternalTime::oneMinute)
    {
        m_sendDictionary = true;
        m_lastMinute = now;
    }

    if (m_createFullConfig)
    {
        CreateFullConfig();
        m_createFullConfig = false;
    }

    if (!CanSendMoreData())
    {
        return true;
    }

    if (m_sendDictionary)
    {
        DWORD written;
        m_telemetryDictionary.CreateFileHeader(m_port.GetHandle(), &written);
        m_written += written;
        m_sendDictionary = false;
    }
    else
    {
        // Get the data from all of the the sources
        CacheDataType cachedData = GetDeviceHolder().GetCache();

        PacketConverter converter(m_dictionaryData, m_telemetryDictionary);
        DeviceHolder::FileParser<PacketConverter> parser(GetDeviceHolder(), converter);
        LN250::InputBuffer<DeviceHolder::FileParser<PacketConverter> > fileBuffer(parser);

        // Collect the data
        for (CacheDataType::const_iterator cit = cachedData.begin(); 
            cit != cachedData.end(); ++cit)
        {
            if (cit->second.size() > 0)
            {
                fileBuffer.AddData(&cit->second[0], &cit->second[0] + cit->second.size());
            }
        }

        if (converter.GetOutput().size() > 0)
        {
            DWORD written = 0;
            m_port.Write(converter.GetOutput(), &written);

            m_written += written;

            // It should never happen that m_written is zero
            // It only happens if the code has ignored the fact tha the port
            // isn't actually open - bad behavior on my part.
            // But useful in some tests.
            if (written != 0)
            {
                m_written += UCSB_Datastream::WriteMessageToFile(
                    m_port.GetHandle(), UCSB_Datastream::TELEMETRY_EOL).size();
            }
        }
    }

    return true;
}

void Telemetry::CreateFullConfig() const
{
    // Iterate through the dictionary and write each device
    UCSB_Datastream::DeviceHolder const& dictionary = GetDeviceHolder();

    string output;

    for(BYTE i=UCSB_Datastream::FIRST_DEVICE_TYPE; i < 
        UCSB_Datastream::LAST_DEVICE_TYPE; ++i)
    {
        UCSB_Datastream::Device const& device = dictionary.GetDevice(i);

        // Invalid devices have a device number of zero
        // any device without channels is invalid
        if(device.DeviceNumber() == 0 || device.GetChannelCount() == 0)
        {
            continue;
        }

        // Device ID
        output += "; " + device.DisplayName() +  "\n[" + 
            GetStringFromCLSID(device.DeviceID()) + "] ";

        // Channel Count
        char buffer[40];
        sprintf_s(buffer, "[%d] ; Channel Count\n", device.GetChannelCount());
        output += buffer;

        vector<string> channels = device.GetChannelNames();
        for (vector<string>::const_iterator cit = channels.begin();
            cit != channels.end(); ++cit)
        {
            output += "\t[" + *cit + "]\n";
        }

        // Extra line to separate the 
        output += "\n";
    }

    if (output.size() == 0)
    {
        return;
    }

    FILE* pFile = NULL;
    fopen_s(&pFile, "telemetryExampleConfig.txt", "wt");

    if (pFile == NULL)
    {
        return;
    }

    // Write the name of the telemetry block
    fprintf_s(pFile, "; Name of the data block we are creating\n"
        "[Full Telemetry]\n\n"
        "; Remember to change the channel count if removing any channels\n");
    fwrite(&output[0], 1, output.size(), pFile);
    fclose(pFile);
}