#pragma once
#include "utility.h"
#include <windows.h>

// Utility class
// For whatever reason Windows uses a broken UINT64 type as FILETIME
// That is, it is separated into two DWORDS
//
// This class and the associated helper functions treat that as
//  a minor inconvinience
//
// It also provides a couple of helper functions such as
//
// Now() - 
//      This returns the current system time as an interchangable internalTime
//
// SystemTimeMinusLocalTime() - 
//      This creates the conversion between local time and system time
//      It may be needed for either:
//      a) systemMinusLocal + Local = system
//      b) system - systemMinusLocal == system - system + local == local
//

namespace InternalTime
{

class internalTime
{
public :
    internalTime() : uint64(0)
    {
        STATIC_ASSERT(sizeof(uint64) == sizeof(filetime));
    }
    internalTime(UINT64 ui) : uint64(ui) {}
    internalTime(FILETIME ft) : filetime(ft) {}

    FILETIME* FileTimePtr() { return &filetime; }
    UINT64* UINT64Ptr()   { return &uint64; }

    operator FILETIME () const { return filetime; }
    operator UINT64 () const { return uint64; }

    static internalTime Now()
    {
        internalTime now;
        GetSystemTimeAsFileTime(&now.filetime);
        
        return now;
    }

    static UINT64 SystemTimeMinusLocalTime()
    {
        SYSTEMTIME stLocalTime;
        GetLocalTime(&stLocalTime);

        SYSTEMTIME stSystemTime;
        GetSystemTime(&stSystemTime);

        internalTime localTime;
        SystemTimeToFileTime(&stLocalTime, localTime.FileTimePtr());

        internalTime systemTime;
        SystemTimeToFileTime(&stSystemTime, systemTime.FileTimePtr());

        return (systemTime - localTime);
    }

    static internalTime NowAsLocal()
    {
        return Now() - SystemTimeMinusLocalTime();
    }

private :
    // Normal class design concerns itself with copying
    // Here we do not, the built-in ones are sufficient and efficient

    union
    {
        FILETIME    filetime;
        UINT64      uint64;
    };
};


// The minimal set of operators has been created
// if more are needed, they can be added
// there is soem question if this might be better as part of the class
// Because internalTime converts to UINT64 it might be cleaner if
// these were class members, even at the expense of requiring all
// expressions be in the form
// internalTime operator (internalTime or convertable)
inline internalTime operator - (internalTime const& lhs, internalTime const& rhs)
{
    return static_cast<UINT64>(lhs) - static_cast<UINT64>(rhs);
}

inline internalTime operator + (internalTime const& lhs, internalTime const& rhs)
{
    return static_cast<UINT64>(lhs) + static_cast<UINT64>(rhs);
}

inline internalTime operator + (internalTime const& lhs, UINT64 const& rhs)
{
    return static_cast<UINT64>(lhs) + rhs;
}

inline internalTime operator + (UINT64 const& lhs, internalTime const& rhs)
{
    return lhs + static_cast<UINT64>(rhs);
}

// Constants 
const UINT64 oneSecond    = static_cast<UINT64>(1000) * 1000 * 1000 / 100;
const UINT64 oneMinute    = oneSecond * 60;
const UINT64 dw100nsPerMS = 10000;

// Take an input string in hh:mm:ss format and calculate the number
// of 100ns intervals that represents as of today (local time)
inline UINT64 GetTimeBoundary(unsigned seconds)
{
    unsigned int h, m, s;
    h = seconds / 3600;
    m = (seconds / 60) % 60;
    s = seconds % 60;
    
    SYSTEMTIME stLocalTime;
    GetLocalTime(&stLocalTime);

    UINT64 systemMinusLocal = internalTime::SystemTimeMinusLocalTime();

    stLocalTime.wHour = static_cast<WORD>(h);
    stLocalTime.wMinute = static_cast<WORD>(m);
    stLocalTime.wSecond = static_cast<WORD>(s);
    stLocalTime.wMilliseconds = 0;

    // Convert the now adjusted time
    internalTime localTime;
    SystemTimeToFileTime(&stLocalTime, localTime.FileTimePtr());

    // localTime[adjusted] + systemMinusLoal = sytemTime[adjusted]
    return localTime + systemMinusLocal;
}

// Sets the input value to a time in the future
inline int FindNextTimeInTheFuture(UINT64& nextTime, UINT64 interval)
{
    int skipped = 0;

    UINT64 now = internalTime::Now() + oneSecond / 100;

    if (nextTime <= now)
    {
        UINT64 diff = now - nextTime;
        skipped = static_cast<int>((diff / interval) + 1);
        nextTime += skipped * interval;
    }

    return skipped;
}

};