// Test-UCSB-DataStream.cpp : Defines the entry point for the console application.
//

// TODO:
// RENAME to something sensible
// Add the ability to separate blocks into files as an option

#include "stdafx.h"
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

vector<string> GetFilenames(string const& rootDirectory);


inline bool operator < (GUID const& lhs, GUID const& rhs)
{
    return memcmp(&lhs, &rhs, sizeof(lhs)) < 0;
}

// File Format
// SPACBALL
// Then filled with blocks
// The first blocks should be the dictionary
// any block that isn't defined is invalid

DeviceHolder g_dictionary;
//typedef struct DeviceHolder::FileParser         FileParser;
typedef struct DeviceHolder::DictionaryBuilder  DictionaryBuilder;
//wstring g_filename;
//string  g_outputFilename;
string g_DirectoryName;

FILE* g_hFile;

struct GetDirectoryName
{
    bool operator()(vector<string> const& fields)
    {
        if (fields.size() < 1)
        {
            UCSBUtility::LogError("%d configuration fields expected"
                " %d found\nNothing will be done\n", 1, fields.size());
        }

        g_DirectoryName = fields[0];
        return true;
    }
};

struct DataAggregator
{
    typedef vector<string>::const_iterator fieldIter;
    typedef vector<string>::const_iterator vectorStringIter;

    struct outputData
    {
        string                      m_name;
        map<GUID, vector<string>>   m_channels;
        map<GUID, vector<DWORD>>    m_channelsIndicies;

        void UpdateIndicies(Device const& device)
        {
            vector<DWORD>& channelIndicies = m_channelsIndicies[device.DeviceID()];
            vector<string>const& channelStrings = m_channels[device.DeviceID()];
            channelIndicies.clear();

            // Create a list of strings from the device
            vector<string> sourceNames = device.GetChannelNames();

            vectorStringIter sourceBegin = sourceNames.begin();
            vectorStringIter sourceEnd = sourceNames.end();

            // do a search for each name
            for(vectorStringIter iter = channelStrings.begin();
                iter < channelStrings.end(); ++iter)
            {
                vectorStringIter item = find(sourceBegin, sourceEnd, *iter);
                if (item == sourceEnd)
                {
                    UCSBUtility::LogError("Data Aggregator config failure: "
                        "%s field does not exist in device %s", iter->c_str(), 
                        device.DisplayName().c_str());
                }
                else
                {
                    channelIndicies.push_back(item - sourceBegin);
                }
            }
        }

        vector<DWORD> GetFields(Device const& device)
        {
            if (m_channels.find(device.DeviceID()) == m_channels.end())
            {
                return vector<DWORD>();
            }

            vector<DWORD>& channels = m_channelsIndicies[device.DeviceID()];
            if (channels.size() == 0 && 
                m_channels[device.DeviceID()].size() != 0)
            {
                UpdateIndicies(device);
            }

            return channels;
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

    fieldIter AddDevice(fieldIter begin, fieldIter end)
    {
        // There must be a device ID, a channel count and at least one channel
        if (end - begin < 3)
        {
            UCSBUtility::LogError("Malformed aggregator configuration:"
                " %d fields expected, %d found\n", 3, end - begin);
            m_valid = false;
            return end;
        }

        string const& guid = *begin; ++begin;
        GUID deviceID = ConvertToGUID(guid);
        // Look up the device, if it isn't in the dictionary, we can't use it

        int count = atoi(begin->c_str()); ++begin;

        vector<string> names;
        for (int i=0; i < count && begin < end; ++i, ++begin)
        {
            names.push_back(*begin);
        }

        m_channels[deviceID] = names;

        return begin;
    }

    bool operator ()(vector<string> const& fields)
    {
        m_destination.m_channels.clear();
        m_destination.m_name.clear();

        m_name = fields[0];
        fieldIter begin = fields.begin() + 1;
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

private :
    DataAggregator& operator= (DataAggregator const& rhs);

private :
    outputData&                 m_destination;
    bool                        m_valid;
    string                      m_name;
    map<GUID, vector<string>>   m_channels;
};

struct MessageProcessor
{
    MessageProcessor(DataAggregator::outputData& aggregator)
        : m_dictionaryChange (false)
        , m_aggregator(&aggregator)
        , m_contiguous(0)
    {
        memset(m_missing, 0, sizeof(m_missing));
        memset(&m_lastValue, 0, sizeof(m_lastValue));
    }
    
    void DictionaryUpdated() 
    {
        //printf("Dictionary Update!\n");
        m_dictionaryChange = true;

        // with each dictionary update we clear all the old info on printing messages
        memset(m_missing, 0, sizeof(m_missing));
    } // Does nothing useful for now

    static void InvalidData(BYTE messageID, Device const& device, 
        BYTE byteCount, BYTE const* pPayload)
    {
        // Silence the compiler warning
        messageID;
        pPayload;


        UCSBUtility::LogError("%s expected %d bytes received %d\n", 
            device.DisplayName().c_str(), device.GetDataSize(), byteCount);
    }

    bool operator ()(vector<string> const& fields)
    {
        return fields.size() > 0;
    }

    void ProcessData(Device const& device, BYTE const* pPayload);

    void MissingDictionaryEntry(BYTE dictionaryEntry)
    {
        // Prevent duplicate messages with a table indicating the message has been printed
        if (m_missing[dictionaryEntry] == false)
        {
            UCSBUtility::LogError("Dictionary Entry %d is missing\n", dictionaryEntry);
        }
        m_missing[dictionaryEntry] = true;
    }

    void PrintStats()
    {
        printf("Packet Stats\n");
        for(map<GUID, fileInfo>::const_iterator cit=histogram.begin();
            cit != histogram.end(); ++cit)
        {
            printf("\t%6d packets of %s\n", cit->second.lineCount, 
                cit->second.deviceName.c_str());
            for (size_t i=0; i < cit->second.histogram.size(); ++i)
            {
                if (cit->second.histogram[i])
                {
                    printf ("%2d : %-15d", i, cit->second.histogram[i]);
                }
            }

            printf ("\n\n");
        }
    }

private :
    MessageProcessor& operator= (MessageProcessor const& rhs);

    struct fileInfo
    {
        FILE*           file;
        DWORD           lineCount;
        string          deviceName;
        unsigned        maxContinguous;
        vector<DWORD>   histogram;
        UINT64          startTime;
        UINT64          elapsed;

        fileInfo() : file(NULL), lineCount(0), maxContinguous(0), startTime(0), elapsed(0) {}
    };

private :
    bool                            m_dictionaryChange;
    map<GUID, fileInfo>             histogram;
    DataAggregator::outputData*     m_aggregator;
    bool                            m_missing[256];
    GUID                            m_lastValue;
    unsigned                        m_contiguous;
};

void MessageProcessor::ProcessData(Device const& device, BYTE const* pPayload)
{
    // We might need to open a new output file here
    if (m_dictionaryChange)
    {
        m_dictionaryChange = false;
        for (map<GUID, fileInfo>::const_iterator cit = histogram.begin();
            cit != histogram.end(); ++cit)
        {
            CloseHandle(cit->second.file);
        }

        if (histogram.size() > 0)
        {
            histogram.clear();
        }
    }

    static const GUID null = {};
    GUID currentGUID = device.DeviceID();
    if (memcmp(&currentGUID, &m_lastValue, sizeof(currentGUID) && 
        memcmp(&m_lastValue, &null, sizeof(null))))
    {
        histogram[device.DeviceID()].maxContinguous = 
            max(histogram[device.DeviceID()].maxContinguous, m_contiguous);
        if (histogram[device.DeviceID()].histogram.size () <= m_contiguous)
        {
            histogram[device.DeviceID()].histogram.resize(m_contiguous + 1);
        }
        ++histogram[device.DeviceID()].histogram[m_contiguous];

        m_contiguous = 0;
    }
    m_lastValue = currentGUID;
    ++m_contiguous;

    ++histogram[device.DeviceID()].lineCount;
    if (histogram[device.DeviceID()].lineCount == 1)
    {
        histogram[device.DeviceID()].deviceName = device.DisplayName();
        VARIANT v = device.GetChannelVARIANT(0, pPayload);
        histogram[device.DeviceID()].startTime = v.ullVal;
        fprintf(stdout, "Found %s\n", device.DisplayName().c_str());
        std::vector<string> channels = device.GetChannelNames();
        fprintf(stdout, "Channels:\n");
        for (std::vector<string>::const_iterator cit = channels.begin();
            cit != channels.end(); ++cit)
        {
            fprintf(stdout, "\t%s\n", cit->c_str());
        }
        fprintf(stdout, "\n");

        histogram[device.DeviceID()].file = g_hFile;
    }

    // Is this a device we are configured to display?
    // then display this data
    vector<DWORD> fields = m_aggregator->GetFields(device);

    // Nothing to do
    if (fields.size() == 0)
    {
        return;
    }

    // Print the names as a header
    map<GUID, fileInfo>::iterator it = histogram.find(device.DeviceID());
    //if (it->second.lineCount == 1)
    {
        //string name = g_outputFilename +  device.DisplayName() + ".csv";
        //UCSBUtility::LogError("Opening file %s\n", name.c_str());
        //fopen_s(&(it->second.file), name.c_str(), "wt");
        //fprintf(it->second.file, "%s\n", device.GetDisplayFields(fields).c_str());
    }

    UINT64 elapsed = device.GetChannelVARIANT(0, pPayload).ullVal - histogram[device.DeviceID()].startTime;
    UINT64 diff = elapsed - histogram[device.DeviceID()].elapsed;
    histogram[device.DeviceID()].elapsed = elapsed;
    fprintf(it->second.file, "%10I64u, %10I64u, %s\n", elapsed, diff, device.GetDisplayData(pPayload, fields).c_str());
}

string ChangeExtension(string const& inFile, string const& newExt)
{
    string ext;
    if (newExt.size () == 0)
    {
        ext = ".";
    }
    else if (*newExt.begin() != '.')
    {
        ext = string(".") + newExt;
    }
    else
    {
        ext = newExt;
    }

    string retv;
    string::size_type lastDot = inFile.find_last_of('.');
    if (lastDot == string::npos)
    {
        lastDot = inFile.size();
    }

    return string(inFile.begin(), inFile.begin() + lastDot) + newExt;
}

struct DefaultThrowingParserHandler
{
    void DictionaryUpdated() const
    {
        // Do nothing
    }
    
    void MissingDictionaryEntry(BYTE messageID) const
    {
        LogError("Unable to process file with missing Dictionary Entry %d\n", messageID);

        throw "Unable to process file with missing Dictionary Entry";
    }

    void InvalidData(BYTE messageID, Device const& device, BYTE byteCount, BYTE const* pPayload) const
    {
        device;

        LogError("Unable to process file with Invalid Data Entry\nMessageID: "
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
    {
        memset(m_deviceNumbers, 0, sizeof(m_deviceNumbers));
    }
    
    ~DataAggregationTool()
    {
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
            LogError("Unexpected Dictionary Entry (dictionary should not"
                " update mid data stream)\n");
            
            throw "Unexpected Dictionary Entry (dictionary should not"
                " update mid data stream)\n";
        }
    }
    
    void ProcessData(Device const& device, BYTE const* pPayload)
    {
        m_dictionaryComplete = true;

        sorter key;
        key.time = devicI e.GetChannelVARIANT(0, pPayload).llVal;
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
            printf ("1) Processed %d\r", m_count);
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
                printf ("2) Processed %d\r", m_count);
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
    DataTree    m_data;
    bool        m_dictionaryComplete;
    int         m_deviceNumbers[256];
    int         m_count;
    bool        m_surveyOnly;
};


// Display all the data
void DisplayAllData(DWORD deviceNumber, BYTE const* pData)
{
    //pData;

    Device const& device = g_dictionary.GetDevice(static_cast<BYTE>(deviceNumber));
    //std::string const fields = device.GetDisplayFields();
    std::string const data = device.GetDisplayData(pData);
}

int ConvertData(int argc, _TCHAR* argv[])
{
    ConfigFileReader::DefaultConfigurationName::Set(L"");
    GetDirectoryName gdn;
    ConfigFileReader::ReadConfigurationFields(argc, argv, gdn);

    //fopen_s(&g_hFile, g_outputFilename.c_str(), "wt");

    DataAggregator::outputData interpreter;
    DataAggregator da (interpreter);
    ConfigFileReader::InterpretConfigurationResourceFields(L"interpreter", 
        da);

    vector<string> files = GetFilenames(g_DirectoryName);
    for(vector<string>::const_iterator cit = files.begin(); cit != files.end();
        ++cit)
    {
        FILE* pErrorFile = NULL;
        string errorFile = ChangeExtension(*cit, ".error");
        freopen_s(&pErrorFile, errorFile.c_str(), "wt", stderr);

        fprintf(stdout, "Processing File %s\n", cit->c_str());
        FileHandle hFile = CreateFileA(cit->c_str(), GENERIC_READ, 
            FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);

        char id[_countof(DeviceHolder::g_header)];
        DWORD read = 0;
        ReadFile(hFile, id, sizeof(id), &read, NULL);
        if (memcmp(&id, DeviceHolder::g_header, sizeof(id)))
        {
            UCSBUtility::LogError("File does not have the expected ID bytes\nFilename: %s", cit->c_str());
            continue;
        }


        string outFile = ChangeExtension(*cit, ".txt");
        fopen_s(&g_hFile, outFile.c_str(), "wt");
        if(g_hFile == NULL)
        {
            UCSBUtility::LogError("Unable to open output file: \n%s\n", outFile.c_str());
            continue;
        }

        MessageProcessor processor (interpreter);
        ConfigFileReader::InterpretConfigurationResourceFields(L"interpreter", 
            processor);

        g_dictionary.ClearDictionary();

        DeviceHolder::FileParser<MessageProcessor> parser(g_dictionary, processor);

        // No need to chunk the data, read it all at once
        vector<BYTE> data;
        DWORD filesize = GetFileSize(hFile, NULL);
        data.resize(filesize);
        //DWORD read = 0;
        ReadFile(hFile, &data[0], filesize, &read, NULL);
        fprintf(stdout, "Closing File %s\n\n", cit->c_str());
        fclose(g_hFile);

        //LN250::ChunkInputData(&data[0], &data[data.size()-1]+1, parser);

        DataAggregationTool dat;
        DeviceHolder::FileParser<DataAggregationTool> datParser(g_dictionary, dat);
        LN250::ChunkInputData(&data[0], &data[data.size()-1]+1, datParser);

        fprintf(stdout, "First Pass Completed\n");

        std::vector<std::pair<BYTE, BYTE> > devices = dat.GetDeviceNumbers();

        fprintf(stdout, "; Found %d devices\n[%d]\n", devices.size(), devices.size());
        
        // Print the list of devices available for display
        for (std::vector<std::pair<BYTE, BYTE> >::const_iterator cit = devices.begin();
            cit != devices.end(); ++cit)
        {
            UCSB_Datastream::Device d = g_dictionary.GetDevice(static_cast<BYTE>(cit->second));
            _bstr_t guidBString;
            StringFromCLSID(d.DeviceID(), guidBString.GetAddress());
            std::string guidString = UCSBUtility::StupidConvertToString(guidBString.GetBSTR());
            guidString = std::string(guidString.begin() + 1, guidString.end() - 1);
            std::vector<std::string> channels = d.GetChannelNames();
            fprintf(stdout, "; Device Found:\n[%s] [%s] [%d] [%d]\n", 
                d.DisplayName().c_str(), guidString.c_str(), cit->first, 
                channels.size());

            // now display all the channels available for display
            fprintf(stdout, "; Channel List:\n");

            for (std::vector<std::string>::const_iterator chnIt = channels.begin();
                chnIt != channels.end(); ++chnIt)
            {
                fprintf(stdout, "\t[%s]\n", chnIt->c_str());
            }
        }

        dat.WalkData(DisplayAllData);
        fprintf(stdout, "Second Pass Completed\n");

        //processor.PrintStats();
    }

    return 0;
}

int _tmain(int argc, _TCHAR* argv[])
{
    ConvertData(argc, argv);

    printf("Done, press any key to exit\n");

    _getch();

	return 0;
}

