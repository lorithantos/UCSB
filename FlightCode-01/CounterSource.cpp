#include "StdAfx.h"
#include "CounterSource.h"

namespace UCSB_CounterSource
{

CounterSource::CounterSource(void)
    : AutoRegister<CounterSource>(0)
{
    //m_Data.index = 0;
    m_Data.ticks = 0;
}

CounterSource::~CounterSource(void)
{
}

}