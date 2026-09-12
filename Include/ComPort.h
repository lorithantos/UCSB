#include "utility.h"
#include <string>

class ComPort
{
    typedef std::string string;
    typedef std::vector<string>::const_iterator FieldIter;

public :
    ComPort() : m_baud(0),
        m_hRead(INVALID_HANDLE_VALUE), m_hWrite(INVALID_HANDLE_VALUE),
        m_byteBits(8), m_parity(1), m_stop (0)
    {
    }

    ~ComPort()
    {
        if (m_hRead != INVALID_HANDLE_VALUE)
        {
            CloseHandle(m_hRead);
        }

        if (m_hWrite != INVALID_HANDLE_VALUE && m_hWrite != m_hRead)
        {
            CloseHandle(m_hWrite);
        }
    }

private :
    bool StartPort()
    {
        m_hRead = CreateFileA(m_portName.c_str(), GENERIC_READ | GENERIC_WRITE, 0, NULL, 
            OPEN_EXISTING, 0, NULL);
        if (m_hRead == INVALID_HANDLE_VALUE)
        {
            UCSBUtility::LogError(__FUNCTION__, __FILE__, __LINE__, "Unable to open the COM Port %s\n", m_portName.c_str());
            return false;
        }

        m_hWrite = m_hRead;

        // Setup the data rate we need
        if (UCSBUtility::SetupPort(m_hRead, m_baud, m_byteBits, m_parity, m_stop) != 0)
        {
            UCSBUtility::LogError(__FUNCTION__, __FILE__, __LINE__, "Unable to setup port %s\n", m_portName.c_str());
            return false;
        }

        return true;
    }

public :
    bool Start()
    {
        if (m_portName.length() > 0)
        {
            return StartPort();
        }

        // Open the read file, if desired
        if (m_readName.length() > 0)
        {
            m_hRead = CreateFileA(m_readName.c_str(), GENERIC_READ, 0, NULL, 
                OPEN_EXISTING, 0, NULL);
            if (m_hRead == INVALID_HANDLE_VALUE)
            {
                UCSBUtility::LogError(__FUNCTION__, __FILE__, __LINE__, "Unable to open the COM Port %s\n", m_portName.c_str());
                return false;
            }
        }

        // Open the write file, if desired
        if (m_writeName.length() > 0)
        {
            m_hWrite = CreateFileA(m_readName.c_str(), GENERIC_WRITE, 0, NULL, 
                CREATE_ALWAYS, 0, NULL);
            if (m_hWrite == INVALID_HANDLE_VALUE)
            {
                UCSBUtility::LogError(__FUNCTION__, __FILE__, __LINE__, "Unable to open the COM Port %s\n", m_portName.c_str());
                return false;
            }
        }

        return true;
    }

    FieldIter ConfigureRead(FieldIter beg, FieldIter end)
    {
        const DWORD paramCount = 1;
        
        // read in the parameters for CounterSource
        if ((end - beg) < paramCount)
        {
            UCSBUtility::LogError(__FUNCTION__, __FILE__, __LINE__, "%s: %d parameters expected"
                ", %d parameters found\n", __FUNCSIG__, 
                paramCount, (end - beg));
            return beg;
        }

        m_readName = *beg++;

        return beg;
    }

    FieldIter ConfigureWrite(FieldIter beg, FieldIter end)
    {
        const DWORD paramCount = 1;
        
        // read in the parameters for CounterSource
        if ((end - beg) < paramCount)
        {
            UCSBUtility::LogError(__FUNCTION__, __FILE__, __LINE__, "%s: %d parameters expected"
                ", %d parameters found\n", __FUNCSIG__, 
                paramCount, (end - beg));
            return beg;
        }

        m_writeName = *beg++;

        return beg;
    }

    FieldIter ConfigureReadWrite(FieldIter beg, FieldIter end)
    {
        const DWORD paramCount = 2;
        
        // read in the parameters for CounterSource
        if ((end - beg) < paramCount)
        {
            UCSBUtility::LogError(__FUNCTION__, __FILE__, __LINE__, "%s: %d parameters expected"
                ", %d parameters found\n", __FUNCSIG__, 
                paramCount, (end - beg));
            return beg;
        }

        m_readName = *beg++;
        m_writeName = *beg++;

        return beg;
    }

    FieldIter Configure(FieldIter beg, FieldIter end)
    {
        const DWORD paramCount = 2;
        
        // read in the parameters for CounterSource
        if ((end - beg) < paramCount)
        {
            UCSBUtility::LogError(__FUNCTION__, __FILE__, __LINE__, "%s: %d parameters expected"
                ", %d parameters found\n", __FUNCSIG__, 
                paramCount, (end - beg));
            return end;
        }

        // We will support 4 types of ports
        // 1) Normal COM port - those will be the ones that are none of the three following
        // 2) READ - this will read the data from a file - the filename will be the first parameter after READ
        // 3) WRITE - this will write the data to a file - the filename will be the first parameter after WRITE
        // 4) READWRITE - this will read data from one file, write to another - the read filename is first, then the write filename

        // test for read, then write, then readwrite
        string portType = UCSBUtility::ToLower(*beg);
        if (!strcmp (portType.c_str(), "read"))
        {
            return ConfigureRead(++beg, end);
        }

        if (!strcmp (portType.c_str(), "write"))
        {
            return ConfigureWrite(++beg, end);
        }

        if (!strcmp (portType.c_str(), "readwrite"))
        {
            return ConfigureReadWrite(++beg, end);
        }

        // port
        m_portName = *beg++;

        // baud
        m_baud = UCSBUtility::ToINT<DWORD>(*beg++);

        return beg;
    }

    template<typename Type, size_t count>
    BOOL Read(Type (&buffer)[count], DWORD* pRead)
    {
        return ReadFile(m_hRead, buffer, count * sizeof(Type), pRead, NULL);
    }

    template<size_t count>
    BOOL Write(char const (&buffer)[count], DWORD writeLen, DWORD* pWritten = NULL)
    {
        DWORD dummy;
        if (pWritten == NULL)
        {
            pWritten = &dummy;
        }

        if (writeLen > count)
        {
            UCSBUtility::LogError(__FUNCTION__, __FILE__, __LINE__, 
                "Write called with bad writeLen %d bytes requested.\n", 
                writeLen, count);
            writeLen = count;
        }

        return WriteFile(m_hWrite, buffer, writeLen, pWritten, NULL);
    }

    template<size_t count>
    BOOL Write(char const (&buffer)[count], DWORD* pWritten = NULL)
    {
        return UCSBUtility::Write(m_hWrite, buffer, pWritten);
    }

    template<typename Type>
    BOOL Write(std::vector<Type> const& buffer, DWORD* pWritten = NULL)
    {
        DWORD dummy;
        if (pWritten == NULL)
        {
            pWritten = &dummy;
        }

        if (buffer.size() == 0)
        {
            *pWritten = 0;
            return false;
        }

        return WriteFile(m_hWrite, &buffer[0], buffer.size() * sizeof(buffer[0]), pWritten, NULL);
    }

    void SetByteBits(char byteBits)
    {
        m_byteBits = byteBits;
    }

    void SetParity(char parity)
    {
        m_parity = parity;
    }

    void SetStop(char stop)
    {
        m_stop = stop;
    }

    HANDLE GetWriteHandle() const
    {
        return m_hWrite;
    }

    DWORD GetBaud() const
    {
        return m_baud;
    }

private :
    std::string m_portName;
    DWORD       m_baud;
    char        m_byteBits;
    char        m_parity;
    char        m_stop;

    std::string m_readName;
    std::string m_writeName;

    HANDLE      m_hRead;
    HANDLE      m_hWrite;
};