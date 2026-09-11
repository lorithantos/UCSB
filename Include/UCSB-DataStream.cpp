#include "stdafx.h"

#include "UCSB-Datastream.h"

// We need this function
using LN250::ProcessFn;
using LN250::InputBuffer;

using namespace UCSB_Datastream;
using namespace UCSBUtility;

using std::string;
using std::vector;

BYTE const DeviceHolder::g_header[8] = { 'S', 'P', 'A', 'C', 'B', 'A', 'L', 'L' };


DataTypeMapping UCSB_Datastream::mapping[DATATYPE_COUNT] = 
{
    { TEXT_CHAR, "TEXT_CHAR", 1 },            // 1 byte, used as a text character
    { UCSB_Datastream::UCHAR, "UCHAR", 1 },    // 1 byte unsigned [0...255]
    { SCHAR,     "SCHAR", 1 },                     // 1 byte signed   [-128...127]
    { USHORT_I,  "USHORT_I", 2 },               // 2 byte unsigned [0...65535] Intel byte ordering
    { USHORT_M,  "USHORT_M", 2 },               // 2 byte unsigned [0...65535] Motorolla byte ordering
    { SSHORT_I,  "SSHORT_I", 2 },               // 2 byte signed   [-32768...32767] Intel byte ordering
    { SSHORT_M,  "SSHORT_M", 2 },               // 2 byte signed   [-32768...32767] Motorolla byte ordering
    { ULONG_I,   "ULONG_I", 4 },                 // 4 byte unsigned [0...4294967295] Intel byte ordering
    { ULONG_M,   "ULONG_M", 4 },                 // 4 byte unsigned [0...4294967295] Motorolla byte ordering
    { SLONG_I,   "SLONG_I", 4 },                 // 4 byte signed   [-2147483648...2147483647] Intel byte ordering
    { SLONG_M,   "SLONG_M", 4 },                 // 4 byte signed   [-2147483648...2147483647] Motorolla byte ordering
    { SPFLOAT_I, "SPFLOAT_I", 4 },             // 4 byte floating point (IEEE 754-2008) Intel byte ordering
    { SPFLOAT_M, "SPFLOAT_M", 4 },             // 4 byte floating point (IEEE 754-2008) Motorola byte ordering
    { DPFLOAT_I, "DPFLOAT_I", 8 },             // 8 byte floating point (IEEE 754-2008) Intel byte ordering
    { DPFLOAT_M, "DPFLOAT_M", 8 },             // 8 byte floating point (IEEE 754-2008) Motorola byte ordering
    { UINT64_I,  "UINT64_I", 8 },              // 8 byte unsigned Intel byte ordering
};

DWORD DeviceHolder::FindDevice(GUID const& deviceID) const
{
    GUID null = {};

    // NULL is not a valid device ID
    if (memcmp(&deviceID, &null, sizeof(deviceID) == 0))
    {
        INVALID_DEVICE_NUMBER;
    }

    for(DWORD device=FIRST_DEVICE_TYPE; 
        device <= LAST_DEVICE_TYPE; 
        ++device)
    {
        if(IsEqualGUID(deviceID, m_Dictionary[device].DeviceID()))
        {
            return device;
        }
    }

    return INVALID_DEVICE_NUMBER;
}

vector<string>::const_iterator
DeviceHolder::InterpretDictionaryConfigBlock(
    vector<string>::const_iterator begin, 
    vector<string>::const_iterator end)
{
    // There needs to be at least three items
    if (end - begin < 3)
    {
        UCSBUtility::LogError(__FUNCTION__, __FILE__, __LINE__, "Malformed config file "
            "- expecting at least three more items %d found\n", end - begin);
        return end;
    }

    //
    string const& guidS = *begin++;
    string const& name = *begin++;
    unsigned channels = atoi(begin->c_str()); ++begin;

    DeviceDefinition newDeviceDefinition;
    newDeviceDefinition.deviceNumber = static_cast<BYTE>(m_NextDeviceID++);
    if (m_NextDeviceID > 255)
    {
        UCSBUtility::LogError(__FUNCTION__, __FILE__, __LINE__, "Too many devices found - unable to continue\n");
        return end;
    }

    newDeviceDefinition.channelCount = static_cast<BYTE>(channels);
    strncpy_s(newDeviceDefinition.displayName, name.c_str(), _countof(newDeviceDefinition.displayName));
    newDeviceDefinition.displayName[_countof(newDeviceDefinition.displayName)-1] = 0;

    RPC_STATUS status = CLSIDFromString((LPOLESTR)UCSBUtility::
        StupidConvertToWString(guidS).c_str(), &newDeviceDefinition.deviceID);    
    if (status != RPC_S_OK)
    {
        UCSBUtility::LogError(__FUNCTION__, __FILE__, __LINE__, "Bad value for ID (GUID expected '%s' found)\n", guidS.c_str());
        return begin;
    }

    // Make sure there's enough data to do the rest of the work
    if (end - begin < 2 * newDeviceDefinition.channelCount)
    {
        UCSBUtility::LogError(__FUNCTION__, __FILE__, __LINE__, "Malformed config file "
            "- expecting %d more items, %d found\n", 2 * newDeviceDefinition.channelCount,
            end - begin);
        return end;
    }

    Device newDevice = Device(newDeviceDefinition);

    for (unsigned i=0; i < newDeviceDefinition.channelCount; ++i)
    {
        string const& typeName = *begin++;
        string const& channelName = *begin++;
        ChannelDefinition cd;
        cd.deviceGUID = newDeviceDefinition.deviceID;
        cd.channelIndex = static_cast<BYTE>(i);
        cd.dataType = static_cast<BYTE>(ConvertStringToDataType(typeName));
        strncpy_s(cd.displayName, channelName.c_str(), _countof(cd.displayName));
        newDevice.AddChannel(cd);
    }

    m_Dictionary[newDeviceDefinition.deviceNumber] = newDevice;

    return begin;
}

unsigned DeviceHolder::BuildDictionary(vector<string> const& fields)
{
    m_Dictionary.clear();
    m_writeDictionary = true;

    // The first three items in each definition are the
    // GUID
    // Name
    // channel count

    for (vector<string>::const_iterator begin = fields.begin(); 
        begin < fields.end();
        )
    {
        begin = InterpretDictionaryConfigBlock(begin, fields.end());
    }

    return 0;
}

void DeviceHolder::BuildDictionary(std::vector<Device> const& devices)
{
    typedef std::vector<Device> Devices;

    m_Dictionary.clear();
    m_Dictionary.resize(256);
    m_writeDictionary = true;

    m_NextDeviceID = FIRST_DEVICE_TYPE;

    for (Devices::const_iterator cit = devices.begin(); 
        cit != devices.end(); ++cit)
    {
        BYTE deviceNumber = static_cast<BYTE>(m_NextDeviceID++);
        m_Dictionary[deviceNumber] = *cit;
        m_Dictionary[deviceNumber].deviceNumber = deviceNumber;
    }
}

// We need to write the definitions to a file
// Start with the ID 
// Then dictionary blocks
// Then we are ready to take data
bool DeviceHolder::CreateFileHeader(HANDLE hFile, DWORD* pWritten) const
{
    if (m_writeDictionary || m_headerData.size() == 0)
    {
        m_writeDictionary = false;

        m_headerData.clear();
        UCSBUtility::AddStructure(m_headerData, g_header);

        // Go through the dictionary write each Device out
        for (unsigned i=FIRST_DEVICE_TYPE; i < LAST_DEVICE_TYPE; ++i)
        {
            if (m_Dictionary[i].DeviceNumber())
            {
                DeviceDefinition output = m_Dictionary[i].GetDefinition();
                output.channelCount = static_cast<BYTE>(m_Dictionary[i].GetChannelCount());
                std::vector<BYTE> deviceBuffer = CreateCacheData(output, DEVICE_DEFINITION);
                m_headerData.insert(m_headerData.end(), deviceBuffer.begin(), deviceBuffer.end());

                // 
                // For each device, write each channel
                for (unsigned j = 0; j < output.channelCount; ++j)
                {
                    ChannelDefinition cd = m_Dictionary[i].GetChannel(j);
                    cd.deviceGUID = output.deviceID;
                    cd.channelIndex = static_cast<BYTE>(j);
                    std::vector<BYTE> channelBuffer = CreateCacheData(cd, CHANNEL_DEFINITION);
                    m_headerData.insert(m_headerData.end(), channelBuffer.begin(), channelBuffer.end());
                }
            }
        }
    }

    if (m_headerData.size() == 0)
    {
        throw "Massive programming error in " __FUNCTION__;
    }

    DWORD written = 0;
    if (pWritten == NULL)
    {
        pWritten = &written;
    }

    WriteFile(hFile, &m_headerData[0], m_headerData.size(), pWritten, NULL);
    return true;
}

void DeviceHolder::AddDevice(BYTE const* pPayload, BYTE byteCount)
{
    // check for bad size
    if (byteCount != sizeof(DeviceDefinition))
    {
        UCSBUtility::LogError(__FUNCTION__, __FILE__, __LINE__, "Bad Device Definition in the stream (wrong size)\n");
        return;
    }

    DeviceDefinition const* pDevice = 
        reinterpret_cast<DeviceDefinition const*>(pPayload);

    m_Dictionary[pDevice->deviceNumber] = Device(*pDevice);
    //m_Dictionary[pDevice->deviceNumber].dataChannels.
    //    resize(pDevice->channelCount);
}

void DeviceHolder::AddChannel(BYTE const* pPayload, BYTE byteCount)
{
    // check for bad size
    if (byteCount != sizeof(ChannelDefinition))
    {
        UCSBUtility::LogError(__FUNCTION__, __FILE__, __LINE__, "Bad Channel Definition in the stream (wrong size)\n");
        return;
    }

    ChannelDefinition const* pChannel = 
        reinterpret_cast<ChannelDefinition const*>(pPayload);

    // Look up the correct device (by GUID)
    // We can't add channels to a device without the device...
    DWORD deviceID = FindDevice(pChannel->deviceGUID);
    if (deviceID == INVALID_DEVICE_NUMBER)
    {
        UCSBUtility::LogError(__FUNCTION__, __FILE__, __LINE__, "Invalid Device ID...Channel must follow the Device\n");
        return;
    }

    //if (m_Dictionary[deviceID].dataChannels.size() <= pChannel->channelIndex)
    //{
    //    if (m_Dictionary[deviceID].dataChannels.size() == 
    //        m_Dictionary[deviceID].channelCount)
    //    {
    //        UCSBUtility::LogError(__FUNCTION__, __FILE__, __LINE__, "Invalid channel number for device definition\n");
    //        return;
    //    }

    //    // We should never get here - the size should be set in the AddDevice code
    //    m_Dictionary[deviceID].dataChannels.resize(
    //        m_Dictionary[deviceID].channelCount);
    //}

    m_Dictionary[deviceID].AddChannel(*pChannel);
}

bool DeviceHolder::DictionaryBuilderImpl(BYTE messageID, BYTE byteCount, BYTE const* pPayload)
{
    // Look at the pre-defined values
    switch(messageID)
    {
    case DEVICE_DEFINITION :
        AddDevice(pPayload, byteCount);
        return true;

    case CHANNEL_DEFINITION :
        AddChannel(pPayload, byteCount);
        return true;

        //case BLOCK_DEFINITION :
        //    AddBlock(pPayload, byteCount);
        //    break;
    }

    return false;
}

bool DeviceHolder::FileParserImpl(BYTE messageID, BYTE byteCount, BYTE const* pPayload)
{
    if (DictionaryBuilderImpl(messageID, byteCount, pPayload))
    {
        return true;
    }

    // If it wasn't a definition, look in the dictionary to parse it
    Device const& device = m_Dictionary[messageID];
    if (device.GetDataSize() != byteCount)
    {
        UCSBUtility::LogError(__FUNCTION__, __FILE__, __LINE__, "%s expected %d bytes received %d\n", 
            device.DisplayName().c_str(), device.GetDataSize(), byteCount);
        return false;
    }

    //string line = device.GetDisplayData(pPayload);
    //fprintf(stdout, "%s\n", line.c_str());

    return true;
}
