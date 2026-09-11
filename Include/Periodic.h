#pragma once

#include <process.h>
#include <windows.h>

#include "internalTime.h"
#include "utility.h"

#include <algorithm>

extern bool g_quit;

namespace Periodic
{

struct PeriodicBase
{
    typedef UCSBUtility::FileHandle TimerHandle;
    typedef UCSBUtility::FileHandle ThreadHandle;
    typedef void (__cdecl * TimedThreadFn) (void *);

    PeriodicBase(PTIMERAPCROUTINE complete, TimedThreadFn timedThread)
        : m_complete(complete)
        , m_timedThread(timedThread)
    {
    }

    HANDLE BeginThreadImpl(DWORD frequency)
    {
        if (frequency == 0)
        {
            frequency = 1000;
        }

        m_frequency = frequency;
        m_thread = reinterpret_cast<HANDLE>(_beginthread(m_timedThread, 0, this));

        return m_thread;
    }

    void TimedThreadImpl()
    {
        TimerHandle hTimer = CreateWaitableTimer(NULL, false, NULL);

        LONG period = 1000 / m_frequency;

        if (period == 0)
        {
            UCSBUtility::LogError(__FUNCTION__, __FILE__, __LINE__, "Invalid frequency (%d) using 1000 Hz\n", m_frequency);
            period = 1;
        }
        
        InternalTime::internalTime next = InternalTime::internalTime::Now() + InternalTime::oneSecond / m_frequency;

        if (!SetWaitableTimer(hTimer, 
            reinterpret_cast<LARGE_INTEGER*>(next.UINT64Ptr()), 
            period, m_complete, this, true))
        {
            printf ("Thread Failure %d\n", GetLastError());
            return;
        }

        while (!g_quit)
        {
            WaitForSingleObjectEx(hTimer, 100, true);
        }

    }

    static HANDLE GetMutex()
    {
        static UCSBUtility::CMutexHolder mutex;

        return mutex.GetMutex(L"Periodic");
    }

private :
    HANDLE              m_thread;
    DWORD               m_frequency;
    PTIMERAPCROUTINE    m_complete;
    TimedThreadFn       m_timedThread;

};

template<typename PeriodicFnType>
struct MakePeriodic : PeriodicBase
{
    static HANDLE BeginThread(PeriodicFnType core, DWORD frequency)
    {
        MakePeriodic* pLeak = new MakePeriodic(core);
        return pLeak->BeginThreadImpl(frequency);
    }

private :
    MakePeriodic(PeriodicFnType core) : PeriodicBase (Complete, TimedThread), m_core(core) 
    {
        m_core.AddDeviceTypes();
    }
    
    static VOID CALLBACK Complete(LPVOID lpArgToCompletionRoutine,
                                  DWORD /*dwTimerLowValue*/,
                                  DWORD /*dwTimerHighValue*/)
    {
        MakePeriodic* pThis = reinterpret_cast<MakePeriodic*>(lpArgToCompletionRoutine);

        //FILETIME ft = {dwTimerLowValue, dwTimerHighValue};
        internalTime ft(UCSBUtility::ReadTime());

        try
        {
            pThis->m_core.Tick(ft);
        }
        catch (const char* pText)
        {
            UCSBUtility::LogError(__FUNCTION__, __FILE__, __LINE__, "Invalid Format: %s\n", pText);
        }
    }

    static void TimedThread(void* pThreadData)
    {
        MakePeriodic* pThis = reinterpret_cast<MakePeriodic*>(pThreadData);
        bool started = false;
        {
            UCSBUtility::CMutexHolder::ScopedMutex mutex(PeriodicBase::GetMutex());
            UCSBUtility::LogError(__FUNCTION__, __FILE__, __LINE__, "Starting Device: %s\n", pThis->m_core.GetName().c_str());
            started = pThis->m_core.Start();
            UCSBUtility::LogError(__FUNCTION__, __FILE__, __LINE__, "Start Device: %s %s\n\n", pThis->m_core.GetName().c_str(), 
                started ? "succeeded" : "failed");
        }
        
        if (started)
        {
            pThis->TimedThreadImpl();
            pThis->m_core.Stop();
        }
        
        delete pThis;
        _endthreadex(0);
    }

private :
    MakePeriodic(MakePeriodic const& rhs);
    MakePeriodic operator = (MakePeriodic const&);

    PeriodicFnType  m_core;
};

template<typename TimedFnType>
HANDLE CreateTimedThread(TimedFnType timedFn, DWORD frequency)
{
    return MakePeriodic<TimedFnType>::BeginThread(timedFn, frequency);
}

template<typename TimedFnType>
HANDLE CreateTimedThreadRef(TimedFnType& timedFn, DWORD frequency)
{
    return MakePeriodic<TimedFnType&>::BeginThread(timedFn, frequency);
}
};

