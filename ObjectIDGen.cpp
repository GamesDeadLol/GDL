
#include "StdAfx.h"
#include "ObjectIDGen.h"
#include "DatabaseIO.h"

CObjectIDGenerator::CObjectIDGenerator()
{
	_hintNextDynamicGUID = 0x80000000;
	LoadState();
}

CObjectIDGenerator::~CObjectIDGenerator()
{
}

void CObjectIDGenerator::LoadState()
{
	_hintNextDynamicGUID = g_pDBIO->GetHighestWeenieID(0x80000000, 0xFF000000);
}

DWORD CObjectIDGenerator::GenerateGUID(eGUIDClass type)
{
	switch (type)
	{
	case eDynamicGUID:
		{
			if (_hintNextDynamicGUID >= 0xF0000000)
			{
				LOG(Temp, Normal, "Dynamic GUID overflow!\n");
				return 0;
			}
			
			return (++_hintNextDynamicGUID);
		}
	}

	return 0;
}