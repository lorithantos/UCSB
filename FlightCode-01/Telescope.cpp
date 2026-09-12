#include "StdAfx.h"
#include "Telescope.h"

namespace UCSB_Telescope
{

Telescope::Telescope(void)
    : AutoRegister<Telescope>(0)
    , m_current(0)
    , m_bufferOffset(0)
{
    localData zero = {};
    m_Data = zero;
}

Telescope::~Telescope(void)
{
}

}