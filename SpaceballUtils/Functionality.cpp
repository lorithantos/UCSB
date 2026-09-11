// This is the main DLL file.

#include "stdafx.h"

#include "functionality.h"

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



struct DataAggregator
{
    typedef vector<string>::const_iterator fieldIter;
    typedef vector<string>::const_iterator vectorStringIter;

    struct outputData
    {
        string                      m_name;
        map<GUID, vector<string>, UCSBUtility::GUIDLess>   m_channels;
        map<GUID, vector<DWORD>, UCSBUtility::GUIDLess>    m_channelsIndicies;

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
    map<GUID, vector<string>, UCSBUtility::GUIDLess>   m_channels;
};

void GetDirectoryInfo(std::string const& directoryName)
{
}

void ConvertFile(std::string const& filename)
{
    FILE* pErrorFile = NULL;
    string errorFile = ChangeExtension(filename, ".error");
    freopen_s(&pErrorFile, errorFile.c_str(), "wt", stderr);

    fprintf(stdout, "Processing File %s\n", filename.c_str());
    FileHandle hFile = CreateFileA(filename.c_str(), GENERIC_READ, 
        FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);

    char id[_countof(DeviceHolder::g_header)];
    DWORD read = 0;
    ReadFile(hFile, id, sizeof(id), &read, NULL);
    if (memcmp(&id, DeviceHolder::g_header, sizeof(id)))
    {
        UCSBUtility::LogError("File does not have the expected ID bytes\nFilename: %s", filename.c_str());
        return;
    }


    string outFile = ChangeExtension(filename, ".txt");
    fopen_s(&g_hFile, outFile.c_str(), "wt");
    if(g_hFile == NULL)
    {
        UCSBUtility::LogError("Unable to open output file: \n%s\n", outFile.c_str());
        return;
    }

    DataAggregator::outputData interpreter;
    DataAggregator da (interpreter);
    ConfigFileReader::InterpretConfigurationResourceFields(L"interpreter", 
        da);

    g_dictionary.ClearDictionary();

    // No need to chunk the data, read it all at once
    vector<BYTE> data;
    DWORD filesize = GetFileSize(hFile, NULL);
    data.resize(filesize);
    //DWORD read = 0;
    ReadFile(hFile, &data[0], filesize, &read, NULL);
    fprintf(stdout, "Closing File %s\n\n", filename.c_str());
    fclose(g_hFile);

    DataAggregationTool dat;
    dat.SetSurveyMode(true);
    DeviceHolder::FileParser<DataAggregationTool> datParser(g_dictionary, dat);
    LN250::ChunkInputData(&data[0], &data[data.size()-1]+1, datParser);

    fprintf(stdout, "First Pass Completed\n");

    std::vector<std::pair<BYTE, BYTE> > devices = dat.GetDeviceNumbers();

    fprintf(stdout, "; Found %d devices\n[%d]\n", devices.size(), devices.size());

    // Print the list of devices available for display
    for (std::vector<std::pair<BYTE, BYTE> >::const_iterator CIT = devices.begin();
        CIT != devices.end(); ++CIT)
    {
        UCSB_Datastream::Device d = g_dictionary.GetDevice(static_cast<BYTE>(CIT->second));
        _bstr_t guidBString;
        StringFromCLSID(d.DeviceID(), guidBString.GetAddress());
        std::string guidString = UCSBUtility::StupidConvertToString(guidBString.GetBSTR());
        guidString = std::string(guidString.begin() + 1, guidString.end() - 1);
        std::vector<std::string> channels = d.GetChannelNames();
        fprintf(stdout, "; Device Found:\n[%s] [%s] [%d] [%d]\n", 
            d.DisplayName().c_str(), guidString.c_str(), CIT->first, 
            channels.size());

        // now display all the channels available for display
        fprintf(stdout, "; Channel List:\n");

        for (std::vector<std::string>::const_iterator chnIt = channels.begin();
            chnIt != channels.end(); ++chnIt)
        {
            fprintf(stdout, "\t[%s]\n", chnIt->c_str());
        }
    }

    //dat.WalkData(DisplayAllData);
    fprintf(stdout, "Second Pass Completed\n");
}