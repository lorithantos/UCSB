#include "StdAfx.h"
#include "MMCSource.h"

#pragma comment(lib, "cbw32.lib")


/* Variable Declarations */
int BoardNum = 0;
int ULStat = 0;
int PortNum, Direction;
int PowerVal, BitValue;
int Zero = 0;
int One = 1;
WORD DataValue;
float    RevLevel = (float)CURRENTREVNUM;

MMCSource::MMCSource(void) 
    : AutoRegister<MMCSource>(0)
{
    ULStat = cbDeclareRevision(&RevLevel);
    PortNum = FIRSTPORTA;
    Direction = DIGITALIN;
    ULStat = cbDConfigPort (BoardNum, PortNum, Direction);
}

MMCSource::~MMCSource(void)
{
}

bool MMCSource::TickImpl(InternalTime::internalTime const& now)
{
    ULStat = cbDIn(BoardNum, PortNum, &DataValue);
    unsigned int flag = 1;
    for (unsigned I = 0; I < 8; I++, flag <<=1)
    {
        BitValue = Zero;
        if (DataValue & flag)
        {
            BitValue = One;
        }

        ld.bits[I] = static_cast<BYTE>(BitValue);
    }

    GetDeviceHolder().WriteDataWithCache(now, *this, ld);
    return true;
}

