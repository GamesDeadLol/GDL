
#include "StdAfx.h"
#include "Corpse.h"

CCorpseWeenie::CCorpseWeenie()
{
	_begin_destroy_at = Timer::cur_time + (60.0 * 3);
}

CCorpseWeenie::~CCorpseWeenie()
{
}

void CCorpseWeenie::ApplyQualityOverrides()
{
	CContainerWeenie::ApplyQualityOverrides();
}

void CCorpseWeenie::SetObjDesc(const ObjDesc &desc)
{
	_objDesc = desc;
}

void CCorpseWeenie::GetObjDesc(ObjDesc &desc)
{
	desc = _objDesc;
}

int CCorpseWeenie::CheckOpenContainer(CWeenieObject *other)
{
	int error = CContainerWeenie::CheckOpenContainer(other);

	if (error != WERROR_NONE)
	{
		return error;
	}

	if (!_hasBeenOpened)
	{
		if (InqIIDQuality(KILLER_IID, 0) != other->GetID() && InqIIDQuality(VICTIM_IID, 0) != other->GetID())
			return WERROR_CHEST_WRONG_KEY;
	}

	if (_begun_destroy)
	{
		return WERROR_OBJECT_GONE;
	}

	return WERROR_NONE;
}

void CCorpseWeenie::OnContainerOpened(CWeenieObject *other)
{
	CContainerWeenie::OnContainerOpened(other);

	_hasBeenOpened = true;
}

void CCorpseWeenie::OnContainerClosed(CWeenieObject *other)
{
	CContainerWeenie::OnContainerClosed(other);

	if (!m_Items.size() && !m_Packs.size())
	{
		BeginGracefulDestroy();
	}
}

void CCorpseWeenie::Tick()
{
	CContainerWeenie::Tick();

	if (!_begun_destroy)
	{
		if (!_openedBy && _begin_destroy_at <= Timer::cur_time)
		{
			BeginGracefulDestroy();
		}
	}
	else
	{
		if (_mark_for_destroy_at <= Timer::cur_time)
		{
			MarkForDestroy();
		}
	}
}

void CCorpseWeenie::BeginGracefulDestroy()
{
	if (_begun_destroy)
	{
		return;
	}

	EmitEffect(PS_Destroy, 1.0f);

	// TODO drop inventory items on the ground

	_mark_for_destroy_at = Timer::cur_time + 2.0;
	_begun_destroy = true;
}


