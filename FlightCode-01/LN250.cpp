#include "stdafx.h"

// STL headers
#include <string>
#include <vector>

// Local headers
#include "LN250.h"
#include "DataSource.h"

using std::string;
using std::vector;

using nsDataSource::AutoRegister;
using nsDataSource::DataSource;
using InternalTime::internalTime;
using nsDataSource::FieldIter;
using UCSBUtility::SetupPort;
using LN250::MessageControl;
using LN250::InputBuffer;
using LN250::HybridInertialData;

static const int ln250Timeout = InternalTime::oneSecond * 10;

// Declare the class we need
// It should derive from DataSource and from AutoRegister
class LN250Reader : public DataSource, public AutoRegister<LN250Reader>
{
public :
    LN250Reader() : AutoRegister<LN250Reader>(0), m_hPort(INVALID_HANDLE_VALUE), m_baud(0), m_current(internalTime::Now())
        , m_lastRead(m_current) 
    {
        m_pBuffer = new InputBuffer<LN250Reader&>(*this);
    }

    ~LN250Reader()
    {
        if (m_hPort != INVALID_HANDLE_VALUE)
        {
            CloseHandle(m_hPort);
        }
    }

    FieldIter Configure(FieldIter beg, FieldIter end)
    {
        const DWORD paramCount = 2;

        // read in the parameters for the IOTech encoder
        if ((end - beg) < paramCount)
        {
            UCSBUtility::LogError(__FUNCTION__, __FILE__, __LINE__, "%d parameters expected"
                ", %d parameters found\n", paramCount, (end - beg));
            return end;
        }

        m_portName = *beg++;
        string const& baud = *beg++;
        m_baud = static_cast<DWORD>(atoi(baud.c_str()));
        
        SetFrequency(static_cast<DWORD>(101));   // Go just a little faster than the data should come in

        return beg;
    }

    virtual bool AddDeviceTypes()
    {
        HybridInertialData hid;
        GetDeviceHolder().AddDeviceType(hid);

        return DataSource::AddDeviceTypes();
    }

    virtual bool Start()
    {
        // Attempt to open the port
        m_hPort = CreateFileA(m_portName.c_str(), GENERIC_READ | GENERIC_WRITE, 0, NULL, 
            OPEN_EXISTING, 0, NULL);
        if (m_hPort == INVALID_HANDLE_VALUE)
        {
            UCSBUtility::LogError(__FUNCTION__, __FILE__, __LINE__, "Unable to open the COM Port %s\n", m_portName.c_str());
            return false;
        }

        // Setup the data rate we need
        if (SetupPort(m_hPort, m_baud) != 0)
        {
            UCSBUtility::LogError(__FUNCTION__, __FILE__, __LINE__, "Unable to setup port %s\n", m_portName.c_str());
            return false;
        }

        //SetCommMask(m_hPort, EV_RXCHAR);
        StartLN250HID();

        GetDeviceHolder().RegisterWriter(*this, HybridInertialData::GetClassGUID());

        return true;
    }
    
    virtual bool TickImpl(internalTime const& now)
    {
        m_current = now;

        BYTE buffer[10240];
        DWORD bytesRead = 0;
        BOOL readSuccess = ReadFile(m_hPort, buffer, sizeof(buffer), &bytesRead, NULL);

        if (!readSuccess || bytesRead == 0)
        {
            // If we haven't heard from the LN250 in a while, let's ping it again 
            if (internalTime::Now() - m_lastRead > ln250Timeout)
            {
                m_lastRead = internalTime::Now();
                StartLN250HID();
            }
            
            return true;
        }

        m_pBuffer->AddData(&buffer[0], &buffer[bytesRead]);

        return true;
    }

    // Whenever the buffer has enough data this function is called to process it
    void operator () (BYTE messageID, BYTE byteCount, BYTE const* pPayload)
    {
        // Only supports ID == 32
        if (messageID != HybridInertialData::MESSAGE_ID ||
            byteCount != sizeof(m_hid))
        {
            return;
        }

        // update the last read tiem - we only get here if we've read the data we want
        m_lastRead = internalTime::Now();

        memcpy(&m_hid, pPayload, byteCount);

        GetDeviceHolder().WriteDataWithCache(m_current, *this, m_hid);
    }

    void StartLN250HID()
    {
        SendLNMessage(m_hPort, MessageControl::MessageOn(HybridInertialData::MESSAGE_ID));
    }

    virtual bool Stop()
    {
        // Default behavior is to do nothing
        return true;
    }

    static string GetName()
    {
        return "LN-250";
    }

private :
    string                      m_portName;
    DWORD                       m_baud;
    HANDLE                      m_hPort;
    InputBuffer<LN250Reader&>*  m_pBuffer;
    HybridInertialData          m_hid;
    internalTime                m_current;
    internalTime                m_lastRead;
};

