#pragma once

#include "utility.h"

#include <map>
#include <string>
#include <vector>

namespace UCSB_CommandInterface
{

using UCSBUtility::NameLookup;
using UCSBUtility::CriticalSectionCache;

class CommandBase
{
public :
    typedef std::string string;

public :

private :
    typedef std::vector<CommandBase*> commandGroup;
    typedef std::map<string, commandGroup> CommandMap;

    // Overrides
    virtual void ExecuteFn (string const& name) = NULL;
    virtual void ExecuteFn (string const& name, string const& param) = NULL;
    virtual std::vector<string> GetFnNames(string const& name) const = NULL;

protected :
    template<typename T>
    int Register(string const& name)
    {
        cbIndex = static_cast<T*>(this)->GetIndex();
        cbName = name;
        GetCommandGroupMap()[name].push_back(this);
        return GetCommandGroupMap()[name].size();
    }

public :
    virtual ~CommandBase()
    {
        RemoveFromCommandGroupMap();
    }

private :
    void RemoveFromCommandGroupMap()
    {
        commandGroup& cg = GetCommandGroupMap()[cbName];
        commandGroup::iterator end = cg.end();
        commandGroup::iterator it = std::find(cg.begin(), end, this);
        if (it != end) 
        {
            *it = NULL;
        }
    }

private :
    static CommandMap& GetCommandGroupMap()
    {
        static CommandMap map;

        return map;
    }

    class FunctionCallNoParam
    {
    public :
        FunctionCallNoParam(string const& fnName, int index) : _fnName(fnName), _index(index) {}
        void operator() (CommandBase* pCmd)
        {
            if (_index == -1 || _index == pCmd->cbIndex)
            {
                // Display purposes, not an error
                UCSBUtility::LogError(__FUNCTION__, __FILE__, __LINE__, "Calling Function: (%s::%s) on object instance %d\n", 
                    pCmd->cbName.c_str(), _fnName.c_str(), pCmd->cbIndex);

                // Execute the function
                pCmd->ExecuteFn(_fnName);
            }
        }

    private :
        string _fnName;
        int    _index;
    };

    class FunctionCallOneParam
    {
    public :
        FunctionCallOneParam(string const& fnName, string const& param, int index) : _fnName(fnName), _index(index), _param(param) {}
        void operator() (CommandBase* pCmd)
        {
            if (_index == -1 || _index == pCmd->cbIndex)
            {
                // Display purposes, not an error
                UCSBUtility::LogError(__FUNCTION__, __FILE__, __LINE__, "Calling Function: (%s::%s (%s)) on object instance %d\n", 
                    pCmd->cbName.c_str(), _fnName.c_str(), _param.c_str(), pCmd->cbIndex);

                // Execute the function
                pCmd->ExecuteFn(_fnName, _param);
            }
        }

    private :
        string _fnName;
        string _param;
        int    _index;
    };

public :
    static void CallFunction(string const& typeName, string const& fnName, int index = -1)
    {
        commandGroup& group = GetCommandGroupMap()[typeName];

        FunctionCallNoParam execute(fnName, index);

        UCSBUtility::CriticalSectionCache::CriticalSection cs(typeName);
        std::for_each(group.begin(), group.end(), execute);
    }

    static void CallFunction(string const& typeName, string const& fnName, string const& param, int index = -1)
    {
        commandGroup& group = GetCommandGroupMap()[typeName];

        FunctionCallOneParam execute(fnName, param, index);

        UCSBUtility::CriticalSectionCache::CriticalSection cs(typeName);
        std::for_each(group.begin(), group.end(), execute);
    }

    static string GetFunctionList()
    {
        string retv;

        CommandMap map = GetCommandGroupMap();
        for (CommandMap::const_iterator cit = map.begin(); cit != map.end(); ++cit)
        {
            // If there are no commands in the command group, there's nothing to report
            if (cit->second.size() == 0)
            {
                continue;
            }

            string typeName = cit->first;
            CommandBase* pBase = cit->second[0];

            std::vector<string> names = pBase->GetFnNames(typeName);
            for (std::vector<string>::const_iterator cit = names.begin(); cit != names.end(); ++cit)
            {
                retv += *cit + "\n";
            }
        }

        return retv;
    }

private :
    int cbIndex;    // CommandBaseIndex - will be the same as the DataSource::index
    string cbName;
};

// NOTE:
// When inheriting you need 
//      a static RegisterCommandFunctions
//      a static GetName which returns something convertable to std::string
//      a GetIndex which returns an integer - which will uniquely identify the instance of the object
//          The GetIndex function must be supplied by a BASE constructor not a member - THIS WILL BITE YOU
//          That's because GetIndex will be called before the initialization of your class members
//              but after the initialization of base classes

// In practice, this is easy
// Derive your class FIRST from nsDataSource::AutoRegister<T>
// then from CommandInterface<T>

template<typename T>
class CommandInterface : public CommandBase
{
    typedef void (T::*MessageFn)();
    typedef void (T::*ParameterFn)(std::string const& value);

public :
    CommandInterface()
    {
        string name = T::GetName();
        if (Register<T>(name) == 1)
        {
            CriticalSectionCache::Add(name);
            T::RegisterCommandFunctions();
        }        
    };

    static void AddFunction(string const& name, MessageFn fn)
    {
        GetMap<MessageFn>().Add(name, fn);
    }

    static void AddFunction(string const& name, ParameterFn fn)
    {
        GetMap<ParameterFn>().Add(name, fn);
    }

private:
    virtual void ExecuteFn(string const& name)
    {
        MessageFn fn;
        if (GetMap<MessageFn>().FindName(name, fn))
        {
            (static_cast<T*>(this)->*fn)();
        }
    }

    virtual void ExecuteFn(string const& name, string const& param)
    {
        ParameterFn fn;
        if (GetMap<ParameterFn>().FindName(name, fn))
        {
            (static_cast<T*>(this)->*fn)(param);
        }
    }

    static std::vector<string> GetNames(string const& name)
    {
        std::vector<string> retv;
        std::vector<string> messageName = GetMap<MessageFn>().GetNames();

        for(std::vector<string>::const_iterator cit = messageName.begin();
            cit != messageName.end(); ++cit)
        {
            retv.push_back(name + "::" + *cit + "()");
        }

        std::vector<string> parameterizedName = GetMap<ParameterFn>().GetNames();

        for(std::vector<string>::const_iterator cit = parameterizedName.begin();
            cit != parameterizedName.end(); ++cit)
        {
            retv.push_back(name + "::" + *cit + "(string const&)");
        }
        return retv;
    }

    virtual std::vector<string> GetFnNames(string const& name) const
    {
        return GetNames(name);
    }

private :
    template<typename MapType>
    static NameLookup<MapType>& GetMap()
    {
        static NameLookup<MapType> map;

        return map;
    }
};

// Hoist this out of the class for ease of use
inline void CallFunction(std::string const& typeName, std::string const& fnName, int index = -1)
{
    CommandBase::CallFunction(typeName, fnName, index);
}

// Hoist this out of the class for ease of use
inline void CallFunction(std::string const& typeName, std::string const& fnName, std::string const& param, int index = -1)
{
    CommandBase::CallFunction(typeName, fnName, param, index);
}

inline std::string GetFunctionList()
{
    return CommandBase::GetFunctionList();
}

};
