// This is the main DLL file.

#include "stdafx.h"

#include "ConversionTool.h"

#include "ConfigFileReader.h"
#include "Ln250.h"
#include "UCSB-Datastream.h"
#include "Utility.h"

#include <algorithm>
#include <map>
#include <string>
#include <vector>

#include <conio.h>
#include <comutil.h>
#include <process.h>
 #pragma comment (lib, "comsupp.lib")

// Declare what pieces we are using
using std::find;
using std::map;
using std::string;
using std::vector;

// We need this function
using LN250::ProcessFn;
using LN250::InputBuffer;

using namespace UCSB_Datastream;
using namespace UCSBUtility;

struct FITSTableID
{
    typedef UCSB_Datastream::DataType DataType;

    //DataType    key;
    char        FITSKey;
    UINT64      zero;
    size_t      bytes;

    FITSTableID::FITSTableID(char key = 0, UINT64 z = 0, size_t b = 0)
        : FITSKey(key)
        , zero(z)
        , bytes(b)
    {
        Init();
    }

    static char GetKey(DataType t)
    {
        return table[t].FITSKey;
    }

    static string GetZero(DataType t)
    {
        if (table[t].zero == 0)
        {
            return "";
        }

        // Special case
        if (t == SCHAR)
        {
            return "-128";
        }

        char buffer[30];
        sprintf_s(buffer, "%I64u", table[t].zero);
        return buffer;
    }

    static size_t GetBytes(DataType t)
    {
        return table[t].bytes;
    }

private :
    inline void Init()
    {
        static bool isInit = false;
        if (isInit)
        {
            return;
        }

        isInit = true;
            
        table[TEXT_CHAR] = FITSTableID('A', 0, 1);                     // 1 byte, used as a text character
        table[UCSB_Datastream::UCHAR] = FITSTableID('B', 0, 1);        // 1 byte unsigned [0...255]
        table[SCHAR] = FITSTableID('B', -128, 1);                      // 1 byte signed   [-128...127]
        table[USHORT_I] = FITSTableID('I', 32768, 2);                  // 2 byte unsigned [0...65535] Intel byte ordering
        table[USHORT_M] = FITSTableID('I', 32768, 2);                  // 2 byte unsigned [0...65535] Motorolla byte ordering
        table[SSHORT_I] = FITSTableID('I', 0, 2);                      // 2 byte signed   [-32768...32767] Intel byte ordering
        table[SSHORT_M] = FITSTableID('I', 0, 2);                      // 2 byte signed   [-32768...32767] Motorolla byte ordering
        table[ULONG_I] = FITSTableID('J', 2147483648, 4);              // 4 byte unsigned [0...4294967295] Intel byte ordering
        table[ULONG_M] = FITSTableID('J', 2147483648, 4);              // 4 byte unsigned [0...4294967295] Motorolla byte ordering
        table[SLONG_I] = FITSTableID('J', 0, 4);                       // 4 byte signed   [-2147483648...2147483647] Intel byte ordering
        table[SLONG_M] = FITSTableID('J', 0, 4);                       // 4 byte signed   [-2147483648...2147483647] Motorolla byte ordering
        table[SPFLOAT_I] = FITSTableID('E', 0, 4);                     // 4 byte floating point (IEEE 754-2008) Intel byte ordering
        table[SPFLOAT_M] = FITSTableID('E', 0, 4);                     // 4 byte floating point (IEEE 754-2008) Motorola byte ordering
        table[DPFLOAT_I] = FITSTableID('D', 0, 8);                     // 8 byte floating point (IEEE 754-2008) Intel byte ordering
        table[DPFLOAT_M] = FITSTableID('D', 0, 8);                     // 8 byte floating point (IEEE 754-2008) Motorola byte ordering
        table[UINT64_I] = FITSTableID('K', 9223372036854775808), 8;    // 8 byte unsigned Intel byte ordering
    }

    static FITSTableID table[DATATYPE_COUNT];
};

FITSTableID FITSTableID::table[DATATYPE_COUNT];


typedef struct DeviceHolder::DictionaryBuilder  DictionaryBuilder;

struct DataAggregator
{
    typedef std::vector<string> strings;
    typedef vector<string>::const_iterator fieldIter;
    typedef vector<string>::const_iterator vectorStringIter;
    struct channelData
    {
        GUID    deviceType;
        DWORD   deviceIndex;
        strings channels;
    };
    struct channelIndexData
    {
        GUID            deviceType;
        DWORD           deviceIndex;
        vector<DWORD>   channels;
        vector<char>    FITSKeys;
        vector<string>  zeros;
        vector<size_t>  sizes;
    };

    typedef vector<channelData> channelType;
    typedef std::vector<channelIndexData> channelIndexedType;


    struct outputData
    {
        string      m_name;
        channelType m_channels;

        channelIndexedType GetIndexedChannels(DeviceHolder const& dictionary) const
        {
            channelIndexedType retv;
            for (channelType::const_iterator cit=m_channels.begin();
                cit != m_channels.end(); ++cit)
            {
                retv.push_back(GetIndexedChannel(dictionary, *cit));
            }

            return retv;
        }

    private :
        channelIndexData GetIndexedChannel(DeviceHolder const& dictionary, channelData const& cd) const
        {
            channelIndexData cid;
            cid.deviceIndex = cd.deviceIndex;
            cid.deviceType = cd.deviceType;

            // Get the list of channels for this device 
            UCSB_Datastream::Device device;
            dictionary.GetDevice(cd.deviceType, device);
            strings channelNames;
            
            for (DWORD i=0; i < device.GetChannelCount(); ++i)
            {
                channelNames.push_back(device.GetChannelName(i));
            }
            
            strings::const_iterator namesEnd = channelNames.end();
            strings::const_iterator namesBegin = channelNames.begin();

            for(strings::const_iterator cit=cd.channels.begin(); 
                cit != cd.channels.end(); ++cit)
            {
                // Now, we have a string, search the dicitonary entry for it
                strings::const_iterator entry = std::find(namesBegin, 
                    namesEnd, *cit);

                if (entry == namesEnd)
                {
                    cid.channels.push_back(static_cast<DWORD>(-1));
                    cid.FITSKeys.push_back(' ');
                    cid.sizes.push_back(0);
                    cid.zeros.push_back("");
                }
                else
                {
                    size_t channel = entry - namesBegin;
                    cid.channels.push_back(channel);
                    
                    UCSB_Datastream::DataType dt = static_cast
                        <UCSB_Datastream::DataType> (device.GetChannel(
                        channel).dataType);

                    cid.FITSKeys.push_back(FITSTableID::GetKey(dt));
                    cid.zeros.push_back(FITSTableID::GetZero(dt));
                    cid.sizes.push_back(FITSTableID::GetBytes(dt));
                }
            }

            return cid;
        }
    };

    DataAggregator(outputData& destination)
        : m_destination(destination)
        , m_valid(true)
    {}

    DataAggregator(DataAggregator const& rhs)
        : m_destination(rhs.m_destination)
        , m_valid(rhs.m_valid)
    {
    }

    bool isValid() const
    {
        return m_valid;
    }

    bool operator ()(vector<string> const& fields)
    {
        if (fields.size() < 5)
        {
            return false;
        }
        m_destination.m_channels.clear();
        m_destination.m_name.clear();

        m_name = fields[0];
        string count = fields[1];
        fieldIter begin = fields.begin() + 2;
        while (begin < fields.end())
        {
            begin = AddDevice(begin, fields.end());
        }

        if (m_valid)
        {
            m_destination.m_channels = m_channels;
            m_destination.m_name = m_name;
        }

        return fields.size() > 0;
    }

    strings GetHeadingStrings() const
    {
        strings stringList;

        for (channelType::const_iterator cit = m_destination.m_channels.begin(); 
            cit != m_destination.m_channels.end(); ++cit)
        {
            stringList.insert(stringList.end(), 
                cit->channels.begin(), 
                cit->channels.end());
        }

        return stringList;
    }

    std::string GetHeadings() const
    {
        std::string retv;
        retv.reserve(2048);

        strings stringList = GetHeadingStrings();
        
        strings::const_iterator cit = stringList.begin();
        if (cit == stringList.end())
        {
            return retv;
        }

        retv = *cit++;

        for (;cit != stringList.end(); ++cit)
        {
            retv += ", " + *cit;
        }
        
        return retv;
    }

    outputData& GetOutputData() { return m_destination; }
    outputData const& GetOutputData() const { return m_destination; }

private :
    fieldIter AddDevice(fieldIter begin, fieldIter end)
    {
        // There must be a device ID, a channel count and at least one channel
        if (end - begin < 4)
        {
            UCSBUtility::LogError(__FUNCTION__, __LINE__, "Malformed aggregator configuration:"
                " %d fields expected, %d found\n", 3, end - begin);
            m_valid = false;
            return end;
        }

        string const& name = *begin; ++begin;
        name;
        string const& guid = *begin; ++begin;
        channelData cd;
        cd.deviceType = ConvertToGUID(guid);
        // Look up the device, if it isn't in the dictionary, we can't use it

        cd.deviceIndex = atoi(begin->c_str()); ++begin;
        int count = atoi(begin->c_str()); ++begin;

        if (end - begin < count)
        {
            UCSBUtility::LogError(__FUNCTION__, __LINE__, "Malformed aggregator configuration:"
                " %d fields expected, %d found\n", 3, end - begin);
            m_valid = false;
            return end;
        }

        for (int i=0; i < count && begin < end; ++i, ++begin)
        {
            cd.channels.push_back(*begin);
        }

        m_channels.push_back(cd);

        return begin;
    }

private :
    DataAggregator& operator= (DataAggregator const& rhs);

private :
    outputData& m_destination;
    bool        m_valid;
    string      m_name;
    channelType m_channels;
};


std::string PrintToString(char const* format, ...)
{
    va_list args;
    va_start(args, format);      // Initialize variable arguments.

    char output[512];

    vsprintf_s(output, format, args);

    va_end(args);                // Reset variable arguments.

    return output;
}

std::string ReadDefaultConfig(std::string filename)
{
    std::string output;
    //FILE* pErrorFile = NULL;
    //string errorFile = ChangeExtension(filename, ".error");
    //freopen_s(&pErrorFile, errorFile.c_str(), "wt", stderr);

    //output += PrintToString("; Processing File %s\n", filename.c_str());
    output += PrintToString("; Configuration based on\n[%s]\n", filename.c_str());
    FileHandle hFile = CreateFileA(filename.c_str(), GENERIC_READ, 
        FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);

    char id[_countof(DeviceHolder::g_header)];
    DWORD read = 0;
    ReadFile(hFile, id, sizeof(id), &read, NULL);
    if (memcmp(&id, DeviceHolder::g_header, sizeof(id)))
    {
        UCSBUtility::LogError(__FUNCTION__, __LINE__, "File does not have "
            "the expected ID bytes\nFilename: %s", filename.c_str());
        return "FAILURE";
    }

    DeviceHolder dictionary;
    dictionary.ClearDictionary();

    // No need to chunk the data, read it all at once
    vector<BYTE> data;
    DWORD filesize = GetFileSize(hFile, NULL);
    data.resize(filesize);

    ReadFile(hFile, &data[0], filesize, &read, NULL);


    DataAggregationTool dat;
    dat.SetSurveyMode(true);
    DeviceHolder::FileParser<DataAggregationTool> datParser(dictionary, dat);

    try
    {
        LN250::ChunkInputData(&data[0], &data[data.size()-1]+1, datParser);
    }
    catch (const char* pText)
    {
        UCSBUtility::LogError(__FUNCTION__, __LINE__, "Invalid Format: %s\n", pText);
        return "";
    }

    fprintf(stdout, "; First Pass Completed\n");

    std::vector<std::pair<BYTE, BYTE> > devices = dat.GetDeviceNumbers();

    output += PrintToString("; Found %d devices\n[%d]\n", devices.size(), devices.size());

    // Print the list of devices available for display
    for (std::vector<std::pair<BYTE, BYTE> >::const_iterator CIT = devices.begin();
        CIT != devices.end(); ++CIT)
    {
        UCSB_Datastream::Device d = dictionary.GetDevice(static_cast<BYTE>(CIT->second));
        std::string guidString = UCSBUtility::GetStringFromCLSID(d.DeviceID());
        std::vector<std::string> channels = d.GetChannelNames();
        output += PrintToString("; Device Found:\n[%s] [%s] [%d] [%d]\n", 
            d.DisplayName().c_str(), guidString.c_str(), CIT->first, 
            channels.size());

        // now display all the channels available for display
        output += PrintToString("; Channel List:\n");

        for (std::vector<std::string>::const_iterator chnIt = channels.begin();
            chnIt != channels.end(); ++chnIt)
        {
            output += PrintToString("\t[%s]\n", chnIt->c_str());
        }
    }

    fprintf(stdout, "Second Pass Completed\n");

    return output;
}

struct directoryInfo
{
    typedef std::string string;
    typedef std::vector<string> strings;

    string  directoryName;
    strings filenames;
    string  defaultConfig;
    string  currentConfig;
};

typedef std::map<std::string, directoryInfo*> DirectoryToFiles;
DirectoryToFiles g_Files;
CMutexHolder g_mutex;

struct threadInfo
{
    uintptr_t       threadHandle;
    directoryInfo*  diPointer;
    bool            stop;
    string          filename;
    string          status;

    threadInfo()
        : threadHandle(0)
        , diPointer(0)
        , stop(false)
    {
    }

    inline static int UpdateStatus(threadInfo* pTI, char const* format, ...)
    {
        va_list args;
        va_start(args, format);      // Initialize variable arguments.

        if (pTI != NULL)
        {
            char buffer[2048];
            vsprintf_s(buffer, format, args);

            pTI->status = buffer;
        }
        else
        {
            vfprintf_s(stdout, format, args);
        }

        va_end(args);                // Reset variable arguments.

        return 0;
    }


};

std::vector<directoryInfo*> g_handles;
std::vector<threadInfo*>    g_threads;

threadInfo* FindThread(SPACEPROCESSHANDLE hSpaceProcessHandle)
{
    CMutexHolder::ScopedMutex mutex(g_mutex.GetMutex(L"ProcessingMutex"));

    std::vector<threadInfo*>::const_iterator cit = std::find(g_threads.begin(), 
        g_threads.end(), static_cast<threadInfo*>(hSpaceProcessHandle));
    
    if (cit == g_threads.end())
    {
        return NULL;
    }

    return *cit;
}

threadInfo* FindThread(uintptr_t threadHandle)
{
    // Can't do a find if people are changing this
    CMutexHolder::ScopedMutex mutex(g_mutex.GetMutex(L"ProcessingMutex"));

    for(vector<threadInfo*>::iterator cit = g_threads.begin();
        cit != g_threads.end(); ++cit)
    {
        if ((*cit)->threadHandle == threadHandle)
        {
            return *cit;
        }
    }

    return NULL;
}

threadInfo* FindThread(directoryInfo*  directoryHandle)
{
    // Can't do a find if people are changing this
    CMutexHolder::ScopedMutex mutex(g_mutex.GetMutex(L"ProcessingMutex"));

    for(vector<threadInfo*>::iterator cit = g_threads.begin();
        cit != g_threads.end(); ++cit)
    {
        if ((*cit)->diPointer == directoryHandle)
        {
            return *cit;
        }
    }

    return NULL;
}

directoryInfo* FindDirectoryInfo(SPACEHANDLE hHandle)
{
    CMutexHolder::ScopedMutex mutex(g_mutex.GetMutex(L"ProcessingMutex"));

    std::vector<directoryInfo*>::const_iterator cit = std::find(g_handles.begin(), 
        g_handles.end(), static_cast<directoryInfo*>(hHandle));
    
    if (cit == g_handles.end())
    {
        return NULL;
    }

    return *cit;
}

void SetVariant(VARIANT* output, string input)
{
    if (output == NULL)
    {
        return;
    }

    VariantClear(output);

    // Create the output text
    //_bstr_t b SysAllocString();
    _variant_t v(UCSBUtility::StupidConvertToWString(input).c_str());
    
    *output = v.Detach();
    //SysFreeString(b);
}

EXPORTABLE 
SPACEHANDLE GetDirectoryInfo(LPCSTR directoryName, VARIANT* configText)
{
    if (directoryName == NULL || configText == NULL)
    {
        UCSBUtility::LogError(__FUNCTION__, __LINE__, "directoryName (%p) "
            "or configText (%p) is NULL\n", 
            directoryName, configText);
        return NULL;
    }

    VariantClear(configText);

    DirectoryToFiles::const_iterator cit = g_Files.find(directoryName);
    if (cit != g_Files.end() && cit->second != NULL)
    {
        SetVariant(configText, cit->second->defaultConfig);
        return static_cast<SPACEHANDLE>(cit->second);
    }

    directoryInfo* pDirectoryInfo = new directoryInfo;

    pDirectoryInfo->filenames = GetFilenames(directoryName);

    if (pDirectoryInfo->filenames.size() == 0)
    {
        delete pDirectoryInfo;
        return NULL;
    }

    // Pick one file to process
    pDirectoryInfo->defaultConfig = ReadDefaultConfig(pDirectoryInfo->filenames
        [pDirectoryInfo->filenames.size()/2]);
    printf("; %s\n", pDirectoryInfo->defaultConfig.c_str());

    // 
    pDirectoryInfo->currentConfig = pDirectoryInfo->defaultConfig;
    pDirectoryInfo->directoryName = directoryName;
    g_Files[directoryName] = pDirectoryInfo;
    g_handles.push_back(pDirectoryInfo);

    SetVariant(configText, pDirectoryInfo->defaultConfig);
    return static_cast<SPACEHANDLE>(pDirectoryInfo);
}

EXPORTABLE void DeleteHandle(SPACEHANDLE hHandle)
{
    directoryInfo* pDirectoryInfo = FindDirectoryInfo(hHandle);

    if (pDirectoryInfo == NULL)
    {
        return;
    }

    // Find if this is in use by another thread
    HANDLE hThread = NULL;

    {
        CMutexHolder::ScopedMutex mutex(g_mutex.GetMutex(L"ProcessingMutex"));

        threadInfo* pTI = FindThread(pDirectoryInfo);
        
        if (pTI != NULL)
        {
            pTI->stop = true;
            hThread = reinterpret_cast<HANDLE>(pTI->threadHandle);
        }
    }
 
    if (hThread != NULL)
    {
        WaitForSingleObject(hThread, INFINITE);
    }

    // Cleanup
    CMutexHolder::ScopedMutex mutex(g_mutex.GetMutex(L"ProcessingMutex"));
    // Remove from map
    g_Files[pDirectoryInfo->directoryName] = NULL;

    // Remove from handle list
    g_handles.erase(std::find(g_handles.begin(), g_handles.end(), hHandle));
    delete pDirectoryInfo;
}

EXPORTABLE void GetDefaultConfigString(SPACEHANDLE hSpaceball, VARIANT* configText)
{
    directoryInfo* pDirectoryInfo = FindDirectoryInfo(hSpaceball);
    if (pDirectoryInfo == NULL || configText == NULL)
    {
        UCSBUtility::LogError(__FUNCTION__, __LINE__, "Invalid Handle (%p) "
            "or configText (%p) is NULL\n", 
            hSpaceball, configText);
        return;
    }

    SetVariant(configText, pDirectoryInfo->defaultConfig);
    return;
}

EXPORTABLE void GetConfigString(SPACEHANDLE hSpaceball, VARIANT* configText)
{
    directoryInfo* pDirectoryInfo = FindDirectoryInfo(hSpaceball);
    if (pDirectoryInfo == NULL || configText == NULL)
    {
        UCSBUtility::LogError(__FUNCTION__, __LINE__, "Invalid Handle (%p)"
            " or configText (%p) is NULL\n", 
            hSpaceball, configText);
        return;
    }

    string* pString = &pDirectoryInfo->currentConfig;

    if (pString->length() == 0)
    {
        pString = &pDirectoryInfo->defaultConfig;
    }

    SetVariant(configText, *pString);
    return;
}

EXPORTABLE DWORD SetConfigString(SPACEHANDLE hSpaceball, VARIANT configText)
{
    directoryInfo* pDirectoryInfo = FindDirectoryInfo(hSpaceball);
    if (pDirectoryInfo == NULL)
    {
        UCSBUtility::LogError(__FUNCTION__, __LINE__, "Invalid Handle (%p)\n", 
            hSpaceball);
        return static_cast<DWORD>(-1);
    }

    if (configText.vt != VT_BSTR)
    {
        UCSBUtility::LogError(__FUNCTION__, __LINE__, "Bad configText type %d\n", 
            configText.vt);
        return static_cast<DWORD>(-2);
    }

    if (configText.bstrVal == NULL)
    {
        UCSBUtility::LogError(__FUNCTION__, __LINE__, "configText is NULL\n");
        return static_cast<DWORD>(-3);
    }

    string sConfigText = UCSBUtility::StupidConvertToString(configText.bstrVal);

    DataAggregator::outputData devices;
    DataAggregator da(devices);
    ConfigFileReader::InterpretStringFields(sConfigText, da);
    if (!da.isValid())
    {
        UCSBUtility::LogError(__FUNCTION__, __LINE__, "Failed to parse configText\n");
        return static_cast<DWORD>(-4);
    }

    pDirectoryInfo->currentConfig = sConfigText;
    return 0;
}

//void ConvertFileDa(std::string const& filename, DataAggregator da);
void ConvertFile(std::string const& filename, DataAggregator da, threadInfo* pTI);

struct ConvertFileAdapter
{
    ConvertFileAdapter(DataAggregator const& da, threadInfo* pTI)
        : m_da (da)
        , m_pTI(pTI)
    {        
    }

    void operator()(std::string const& filename)
    {
        if (m_pTI != NULL)
        {
            if (m_pTI->stop)
            {
                return;
            }

            m_pTI->filename = filename;
        }

        try
        {
            ConvertFile(filename, m_da, m_pTI);
        }
        catch(char const* pErrorText)
        {
            UCSBUtility::LogError(__FUNCTION__, __LINE__, "Error Processing"
                " File %s\n%s\n", filename.c_str(), pErrorText);
        }
    }

    DataAggregator  m_da;
    threadInfo*     m_pTI;
};

EXPORTABLE void ProcessDirectory(SPACEHANDLE hSpaceball)
{
    directoryInfo* pDirectoryInfo = NULL;    
    {
        CMutexHolder::ScopedMutex mutex(g_mutex.GetMutex(L"ProcessingMutex"));
        pDirectoryInfo = FindDirectoryInfo(hSpaceball);
        if (pDirectoryInfo == NULL)
        {
            UCSBUtility::LogError(__FUNCTION__, __LINE__, "Invalid Handle (%p)\n", 
                hSpaceball);
            return;
        }
    }

    threadInfo* pTI = FindThread(pDirectoryInfo);

    DataAggregator::outputData devices;
    DataAggregator da(devices);
    ConfigFileReader::InterpretStringFields(pDirectoryInfo->currentConfig, da);
    if (!da.isValid())
    {
        UCSBUtility::LogError(__FUNCTION__, __LINE__, "Current config is "
            "invalid - should be impossible\n");
        return;
    }

    std::vector<string> const& names = pDirectoryInfo->filenames;
    int i = names.size();
    printf("%d %s\n", i, names.at(0).c_str());

    std::for_each(pDirectoryInfo->filenames.begin(), pDirectoryInfo->filenames.end(),
        ConvertFileAdapter(da, pTI));

    if (pTI)
    {
        CMutexHolder::ScopedMutex mutex(g_mutex.GetMutex(L"ProcessingMutex"));
        g_threads.erase(std::find(g_threads.begin(), g_threads.end(), pTI));
        delete pTI;
    }
}

SPACEPROCESSHANDLE ProcessDirectoryStart(SPACEHANDLE hSpaceball)
{
    directoryInfo* pDirectoryInfo = FindDirectoryInfo(hSpaceball);
    if (pDirectoryInfo == NULL)
    {
        UCSBUtility::LogError(__FUNCTION__, __LINE__, "Invalid Handle (%p)\n", 
            hSpaceball);
        return NULL;
    }

    threadInfo* pTI = new threadInfo;
    pTI->diPointer = pDirectoryInfo;
    g_threads.push_back(pTI);
    pTI->threadHandle = _beginthread(ProcessDirectory, 0, hSpaceball);

    return pTI;
}

EXPORTABLE SPACEPROCESSHANDLE GetProcessStatus(SPACEPROCESSHANDLE hProcess, 
                                               VARIANT* filename, 
                                               VARIANT* lastStatus)
{
    threadInfo* pTI = FindThread(hProcess);
    if (pTI == NULL)
    {
        UCSBUtility::LogError(__FUNCTION__, __LINE__, "Invalid Handle (%p)\n", 
            hProcess);
        return NULL;
    }

    if (filename == NULL || lastStatus == NULL)
    {
        UCSBUtility::LogError(__FUNCTION__, __LINE__, "Invalid filename (%p) or lastStatus (%p)\n", 
            filename, lastStatus);

        return NULL;
    }

    SetVariant(lastStatus, pTI->status);
    SetVariant(filename, pTI->filename);

    return hProcess;
}

void ProcessStop(SPACEPROCESSHANDLE hProcess)
{
    threadInfo* pTI = FindThread(hProcess);
    if (pTI == NULL)
    {
        UCSBUtility::LogError(__FUNCTION__, __LINE__, "Invalid Handle (%p)\n", 
            hProcess);
        return;
    }

    pTI->stop = true;
    WaitForSingleObject(reinterpret_cast<HANDLE> (pTI->threadHandle), INFINITE);
}

void ReadFileToVector(std::string const& filename, std::vector<BYTE>& data)
{
    FileHandle hFile = CreateFileA(filename.c_str(), GENERIC_READ, 
        FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);

    char id[_countof(DeviceHolder::g_header)];
    DWORD read = 0;
    ReadFile(hFile, id, sizeof(id), &read, NULL);
    if (memcmp(&id, DeviceHolder::g_header, sizeof(id)))
    {
        UCSBUtility::LogError(__FUNCTION__, __LINE__, "File does not have the expected ID bytes\nFilename: %s", filename.c_str());
        return;
    }

    // No need to chunk the data, read it all at once
    DWORD filesize = GetFileSize(hFile, NULL);
    data.resize(filesize);

    ReadFile(hFile, &data[0], filesize, &read, NULL);
}

// Assumes the second channel is the deviceIndex
BYTE GetIndex(UCSB_Datastream::DeviceHolder const& dictionary, 
               BYTE dictionaryIndex, BYTE const* pData)
{
    UCSB_Datastream::Device const& device = dictionary.
        GetDevice(dictionaryIndex);

    // ASSERT needed

    return device.GetChannelVARIANT(1, pData).bVal;
}

class OutputBase
{
protected :
    struct dataEntry
    {
        BYTE        dictionaryIndex;
        BYTE const* pData;
        BYTE        deviceIndex;
    };
public :
    struct inputData
    {
        DeviceHolder*   pDictionary;
        DataAggregator* pDataAggregator;
        //FILE*           pOutFile;
        threadInfo*     pTI;

        inputData(DeviceHolder& dictionary, DataAggregator& da, threadInfo* pThreadInfo)
            : pDictionary(&dictionary)
            , pDataAggregator(&da)
            //, pOutFile(pFile)
            , pTI (pThreadInfo)
        {
        }

    };
};

class OutputCSV : public OutputBase
{
public :
    struct outputData
    {
        string    data;
    };


    OutputCSV(inputData const& in, outputData& out)
        : pDictionary(in.pDictionary)
        , indexedChannels(in.pDataAggregator->GetOutputData().GetIndexedChannels(*in.pDictionary))
        , timestamp(0)
        , m_pTI(in.pTI)
        , m_pOutputData(&out)
    {
        m_pOutputData->data.clear();
        m_pOutputData->data.reserve(1024 * 1024);

        DataAggregator::channelIndexedType::const_iterator cit = 
            indexedChannels.begin();
        for(; cit != indexedChannels.end(); ++cit)
        {
            for (size_t i = 0; i < cit->channels.size();++i)
            {
                m_emptyLine.insert(m_emptyLine.length(),  ", ");
            }
        }
    }
    
    ~OutputCSV()
    {
        DoOutput();
    }

    void DoOutput()
    {
        if (m_sample.size() == 0)
        {
            return;
        }

        // The output is an entry for each selected channel in the output data
        // iterate through the output data list
        // for each deviceIndex, find the LAST sample that corresponds to the
        // given deviceIndex
        std::string outputLine;
        DataAggregator::channelIndexedType::const_iterator cit = 
            indexedChannels.begin();
        bool noChannelsFound = true;
        for(; cit != indexedChannels.end(); ++cit)
        {
            // Find a matching deviceIndex in the samples
            // Search backwards
            const size_t notFound = static_cast<size_t>(-1);
            size_t found = notFound;
            for (size_t i=m_sample.size(); i > 0; --i)
            {
                size_t index = i - 1;
                if (m_sample[index].deviceIndex == cit->deviceIndex)
                {
                    found = index;
                    break;
                }
            }

            if (found == notFound)
            {
                outputLine.insert(outputLine.end(), m_emptyLine.begin(), m_emptyLine.begin() + cit->channels.size() * 2);
            }
            else
            {
                noChannelsFound = false;
                UCSB_Datastream::Device device;
                if (pDictionary->GetDevice(cit->deviceType, device))
                {
                    outputLine += device.GetDisplayData(m_sample[found].pData, cit->channels) + ", ";
                }
                else
                {
                    // ERROR CONDITION, we should never get here
                    // this means we have a device we know enough 
                    // about to have channels, but is suddenly empty!
                    outputLine.insert(outputLine.end(), m_emptyLine.begin(), m_emptyLine.begin() + cit->channels.size() * 2);
                }
            }
        }

        if (noChannelsFound == false)
        {
            m_pOutputData->data.insert(m_pOutputData->data.end(), outputLine.begin(), outputLine.end());
            m_pOutputData->data.insert(m_pOutputData->data.size(), "\n");
        }

        m_sample.clear();
    }

    // ignore const :-(
    void Process(DWORD dictionaryIndex, BYTE const* pData)
    {
        UCSB_Datastream::Device const& d = 
            pDictionary->GetDevice(static_cast<BYTE>(dictionaryIndex));

        UINT64 t = d.GetChannelVARIANT(0, pData).ullVal;
        if (t != timestamp)
        {
            DoOutput();
        }

        timestamp = t;
        dataEntry de;
        de.dictionaryIndex = static_cast<BYTE>(dictionaryIndex);
        de.pData = pData;
        de.deviceIndex = GetIndex(*pDictionary, de.dictionaryIndex, de.pData);
        m_sample.push_back(de);
    }

    void operator()(DWORD index, BYTE const* pData) const
    {
        if (m_pTI && m_pTI->stop)
        {
            return;
        }

        const_cast<OutputCSV*>(this)->Process(index, pData);
    }
    
    
private :
    OutputCSV(OutputCSV const&);
    OutputCSV& operator = (OutputCSV const&);
    DeviceHolder const*   pDictionary;
    
    DataAggregator::channelIndexedType  indexedChannels;
    UINT64                              timestamp;
    std::vector<dataEntry>              m_sample;
    //FILE*                               m_pFile;
    threadInfo*                         m_pTI;
    outputData*                         m_pOutputData;
    string                              m_emptyLine;
};

class OutputFITS : public OutputBase
{
public :
    struct outputData
    {
        DWORD           cols;
        DWORD           lines;
        vector<double>  data;
    };


    OutputFITS(inputData const& in, outputData& out) //DeviceHolder& dictionary, DataAggregator& da, FILE* pFile, threadInfo* pTI
        : pDictionary(in.pDictionary)
        , indexedChannels(in.pDataAggregator->GetOutputData().GetIndexedChannels(*in.pDictionary))
        , timestamp(0)
        //, m_pFile(in.pOutFile)
        , m_pTI(in.pTI)
        , m_pOutputData(&out)
    {
        m_pOutputData->cols = 0;
        m_pOutputData->lines = 0;
        m_pOutputData->data.clear();
        m_pOutputData->data.reserve(1024 * 1024);

        DataAggregator::channelIndexedType::const_iterator cit = 
            indexedChannels.begin();
        for(; cit != indexedChannels.end(); ++cit)
        {
            for (size_t i = 0; i < cit->channels.size();++i)
            {
                m_emptyLine.push_back(static_cast<double>(UCSBUtility::GetNan()));
            }
        }

        m_pOutputData->cols = m_emptyLine.size();
    }
    
    ~OutputFITS()
    {
        DoOutput();
    }

    void DoOutput()
    {
        if (m_sample.size() == 0)
        {
            return;
        }

        vector<double> line;
        line.reserve(m_emptyLine.size());

        // The output is an entry for each selected channel in the output data
        // iterate through the output data list
        // for each deviceIndex, find the LAST sample that corresponds to the
        // given deviceIndex
        std::string outputLine;
        DataAggregator::channelIndexedType::const_iterator cit = 
            indexedChannels.begin();
        bool noChannelsFound = true;
        for(; cit != indexedChannels.end(); ++cit)
        {
            // Find a matching deviceIndex in the samples
            // Search backwards
            const size_t notFound = static_cast<size_t>(-1);
            size_t found = notFound;
            for (size_t i=m_sample.size(); i > 0; --i)
            {
                size_t index = i - 1;
                if (m_sample[index].deviceIndex == cit->deviceIndex)
                {
                    found = index;
                    break;
                }
            }

            if (found == notFound)
            {
                line.insert(line.end(), m_emptyLine.begin(), m_emptyLine.begin() + cit->channels.size());
            }
            else
            {
                noChannelsFound = false;
                UCSB_Datastream::Device device;
                if (pDictionary->GetDevice(cit->deviceType, device))
                {
                    //outputLine += device.GetDisplayData(m_sample[found].pData, cit->channels) + ", ";
                    std::vector<DWORD>::const_iterator channel = cit->channels.begin();
                    for (; channel != cit->channels.end(); ++channel)
                    {
                        line.push_back(static_cast<double>(device.GetChannelDouble(*channel, m_sample[found].pData)));
                    }
                }
                else
                {
                    for (size_t i = 0; i < cit->channels.size();++i)
                    {
                        //outputLine += ", ";
                        line.push_back(static_cast<double>(UCSBUtility::GetNan()));
                    }
                }
            }
        }

        if (noChannelsFound == false)
        {
            vector<double> outLine;
            outLine.resize(line.size());
            for (size_t i=0; i < line.size(); ++i)
            {
                UCSBUtility::s_reverse(outLine[i], line[i]);
            }

            //fprintf(m_pFile, "%s\n", outputLine.c_str());
            //fwrite(&outLine[0], sizeof(outLine[0]), outLine.size(), m_pFile);
            m_pOutputData->data.insert(m_pOutputData->data.end(), outLine.begin(), outLine.end());

            ++m_pOutputData->lines;
        }

        m_sample.clear();
    }

    // ignore const :-(
    void Process(DWORD dictionaryIndex, BYTE const* pData)
    {
        UCSB_Datastream::Device const& d = 
            pDictionary->GetDevice(static_cast<BYTE>(dictionaryIndex));

        UINT64 t = d.GetChannelVARIANT(0, pData).ullVal;
        if (t != timestamp)
        {
            DoOutput();
        }

        timestamp = t;
        dataEntry de;
        de.dictionaryIndex = static_cast<BYTE>(dictionaryIndex);
        de.pData = pData;
        de.deviceIndex = GetIndex(*pDictionary, de.dictionaryIndex, de.pData);
        m_sample.push_back(de);
    }

    void operator()(DWORD index, BYTE const* pData) const
    {
        if (m_pTI && m_pTI->stop)
        {
            return;
        }

        const_cast<OutputFITS*>(this)->Process(index, pData);
    }
    
    
private :
    OutputFITS(OutputFITS const&);
    OutputFITS& operator = (OutputFITS const&);
    DeviceHolder const*   pDictionary;
    
    DataAggregator::channelIndexedType  indexedChannels;
    UINT64                              timestamp;
    std::vector<dataEntry>              m_sample;
    //FILE*                               m_pFile;
    threadInfo*                         m_pTI;
    outputData*                         m_pOutputData;
    std::vector<double>                 m_emptyLine;
};

template<typename imageType>
void PadDataForFITSOutput(vector<imageType>& frame, imageType padValue = 0)
{
    if (frame.size() == 0)
    {
        return;
    }

    // PAD out to FITS approved 2880 bytes (36 cards of 80 bytes)
    // Remember, 2880 BYTES, not 2880 shorts
    unsigned padding = (FITS_RECORD_SIZE - frame.size() * sizeof(frame[0]) % FITS_RECORD_SIZE) / sizeof(frame[0]);
    if (padding != FITS_RECORD_SIZE / sizeof(frame[0]))
    {
        frame.insert(frame.end(), padding, padValue);
        //frame.resize(frame.size() + padding);
    }
}

// FITS record size
static const int FITS_RECORD_SIZE = 2880;

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


// ENSURE code failure when using a non-enumerated type
template<typename dataType>
struct FITSDataSize { enum { size = dataType::size }; };

template<> struct FITSDataSize<BYTE>   { enum { size = 8 }; };
template<> struct FITSDataSize<short>  { enum { size = 16 }; };
template<> struct FITSDataSize<long>   { enum { size = 32 }; };
template<> struct FITSDataSize<float>  { enum { size = -16 }; };
template<> struct FITSDataSize<double> { enum { size = -32 }; };

string SpacesToUnderscore(string const& inString)
{
    if (find(inString.begin(), inString.end(), ' ') == inString.end())
    {
        return inString;
    }

    string outString;
    outString.resize(inString.size());
    
    for(size_t i = 0; i < inString.size(); ++i)
    {
        outString[i] = inString[i] == ' ' ? '_' : inString[i];
    }

    return outString;
}

string QuoteString20(string const& inString)
{
    if (inString.size() == 0)
    {
        return QuoteString20("''");
    }

    string outstring = inString;
    if (inString[0] != '\'')
    {
        outstring = "'" + inString;
    }

    // limit to 19 characters (including the inital ')
    if (outstring.length() > 19 && outstring[outstring.length() - 1] != '\'')
    {
        outstring = string(outstring.begin(), outstring.begin() + 19);
    }
    
    if (outstring[outstring.length() - 1] != '\'')
    {
        outstring += '\'';
    }

    if (outstring.length() < 20)
    {
        outstring.insert(outstring.end(), 20 - outstring.length(), ' ');
    }

    return outstring;
}

vector<char>
CreateFITSHeader(unsigned cols, unsigned rows, vector<string> const& columnNames, bool useIndex)
{
    vector<char> header;
    header.reserve(FITS_RECORD_SIZE);

    AppendCard(header, CreateFITS_HeaderCard("SIMPLE", "T"));
    AppendCard(header, CreateFITS_HeaderCard("BITPIX", FITSDataSize<BYTE>::size));
    AppendCard(header, CreateFITS_HeaderCard("NAXIS", "0"));
    AppendCard(header, CreateFITS_HeaderCard("EXTEND", "T", "File contains extensions"));
    // Last non-blank card in the header
    AppendCard(header, CreateFITS_HeaderCard("END"));
    PadDataForFITSOutput(header, ' ');

    // Now we need to generate another header
    // This is the extension header

    AppendCard(header, CreateFITS_HeaderCard("XTENSION", "'BINTABLE'"));
    AppendCard(header, CreateFITS_HeaderCard("BITPIX", FITSDataSize<BYTE>::size));
    AppendCard(header, CreateFITS_HeaderCard("NAXIS", 2));
    AppendCard(header, CreateFITS_HeaderCard("NAXIS1", sizeof(double) * cols));  // BUGBUG sum of all sizes
    AppendCard(header, CreateFITS_HeaderCard("NAXIS2", rows));
    AppendCard(header, CreateFITS_HeaderCard("PCOUNT", 0));
    AppendCard(header, CreateFITS_HeaderCard("GCOUNT", 1));
    AppendCard(header, CreateFITS_HeaderCard("TFIELDS", cols));
    AppendCard(header, CreateFITS_HeaderCard("EXTNAME", QuoteString20("SB-Extract")));  // BUGBUG Get from user


    for (size_t i = 0; i < cols; ++i)
    {
        char card[80];
        sprintf_s(card, "TFORM%d", i + 1);
        AppendCard(header, CreateFITS_HeaderCard(card, QuoteString20("D")));

        sprintf_s(card, "TTYPE%d", i + 1);
        char fieldType[80];
        if (i >= columnNames.size())
        {
            sprintf_s(fieldType, "'FIELD%d'", i);
        }
        else if (useIndex)
        {
            sprintf_s(fieldType, "'%s_%d'", columnNames[i].c_str(), i);
        }
        else
        {
            sprintf_s(fieldType, "'%s'", columnNames[i].c_str());
        }

        AppendCard(header, CreateFITS_HeaderCard(card, QuoteString20(fieldType)));
    }

    AppendCard(header, CreateFITS_HeaderCard("END"));
    PadDataForFITSOutput(header, ' ');

    return header;
}
void ConvertFile(std::string const& filename, DataAggregator da, threadInfo* pTI)
{
    pTI->UpdateStatus(pTI, "Processing File %s\n", filename.c_str());
    DeviceHolder dictionary;
    dictionary.ClearDictionary();

    vector<BYTE> data;
    ReadFileToVector(filename, data);
    if (data.size () == 0)
    {
        UCSBUtility::LogError(__FUNCTION__, __LINE__, "No data found in Filename: %s", filename.c_str());
        return;
    }

    DataAggregationTool dat;
    dat.SetSurveyMode(false);
    dat.SetUpdateStatusFn(threadInfo::UpdateStatus, pTI);
    DeviceHolder::FileParser<DataAggregationTool> datParser(dictionary, dat);
    LN250::ChunkInputData(&data[0], &data[data.size()-1]+1, datParser);

    pTI->UpdateStatus(pTI, "First Pass Completed\n");

    std::vector<std::pair<BYTE, BYTE> > devices = dat.GetDeviceNumbers();

    OutputFITS::inputData id (dictionary, da, pTI);
    OutputFITS::outputData od;

    if (pTI->stop)
    {
        pTI->UpdateStatus(pTI, "Stopped by User\n");
        return;
    }

    dat.WalkData(OutputFITS(id, od));
    OutputCSV::outputData odCSV;
    dat.WalkData(OutputCSV(id, odCSV));
    pTI->UpdateStatus(pTI, "Second Pass Completed\n");

    string outputFileFIT = ChangeExtension(filename, ".fit");
    FILE* pOutputFile = NULL;
    fopen_s(&pOutputFile, outputFileFIT.c_str(), "wb");

    if (pOutputFile)
    {
        if (od.data.size() > 0)
        {
            bool useIndex = false;
            vector<string> nameStrings = da.GetHeadingStrings();
            vector<string>::const_iterator end = nameStrings.end();
            for(vector<string>::const_iterator cit = nameStrings.begin();
                cit != end; ++cit)
            {
                if (find(cit + 1, end, *cit) != end)
                {
                    useIndex = true;
                    break;
                }
            }

            vector<char> header = CreateFITSHeader(od.cols, od.lines, nameStrings, useIndex);

            fwrite(&header[0], sizeof(header[0]), header.size(), pOutputFile);
            PadDataForFITSOutput(od.data);
            fwrite(&od.data[0], sizeof(od.data[0]), od.data.size(), pOutputFile);
        }

        fclose(pOutputFile);
        pOutputFile = NULL;
    }

    string outputFileCSV = ChangeExtension(filename, ".csv");
    fopen_s(&pOutputFile, outputFileCSV.c_str(), "wt");
    if (pOutputFile && odCSV.data.size() > 0)
    {
        std::string headings = da.GetHeadings();
        fprintf(pOutputFile, "%s\n", headings.c_str());
        fwrite(&odCSV.data[0], sizeof(odCSV.data[0]), odCSV.data.size(), pOutputFile);
        fclose(pOutputFile);
        pOutputFile = NULL;
    }
}
