#include "StdAfx.h"
#include "Ashtech.h"

namespace UCSB_Ashtech
{

Ashtech::Ashtech(void)
    : AutoRegister<Ashtech>(0)
    , m_current(0)
    //, m_pFileDummy(NULL)
{
    localData zero = {};
    m_Data = zero;

    //fopen_s(&m_pFileDummy, "adu.txt", "wb");
}

Ashtech::~Ashtech(void)
{
    //if (m_pFileDummy)
    //{
    //    fclose(m_pFileDummy);
    //}
}

}