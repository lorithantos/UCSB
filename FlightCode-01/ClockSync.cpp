#include "StdAfx.h"
#include "ClockSync.h"

namespace UCSB_ClockSync
{

ClockSync::ClockSync(void)
    : AutoRegister<ClockSync>(0)
{
    m_Data.filetime = 0;
}

ClockSync::~ClockSync(void)
{
}

}