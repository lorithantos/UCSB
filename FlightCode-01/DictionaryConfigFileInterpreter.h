#include "UCSB-Datastream.h"
#include "utility.h"

#include <algorithm>
#include <map>
#include <string>
#include <vector>

struct DictionaryConfigFileInterpreter
{
    typedef std::string string;
    typedef std::vector<string> strings;
    typedef std::vector<DWORD> DWORDs;
    typedef strings::const_iterator fieldIter;
    typedef strings::const_iterator vectorStringIter;
    typedef std::map<GUID, strings, UCSBUtility::GUIDLess>  GUIDToStrings;
    typedef std::map<GUID, DWORDs, UCSBUtility::GUIDLess>   GUIDToDWORDs;
    typedef UCSB_Datastream::Device Device;
    typedef UCSB_Datastream::DeviceHolder DeviceHolder;

    class dictionaryData
    {
    public :

        void Assign(string const& name, GUIDToStrings const& channels)
        {
            m_name = name;
            m_channels = channels;
        }

        void Clear ()
        {
            m_channels.clear();
            m_name.clear();
        }

        DWORDs GetFields(Device const& device) const
        {
            GUIDToStrings::const_iterator cit = m_channels.
                find(device.DeviceID());

            if (cit == m_channels.end())
            {
                return DWORDs();
            }

            DWORDs& channels = m_channelsIndicies[device.DeviceID()];
            if (channels.size() == 0 && cit->second.size() != 0)
            {
                UpdateIndicies(device);
            }

            return channels;
        }

        std::vector<GUID> GetDevices() const
        {
            std::vector<GUID> devices;

            for (GUIDToStrings::const_iterator cit = m_channels.begin();
                cit != m_channels.end(); ++cit)
            {
                devices.push_back(cit->first);
            }

            return devices;
        }

    private :
        void UpdateIndicies(Device const& device) const
        {
            GUIDToStrings::const_iterator cit = m_channels.
                find(device.DeviceID());

            if (cit == m_channels.end())
            {
                return;
            }

            DWORDs& channelIndicies = m_channelsIndicies[device.DeviceID()];
            strings const& channelStrings = cit->second;
            channelIndicies.clear();

            // Create a list of strings from the device
            strings sourceNames = device.GetChannelNames();

            vectorStringIter sourceBegin = sourceNames.begin();
            vectorStringIter sourceEnd = sourceNames.end();

            // do a search for each name
            for(vectorStringIter iter = channelStrings.begin();
                iter < channelStrings.end(); ++iter)
            {
                vectorStringIter item = std::find(sourceBegin, sourceEnd, *iter);
                if (item == sourceEnd)
                {
                    UCSBUtility::LogError(__FUNCTION__, __FILE__, __LINE__, "Data Aggregator config failure: "
                        "%s field does not exist in device %s", iter->c_str(), 
                        device.DisplayName().c_str());
                }
                else
                {
                    channelIndicies.push_back(item - sourceBegin);
                }
            }
        }

    private :
        string                  m_name;
        GUIDToStrings           m_channels;

    private :
        mutable GUIDToDWORDs    m_channelsIndicies;
    };

    DictionaryConfigFileInterpreter(dictionaryData& destination)
        : m_destination(destination)
        , m_valid(true)
    {}

    DictionaryConfigFileInterpreter(DictionaryConfigFileInterpreter const& rhs)
        : m_destination(rhs.m_destination)
        , m_valid(rhs.m_valid)
    {
    }

    fieldIter AddDevice(fieldIter begin, fieldIter end)
    {
        // There must be a device ID, a channel count and at least one channel
        if (end - begin < 3)
        {
            UCSBUtility::LogError(__FUNCTION__, __FILE__, __LINE__, "Malformed aggregator configuration:"
                " %d fields expected, %d found\n", 3, end - begin);
            m_valid = false;
            return end;
        }

        string const& guid = *begin; ++begin;
        GUID deviceID = UCSBUtility::ConvertToGUID(guid);
        // Look up the device, if it isn't in the dictionary, we can't use it

        int count = atoi(begin->c_str()); ++begin;

        strings names;
        for (int i=0; i < count && begin < end; ++i, ++begin)
        {
            names.push_back(*begin);
        }

        m_channels[deviceID] = names;

        return begin;
    }

    bool operator ()(strings const& fields)
    {
        m_destination.Clear();

        m_name = fields[0];
        fieldIter begin = fields.begin() + 1;
        while (begin < fields.end())
        {
            begin = AddDevice(begin, fields.end());
        }

        if (m_valid)
        {
            m_destination.Assign(m_name, m_channels);
        }

        return fields.size() > 0;
    }

private :
    DictionaryConfigFileInterpreter& operator= (DictionaryConfigFileInterpreter const& rhs);

private :
    dictionaryData& m_destination;
    bool            m_valid;
    string          m_name;
    GUIDToStrings   m_channels;
};

