#pragma once

// utility.h
// This file is for SMALL code helpers
// A good example is the static assert
// it is a useful utility, but perhaps too small to be granted
// a file of its own.
//

#include <algorithm>
#include <string>
#include <vector>
#include <io.h>
#include <map>

#include <windows.h>
#include <Objbase.h>

#include "logging.h"
#pragma comment (lib, "Ole32.lib")

namespace UCSBUtility
{

using std::string;
using std::wstring;

// The following construct seems questionable to Visual Studio
// but is done to generate errors when compiling if assumptions are wrong
// This way the errors aren't propagated into executing code.
#pragma warning (push)
#pragma warning (disable : 4189)
// Utility to ensure certain errors are caught at compilation
template <bool test>
inline void STATIC_ASSERT_IMPL()
{
    // test will be true or false, which will implictly convert to 1 or 0
    char STATIC_ASSERT_FAILURE[test] = {0};
}
#define STATIC_ASSERT(test) UCSBUtility::STATIC_ASSERT_IMPL <test>()
#pragma warning (pop)

struct StringPtrPair
{
    LPCSTR  type;
    LPCSTR  name;
};

inline UINT64 ReadTime ()
{
    __asm
    {
        rdtsc
    }
}


inline void LogError(char const* function, char const* file, int line, char const* format, ...)
{
    va_list args;
    va_start(args, format);      /* Initialize variable arguments. */

    Logging::LogError("%s(%d) %s : ", file, line, function);
    Logging::LogErrorVA(format, args);

    va_end(args);                /* Reset variable arguments.      */
}

// Simpleminded code to convert the input ascii to wide chars
inline wstring StupidConvertToWString(string const& input)
{
    wstring retv;
    retv.resize(input.size());
    for(string::size_type i=0; i<input.size(); ++i)
    {
        retv[i] = input[i];
    }

    return retv;
}

// Simpleminded code to convert the input ascii to wide chars
inline string StupidConvertToString(wstring const& input)
{
    string retv;
    retv.resize(input.size());
    for(string::size_type i=0; i<input.size(); ++i)
    {
        retv[i] = static_cast<string::value_type>(input[i]);
    }

    return retv;
}

template<size_t bufferSize>
inline string ConvertBufferToString(char const (&buffer)[bufferSize])
{
    // Copy the buffer to a string buffer
    string retv(buffer, buffer+bufferSize);
    
    // return only the minimal string (if zero terminated, limits the size)
    return string(retv.begin(), retv.begin() + strlen(retv.c_str()));
}

inline string ToLower(string const& s)
{
    string retv;
    retv.resize(s.length());

    string::iterator out = retv.begin();
    for(string::const_iterator cit = s.begin(); cit != s.end(); ++cit, ++out)
    {
        *out = static_cast<char>(tolower(*cit));
    }

    return retv;
}

inline void PadToWidth(string& padString, size_t width)
{
    padString += string((width - (padString.size() % width)) % width,' ');
}

template<typename T>
T ToINT(std::string const& in)
{
    return static_cast<T>(atol(in.c_str()));
}

inline bool HasExtension(string const& filename, string const& ext)
{
    if (filename.length() < ext.length())
    {
        return false;
    }

    string lowerFilename = ToLower(filename);
    string lowerExt = ToLower(ext);

    string::size_type extPos = lowerFilename.rfind(lowerExt);
    return extPos == (lowerFilename.length() - lowerExt.length());
}

inline void reverse(void* pOut, void const* pIn, unsigned count)
{
    unsigned char const* pI = static_cast<unsigned char const*>(pIn);
    unsigned char* pO = static_cast<unsigned char*>(pOut);

    if (static_cast<unsigned>(abs(pI - pO)) > count)
    {
        pO += count - 1;
        for(unsigned i=0; i < count; ++i, ++pI, --pO)
        {
            *pO = *pI;
        }
    }
    else
    {
        // quick and dirty coding - copy to a temporary
        // A better code would look for pI == pO and do a swap on half
        std::vector<unsigned char> source(pI, pI + count);
        reverse(pOut, &source[0], count);
    }
}

template<typename T>
inline void s_reverse(T& out, T const& in)
{
    reverse(&out, &in, sizeof(T));
}

// Hey, you can't reverse a byte - make it do nothing
inline void s_reverse(BYTE&, BYTE const&)
{
    return;
}

// Uses of the above class
#ifdef _M_CEE
// A simple, and simpleminded template class to ensure that handles are automatically closed
// DO NOT call the explicit close function or the OS may reject the automatic version
// These do not have copy constructors - that's a feature, not a bug
// copying these objects would result in double closeFn calls
// If you think this needs to be copyable, then you need to write
// a reference counting system
template <typename HANDLETYPE, HANDLETYPE invalidValue, typename CloseFnType, CloseFnType CloseFn>
class AutoCloseHandle
{
public :
    AutoCloseHandle(HANDLETYPE h) : m_handle(h)
    {
        if (h == invalidValue)
        {
            // Could throw here
        }
    }

    virtual ~AutoCloseHandle() 
    {
        if (m_handle != invalidValue)
        {
            CloseFn (m_handle);
        }
    }

    operator HANDLETYPE() const { return m_handle; }

    bool isValid()
    {
        return m_handle != invalidValue;
    }

private :
    AutoCloseHandle(AutoCloseHandle const&);
    AutoCloseHandle& operator = (AutoCloseHandle const&);

    HANDLETYPE  m_handle;
};

typedef AutoCloseHandle<HANDLE, INVALID_HANDLE_VALUE, BOOL(WINAPI *)(HANDLE), CloseHandle>  FileHandle;
#else

class FileHandle
{
public :
    FileHandle(HANDLE h) : m_handle(h)
    {
        if (h == INVALID_HANDLE_VALUE)
        {
            // Could throw here
        }
    }

    virtual ~FileHandle() 
    {
        if (m_handle != INVALID_HANDLE_VALUE)
        {
            CloseHandle (m_handle);
        }
    }

    operator HANDLE() const { return m_handle; }

    bool isValid()
    {
        return m_handle != INVALID_HANDLE_VALUE;
    }

private :
    FileHandle(FileHandle const&);
    FileHandle& operator = (FileHandle const&);

    HANDLE  m_handle;
};

#endif

class CMutexHolder
{
public :
    CMutexHolder() : m_hMutex(INVALID_HANDLE_VALUE) {}
    ~CMutexHolder() { CloseHandle(m_hMutex); }

public :
    class ScopedMutex
    {
    public :
        ScopedMutex(HANDLE hMutex) : m_hMutex(hMutex) 
        {
            WaitForSingleObject(m_hMutex, INFINITE);
        }
        
        ~ScopedMutex()
        {
            ReleaseMutex(m_hMutex);
        }

    private :
        ScopedMutex(ScopedMutex const&);
        ScopedMutex& operator = (ScopedMutex const&);
        
    private :
        HANDLE m_hMutex;
    };
    
public :
    HANDLE GetMutex(LPCWSTR name = NULL) const
    {
        if (m_hMutex == INVALID_HANDLE_VALUE)
        {
            m_hMutex = CreateMutex(NULL, false, name);
        }

        return m_hMutex;
    }


private :
    CMutexHolder(CMutexHolder const&);
    CMutexHolder& operator = (CMutexHolder const&);
    
private :
    mutable HANDLE m_hMutex;
};


template<typename Value>
class NameLookup
{
    typedef std::string string;
    typedef std::map<std::string, Value> MessageMap;

public:
    NameLookup () {}

    void Add(string const& name, Value fn)
    {
        m_map[name] = fn;
    }

    bool FindName(string const& name, Value& outValue)
    {
        MessageMap::const_iterator cit = m_map.find(name);
        if (cit == m_map.end())
        {
            //// Display purposes, not an error
            //UCSBUtility::LogError(__FUNCTION__, __FILE__, __LINE__, "Name (%s) is not in map\n", 
            //    name.c_str());
            return false;
        }

        outValue = cit->second;
        return true;
    }

    bool FindName(string const& name, Value** ppOutValue)
    {
        MessageMap::const_iterator cit = m_map.find(name);
        if (cit == m_map.end())
        {
            //// Display purposes, not an error
            //UCSBUtility::LogError(__FUNCTION__, __FILE__, __LINE__, "Name (%s) is not in map\n", 
            //    name.c_str());
            return false;
        }

        *ppOutValue = const_cast<Value*>(&cit->second);
        return true;
    }

    std::vector<string> GetNames()
    {
        std::vector<string> retv;
        MessageMap::const_iterator cit = m_map.begin();
        for(MessageMap::const_iterator cit = m_map.begin();
            cit != m_map.end(); ++cit)
        {
            retv.push_back(cit->first);
        }

        return retv;
    }

private :
    MessageMap          m_map;
};

// Uses only lower case versions of the names passed to it
// Serializes only the object registered via Add
class CriticalSectionCache
{
    typedef std::string string;

public :

    class CriticalSection
    {
    public :
        CriticalSection(string const& name)
            : pcs(Enter(ToLower(name)))
        {
        }
        
        ~CriticalSection()
        {
            if (pcs)
            {
                LeaveCriticalSection(pcs);
            }
            else
            {
                int i = 0;
                i;
            }
        }

    private :
        CriticalSection(CriticalSection const&);
        CriticalSection operator = (CriticalSection const&);

        CRITICAL_SECTION* pcs;
    };

    static void Add(string const& name)
    {
        NameLookup<CRITICAL_SECTION>& map = GetMap();
        CRITICAL_SECTION cs = {};

        string _name = ToLower(name);

        // Only add once
        if (map.FindName(_name, cs))
        {
            return;
        }
        
        // Add a copy of the current (empty) critical section
        // In order to initialize the one that will actually be used
        // we will need to grab the real one after this call
        map.Add(_name, cs);

        // Get the real critical section back
        CRITICAL_SECTION* real;
        map.FindName(_name, &real);

        InitializeCriticalSectionAndSpinCount(real, 0x400);
    }

private :
    static CRITICAL_SECTION* Enter(string const& name)
    {
        CRITICAL_SECTION* pcs;

        if (GetMap().FindName(name, &pcs))
        {
            ::EnterCriticalSection(pcs);
            return pcs;
        }

        return NULL;
    }

    //static void Leave(string const& name)
    //{
    //    CRITICAL_SECTION* pcs;

    //    if (GetMap().FindName(name, &pcs))
    //    {
    //        ::LeaveCriticalSection(pcs);
    //    }
    //}

private :
    static NameLookup<CRITICAL_SECTION>& GetMap()
    {
        static NameLookup<CRITICAL_SECTION> criticalSections;

        static bool init;

        if (!init)
        {
            init = true;
            _onexit(Cleanup);
        }

        return criticalSections;
    }

    static int Cleanup()
    {
        NameLookup<CRITICAL_SECTION>& map = GetMap();
        std::vector<string> names = map.GetNames();
        for (std::vector<string>::const_iterator cit = names.begin();
            cit != names.end(); ++cit)
        {
            CRITICAL_SECTION* pcs;
            map.FindName(*cit, &pcs);
            DeleteCriticalSection(pcs);
        }
        return 0;
    }
};

inline GUID ConvertToGUID(const std::string& guidText)
{
    GUID retv = {};

    if (guidText.length() < 36)
    {
        return retv;
    }

    std::string tempGuidText;
    std::string const* pGuidText = &guidText;

    // Do fixup for missing surrounding brackets
    // If otherwise malformed, just let it return an empty GUID
    if (guidText[0] != '{')
    {
        tempGuidText = string("{") + guidText + string("}");
        pGuidText = &tempGuidText;
    }

    CLSIDFromString(const_cast<LPOLESTR>(StupidConvertToWString(*pGuidText).c_str()), 
            &retv);

    return retv;
}

// This function is for debugging purposes
inline void DisplayDCB(DCB const& dcb)
{
    UCSBUtility::LogError(__FUNCTION__, __FILE__, __LINE__, "BaudRate           = %d\n", dcb.BaudRate);
    UCSBUtility::LogError(__FUNCTION__, __FILE__, __LINE__, "ByteSize           = %d\n", dcb.ByteSize);
    UCSBUtility::LogError(__FUNCTION__, __FILE__, __LINE__, "Parity             = %d\n", dcb.Parity);
    UCSBUtility::LogError(__FUNCTION__, __FILE__, __LINE__, "StopBits           = %d\n", dcb.StopBits);
}

inline int SetupPort(HANDLE hPort, DWORD baud, BYTE byteSize = 8, BYTE parity = 1, BYTE stopBits = 0)
{
    DCB dcb = { sizeof(DCB) };

    if (!GetCommState(hPort, &dcb))
    {
        UCSBUtility::LogError(__FUNCTION__, __FILE__, __LINE__, "Error getting current DCB settings\n");
        return 1;
    }

    UCSBUtility::LogError(__FUNCTION__, __FILE__, __LINE__, "Before\n");
    DisplayDCB(dcb);

    dcb.BaudRate = baud;
    dcb.ByteSize = byteSize;
    dcb.Parity = parity;
    dcb.StopBits = stopBits;
    if (!SetCommState(hPort, &dcb))
    {
        UCSBUtility::LogError(__FUNCTION__, __FILE__, __LINE__, "Error aetting new DCB settings %d\n", GetLastError());
        return 1;
    }
                      
    // DO NOT BLOCK waiting for the buffer to be full!!!
    COMMTIMEOUTS timeouts = {};
    timeouts.ReadIntervalTimeout = MAXDWORD;
    SetCommTimeouts(hPort, &timeouts);

    UCSBUtility::LogError(__FUNCTION__, __FILE__, __LINE__, "After\n");
    GetCommState(hPort, &dcb);
    DisplayDCB(dcb);

    return 0;
}

template<typename GetFnType, typename dataType>
inline bool ThreadedGetData(HANDLE hMutex, GetFnType GetSource, dataType const** ppLast, dataType& localCopy)
{
    // If you are stupid enough to pass a NULL pointer here, 
    // you will NEVER get any data
    if (!ppLast)
    {
        return false;
    }
    dataType const* pLast = *ppLast;

    WaitForSingleObject(hMutex, INFINITE);
    dataType const* pSource = GetSource();
    if (pSource && pSource != pLast)
    {
        memcpy(&localCopy, pSource, sizeof(localCopy));
    }
    ReleaseMutex(hMutex);

    bool retv = pSource && pSource != pLast;
    *ppLast = pSource;
    return retv;
}

inline void NowToSubDirectoryAndFilename(wstring const& extention, wstring& subDirectory, wstring& filename)
{
    SYSTEMTIME st;
    GetLocalTime(&st);
    WCHAR buffer[100];
    swprintf_s(buffer, L"\\%04d%02d%02d\\", st.wYear, st.wMonth, st.wDay);
    subDirectory = wstring(buffer);
    swprintf_s(buffer, L"%02d%02d%02d.%ls", st.wHour, st.wMinute, st.wSecond, extention.c_str());
    filename = wstring(buffer);
}

#pragma warning(push)
#pragma warning(disable : 4800)
inline bool CreateDirectoryGetFilename(wstring const& rootDirectory, 
                                  wstring const& extention, 
                                  wstring& filename)
{
    wstring subDirectory;
    wstring internalRoot = rootDirectory;
    
    // Make sure the root directory does NOT have a "\\" at the end
    if (internalRoot.length() == 0)
    {
        internalRoot = L".";
    }

    if (*internalRoot.rbegin() == L'\\')
    {
        internalRoot = wstring(internalRoot.begin(), internalRoot.end() - 1);
    }

    wstring internalExt = extention;
    if (internalExt.length() > 0 && internalExt[0] == L'.')
    {
        internalExt = wstring(internalExt.begin() + 1, internalExt.end());
    }

    // Make sure the root exists already
    CreateDirectory(internalRoot.c_str(), NULL);

    NowToSubDirectoryAndFilename(internalExt, subDirectory, filename);
    filename = internalRoot + subDirectory + filename;
    return CreateDirectory((internalRoot + subDirectory).c_str(), NULL);    
}


// Find all the spaceball files in a directory
inline std::vector<std::string> GetFilenames(std::string const& rootDirectoryPassed)
{
    std::string path;
    std::string rootPath;
    // ensure there is a final "\"
    if (rootDirectoryPassed.size() == 0)
    {
        rootPath = ".\\";
    }
    else if (*rootDirectoryPassed.rbegin() != '\\')
    {
        rootPath = rootDirectoryPassed + "\\";
    }
    else
    {
        rootPath = rootDirectoryPassed;
    }

    // Do Directories first

    // Now add the *.spaceball to find all the files
    path = rootPath + "*.*";

    std::vector<std::string> retv;

    WIN32_FIND_DATAA findData = {};
    HANDLE hFind = FindFirstFileA(path.c_str(), &findData);
    if (hFind == INVALID_HANDLE_VALUE)
    {
        UCSBUtility::LogError(__FUNCTION__, __FILE__, __LINE__, "Unable to find anything that matches:\n"
            "%s\n\nExiting without working\n", path.c_str());
        return retv;
    }
    do
    { 
        if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY && findData.cFileName[0] != '.')
        {
            std::vector<std::string> dir = GetFilenames(rootPath + findData.cFileName);
            retv.insert(retv.end(), dir.begin(), dir.end());
        }
        else if (HasExtension(findData.cFileName, ".spaceball"))
        {
            retv.push_back(rootPath + findData.cFileName);
        }
    } 
    while (FindNextFileA(hFind, &findData));

    std::sort(retv.begin(), retv.end());

    return retv;
}

inline string ChangeExtension(string const& inFile, string const& newExt)
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

inline std::string GetStringFromCLSID(GUID const& guid)
{
    LPOLESTR guidOLEString = NULL;
    StringFromCLSID(guid, &guidOLEString);
    std::string guidString = UCSBUtility::StupidConvertToString(guidOLEString);
    CoTaskMemFree(guidOLEString);

    return std::string(guidString.begin() + 1, guidString.end() - 1);
}

inline bool FileExits(string const& filename)
{
    HANDLE hFile = CreateFileA(filename.c_str(), GENERIC_READ, 
        FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);

    if (hFile != INVALID_HANDLE_VALUE)
    {
        CloseHandle(hFile);

        return true;
    }

    return false;
}

inline bool FileExits(wstring const& filename)
{
    return FileExits(StupidConvertToString(filename));
}


#pragma warning(push)
#pragma warning(disable :  4723)
inline double calculateNan()
{
    double zero = 0.0;
    double inf = 1.0 / zero;
    double nan = inf / inf;

    return nan;
}

inline double GetNan()
{
    static double retv = calculateNan();
    return retv;
}
#pragma warning(pop)

// output, input, input
template<typename vectorType>
inline void AddData(vectorType& dst, void const* pData, DWORD byteCount)
{
    dst.insert(dst.end(), reinterpret_cast<BYTE const*>(pData), 
        reinterpret_cast<BYTE const*>(pData) + byteCount);
}

// output, input, input
template<typename vectorType>
inline void Append(vectorType& dst, vectorType const& src)
{
    dst.insert(dst.end(), src.begin(), src.end());
}

// output, input, input
template<typename vectorType, typename DataType>
inline void AddStructure(std::vector<vectorType>& dst, DataType const& src)
{
    AddData(dst, &src, sizeof(src));
}

template<typename DataType, size_t count>
inline std::vector<DataType> ConvertToVector(DataType const(&buffer)[count])
{
    return std::vector<DataType> (buffer, buffer + count);
}

template<size_t count>
inline void CopyStringToBuffer(char (&buffer)[count], std::string const& input)
{
    size_t copyBytes = min(count, input.size() + 1);

    memcpy(buffer, input.c_str(), copyBytes);
}

template<size_t count>
inline BOOL Read(HANDLE hHandle, char (&buffer)[count], DWORD* pRead)
{
    return ReadFile(hHandle, buffer, count, pRead, NULL);
}

template<typename Type, size_t count>
inline BOOL Write(HANDLE hHandle, Type const (&buffer)[count], DWORD* pWritten = NULL)
{
    STATIC_ASSERT(sizeof(Type) == 1);

    DWORD dummy;
    if (pWritten == NULL)
    {
        pWritten = &dummy;
    }

    return WriteFile(hHandle, buffer, count, pWritten, NULL);
}

template<typename Type, size_t count>
inline BOOL Write(HANDLE hHandle, Type const (&buffer)[count], DWORD writeLen, DWORD* pWritten = NULL)
{
    STATIC_ASSERT(sizeof(Type) == 1);

    DWORD dummy;
    if (pWritten == NULL)
    {
        pWritten = &dummy;
    }

    if (writeLen > count * sizeof(Type))
    {
        UCSBUtility::LogError(__FUNCTION__, __FILE__, __LINE__, 
            "Write called with bad writeLen %d bytes requested.\n", 
            writeLen, count);
        writeLen = count;
    }

    return WriteFile(hHandle, buffer, writeLen, pWritten, NULL);
}

inline void ClearScreen()
{
    CONSOLE_SCREEN_BUFFER_INFO sbi;
    GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &sbi);
    DWORD dummy = 0;
    FillConsoleOutputAttribute(GetStdHandle(STD_OUTPUT_HANDLE), sbi.wAttributes,
        sbi.dwSize.X * sbi.dwSize.Y, COORD (), &dummy);
    FillConsoleOutputCharacter(GetStdHandle(STD_OUTPUT_HANDLE), L' ',
        sbi.dwSize.X * sbi.dwSize.Y, COORD (), &dummy);
    SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), COORD());
}

inline int CompareGUID(GUID const& lhs, GUID const& rhs)
{
    return memcmp(&lhs, &rhs, sizeof(lhs));
}

struct GUIDLess
{
    inline bool operator ()(GUID const& lhs, GUID const& rhs) const
    {
        return CompareGUID(lhs, rhs) < 0;
    }
};

#pragma warning(pop)

}; // end namespace UCSBUtility