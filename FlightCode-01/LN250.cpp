#include "stdafx.h"

// STL headers
#include <string>
#include <vector>
#include <deque>

// Local headers
#include "DataSource.h"

// #define test
// #define write_raw
// #define no_writes

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
using LN250::INSStatusData;

static const int ln250Timeout = InternalTime::oneSecond * 10;
static const int ln250WritePeriod = InternalTime::oneSecond / 50; // 20 ms, half of the supported 100Hz

// Declare the class we need
// It should derive from DataSource and from AutoRegister
class LN250Reader : public DataSource, public AutoRegister<LN250Reader>
{
private :

    // Because the data model is "one device, one output structure" we need
    // a separate class for each data structure

    class LN250_INSStatusDataReader : public DataSource, public AutoRegister<LN250_INSStatusDataReader>
    {
    public :
        LN250_INSStatusDataReader () : AutoRegister<LN250_INSStatusDataReader>(0)
        {
        }

        virtual bool Start()
        {
            // Does nothing but registration tasks
            GetDeviceHolder().RegisterWriter(*this, INSStatusData::GetClassGUID());
            return false;
        }

        virtual bool AddDeviceTypes()
        {
            INSStatusData mcd;
            GetDeviceHolder().AddDeviceType(mcd);

            return DataSource::AddDeviceTypes();
        }

        virtual bool TickImpl(internalTime const&)
        {
            return false;
        }

        static string GetName()
        {
            return "LN-250-MCD";
        }

        void ReadINSStatusData(BYTE byteCount, BYTE const* pPayload)
        {
            if (byteCount != sizeof(m_mcd))
            {
                return;
            }

            memcpy(&m_mcd, pPayload, byteCount);

            GetDeviceHolder().WriteDataWithCache(internalTime::Now(), *this, m_mcd);
        }

    private :
        INSStatusData             m_mcd;
    };

public :
    LN250Reader() : AutoRegister<LN250Reader>(0), m_hPort(INVALID_HANDLE_VALUE), m_baud(0), m_current(internalTime::Now())
        , m_lastRead(m_current), m_lastWrite(0)
    {
        HybridInertialData empty = {};
        m_hid = empty;
        m_pBuffer = new InputBuffer<LN250Reader&>(*this);

#ifdef write_raw
        m_RawOutput = CreateFile(L".\\LN250_Raw.dat", GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, 0, NULL);
#endif
    }

    ~LN250Reader()
    {
        delete m_pBuffer;
        if (m_hPort != INVALID_HANDLE_VALUE)
        {
            CloseHandle(m_hPort);
        }
#ifdef write_raw
        CloseHandle(m_RawOutput);
#endif
    }

    FieldIter Configure(FieldIter beg, FieldIter end)
    {
        const DWORD paramCount = 3;

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
        string const& orientation = *beg++;
        m_orientation = static_cast<WORD>(atoi(orientation.c_str()));
        
        SetFrequency(static_cast<DWORD>(101));   // Go just a little faster than the data should come in

        return beg;
    }

    virtual bool AddDeviceTypes()
    {
        HybridInertialData hid;
        GetDeviceHolder().AddDeviceType(hid);
        
        m_mcdReader.SetDeviceHolder(GetDeviceHolder());
        m_mcdReader.AddDeviceTypes();

        return DataSource::AddDeviceTypes();
    }

    virtual bool Start()
    {
#ifndef test
        // Attempt to open the port
        m_hPort = CreateFileA(m_portName.c_str(), GENERIC_READ | GENERIC_WRITE, 0, NULL, 
            OPEN_EXISTING, 0, NULL);
        if (m_hPort == INVALID_HANDLE_VALUE)
        {
            UCSBUtility::LogError(__FUNCTION__, __FILE__, __LINE__, "Unable to open the COM Port %s\n", m_portName.c_str());
            return false;
        }

#ifndef no_writes
        // Setup the data rate we need
        if (SetupPort(m_hPort, m_baud) != 0)
        {
            UCSBUtility::LogError(__FUNCTION__, __FILE__, __LINE__, "Unable to setup port %s\n", m_portName.c_str());
            return false;
        }
#endif

#endif
        //SetCommMask(m_hPort, EV_RXCHAR);
        StartLN250HID();
        GetDeviceHolder().RegisterWriter(*this, HybridInertialData::GetClassGUID());

        m_mcdReader.Start();

        return true;
    }
    
    virtual bool TickImpl(internalTime const&)
    {
        m_current = m_current.Now();

        ProcessOutputMessages();

#ifndef test
        BYTE buffer[10240];
        DWORD bytesRead = 0;
        BOOL readSuccess = ReadFile(m_hPort, buffer, sizeof(buffer), &bytesRead, NULL);

        if (!readSuccess || bytesRead == 0)
        {
            // If we haven't heard from the LN250 in a while, let's ping it again 
            if (m_current - m_lastRead > ln250Timeout)
            {
                m_lastRead = m_current;
                StartLN250HID();
            }
            
            return true;
        }

        m_pBuffer->AddData(&buffer[0], &buffer[bytesRead]);

#ifdef write_raw
        if (m_RawOutput != INVALID_HANDLE_VALUE)
        {
            DWORD written = 0;
            WriteFile(m_RawOutput, &buffer[0], bytesRead, &written, NULL);
        }
#endif
#else
        operator()(HybridInertialData::MESSAGE_ID, sizeof(m_hid), reinterpret_cast<BYTE const*> (&m_hid));

        static INSStatusData mcd = {};
        operator()(INSStatusData::MESSAGE_ID, sizeof(mcd), reinterpret_cast<BYTE const*> (&mcd));

#ifdef write_raw
        if (m_RawOutput != INVALID_HANDLE_VALUE)
        {
            LN250::SendLNMessage(m_RawOutput, m_hid);
            LN250::SendLNMessage(m_RawOutput, mcd);
        }
#endif
#endif

        return true;
    }

    void ReadHybridInertialData(BYTE byteCount, BYTE const* pPayload)
    {
        if (byteCount != sizeof(m_hid))
        {
            return;
        }

        memcpy(&m_hid, pPayload, byteCount);

        GetDeviceHolder().WriteDataWithCache(m_current, *this, m_hid);

        // update the last read time - we only get here if we've read the data we want
        m_lastRead = internalTime::Now();
    }

    // Whenever the buffer has enough data this function is called to process it
    void operator () (BYTE messageID, BYTE byteCount, BYTE const* pPayload)
    {
        switch (messageID)
        {
        case HybridInertialData::MESSAGE_ID :
            ReadHybridInertialData(byteCount, pPayload);
            break;

        case INSStatusData::MESSAGE_ID :
            m_mcdReader.ReadINSStatusData(byteCount, pPayload);
            break;

        default :
            UCSBUtility::LogError(__FUNCTION__, __FILE__, __LINE__, 
                "Unrecognized message from the LN-250 0x%x with %d bytes\n", 
                messageID, byteCount);
        }
    }

    void StartLN250HID()
    {
        // We need to send messages to the LN-250 at a controlled pace
        // using the write queue, we can perform the operations in 
        // a timely fashion
        m_writeQueue.push_back(&LN250Reader::SendHybridInertialDataOn);
        m_writeQueue.push_back(&LN250Reader::SendINSStatusDataOn);
        m_writeQueue.push_back(&LN250Reader::SendModeCommandData);
    }

    void ProcessOutputMessages()
    {
        // Two tests, has it been at least ln250WritePeriod and 
        // is there anything to send?
        // If neither of those are true, do nothing
        if (m_current - m_lastWrite < ln250WritePeriod || m_writeQueue.size() == 0)
        {
#ifdef test
            if (m_writeQueue.size() > 0)
            {
                // Calculate write difference
                long diff = static_cast<long>((m_current - m_lastWrite) / InternalTime::dw100nsPerMS);
                UCSBUtility::LogError(__FUNCTION__, __FILE__, __LINE__, "%8d ms since last write\n", diff);
            }
#endif
            return;
        }

        // Update the last write time
        m_lastWrite = m_current;

        // pull the function from the front of the queue
        MessageFn call = *m_writeQueue.begin();
        m_writeQueue.pop_front();

        // Call the function
        (this->*call)();
    }

    void QueueHybridInertialDataOn()
    {
        m_writeQueue.push_back(&LN250Reader::SendHybridInertialDataOn);
    }

    void SendHybridInertialDataOn()
    {
        // Not an error, just logging the function - so we can see it is being called
        UCSBUtility::LogError(__FUNCTION__, __FILE__, __LINE__, "\n");

        SendLNMessage(m_hPort, MessageControl::MessageOn(HybridInertialData::MESSAGE_ID));
    };

    void QueueINSStatusDataOn()
    {
        m_writeQueue.push_back(&LN250Reader::SendINSStatusDataOn);
    }

    void SendINSStatusDataOn()
    {
        // Not an error, just logging the function - so we can see it is being called
        UCSBUtility::LogError(__FUNCTION__, __FILE__, __LINE__, "\n");

        SendLNMessage(m_hPort, MessageControl::MessageOn(INSStatusData::MESSAGE_ID));
    };

    void QueueSendModeCommandData()
    {
        m_writeQueue.push_back(&LN250Reader::SendModeCommandData);
    }

    void SendModeCommandData()
    {
        // Not an error, just logging the function - so we can see it is being called
        UCSBUtility::LogError(__FUNCTION__, __FILE__, __LINE__, "\n");

        LN250::ModeCommandData mcd = { };
		mcd.Miscellaneous = 200 + m_orientation;//the orientation code
		mcd.MessageValidity = 16;//bit 04 is whether the Misc. word is valid
        LN250::SendLNMessage(m_hPort, mcd.Reorder(mcd));
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
    WORD                        m_orientation;
    InputBuffer<LN250Reader&>*  m_pBuffer;
    HybridInertialData          m_hid;
    internalTime                m_current;
    internalTime                m_lastRead;
    LN250_INSStatusDataReader   m_mcdReader;

    // Output Message Queuing
    typedef void (LN250Reader::*MessageFn)();
    internalTime                m_lastWrite;
    std::deque<MessageFn>       m_writeQueue;


#ifdef write_raw
    HANDLE                      m_RawOutput;
#endif
};

