#pragma once

#include "internaltime.h"
#include "UCSB-Datastream.h"
#include "utility.h"

#include <string>
#include <map>
#include <vector>

namespace nsDataSource
{

using   std::string;
typedef std::vector<string>             strings;
typedef strings::const_iterator         FieldIter;
using   InternalTime::internalTime;
using   UCSB_Datastream::DeviceHolder;
using   UCSBUtility::CMutexHolder;

typedef UCSBUtility::StringPtrPair ChannelDefinition;


class DataSource
{
protected :
    DataSource () : m_frequency (101), m_pDevices(NULL), m_index(m_lastDevice++) {}

public :
    virtual ~DataSource() {};

    // No data device can work without having its own Tick function
    virtual bool Tick(internalTime const& now)
    {
        bool retv = TickImpl(now);
        return retv;
    }
    virtual bool TickImpl(internalTime const& now) = NULL;

    virtual bool AddDeviceTypes() = NULL
    {
        return true;
    }
    
    virtual bool Start()
    {
        // Default Behavior is to do nothing
        return true;
    }
    
    virtual bool UpdateConfigField(string const& field, string const& value)
    {
        field;
        value;
        // Default behavior is nothing, but no call should be made in that case
        return false;
    }

    virtual FieldIter Configure(FieldIter beg, FieldIter end)
    {
        // Default behavior is to do nothing
        end;
        return beg;
    }

    virtual bool Stop()
    {
        // Default behavior is to do nothing
        return true;
    }

    void SetDeviceHolder(DeviceHolder& devices)
    {
        m_pDevices = &devices;
    }
    
    DeviceHolder& GetDeviceHolder()
    {
        if (m_pDevices == NULL)
        {
            throw "Coding Error - devices must not be null " __FUNCTION__;
        }

        return *m_pDevices;
    }

    DeviceHolder const& GetDeviceHolder() const
    {
        return const_cast<DataSource*>(this)->GetDeviceHolder();
    }


    DWORD GetFrequency() const { return (m_frequency); }
    DWORD SetFrequency(DWORD frequency) { m_frequency = frequency; return (m_frequency); }
    string GetName() const { return m_name; }
    string SetName(string const& name) { m_name = name; return m_name; }
    BYTE GetIndex() const { return m_index; }

private :
    DataSource(DataSource const&);
    DataSource& operator = (DataSource const&);

private :
    DeviceHolder*       m_pDevices;
    DWORD               m_frequency;
    string              m_name;
    std::vector<BYTE>   m_mostRecentData;
    BYTE                m_index;

    static              BYTE m_lastDevice;
};


typedef DataSource*(*CreateDeviceFn)(DeviceHolder& devices);
typedef std::map<std::string, CreateDeviceFn>::const_iterator DeviceFactoryIter;

template<typename DataDeviceClass>
struct RegisterHelper
{
    static DataSource* Create(DeviceHolder& devices)
    {
        DataSource* pNewDataSource = new DataDeviceClass;
        pNewDataSource->SetDeviceHolder(devices);
        return pNewDataSource;
    }

    RegisterHelper()
    {
        DeviceFactoryMap()[UCSBUtility::ToLower(
            DataDeviceClass::GetName())] = Create;
    }

    // Yes this is stupid.  Don't remove it unless you have
    // verified that the compiler is not optimizing away
    // the very effect I am trying to create here
    // Check Debug & Release modes

    // This ensures the creation of the autoRegistration member
    std::string dummyVar;
};


template<typename DataDeviceClass>
struct AutoRegister
{
    AutoRegister (int)
    {
        // Yes this is stupid.  Don't remove it unless you have
        // verified that the compiler is not optimizing away
        // the very effect I am trying to create here
        // Check Debug & Release modes

        // This ensures the creation of the autoRegistration member
        autoRegistration.dummyVar.c_str();
    }

private :
    static RegisterHelper<DataDeviceClass> autoRegistration;
};

// Static variable used by AutoRegister
template<typename DataDeviceClass> 
RegisterHelper<DataDeviceClass> AutoRegister<DataDeviceClass>::autoRegistration;

// Function to get the DeviceFactoryMap
std::map<std::string, CreateDeviceFn>& DeviceFactoryMap();

// Function to create and configure all DataDevices from a set of string fields
std::vector<DataSource*> CreateDevices(FieldIter begin, FieldIter end, DeviceHolder& devices);

} // End nsDataSource
