#include "stdafx.h"

#include "DataSource.h"
#include "internalTime.h"
#include "ioTech.h"

using nsDataSource::AutoRegister;
using nsDataSource::DataSource;
using InternalTime::internalTime;
using   std::string;
typedef std::vector<string>             strings;
typedef strings::const_iterator         FieldIter;

const GUID GUID_IOTechDevice = UCSBUtility::ConvertToGUID("{3C452666-DBEA-42ff-93C1-F6D0A183161B}");

#if 0

// Take care of the two things this needs to do
// Read the data when passed an internalTime
// Write each of the data fields
struct EncoderWrapper : public DataSource, public AutoRegister<EncoderWrapper>
{
    typedef ioTech::IOTech IOTech;
    typedef IOTech::Encoder Encoder;

    struct DefinitionWrapper : public Encoder
    {
        static std::vector<UCSBUtility::StringPtrPair>  GetClassDescription()
        {
            static UCSBUtility::StringPtrPair definition[] =
            {
                "ULONG_I",  "Encoder",			// At the moment this is the only thing we care about
                "ULONG_I",  "Digital 2",
                "ULONG_I",  "Digital 3",
                            };

            return UCSBUtility::ConvertToVector(definition);
        }

        static std::string GetName()
        {
            return "DaqBoard3031USB{325188}";
        }

        static GUID GetClassGUID()
        {
            return UCSBUtility::ConvertToGUID("{3C452666-DBEA-42ff-93C1-F6D0A183161B}");
        }
    };

    EncoderWrapper() // string const& boardName, float rate
        : AutoRegister<EncoderWrapper>(0)
        , m_hFile(g_hFile)
        , m_current(internalTime::Now())
        , m_configured(false)
    {
    }

    virtual bool AddDeviceTypes()
    {
        DefinitionWrapper encoder;
        GetDeviceHolder().AddDeviceType(encoder);
        return DataSource::AddDeviceTypes();
    }

    virtual bool Start()
    {
        if (!m_configured)
        {
            return false;
        }

        GetDeviceHolder().RegisterWriter(*this, DefinitionWrapper::GetClassGUID());
        //GetDeviceHolder().RegisterWriter(m_current, GUID_InternalTimer);
        return m_ioTech.Setup(m_boardName, m_rate, m_scans, m_clearOnZ);
    }

    virtual bool TickImpl (internalTime const& now)
    {
        m_current = now;
        //GetDeviceHolder().WriteData(g_hFile, m_current, m_current);
        m_ioTech.ReadAndProcess(*this);
        return true;
    }

    void operator()(Encoder const& data)
    {
        // Write Data to the file
        GetDeviceHolder().WriteDataWithCache(m_current, *this, data);
    }

    FieldIter Configure(FieldIter beg, FieldIter end)
    {
        // read in the parameters for the IOTech encoder
        const DWORD paramCount = 4;

        // read in the parameters for CounterSource
        if ((end - beg) < paramCount)
        {
            UCSBUtility::LogError(__FUNCTION__, __FILE__, __LINE__, "%d parameters expected"
                ", %d parameters found\n", paramCount, (end - beg));
            return end;
        }

        // Grab the various strings
        m_boardName = *beg++;
        string const& rate = *beg++;
        string const& scans = *beg++;
        string const& clearOnZ = UCSBUtility::ToLower(*beg++);

        // Interpre the ones we need
        m_rate = static_cast<float>(atof(rate.c_str()));
        SetFrequency(static_cast<DWORD>(m_rate) * 2);

        m_scans = static_cast<DWORD>(atoi(scans.c_str()));
        m_clearOnZ = strcmp(clearOnZ.c_str(), "true") == 0;

        m_configured = true;

        return beg;
    }

    virtual void UpdateMostRecentData(std::vector<BYTE>& mrd) const
    {
        mrd;
    }

    static string GetName()
    {
        return "IOTech";
    }

private :    
    EncoderWrapper(EncoderWrapper const&);
    EncoderWrapper operator = (EncoderWrapper const&);

private:
    bool                m_configured;
    internalTime        m_current;
    ioTech::IOTech      m_ioTech;
    HANDLE&             m_hFile;
    string              m_boardName;
    float               m_rate;
    DWORD               m_scans;
    bool                m_clearOnZ;
};
#else
#endif
