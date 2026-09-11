#pragma once

// File ConfigFileReader.h
// Provides utilities to assist with reading config files
//

#include "utility.h"

#include <vector>
#include <string>

namespace ConfigFileReader
{

using std::wstring;
using std::string;
using std::vector;

using UCSBUtility::StupidConvertToString;
using UCSBUtility::FileHandle;

// Convert the buffer to individual lines
// skip 
//  empty
//  first non-space char is not '['
// (this more forgiving than the specification calls for
// it means that any line can be a comment, so long as it does not
// start with a '[')
inline vector<string> ConvertBufferToLines(string const& buffer)
{
    vector<string> retv;
    string current;
    for (string::const_iterator cit=buffer.begin(); cit < buffer.end(); ++cit)
    {
        // END of line markers mean start a new line
        if (*cit == '\n' || *cit == '\n')
        {
            if (current.length() > 0)
            {
                if (current[0] == '[')
                {
                    retv.push_back(current);
                }

                current.clear();
            }

            continue;
        }

        // skip leading whitespace
        if (current.length() == 0 && isspace(*cit))
        {
            continue;
        }

        current.push_back(*cit);
    }

    if (current.length() > 0 && (current[0] == '['))
    {
        retv.push_back(current);
    }

    return retv;
}

// all fields must be surrounded by brackets
// this returns a list of items (bracket are removed)
inline vector<string> ConvertLineToFields(string const& line)
{
    vector<string> retv;
    string field;
    bool start = false;

    for(string::const_iterator cit = line.begin(); cit != line.end(); ++cit)
    {
        if (*cit == '[')
        {
            field.clear();
            start = true;
            continue;
        }
        
        if (*cit == ']')
        {
            if (start)
            {
                retv.push_back(field);
            }

            field.clear();
            start = false;
            continue;
        }

        if (start)
        {
            field.push_back(*cit);
        }
    }

    return retv;
}

inline string GetResourceString(wstring const& resourceName)
{
    string retv;

    HRSRC hRsrc = FindResource(NULL, resourceName.c_str(), L"TXT");
    if (hRsrc == NULL)
    {
        UCSBUtility::LogError(__FUNCTION__, __FILE__, __LINE__, "Failed to find String Data: %ls\n", resourceName.c_str());
        return retv;
    }

    HGLOBAL hGlobal = LoadResource(NULL, hRsrc);
    if (hGlobal == NULL)
    {
        // This pretty much can't happen now days
        UCSBUtility::LogError(__FUNCTION__, __FILE__, __LINE__, "Failed to load String Data: %ls\n", resourceName.c_str());
        return retv;
    }
    char* pText = static_cast<char*>(LockResource(hGlobal));
    DWORD length = SizeofResource(NULL, hRsrc);

    retv = string (pText, pText + length);
    FreeResource(hGlobal);

    return retv;
}

inline bool ProcessHelp(wstring const& filename = L"/?")
{
    // The filename may be a cry for help.  If so, display the help
    if (wcscmp(filename.c_str(), L"/?") != 0 &&
        wcscmp(filename.c_str(), L"-?") != 0)
    {
        // Not a help request
        return false;
    }

    printf("Displaying Help\n");
    
    printf("%s\n", GetResourceString(L"HELPDATA").c_str());

    return true;
}

inline bool ReadCommandLine(int argc, WCHAR* argv[], vector<string>& lines)
{
    // Is there anything on the command line besides the exe name?
    if (argc < 2)
    {
        return false;
    }

    // if the command line uses brackets, treat as a single config line
    // So check the first one
    if (argv[1][0] != '[')
    {
        return false;
    }

    // we've passed the tests
    // now create a single string from the command line arguments
    // (skip the program name)
    wstring commandLine;
    for (int i=1; i < argc;++i)
    {
        commandLine += wstring(argv[i]) + L" ";
    }

    lines = ConvertBufferToLines(StupidConvertToString(commandLine));
    return true;
}

inline bool ReadConfigurationFile(wstring const& filename, vector<string>& lines)
{
    // Read each line
    // if the line begins with a ';' discard it
    // strip out the whitespace at the beginning
    FileHandle hFile = CreateFile(filename.c_str(), GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL);
    if (hFile == INVALID_HANDLE_VALUE)
    {
        if (filename.length() > 0)
        {
            UCSBUtility::LogError(__FUNCTION__, __FILE__, __LINE__, "Unable to open the config file %ls\n", filename.c_str());
        }
        return false;
    }

    // We will assume the config file is less than 4GB long.  If you have one that big, the error is yours
    string buffer;
    buffer.resize(GetFileSize(hFile, NULL) + 1);
    buffer[buffer.size()-1] = 0;
    DWORD readLen = 0;
    ReadFile(hFile, &buffer[0], buffer.size()-1, &readLen, NULL);

    lines = ConvertBufferToLines(buffer);
    return true;
}

struct DefaultConfigurationName
{
    static void Set (std::wstring const& newDefault)
    {
        GetName() = newDefault;
    }

    static std::wstring Get()
    {
        return GetName();
    }

private :
    static std::wstring& GetName()
    {
        static wstring name = L"configuration.cfg";

        return name;
    }
};

// Interpret the configuration file
// all lines that start with ';' are comments
// valid lines have the format
// [url] [dest] [hh:mm:ss] [hh:mm:ss]
// where the first [hh:mm:ss] is the time of day to start downloading
// and the second is the amount of time between downloads
template<typename ProcessLinesFn>
inline unsigned ReadConfiguration(int argc, WCHAR* argv[], ProcessLinesFn processLines)
{
    if((argc > 1) && ProcessHelp(argv[1]))
    {
        return 1;
    }

    wstring filename = DefaultConfigurationName::Get();

    // Two possibilities, either the filename is first
    // or the arguments have been passed to the command line
    // so, harmless assumption that the first argument is the filename
    if (argc > 1)
    {
        filename = argv[1];
    }

    vector<string> lines;
    // Try the command line first, then the config file
    if (!ReadCommandLine(argc, argv, lines) && !ReadConfigurationFile(filename, lines))
    {
        ProcessHelp();
        return 1;
    }

    if (lines.size() == 0)
    {
        UCSBUtility::LogError(__FUNCTION__, __FILE__, __LINE__, "No valid lines found in config file %ls\n", filename.c_str());
        return 1;
    }

    UCSBUtility::LogError(__FUNCTION__, __FILE__, __LINE__, "Configuration lines found\n");
    for (vector<string>::size_type i=0; i < lines.size(); ++i)
    {
        UCSBUtility::LogError(__FUNCTION__, __FILE__, __LINE__, "%s\n", lines[i].c_str());
    }

    return processLines(lines);
}

template<typename processFnType>
struct ReadFieldsAdapter
{
    ReadFieldsAdapter(ReadFieldsAdapter const& rhs) :
        m_processFn(rhs.m_processFn)
    {

    }

    ReadFieldsAdapter& operator = (ReadFieldsAdapter const& rhs)
    {
        m_processFn = rhs.m_processFn;
        return *this
    }

    ReadFieldsAdapter(processFnType& processFn) :
        m_processFn(processFn) {}

        unsigned operator() (vector<string> const& input)
        {
            // Collect all the data into a single vector of fields
            vector<string> fields;    
            for(vector<string>::size_type i=0; i < input.size(); ++i)
            {
                vector<string> subFields = ConfigFileReader::ConvertLineToFields(input[i]);
                fields.insert(fields.end(), subFields.begin(), subFields.end());
            }

            if (fields.size() == 0)
            {
                UCSBUtility::LogError(__FUNCTION__, __FILE__, __LINE__, "%s: Empty Configuration\n", __FUNCSIG__);
                return static_cast<unsigned>(-1);
            }

            return m_processFn(fields);
        }

private :
    processFnType& m_processFn;
};

template<typename processFnType>
ReadFieldsAdapter<processFnType> GetReadFieldsAdapter(processFnType& processFn)
{
    return ReadFieldsAdapter<processFnType>(processFn);
}

template<typename ProcessFieldsFn>
inline unsigned ReadConfigurationFields(int argc, WCHAR* argv[], ProcessFieldsFn& processFields)
{
    return ReadConfiguration(argc, argv, GetReadFieldsAdapter(processFields));
}

#pragma warning (push)
#pragma warning (disable : 4800)
template<typename ProcessFieldsFn>
inline bool InterpretConfigurationFileFields(wstring const& filename, ProcessFieldsFn processFields)
{
    std::vector<string> lines;
    ReadConfigurationFile(filename, lines);

    // Require each line to be a device
    for (std::vector<string>::const_iterator cit = lines.begin(); cit != lines.end(); ++cit)
    {
        processFields(ConfigFileReader::ConvertLineToFields(*cit));
    }
    return true;
}

template<typename ProcessFieldsFn>
inline bool InterpretConfigurationResourceFields(wstring const& resourceName, ProcessFieldsFn processFields)
{
    std::vector<string> lines = ConvertBufferToLines(GetResourceString(resourceName));
    if (lines.size() == 0)
    {
        UCSBUtility::LogError(__FUNCTION__, __FILE__, __LINE__, "%s: Empty (or missing) ConfigurationResource %ls\n", __FUNCSIG__, resourceName.c_str());
        return false;
    }

    return GetReadFieldsAdapter(processFields) (lines);
}

template<typename ProcessFieldsFn>
inline bool InterpretConfigurationFields(wstring const& configName, ProcessFieldsFn processFields)
{
    // Look for it in the local directory
    // if it exists, use the file
    // if it does not, use the resource
    wstring filename = configName + L".cfg";

    if (UCSBUtility::FileExits(filename))
    {
        UCSBUtility::Logging::LogError("Using File %ls\n", filename.c_str());
        return InterpretConfigurationFileFields(filename, processFields);
    }

    UCSBUtility::Logging::LogError("Using Resource %ls\n", configName.c_str());
    return InterpretConfigurationResourceFields(configName, processFields);
}

template<typename ProcessFieldsFn>
inline bool InterpretStringFields(string const& source, ProcessFieldsFn processFields)
{
    std::vector<string> lines = ConvertBufferToLines(source);
    if (lines.size() == 0)
    {
        UCSBUtility::LogError(__FUNCTION__, __FILE__, __LINE__, "%s: empty string\n%s\n", __FUNCSIG__, source.c_str());
        return false;
    }

    return GetReadFieldsAdapter(processFields) (lines);
}
#pragma warning (pop)
};
