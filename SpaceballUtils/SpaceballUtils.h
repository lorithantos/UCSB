// SpaceballUtils.h

#pragma once
#include "functionality.h"

#include <string>
#include <cliext/list>

using namespace System;
using namespace cliext;

namespace SpaceballUtils {

    void MarshalString ( String ^ s, std::string& os ) {
       using namespace Runtime::InteropServices;
       const char* chars = 
          (const char*)(Marshal::StringToHGlobalAnsi(s)).ToPointer();
       os = chars;
       Marshal::FreeHGlobal(IntPtr((void*)chars));
    }

    public ref class Channel
    {
    public :
        String^ m_name;
        String^ m_type;
    };

    public ref class Device
    {
    public :
        Device()
            : m_channels (gcnew list<Channel^>)
        {
        }

        String^ Name()
        {
            return m_name;
        }

        void SetName(std::string const& name)
        {
            m_name = gcnew String(name.c_str());
        }

        void AddChannel(std::string const& name, std::string const& type)
        {
            Channel^ newChannel = gcnew Channel;
            newChannel->m_name = gcnew String(name.c_str());
            newChannel->m_type = gcnew String(type.c_str());
            m_channels->insert(m_channels->end(), newChannel);
        }

    private :
        String^         m_name;
        list<Channel^>^ m_channels;
    };

    public ref class ConvertUtils
	{
    public:
        String^ Name()
        {
            return gcnew System::String("ConvertUtils");
        }

        void ParseFile(String^ filename)
        {
            std::string osFilename;
            MarshalString(filename, osFilename);
            ReadFile(osFilename);
        }
	};

    void __clrcall CreateHandler()
    {

    }
}
