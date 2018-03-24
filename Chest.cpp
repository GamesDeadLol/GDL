
#include "StdAfx.h"
#include "Chest.h"
#include "World.h"

CChestWeenie::CChestWeenie()
{
}

CChestWeenie::~CChestWeenie()
{
}

void CChestWeenie::ApplyQualityOverrides()
{
	CContainerWeenie::ApplyQualityOverrides();

	m_Qualities.SetInt(ITEMS_CAPACITY_INT, 120);
	m_Qualities.SetInt(CONTAINERS_CAPACITY_INT, 10);
}

void CChestWeenie::PostSpawn()
{
	CContainerWeenie::PostSpawn();
}

void CChestWeenie::OnContainerOpened(CWeenieObject *other)
{
	CContainerWeenie::OnContainerOpened(other);

	DoForcedMotion(Motion_On);
}

void CChestWeenie::OnContainerClosed(CWeenieObject *other)
{
	CContainerWeenie::OnContainerClosed(other);

	DoForcedMotion(Motion_Off);
}
