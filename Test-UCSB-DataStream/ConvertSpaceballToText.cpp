// ConvertSpaceballToText.cpp : Defines the entry point for the console application.
//

// Add the ability to separate blocks into files as an option

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
string g_DirectoryName = ".\\";

FILE* g_hFile;

struct GetDirectoryName
{
    bool operator()(vector<string> const& fields)
    {
        if (fields.size() < 1)
        {
            UCSBUtility::LogError(__FUNCTION__, __FILE__, __LINE__, "%d configuration fields expected"
                " %d found\nNothing will be done\n", 1, fields.size());
        }

        g_DirectoryName = fields[0];
        return true;
    }
};

void DisplayAllData(DWORD deviceNumber, BYTE const* pData)
{
    //pData;

    Device const& device = g_dictionary.GetDevice(static_cast<BYTE>(deviceNumber));
    //std::string const fields = device.GetDisplayFields();
    std::string const data = device.GetDisplayData(pData);
}

//void ConvertFile(std::string const& filename)
//{
//    FILE* pErrorFile = NULL;
//    string errorFile = ChangeExtension(filename, ".error");
//    freopen_s(&pErrorFile, errorFile.c_str(), "wt", stderr);
//
//    fprintf(stdout, "Processing File %s\n", filename.c_str());
//    FileHandle hFile = CreateFileA(filename.c_str(), GENERIC_READ, 
//        FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
//
//    char id[_countof(DeviceHolder::g_header)];
//    DWORD read = 0;
//    ReadFile(hFile, id, sizeof(id), &read, NULL);
//    if (memcmp(&id, DeviceHolder::g_header, sizeof(id)))
//    {
//        UCSBUtility::LogError(__FUNCTION__, __FILE__, __LINE__, "File does not have the expected ID bytes\nFilename: %s", filename.c_str());
//        return;
//    }
//
//
//    string outFile = ChangeExtension(filename, ".txt");
//    fopen_s(&g_hFile, outFile.c_str(), "wt");
//    if(g_hFile == NULL)
//    {
//        UCSBUtility::LogError(__FUNCTION__, __FILE__, __LINE__, "Unable to open output file: \n%s\n", outFile.c_str());
//        return;
//    }
//
//    DataAggregator::outputData interpreter;
//    DataAggregator da (interpreter);
//    ConfigFileReader::InterpretConfigurationResourceFields(L"interpreter", 
//        da);
//
//    MessageProcessor processor (interpreter);
//    ConfigFileReader::InterpretConfigurationResourceFields(L"interpreter", 
//        processor);
//
//    g_dictionary.ClearDictionary();
//
//    DeviceHolder::FileParser<MessageProcessor> parser(g_dictionary, processor);
//
//    // No need to chunk the data, read it all at once
//    vector<BYTE> data;
//    DWORD filesize = GetFileSize(hFile, NULL);
//    data.resize(filesize);
//    //DWORD read = 0;
//    ReadFile(hFile, &data[0], filesize, &read, NULL);
//    fprintf(stdout, "Closing File %s\n\n", filename.c_str());
//    fclose(g_hFile);
//
//    DataAggregationTool dat;
//    dat.SetSurveyMode(true);
//    DeviceHolder::FileParser<DataAggregationTool> datParser(g_dictionary, dat);
//    LN250::ChunkInputData(&data[0], &data[data.size()-1]+1, datParser);
//
//    fprintf(stdout, "First Pass Completed\n");
//
//    std::vector<std::pair<BYTE, BYTE> > devices = dat.GetDeviceNumbers();
//
//    fprintf(stdout, "; Found %d devices\n[%d]\n", devices.size(), devices.size());
//
//    // Print the list of devices available for display
//    for (std::vector<std::pair<BYTE, BYTE> >::const_iterator CIT = devices.begin();
//        CIT != devices.end(); ++CIT)
//    {
//        UCSB_Datastream::Device d = g_dictionary.GetDevice(static_cast<BYTE>(CIT->second));
//        std::string guidString = UCSBUtility::GetStringFromCLSID(d.DeviceID());
//        std::vector<std::string> channels = d.GetChannelNames();
//        fprintf(stdout, "; Device Found:\n[%s] [%s] [%d] [%d]\n", 
//            d.DisplayName().c_str(), guidString.c_str(), CIT->first, 
//            channels.size());
//
//        // now display all the channels available for display
//        fprintf(stdout, "; Channel List:\n");
//
//        for (std::vector<std::string>::const_iterator chnIt = channels.begin();
//            chnIt != channels.end(); ++chnIt)
//        {
//            fprintf(stdout, "\t[%s]\n", chnIt->c_str());
//        }
//    }
//
//    dat.WalkData(DisplayAllData);
//    fprintf(stdout, "Second Pass Completed\n");
//}
//
int ConvertData(int argc, _TCHAR* argv[])
{
    _variant_t text;
    ConfigFileReader::DefaultConfigurationName::Set(L"");
    GetDirectoryName gdn;
    ConfigFileReader::ReadConfigurationFields(argc, argv, gdn);

    //fopen_s(&g_hFile, g_outputFilename.c_str(), "wt");

    // Does it work?
    //SPACEHANDLE handle = GetDirectoryInfo(g_DirectoryName.c_str(), text.GetAddress());
    //if (handle != GetDirectoryInfo(g_DirectoryName.c_str(), text.GetAddress()))
    //{
    //    fprintf(stderr, "Multiple calls with the same directory give different results\n");
    //}

    //// Can it delete a handle?
    //DeleteHandle(handle);

    //// Is it robust when doing mutliple deletes?
    //DeleteHandle(handle);


    SPACEHANDLE handle = GetDirectoryInfo(g_DirectoryName.c_str(), text.GetAddress());
    SetConfigString(handle, text);
    
    ProcessDirectory(handle);

    DeleteHandle(handle);



    //vector<string> files = GetFilenames(g_DirectoryName);
    //for(vector<string>::const_iterator cit = files.begin(); cit != files.end();
    //    ++cit)
    //{
    //    ConvertFile(*cit);
    //}

    return 0;
}

int _tmain(int argc, _TCHAR* argv[])
{
    ConvertData(argc, argv);

    printf("Done, press any key to exit\n");

    _getch();

	return 0;
}

