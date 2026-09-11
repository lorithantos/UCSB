#include "utility.h"
#include <string>

class ComPort
{
    typedef std::string string;
    typedef std::vector<string>::const_iterator FieldIter;

public :
    ComPort() : m_baud(0), m_hPort(INVALID_HANDLE_VALUE),
        m_byteBits(8), m_parity(1), m_stop (0)
    {
    }

    ~ComPort()
    {
        if (m_hPort != INVALID_HANDLE_VALUE)
        {
            CloseHandle(m_hPort);
        }
    }

    bool Start()
    {
        m_hPort = CreateFileA(m_portName.c_str(), GENERIC_READ | GENERIC_WRITE, 0, NULL, 
            OPEN_EXISTING, 0, NULL);
        if (m_hPort == INVALID_HANDLE_VALUE)
        {
            UCSBUtility::LogError(__FUNCTION__, __FILE__, __LINE__, "Unable to open the COM Port %s\n", m_portName.c_str());
            return false;
        }

        // Setup the data rate we need
        if (UCSBUtility::SetupPort(m_hPort, m_baud, m_byteBits, m_parity, m_stop) != 0)
        {
            UCSBUtility::LogError(__FUNCTION__, __FILE__, __LINE__, "Unable to setup port %s\n", m_portName.c_str());
            return false;
        }

        return true;
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

        // port
        m_portName = *beg++;
        
        // baud
        m_baud = UCSBUtility::ToINT<DWORD>(*beg++);

        return beg;
    }

    template<typename Type, size_t count>
    BOOL Read(Type (&buffer)[count], DWORD* pRead)
    {
        return ReadFile(m_hPort, buffer, count * sizeof(Type), pRead, NULL);
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

        return WriteFile(m_hPort, buffer, writeLen, pWritten, NULL);
    }

    template<size_t count>
    BOOL Write(char const (&buffer)[count], DWORD* pWritten = NULL)
    {
        return UCSBUtility::Write(m_hPort, buffer, pWritten);
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

        return WriteFile(m_hPort, &buffer[0], buffer.size() * sizeof(buffer[0]), pWritten, NULL);
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

    HANDLE GetHandle() const
    {
        return m_hPort;
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
    HANDLE      m_hPort;
};