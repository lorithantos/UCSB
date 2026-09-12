#pragma once

#include <windows.h>
#include "LN250.h"

#include <map>
#include <string>
#include <vector>

#define USE_TIMESTAMP 1

namespace UCSB_Datastream
{
template<typename T>
inline std::vector<BYTE> WriteBlockToFile(HANDLE hFile, T const& t, BYTE blockType)
{
    return LN250::SendLNMessage(hFile, t, blockType);
}

inline std::vector<BYTE> WriteMessageToFile(HANDLE hFile, BYTE messageNumber)
{
    return LN250::SendLNHeader(hFile, messageNumber);
}

#pragma pack (push)

// Ensure all data is tightly packed
#pragma pack (1)

template<typename DataType>
struct CacheData
{
    enum { DATA_SIZE =  sizeof(LN250::HighSpeedPortMessageHeader) + sizeof(DataType) + 1 };
    
    CacheData(DataType const& data, BYTE blockType)
        : m_header(blockType, sizeof(data))
        , m_data(data)
        , m_checksum(LN250::CreateMessageChecksum(data))
    {
    }

    LN250::HighSpeedPortMessageHeader m_header;
    DataType m_data;
    BYTE     m_checksum;
};

template<typename T>
inline std::vector<BYTE> CreateCacheData(T const& t, BYTE blockType)
{
    LN250::HighSpeedPortMessageHeader header(blockType, sizeof(t));
    
    DWORD size = sizeof(header) + sizeof(t) + 1;
    std::vector<BYTE> output;
    output.reserve(size);

    // Write block header
    UCSBUtility::AddStructure(output, header);

    // Write data
    UCSBUtility::AddStructure(output, t);

    unsigned char checksum = LN250::CreateMessageChecksum(t);

    // Write checksum
    UCSBUtility::AddStructure(output, checksum);
    
    return output;
}

inline std::vector<BYTE> CreateCacheData(std::vector<BYTE> input, BYTE blockType)
{
    if (input.size() == 0)
    {
        return std::vector<BYTE>();
    }

    if (input.size() > 255)
    {
        // Throw here?
        return std::vector<BYTE>();
    }

    LN250::HighSpeedPortMessageHeader header(blockType, static_cast<BYTE>(input.size()));
    
    DWORD size = sizeof(header) + input.size() + 1;
    std::vector<BYTE> output;
    output.reserve(size);

    // Write block header
    UCSBUtility::AddStructure(output, header);

    // Write data
    UCSBUtility::Append(output, input);

    unsigned char checksum = LN250::CreateChecksum(&input[0], input.size());

    // Write checksum
    UCSBUtility::AddStructure(output, checksum);
    
    return output;
}

// Intel byte ordering refers to "Little Endian" 0123
// Motorola byte ordering refers to "Big Endian" 3210

// Changes to this enumeration invalidate ALL data formats
// That depend on it.  - DON'T DO THAT -
// Really, DO NOT CHANGE THIS
// Please, there's a lot of data going to be generated
// Do you want it all destroyed?
enum DataType
{
    INVALID_DATA_TYPE_VALUE = -1,
    TEXT_CHAR,              // 1 byte, used as a text character
    UCHAR,                  // 1 byte unsigned [0...255]
    SCHAR,                  // 1 byte signed   [-128...127]
    USHORT_I,               // 2 byte unsigned [0...65535] Intel byte ordering
    USHORT_M,               // 2 byte unsigned [0...65535] Motorolla byte ordering
    SSHORT_I,               // 2 byte signed   [-32768...32767] Intel byte ordering
    SSHORT_M,               // 2 byte signed   [-32768...32767] Motorolla byte ordering
    ULONG_I,                // 4 byte unsigned [0...4294967295] Intel byte ordering
    ULONG_M,                // 4 byte unsigned [0...4294967295] Motorolla byte ordering
    SLONG_I,                // 4 byte signed   [-2147483648...2147483647] Intel byte ordering
    SLONG_M,                // 4 byte signed   [-2147483648...2147483647] Motorolla byte ordering
    SPFLOAT_I,              // 4 byte floating point (IEEE 754-2008) Intel byte ordering
    SPFLOAT_M,              // 4 byte floating point (IEEE 754-2008) Motorola byte ordering
    DPFLOAT_I,              // 8 byte floating point (IEEE 754-2008) Intel byte ordering
    DPFLOAT_M,              // 8 byte floating point (IEEE 754-2008) Motorola byte ordering
    UINT64_I,               // 8 byte unsigned Intel byte ordering
    DATATYPE_COUNT
};

inline DataType ConvertStringToDataType(std::string const& inputString);

enum MESSAGE_TYPE
{
    TELEMETRY_EOL = 0x0f,
    FIRST_DEVICE_TYPE = 0x10,
    LAST_DEVICE_TYPE = 0xef,

    DEVICE_DEFINITION = 0xf0,
    CHANNEL_DEFINITION,
    BLOCK_DEFINITION,
};

// There are some definitional data blocks required

struct DeviceDefinition
{
    GUID    deviceID;                       // Uniquely identify a device
    char    displayName[30];                // Human readable name, not used otherwise
    BYTE    deviceNumber;                   // Shortcut for deviceID - used in all subsequent communication
                                            // This is the biggest potential source of corruption
                                            // Since this number is meaningless without the dictionary
    
    BYTE    channelCount;                   // Number of channels of data on this device

//  BYTE    channelType[channelCount];      // The data types for each channel on this device
                                            // This is redundant once you have the channel definitions
                                            // The reason to support this is to allow the interpretation
                                            // of data that only includes the Device definitions
                                            // and not the Channel definitions
};

struct ChannelDefinition
{
    GUID    deviceGUID;                     // Channels are defined based on their parent device GUID
                                            // In all other cases the device is identified by its
                                            // deviceNumber, but the channel & device definitions are
                                            // independent of their usage.  That means any channel
                                            // definition will be the same, no matter how the devices
                                            // are ordererd or configured.

    char    displayName[30];                // Human readable name
    BYTE    channelIndex;                   // It is the sender's responsibility to avoid collisions
                                            // in the case of channelIndex colliisons, the most recent
                                            // will be considered correct

    BYTE    dataType;                       // Value based on DataTypes enumeration above
                                            // Changes to that enumeration invalidate ALL data formats
                                            // That depend on it.  DON'T DO THAT
};

inline unsigned GetDataSize(UCSB_Datastream::DataType dt);
inline std::string ConvertDataToString(DataType dt, BYTE const* pData);
inline VARIANT ConvertDataToVARIANT(DataType dt, BYTE const* pData);
inline double ConvertDataToDouble(DataType dt, BYTE const* pData);

struct BaseData
{
    UINT64  bd_timer;
    BYTE    bd_index;
};
// Make sure that we have a pragma pack(1)
template<typename T>
struct timedT : public BaseData, public T 
{
    static std::vector<UCSBUtility::StringPtrPair>  GetClassDescription()
    {
        static UCSBUtility::StringPtrPair definition[] =
        {
            "UINT64_I",	"computerClock",
            "UCHAR",	"index",
        };

        std::vector<UCSBUtility::StringPtrPair> retv = UCSBUtility::ConvertToVector(definition);

        std::vector<UCSBUtility::StringPtrPair> base = T::GetClassDescription();
        retv.insert(retv.end(), base.begin(), base.end());

        return retv;
    }

    timedT(UINT64 time, BYTE index, T value)
    {
        this->bd_timer = time;
        this->bd_index = index;
        //STATIC_ASSERT(sizeof(*this) == (sizeof(timer) + sizeof(T)));
        //memcpy(&timer + 1, &value, sizeof(value));
        T* pChild = static_cast<T*> (this);
        memcpy(pChild, &value, sizeof(value));
    }
};

struct Device : private DeviceDefinition
{
public :
    Device()
    {
        GUID nullGUID = {};
        deviceID = nullGUID;
        strcpy_s(displayName, "EMPTY");
        deviceNumber = 0;        
        channelCount = 0;
    }

    explicit Device(DeviceDefinition const& source)
        : DeviceDefinition (source)
    {
    }

    DeviceDefinition const& GetDefinition() const
    {
        return *this;
    }

    unsigned GetDataSize() const
    {
        UpdateFieldOffsets();

        return *channelOffsets.rbegin();
    }

    unsigned GetFieldOffset(DWORD field) const
    {
        if (field > dataChannels.size())
        {
            UCSBUtility::LogError(__FUNCTION__, __FILE__, __LINE__, "Searching for a non-existent field %d, there are only %d\n", 
                field, dataChannels.size());
            return 0;
        }

        UpdateFieldOffsets();

        if (field == 0)
        {
            return 0;
        }

        return channelOffsets[field-1];
    }

    std::string GetDisplayData(BYTE const* pPayload) const
    {
        std::string retv;
        retv.reserve(1024);

        std::vector<ChannelDefinition>::const_iterator cit = 
            dataChannels.begin();
        for (unsigned i=0; cit < dataChannels.end(); ++cit, ++i)
        {
            retv += ConvertDataToString(static_cast<DataType>(cit->dataType), pPayload);
            if (cit->dataType != TEXT_CHAR && i < (dataChannels.size() - 1))
            {
                retv += ", ";
            }

            pPayload += UCSB_Datastream::GetDataSize(static_cast<DataType>(cit->dataType));
        }

        return retv;
    }

    std::string GetDisplayData(BYTE const* pPayload, std::vector<DWORD> const& fields) const
    {
        std::string retv;
        retv.reserve(1024);

        std::vector<DWORD>::const_iterator cit = 
            fields.begin();
        for (unsigned i=0; cit < fields.end(); ++cit, ++i)
        {
            retv += ConvertDataToString(static_cast<DataType>(dataChannels[*cit].dataType), 
                pPayload + GetFieldOffset(*cit));
            if (dataChannels[*cit].dataType != TEXT_CHAR && i < (fields.size() - 1))
            {
                retv += ", ";
            }
        }

        return retv;
    }

    std::string GetDisplayFields(std::vector<DWORD> const& fields) const
    {
        std::string retv;
        retv.reserve(1024);

        if (fields.size() == 0)
        {
            return retv;
        }

        std::vector<DWORD>::const_iterator cit = 
            fields.begin();

        retv = GetChannelName(fields[0]);

        for (; cit < fields.end(); ++cit)
        {
            retv += ", " + GetChannelName(*cit);
        }

        return retv;
    }

    std::string GetDisplayFields() const
    {
        std::vector<std::string> names = GetChannelNames();
        if (names.size() == 1)
        {
            return names[0];
        }

        std::string retv = names[0];
        std::vector<std::string>::const_iterator cit = names.begin() + 1;
        for (;cit < names.end(); ++cit)
        {
            retv += ", " + *cit;
        }

        return retv;
    }

    std::string GetChannelName(DWORD channel) const
    {
        return UCSBUtility::ConvertBufferToString(dataChannels[channel].displayName);
    }

    VARIANT GetChannelVARIANT(DWORD channel, BYTE const* pPayload) const
    {
        return ConvertDataToVARIANT(static_cast<DataType>(dataChannels[channel].dataType), 
                pPayload + GetFieldOffset(channel));
    }

    double GetChannelDouble(DWORD channel, BYTE const* pPayload) const
    {
        return ConvertDataToDouble(static_cast<DataType>(dataChannels[channel].dataType), 
                pPayload + GetFieldOffset(channel));
    }

    std::vector<BYTE> GetChannelData(DWORD channel, BYTE const* pPayload) const
    {
        std::vector<BYTE> retv;

        BYTE const* pSource = pPayload + GetFieldOffset(channel);
        DataType dt = static_cast<DataType>(dataChannels[channel].dataType);
        size_t size = UCSB_Datastream::GetDataSize(dt);
        retv.resize(size);

        memcpy(&retv[0], pSource, size);

        return retv;
    }

    std::vector<BYTE> GetChannelDataIntel(DWORD channel, BYTE const* pPayload) const
    {
        std::vector<BYTE> retv;

        BYTE const* pSource = pPayload + GetFieldOffset(channel);
        DataType dt = static_cast<DataType>(dataChannels[channel].dataType);
        size_t size = UCSB_Datastream::GetDataSize(dt);
        retv.resize(size);

        // if we reverse, do that now
        // We are returning Intel data, so reverse any Motorola values
        switch (dt)
        {
        case USHORT_M :
        case SSHORT_M :
        case ULONG_M :
        case SLONG_M :
        case SPFLOAT_M :
        case DPFLOAT_M :
            UCSBUtility::reverse(&retv[0], pSource, size);
            break;

        default :
            memcpy(&retv[0], pSource, size);
        }

        return retv;
    }

    std::vector<BYTE> GetChannelDataMotorola(DWORD channel, BYTE const* pPayload) const
    {
        std::vector<BYTE> retv;

        BYTE const* pSource = pPayload + GetFieldOffset(channel);
        DataType dt = static_cast<DataType>(dataChannels[channel].dataType);
        size_t size = UCSB_Datastream::GetDataSize(dt);
        retv.resize(size);

        // if we reverse, do that now
        // We are returning Motorola data, so reverse any Intel values
        switch (dt)
        {
        case USHORT_I :
        case SSHORT_I :
        case ULONG_I :
        case SLONG_I :
        case SPFLOAT_I :
        case DPFLOAT_I :
        case UINT64_I :
            UCSBUtility::reverse(&retv[0], pSource, size);
            break;
        default :
            memcpy(&retv[0], pSource, size);
        }

        return retv;
    }

    void AddChannel(ChannelDefinition const& cd, bool useIndex = true)
    {
        ChannelDefinition localCD (cd);

        if (!useIndex)
        {
            localCD.channelIndex = static_cast<BYTE>(dataChannels.size());
        }
        
        if (localCD.channelIndex >= dataChannels.size())
        {
            dataChannels.resize(localCD.channelIndex + 1);
        }
        dataChannels[localCD.channelIndex] = localCD;
        channelOffsets.clear();
    }

    std::vector<std::string> GetChannelNames() const
    {
        std::vector<std::string> sourceNames;        
        for(std::vector<ChannelDefinition>::const_iterator cit = 
            dataChannels.begin(); cit < dataChannels.end(); ++cit)
        {
            sourceNames.push_back(cit->displayName);
        }

        return sourceNames;
    }

    DWORD GetChannelCount() const
    {
        return dataChannels.size();
    }

    ChannelDefinition GetChannel(DWORD index) const
    {
        if(index > GetChannelCount())
        {
            ChannelDefinition cd = {};
            return cd;
        }

        return dataChannels[index];
    }

    GUID DeviceID() const
    {
        return deviceID;
    }

    std::string DisplayName() const
    {
        //strcpy_s(
        return UCSBUtility::ConvertBufferToString(displayName);
    }

    DWORD DeviceNumber() const
    {
        return deviceNumber;
    }

private :
    void UpdateFieldOffsets() const
    {
        if (channelOffsets.size() == dataChannels.size())
        {
            return;
        }
     
        channelOffsets.clear();
        channelOffsets.reserve(dataChannels.size());
        unsigned offset = 0;

        std::vector<ChannelDefinition>::const_iterator cit = 
            dataChannels.begin();

        for (; cit < dataChannels.end(); ++cit)
        {
            offset += UCSB_Datastream::GetDataSize(static_cast<DataType>(cit->dataType));
            channelOffsets.push_back(offset);
        }
    }

private :

    friend class DeviceHolder;

    std::vector<ChannelDefinition>  dataChannels;
    mutable std::vector<DWORD>      channelOffsets;
};

class DeviceHolder
{
public :
    typedef std::map<void const*, std::vector<BYTE> > CacheDataType;

public :
    static const BYTE   g_header[8];// = { 'S', 'P', 'A', 'C', 'B', 'A', 'L', 'L' };
    static const DWORD   INVALID_DEVICE_NUMBER = static_cast<DWORD>(-1);

public :
    DeviceHolder() : m_NextDeviceID(FIRST_DEVICE_TYPE), m_writeDictionary(false), m_pCache(&m_fileData[0])
    {
        ClearDictionary();
        m_fileData[0].reserve(10 * 1024 * 1024);
        m_fileData[1].reserve(10 * 1024 * 1024);
    }

    void ClearDictionary()
    {
        std::vector<Device> empty(256);
        m_Dictionary = empty;
    }

    unsigned BuildDictionary(std::vector<std::string> const& fields);
    
    void BuildDictionary(std::vector<Device> const& devices);

    bool CreateFileHeader(HANDLE hFile, DWORD* pWritten = NULL) const;

    struct DictionaryBuilder
    {
        DictionaryBuilder(DeviceHolder& holder) : m_holder(holder) {}

        unsigned operator() (std::vector<std::string> const& fields)
        {
            return m_holder.BuildDictionary(fields);
        }

        DictionaryBuilder(DictionaryBuilder const& rhs) : m_holder(rhs.m_holder){}
    private :
        DictionaryBuilder& operator = (DictionaryBuilder const&);

        DeviceHolder& m_holder;
    };

    template<typename Processor>
    struct FileParser
    {
        FileParser(DeviceHolder& holder, Processor& handler) 
            : m_holder(holder), 
            m_handler(handler) {}
        FileParser(FileParser const& rhs) 
            : m_holder(rhs.m_holder)
            , m_handler(rhs.m_handler) {}

        bool operator() (BYTE messageID, BYTE byteCount, BYTE const* pPayload)
        {
            return m_holder.FileParserImpl(messageID, byteCount, pPayload, m_handler);
        }

    private :
        DeviceHolder& m_holder;
        Processor&    m_handler;
        FileParser& operator = (FileParser const&);
    };

public :
    template<typename Type>
    void RegisterWriter(Type const& source, GUID const& deviceID)
    {
        DWORD deviceNumber = FindDevice(deviceID);

        // Since we require a DataSource in the write data
        // Make sure you can't register without already being one
        static_cast<nsDataSource::DataSource const*>(&source);

        if (deviceNumber == INVALID_DEVICE_NUMBER)
        {
            throw __FUNCTION__ " Cannot register\n No matching GUID found";
        }

        m_writeMap[&source] = static_cast<BYTE>(deviceNumber);

        // Try to minimize memory allocations when processing
        m_cache[&source].reserve(m_Dictionary[deviceNumber].GetDataSize() + 
            sizeof(LN250::HighSpeedPortMessageHeader) + 1);
    }

    template<typename SrcType, typename DataType>
    void WriteDataWithCache(UINT64 /*timer*/, SrcType const& source, DataType const& data) const
    {
        std::map<void const*, BYTE>::const_iterator cit = m_writeMap.find(&source);

        // Not found
        if (cit == m_writeMap.end())
        {
            UCSBUtility::LogError(__FUNCTION__, __FILE__, __LINE__, "WriteData recieved a bad pointer %p\n", &source);
            return;
        }

#if USE_TIMESTAMP
        static_cast<nsDataSource::DataSource const*>(&source);
        timedT<DataType> writeData(UCSBUtility::ReadTime(), source.GetIndex(), data);
#else
        timer;
        DataType const& writeData = data;
#endif

        // Wrong size
        if (m_Dictionary[cit->second].GetDataSize() != sizeof(writeData))
        {
            UCSBUtility::LogError(__FUNCTION__, __FILE__, __LINE__, "%s called with an unexpected data size "
                "(%d) bytes expected (%d) bytes passed\n", __FUNCSIG__, 
                m_Dictionary[cit->second].GetDataSize(), sizeof(writeData));

            throw "Fatal programming error";
        }

        // Ensure the cache is protected by syncronization
        // Note, minimize the code after the start of the scope or
        // enclose it in brackets
        if (m_writeDictionary)
        {
             throw "Bad programming in " __FUNCSIG__;
        }

        // Write the data to a file, get back the data written
#if 0
        std::vector<BYTE> cachedData = CreateCacheData(writeData, cit->second);
        UCSBUtility::CMutexHolder::ScopedMutex scope(m_cacheMutex.GetMutex(L"CacheMutex"));
        m_cache[&source] = cachedData;
        m_pCache->insert(m_pCache->end(), cachedData.begin(), cachedData.end());
#else
        CacheData<timedT<DataType> > cachedVersion(writeData, cit->second);
        UCSBUtility::CMutexHolder::ScopedMutex scope(m_cacheMutex.GetMutex(L"CacheMutex"));
        m_cache[&source].clear();
        UCSBUtility::AddStructure(m_cache[&source], cachedVersion);
        m_pCache->insert(m_pCache->end(), m_cache[&source].begin(), m_cache[&source].end());
#endif
    }

    CacheDataType GetCache() const
    {
        UCSBUtility::CMutexHolder::ScopedMutex scope(m_cacheMutex.GetMutex(L"CacheMutex"));
        return m_cache;
    }

    void WriteCurrentData(HANDLE hFile)
    {
        std::vector<BYTE>* pWriteData = GetLastCache();

        if (pWriteData->size() == 0)
        {
            return;
        }

        DWORD written = 0;
        WriteFile(hFile, &(*pWriteData)[0], pWriteData->size(), &written, NULL);
        pWriteData->clear();
    }
    
    bool GetDevice(CacheDataType::key_type const& key, Device& output) const
    {
        std::map<void const*, BYTE>::const_iterator cit = m_writeMap.find(key);

        // Not found
        if (cit == m_writeMap.end())
        {
            return false;
        }

        output = m_Dictionary[cit->second];

        return true;
    }

    bool GetDevice(GUID const& deviceID, Device& output) const
    {
        DWORD deviceNumber = FindDevice(deviceID);
        if (deviceNumber == INVALID_DEVICE_NUMBER)
        {
            return false;
        }

        output = m_Dictionary[deviceNumber];
        return true;
    }

    Device const& GetDevice(BYTE deviceNumber) const
    {
        return m_Dictionary[deviceNumber];
    }

private :
    bool GetDevice(BYTE const* pMessageContent, size_t messageLength, Device& output) const
    {
        if (messageLength == 0)
        {
            return false;
        }

        // Lookup device based on first byte
        // make sure the 

        BYTE deviceIndex = *pMessageContent++;
        if (m_Dictionary[deviceIndex].GetDataSize() == 
            (messageLength - 1))
        {
            return false;
        }

        output = m_Dictionary[deviceIndex];

        return true;
    }

public :

    // BUGBUG needs to be reworked so it doesn't "know" about internals it should know nothing about
    bool GetDeviceFromMessage(std::vector<BYTE> const& message, Device& output) const
    {
        //LN250::HighSpeedPortMessageHeader msgHeader;
        if (message.size() < sizeof(LN250::HighSpeedPortMessageHeader))
        {
            return false;
        }

        return GetDevice(&message[0] + sizeof(LN250::HighSpeedPortMessageHeader), 
            message.size() - sizeof(LN250::HighSpeedPortMessageHeader), output);
    }

    template<typename SourceType>
    bool AddDeviceType(SourceType const&)
    {
#if  USE_TIMESTAMP
        return AddDeviceTypeImpl<timedT<SourceType> >();
#else
        return AddDeviceTypeImpl<SourceType>();
#endif
    }

private :
    template<typename SourceType>
    bool AddDeviceTypeImpl()
    {
        UCSBUtility::CMutexHolder::ScopedMutex mutex(m_cacheMutex.GetMutex());
        GUID guid = SourceType::GetClassGUID();

        Device oldDevice;
        if (GetDevice(guid, oldDevice))
        {
            return true;
        }

        // 
        if (m_NextDeviceID >= LAST_DEVICE_TYPE)
        {
            return false;
        }

        std::vector<nsDataSource::ChannelDefinition> definition = 
            SourceType::GetClassDescription();

        DeviceDefinition newDeviceDefinition;
        newDeviceDefinition.deviceNumber = static_cast<BYTE>(++m_NextDeviceID);
        newDeviceDefinition.deviceID = guid;
        UCSBUtility::CopyStringToBuffer(newDeviceDefinition.displayName, SourceType::GetName());
        newDeviceDefinition.channelCount = static_cast<BYTE>(definition.size());

        Device newDevice (newDeviceDefinition);

        for (size_t i = 0; i < definition.size(); ++i)
        {
            ChannelDefinition cd;
            cd.deviceGUID = guid;
            cd.channelIndex = static_cast<BYTE>(i);
            cd.dataType = static_cast<BYTE>(ConvertStringToDataType(definition[i].type));
            UCSBUtility::CopyStringToBuffer(cd.displayName, definition[i].name);
            newDevice.AddChannel(cd);
        }

        m_Dictionary[newDevice.DeviceNumber()] = newDevice;
        m_writeDictionary = true;

        return true;
    }

    std::vector<BYTE>* GetLastCache()
    {
        UCSBUtility::CMutexHolder::ScopedMutex scope(m_cacheMutex.GetMutex(L"CacheMutex"));
        std::vector<BYTE>* pRetv = m_pCache;
        if (m_pCache == &m_fileData[0])
        {
            m_pCache = &m_fileData[1];
        }
        else
        {
            m_pCache = &m_fileData[0];
        }

        return pRetv;
    }

    //template <bool test>
    //inline void STATIC_ASSERT_IMPL()
    //{
    //    // test will be true or false, which will implictly convert to 1 or 0
    //    char STATIC_ASSERT_FAILURE[test] = {0};
    //}

    //template <typename BaseType>
    //struct nonDeviceMessage : BaseType
    //{

    //};

    template<typename Processor>
    bool FileParserImpl(BYTE messageID, BYTE byteCount, BYTE const* pPayload, Processor& handler)
    {
        if (DictionaryBuilderImpl(messageID, byteCount, pPayload))
        {
            handler.DictionaryUpdated();
            return true;
        }

        if (messageID < FIRST_DEVICE_TYPE)
        {
            handler.ControlData(messageID);
            return true;
        }

        // If it wasn't a definition, look in the dictionary to parse it
        Device const& device = m_Dictionary[messageID];
        if (device.DeviceNumber() == 0)
        {
            handler.MissingDictionaryEntry(messageID);
            return false;
        }
        
        if (device.GetDataSize() != byteCount)
        {
            handler.InvalidData(messageID, device, 
                byteCount, pPayload);

            return false;
        }

        handler.ProcessData(device, pPayload);

        return true;
    }


    DWORD FindDevice(GUID const& deviceID) const;
    
    std::vector<std::string>::const_iterator
        InterpretDictionaryConfigBlock(
            std::vector<std::string>::const_iterator begin, 
            std::vector<std::string>::const_iterator end);

    void AddDevice(BYTE const* pPayload, BYTE byteCount);
    void AddChannel(BYTE const* pPayload, BYTE byteCount);
    bool DictionaryBuilderImpl(BYTE messageID, BYTE byteCount, BYTE const* pPayload);
    bool FileParserImpl(BYTE messageID, BYTE byteCount, BYTE const* pPayload);

private :
    std::vector<Device>         m_Dictionary;
    DWORD                       m_NextDeviceID; // First 15 block IDs are reserved
    std::map<void const*, BYTE> m_writeMap;
    mutable CacheDataType       m_cache;
    UCSBUtility::CMutexHolder   m_cacheMutex;
    mutable bool                m_writeDictionary;
    std::vector<BYTE>           m_fileData[2];
    mutable std::vector<BYTE>*  m_pCache;
    mutable std::vector<BYTE>   m_headerData;
};

struct DefaultThrowingParserHandler
{
    void ControlData(BYTE) const {};
    
    void DictionaryUpdated() const
    {
        // Do nothing
    }
    
    void MissingDictionaryEntry(BYTE messageID) const
    {
        UCSBUtility::LogError(__FUNCTION__, __FILE__, __LINE__, "Unable to process file with missing Dictionary Entry %d\n", messageID);

        throw "Unable to process file with missing Dictionary Entry";
    }

    void InvalidData(BYTE messageID, Device const& device, BYTE byteCount, BYTE const* pPayload) const
    {
        device;

        UCSBUtility::LogError(__FUNCTION__, __FILE__, __LINE__, "Unable to process file with Invalid Data Entry\nMessageID: "
            "%d\nByte Count: %d\nPayload Pointer: %p\n", messageID, 
            byteCount, pPayload);

        throw "Unable to process file with Invalid Data Entry";
    }
};

// This structure is designed to hold all of the data
// It expects that the dictionary occurs only once
// That means that it marks the end of the dictionary
// when it sees the first non-dictionary element.

// Because we know that the same dictionary is to be used
// for the entire data set, we can cache the device index
// for each block without fear that the dictionary will change
//
// We are also writing this structure for use ONLY with a
// data set that is valid for all calls to the data structure
// that is, processing WHOLE FILES.  Do not use this with buffered
// file reads or for streamed data.  It is likely to break.

// 
// As each element comes in it is sorted by
// 1) time code
// 2) Device Index

class DataAggregationTool : public DefaultThrowingParserHandler
{
    typedef int (*UpdateStatusFn)(void* pHandle, char const* format, ...);

    // Sort by time and then by index
    struct sorter
    {
        UINT64  time;
        DWORD   index;

        bool operator < (sorter const& rhs) const
        {
            if (time < rhs.time)
            {
                return true;
            }

            if (time > rhs.time)
            {
                return false;
            }

            return index < rhs.index;
        }
    };

    struct dataElement
    {
        DWORD       deviceNumber;
        BYTE const* pData;
    };

    typedef UCSB_Datastream::Device Device;
    typedef std::map<sorter, dataElement> DataTree;


public :
    DataAggregationTool() 
        : m_dictionaryComplete(false)
        , m_count(0)
        , m_surveyOnly(false)
        , m_updateStatus(NULL)
        , m_updatePtr(NULL)
    {
        memset(m_deviceNumbers, 0, sizeof(m_deviceNumbers));
        memset(m_validDevices, 0, sizeof(m_validDevices));
        SetUpdateStatusFn(fprintf, stdout);
    }
    
    ~DataAggregationTool()
    {
    }

    template<typename HandleType>
    void SetUpdateStatusFn(int (*fn)(HandleType, char const* , ...), HandleType ptr)
    {
        m_updateStatus = reinterpret_cast<UpdateStatusFn>(fn);
        m_updatePtr = ptr;
    }


    bool isSurvey() const
    {
        return m_surveyOnly;
    }

    void SetSurveyMode(bool survey)
    {
        m_surveyOnly = survey;
    }

    void DictionaryUpdated() const
    {
        if (m_dictionaryComplete)
        {
            UCSBUtility::LogError(__FUNCTION__, __FILE__, __LINE__, "Unexpected Dictionary Entry (dictionary should not"
                " update mid data stream)\n");
            
            throw "Unexpected Dictionary Entry (dictionary should not"
                " update mid data stream)\n";
        }
    }

    void ProcessData(Device const& device, BYTE const* pPayload)
    {
        m_dictionaryComplete = true;

        sorter key;
        if (!m_validDevices[device.DeviceNumber()])
        {
            if (strcmp(device.GetChannelName(0).c_str(), "implicitTimer") && 
                strcmp(device.GetChannelName(0).c_str(), "computerClock"))
            {
                throw "Requires new format with implicitTimer";
            }

            if (strcmp(device.GetChannelName(1).c_str(), "index"))
            {
                throw "Requires new format with index";
            }

            m_validDevices[device.DeviceNumber()] = true;
        }

        key.time = device.GetChannelVARIANT(0, pPayload).llVal;
        key.index = device.GetChannelVARIANT(1, pPayload).bVal;
        
        dataElement data;
        data.deviceNumber = device.DeviceNumber();
        data.pData = pPayload;
        if (!m_surveyOnly)
        {
            m_data[key] = data;
        }

        m_deviceNumbers[key.index] = data.deviceNumber;

        if (m_data.size() > 1000)
        {
            m_array.push_back(m_data.begin()->second);
            m_data.erase(m_data.begin());
        }

        if (++m_count % 1000 == 0)
        {
            m_updateStatus(m_updatePtr, "1) Processed %d\r", m_count);
        }
    }

    template<typename ProcessFn>
    void WalkData(ProcessFn const& process)
    {
        m_count = 0;

        // First copy the remaining data to the array
        DataTree::const_iterator cit = m_data.begin();
        DataTree::const_iterator end = m_data.end();
        while(cit != end)
        {
            m_array.push_back(m_data.begin()->second);
            m_data.erase(cit++);
        }

        // Now process just the array
        // this also means we have the data here for multiple processing if needed
        for (std::vector<dataElement>::const_iterator vcit = m_array.begin();
            vcit != m_array.end(); ++vcit)
        {
            process(vcit->deviceNumber, vcit->pData);

            if (++m_count % 1000 == 0)
            {
                m_updateStatus(m_updatePtr, "2) Processed %d\r", m_count);
            }
        }
    }

    std::vector<std::pair<BYTE, BYTE> > GetDeviceNumbers() const
    {
        std::vector<std::pair<BYTE, BYTE> > retv;

        for (int i=0; i < _countof(m_deviceNumbers); ++i)
        {
            if (m_deviceNumbers[i] > 0)
            {
                std::pair<BYTE, BYTE> value(static_cast<BYTE>(i), 
                    static_cast<BYTE>(m_deviceNumbers[i]));
                retv.push_back(value);
            }
        }

        return retv;
    }

private :
    std::vector<dataElement> m_array;
    DataTree        m_data;
    bool            m_dictionaryComplete;
    int             m_deviceNumbers[256];
    bool            m_validDevices[256];
    int             m_count;
    bool            m_surveyOnly;
    UpdateStatusFn  m_updateStatus;
    void*           m_updatePtr;
};

extern struct DataTypeMapping
{
    UCSB_Datastream::DataType   dt;
    char const*                 name;
    unsigned                    size;
} mapping[DATATYPE_COUNT]; 

inline unsigned GetDataSize(UCSB_Datastream::DataType dt)
{
    STATIC_ASSERT(TEXT_CHAR == 0);
    STATIC_ASSERT((DATATYPE_COUNT) == _countof(mapping));
    if (dt >= TEXT_CHAR && dt < DATATYPE_COUNT)
    {
        return mapping[dt].size;
    }

    return 0;
}

inline std::string ConvertINVALID_DATA_TYPE_VALUE_ToString(BYTE const*)
{
    return "Invalid Data Type";
}

inline std::string ConvertTEXT_CHAR_ToString(BYTE const* pData)
{
    char buffer[160] = {};
    sprintf_s(buffer, "%c", *pData);

    return buffer;
}

inline std::string ConvertUCHAR_ToString(BYTE const* pData)
{
    char buffer[160] = {};
    unsigned char const* p = static_cast<unsigned char const*>(pData);
    sprintf_s(buffer, "%u", *p);

    return buffer;
}

inline std::string ConvertSCHAR_ToString(BYTE const* pData)
{
    char buffer[160] = {};
    signed char const* p = reinterpret_cast<signed char const*>(pData);
    sprintf_s(buffer, "%d", *p);

    return buffer;
}

inline std::string ConvertUSHORT_I_ToString(BYTE const* pData)
{
    char buffer[160] = {};
    unsigned short const* p = reinterpret_cast<unsigned short const*>(pData);
    sprintf_s(buffer, "%u", *p);

    return buffer;
}

inline std::string ConvertUSHORT_M_ToString(BYTE const* pData)
{
    char buffer[160] = {};
    unsigned short const* pSource = reinterpret_cast<unsigned short const*>(pData);
    unsigned short d;
    UCSBUtility::s_reverse(d, *pSource);
    sprintf_s(buffer, "%u", d);

    return buffer;
}

inline std::string ConvertSSHORT_I_ToString(BYTE const* pData)
{
    char buffer[160] = {};
    short const* p = reinterpret_cast<short const*>(pData);
    sprintf_s(buffer, "%u", *p);

    return buffer;
}

inline std::string ConvertSSHORT_M_ToString(BYTE const* pData)
{
    char buffer[160] = {};
    short const* pSource = reinterpret_cast<short const*>(pData);
    short d;
    UCSBUtility::s_reverse(d, *pSource);
    sprintf_s(buffer, "%u", d);

    return buffer;
}

inline std::string ConvertULONG_I_ToString(BYTE const* pData)
{
    char buffer[160] = {};
    unsigned long const* p = reinterpret_cast<unsigned long const*>(pData);
    sprintf_s(buffer, "%u", *p);

    return buffer;
}

inline std::string ConvertULONG_M_ToString(BYTE const* pData)
{
    char buffer[160] = {};
    unsigned long const* pSource = reinterpret_cast<unsigned long const*>(pData);
    unsigned long d;
    UCSBUtility::s_reverse(d, *pSource);
    sprintf_s(buffer, "%u", d);

    return buffer;
}

inline std::string ConvertSLONG_I_ToString(BYTE const* pData)
{
    char buffer[160] = {};
    long const* p = reinterpret_cast<long const*>(pData);
    sprintf_s(buffer, "%u", *p);

    return buffer;
}

inline std::string ConvertSLONG_M_ToString(BYTE const* pData)
{
    char buffer[160] = {};
    long const* pSource = reinterpret_cast<long const*>(pData);
    long d;
    UCSBUtility::s_reverse(d, *pSource);
    sprintf_s(buffer, "%u", d);

    return buffer;
}

inline std::string ConvertSPFLOAT_I_ToString(BYTE const* pData)
{
    char buffer[160] = {};
    float const* p = reinterpret_cast<float const*>(pData);
    sprintf_s(buffer, "%f", *p);

    return buffer;
}

inline std::string ConvertSPFLOAT_M_ToString(BYTE const* pData)
{
    char buffer[160] = {};
    float const* pSource = reinterpret_cast<float const*>(pData);
    float d;
    UCSBUtility::s_reverse(d, *pSource);
    sprintf_s(buffer, "%f", d);

    return buffer;
}

inline std::string ConvertDPFLOAT_I_ToString(BYTE const* pData)
{
    char buffer[160] = {};
    double const* p = reinterpret_cast<double const*>(pData);
    sprintf_s(buffer, "%g", *p);

    return buffer;
}

inline std::string ConvertDPFLOAT_M_ToString(BYTE const* pData)
{
    char buffer[160] = {};
    double const *pSource = reinterpret_cast<double const*>(pData);
    double d;
    UCSBUtility::s_reverse(d, *pSource);
    sprintf_s(buffer, "%g", d);

    return buffer;
}

inline std::string ConvertUINT64_I_ToString(BYTE const* pData)
{
    char buffer[160] = {};
    UINT64 const* p = reinterpret_cast<UINT64 const*>(pData);
    sprintf_s(buffer, "%I64u", *p);

    return buffer;
}


inline std::string ConvertDataToString(DataType dt, BYTE const* pData)
{
    switch (dt)
    {
    case TEXT_CHAR :
        return ConvertTEXT_CHAR_ToString(pData);

    case UCSB_Datastream::UCHAR :
        return ConvertUCHAR_ToString(pData);

    case SCHAR :
        return ConvertSCHAR_ToString(pData);

    case USHORT_I :
        return ConvertUSHORT_I_ToString(pData);

    case USHORT_M :
        return ConvertUSHORT_M_ToString(pData);

    case SSHORT_I :
        return ConvertSSHORT_I_ToString(pData);

    case SSHORT_M :
        return ConvertSSHORT_M_ToString(pData);

    case ULONG_I :
        return ConvertULONG_I_ToString(pData);

    case ULONG_M :
        return ConvertULONG_M_ToString(pData);

    case SLONG_I :
        return ConvertSLONG_I_ToString(pData);

    case SLONG_M :
        return ConvertSLONG_M_ToString(pData);

    case SPFLOAT_I :
        return ConvertSPFLOAT_I_ToString(pData);

    case SPFLOAT_M :
        return ConvertSPFLOAT_M_ToString(pData);

    case DPFLOAT_I :
        return ConvertDPFLOAT_I_ToString(pData);

    case DPFLOAT_M :
        return ConvertDPFLOAT_M_ToString(pData);

    case UINT64_I :
        return ConvertUINT64_I_ToString(pData);

    case INVALID_DATA_TYPE_VALUE :
    default :
        return ConvertINVALID_DATA_TYPE_VALUE_ToString(pData);
    }
}

template<size_t size>
inline VARIANT CreateVariant(VARTYPE type, BYTE const* pData)
{
    VARIANT retv = { type };

    STATIC_ASSERT(size <= sizeof(retv.ullVal));

    memcpy(&retv.ullVal, pData, size);

    return retv;
}

template<size_t size>
inline VARIANT CreateVariantReverse(VARTYPE type, BYTE const* pData)
{
    VARIANT retv = { type };

    STATIC_ASSERT(size <= sizeof(retv.ullVal));

    UCSBUtility::reverse(&retv.ullVal, pData, size);

    return retv;
}

inline VARIANT ConvertDataToVARIANT(DataType dt, BYTE const* pData)
{
    switch (dt)
    {
    case TEXT_CHAR :
        return CreateVariant<1>(VT_UI1, pData);

    case UCSB_Datastream::UCHAR :
        return CreateVariant<1>(VT_UI1, pData);

    case SCHAR :
        return CreateVariant<1>(VT_I1, pData);

    case USHORT_I :
        return CreateVariant<2>(VT_UI2, pData);

    case USHORT_M :
        return CreateVariantReverse<2>(VT_UI2, pData);

    case SSHORT_I :
        return CreateVariant<2>(VT_I2, pData);

    case SSHORT_M :
        return CreateVariantReverse<2>(VT_I2, pData);

    case ULONG_I :
        return CreateVariant<4>(VT_UI4, pData);

    case ULONG_M :
        return CreateVariantReverse<4>(VT_UI4, pData);

    case SLONG_I :
        return CreateVariant<4>(VT_I4, pData);

    case SLONG_M :
        return CreateVariantReverse<4>(VT_I4, pData);

    case SPFLOAT_I :
        return CreateVariant<4>(VT_R4, pData);

    case SPFLOAT_M :
        return CreateVariantReverse<4>(VT_R4, pData);

    case DPFLOAT_I :
        return CreateVariant<8>(VT_R8, pData);

    case DPFLOAT_M :
        return CreateVariantReverse<8>(VT_R8, pData);

    case UINT64_I :
        return CreateVariant<8>(VT_UI8, pData);

    //case INVALID_DATA_TYPE_VALUE :
    //default :
    //    return ConvertINVALID_DATA_TYPE_VALUE_ToString(pData);
    }

    VARIANT retv = { VT_EMPTY };
    return retv;
}

template<typename InputType>
double CreateDouble(BYTE const* pData)
{
    InputType const& retv = *reinterpret_cast<InputType const*>(pData);
    return static_cast<double>(retv);
}

template<typename InputType>
double CreateDoubleReverse(BYTE const* pData)
{
    InputType reversed;
    UCSBUtility::reverse(&reversed, pData, sizeof(reversed));
    return static_cast<double>(reversed);
}

inline double ConvertDataToDouble(DataType dt, BYTE const* pData)
{
    switch (dt)
    {
    case TEXT_CHAR :
        return CreateDouble<BYTE>(pData);

    case UCSB_Datastream::UCHAR :
        return CreateDouble<BYTE>(pData);

    case SCHAR :
        return CreateDouble<BYTE>(pData);

    case USHORT_I :
        return CreateDouble<unsigned short>(pData);

    case USHORT_M :
        return CreateDoubleReverse<unsigned short>(pData);

    case SSHORT_I :
        return CreateDouble<signed short>(pData);

    case SSHORT_M :
        return CreateDoubleReverse<signed short>(pData);

    case ULONG_I :
        return CreateDouble<unsigned long>(pData);

    case ULONG_M :
        return CreateDoubleReverse<unsigned long>(pData);

    case SLONG_I :
        return CreateDouble<signed long>(pData);

    case SLONG_M :
        return CreateDoubleReverse<signed long>(pData);

    case SPFLOAT_I :
        return CreateDouble<float>(pData);

    case SPFLOAT_M :
        return CreateDoubleReverse<float>(pData);

    case DPFLOAT_I :
        return CreateDouble<double>(pData);

    case DPFLOAT_M :
        return CreateDoubleReverse<double>(pData);

    case UINT64_I :
        return CreateDouble<__int64>(pData);
    }

    return UCSBUtility::GetNan();
}

inline DataType ConvertStringToDataType(std::string const& inputString)
{
    for (unsigned i=0; i < _countof(mapping); ++i)
    {
        if (_stricmp(mapping[i].name, inputString.c_str()) == 0)
        {
            return mapping[i].dt;
        }
    }

    return INVALID_DATA_TYPE_VALUE;
}

// RE-Use of the LN250 High Speed Port data transmision protocol
typedef LN250::HighSpeedPortMessageHeader MessageHeader;

#pragma pack (pop)
};
