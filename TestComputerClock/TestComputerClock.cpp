// TestComputerClock.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

#include <math.h>
#include <windows.h>

#include <algorithm>
#include <vector>

union systemFileTime
{
    FILETIME    filetime;
    UINT64      uint64;
};

struct timeData
{
    UINT64  time;
    UINT64  deltaTime;
    size_t  count;
    UINT64  tsc;
    UINT64  deltaTSC;

    timeData(UINT64 t, UINT64 deltaT, size_t c, UINT64 ts, UINT64 deltaTS)
        : time (t)
        , deltaTime(deltaT)
        , count(c)
        , tsc(ts)
        , deltaTSC(deltaTS)
    {
    }
};

static const size_t numValues = 20;

inline UINT64 ReadTime ()
{
    __asm
    {
        rdtsc
    }
}

UINT64 Mean(std::vector<timeData> const& data)
{
    UINT64 sum = 0;
    for (std::vector<timeData>::const_iterator cit = data.begin();
        cit != data.end(); ++cit)
    {
        sum += cit->deltaTSC;
    }

    return sum / data.size();
}

UINT64 StdDev(std::vector<timeData> const& data, UINT64 mean)
{
    // Std Dev
    // square of each difference
    // take the square root of that and divide by n
    UINT64 sum = 0;
    
    for (std::vector<timeData>::const_iterator cit = data.begin();
        cit != data.end(); ++cit)
    {
        sum += (cit->deltaTSC - mean) * (cit->deltaTSC - mean);
    }

    return static_cast<UINT64>(sqrt(static_cast<long double>(sum / data.size())));
}

bool compareTSC(timeData const& lhs, timeData const& rhs)
{
    return lhs.deltaTSC < rhs.deltaTSC;
}

int _tmain(int argc, _TCHAR* argv[])
{
    systemFileTime test0, test1;

    GetSystemTimeAsFileTime(&test0.filetime);
    GetSystemTimeAsFileTime(&test1.filetime);

    std::vector<timeData> data;

    data.reserve(numValues);


    for (size_t i = 0; i < numValues; ++i)
    {
        // First, adjust to the next time
        Sleep(0);
        GetSystemTimeAsFileTime(&test1.filetime);
        test0.filetime = test1.filetime;
        for (;test1.uint64 == test0.uint64;)
        {
            GetSystemTimeAsFileTime(&test1.filetime);
        }
        test0.filetime = test1.filetime;
        UINT64 tsc0 = ReadTime();
        UINT64 tsc1 = ReadTime();

        size_t count = 0;
        for (;test1.uint64 == test0.uint64;++count)
        {
            GetSystemTimeAsFileTime(&test1.filetime);
        }
        tsc1 = ReadTime();

        data.push_back(timeData(test0.uint64, test1.uint64 - test0.uint64, count, tsc0, tsc1 - tsc0));
        test0.filetime = test1.filetime;
    }

    UINT64 endTime = ReadTime();

    for (size_t i = 0; i < numValues; ++i)
    {
        printf("%I64d - %6d iterations %d clocks\n", data[i].time, data[i].count, data[i].deltaTSC);
    }

    std::sort(data.begin(), data.end(), compareTSC);

    // Calculate the mean & stdDev
    UINT64 mean = Mean(data);
    UINT64 stdDev = StdDev(data, mean);
    double percent = stdDev * 100.0 / mean;

    printf("Original Mean & Std Dev: %I64d : %I64d (%.2f%%)\n", mean, stdDev, percent);

    // Remove outliers
    data = std::vector<timeData>(data.begin() + 1, data.end() - 1);

    mean = Mean(data);
    stdDev = StdDev(data, mean);
    percent = stdDev * 100.0 / mean;
    printf("Removed outliers:\n\tMean & Std Dev: %I64d : %I64d (%.2f%%)\n", mean, stdDev, percent);

    while (percent > 0.5 && data.size() > 8)
    {
        // See if there are any outside of the range
        UINT64 min = mean - stdDev;
        UINT64 max = mean + stdDev;

        std::vector<timeData>::const_iterator lowBound = data.begin();
        std::vector<timeData>::const_iterator highBound = data.end();

        for(;lowBound < highBound;lowBound++)
        {
            if (lowBound->deltaTSC > min)
            {
                break;
            }
        }
        

        for(;lowBound < highBound;highBound--)
        {
            if ((highBound - 1)->deltaTSC < max)
            {
                break;
            }
        }
        
        // Remove outliers
        data = std::vector<timeData>(lowBound, highBound);

        mean = Mean(data);
        stdDev = StdDev(data, mean);
        percent = stdDev * 100.0 / mean;
        printf("Removed outliers:\n\tMean & Std Dev: %I64d : %I64d (%.2f%%) %d elements\n", 
            mean, stdDev, percent, data.size());
    }

    size_t middle = data.size() / 2;
    printf("Precise Time: %I64d clocks = %I64d * 100ns\n", data.begin()->tsc, data.begin()->time);
    printf("Precise Time: %I64d clocks = %I64d * 100ns\n", data[middle].tsc, data[middle].time);
    printf("Precise Time: %I64d clocks = %I64d * 100ns\n", data.rbegin()->tsc, data.rbegin()->time);
    
    UINT64 clocksPer100ns = mean / data[middle].deltaTime * 1000;
    printf("Estimated clocks per 0.1ms (100us) = %I64d\n", clocksPer100ns);

    printf("Estimated processor clock speed: %gHz\n", clocksPer100ns * 10000.0);

    return 0;
}

