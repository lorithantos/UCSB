#include "StdAfx.h"
#include "Magnetometer.h"

namespace UCSB_Magnetometer
{

Magnetometer::Magnetometer(void)
    : AutoRegister<Magnetometer>(0)
    , m_current(0)
{
    localData zero = {};
    m_Data = zero;
}

Magnetometer::~Magnetometer(void)
{
}

}