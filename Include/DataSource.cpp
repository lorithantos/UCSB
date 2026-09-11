#include "StdAfx.h"
#include "DataSource.h"
#include "utility.h"

using std::map;
using std::string;
using UCSBUtility::ToLower;

namespace nsDataSource
{
    
BYTE DataSource::m_lastDevice = 0;


class TestDevice : public DataSource, private AutoRegister<TestDevice>
{
public :
    static std::string GetName()
    {
        return "TestDevice";
    }

    TestDevice () : AutoRegister<TestDevice>(0) 
    {
        SetFrequency(1);
    }

    virtual bool AddDeviceTypes()
    {
        return DataSource::AddDeviceTypes();
    }

    virtual bool TickImpl(internalTime const& /* now */)
    {
        UCSBUtility::LogError(__FUNCTION__, __FILE__, __LINE__, "Tick\n");
        return true;
    };
};

std::map<std::string, CreateDeviceFn>& DeviceFactoryMap()
{
    static std::map<std::string, CreateDeviceFn> retv;
    return retv;
}

// Function to create the Devices from configuration fields
FieldIter CreateDevice(FieldIter begin, FieldIter end, DeviceHolder& devices, DataSource** pNewDevice)
{
    if (pNewDevice == NULL)
    {
        UCSBUtility::LogError(__FUNCTION__, __FILE__, __LINE__, "Programmer error, NULL pointer passed to CreateDevice\n");
        return end;
    }

    string name = ToLower(*begin++);
    DeviceFactoryIter deviceCreator = DeviceFactoryMap().find(name);
    if (deviceCreator == DeviceFactoryMap().end())
    {
        UCSBUtility::LogError(__FUNCTION__, __FILE__, __LINE__, "Unable to find device type '%s'"
            " nothing will be created for it\n", name.c_str());
        return end;
    }

    UCSBUtility::LogError(__FUNCTION__, __FILE__, __LINE__, "Creating '%s'\n", name.c_str());
    DataSource& dataDevice = *deviceCreator->second (devices);
    UCSBUtility::LogError(__FUNCTION__, __FILE__, __LINE__, "Configuring '%s'\n", name.c_str());
    begin = dataDevice.Configure(begin, end);

    *pNewDevice = &dataDevice;
    dataDevice.SetName(name);

    return begin;
}

std::vector<DataSource*> CreateDevices(FieldIter begin, FieldIter end, DeviceHolder& devices)
{
    std::vector<DataSource*> retv;

    for(;begin < end;)
    {
        DataSource* pNewDevice = NULL;
        begin = CreateDevice(begin, end, devices, &pNewDevice);

        if (pNewDevice == NULL)
        {
            return retv;
        }

        retv.push_back(pNewDevice);
    }

    return retv;
}

}