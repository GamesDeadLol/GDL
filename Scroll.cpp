
#include "StdAfx.h"
#include "WeenieObject.h"
#include "Scroll.h"
#include "Player.h"

CScrollWeenie::CScrollWeenie()
{
	SetName("Scroll");
	m_Qualities.m_WeenieType = Scroll_WeenieType;
}

CScrollWeenie::~CScrollWeenie()
{
}

void CScrollWeenie::ApplyQualityOverrides()
{
}

const CSpellBase *CScrollWeenie::GetSpellBase()
{
	DWORD spell_id = 0;
	if (m_Qualities.InqDataID(SPELL_DID, spell_id))
	{
		return MagicSystem::GetSpellTable()->GetSpellBase(spell_id);
	}

	return NULL;
}

int CScrollWeenie::Use(CPlayerWeenie *player)
{
	const CSpellBase *spell = GetSpellBase();

	if (!spell)
	{
		player->SendText("This scroll doesn't have a spell?", LTT_DEFAULT);
		player->NotifyInventoryFailedEvent(GetID(), WERROR_NONE);
		return WERROR_NONE;
	}
	
	DWORD magic_skill = 0;
	if (!player->InqSkill(spell->InqSkillForSpell(), magic_skill, TRUE) || !magic_skill)
	{
		player->SendText(csprintf("You are not trained in %s!", CachedSkillTable->GetSkillName(spell->InqSkillForSpell()).c_str()), LTT_DEFAULT);
		player->NotifyInventoryFailedEvent(GetID(), WERROR_SKILL_TOO_LOW);
		return WERROR_NONE;
	}

	if (!player->FindContainedItem(GetID()))
	{
		player->NotifyInventoryFailedEvent(GetID(), WERROR_OBJECT_GONE);
		return WERROR_NONE;
	}

	CScrollUseEvent *useEvent = new CScrollUseEvent;
	useEvent->_target_id = GetID();
	player->ExecuteUseEvent(useEvent);
	return WERROR_NONE;
}

void CScrollUseEvent::OnReadyToUse()
{
	ExecuteUseAnimation(Motion_Reading);
}

void CScrollUseEvent::OnUseAnimSuccess(DWORD motion)
{
	CWeenieObject *target = GetTarget();

	if (target)
	{
		if (_weenie->LearnSpell(target->InqDIDQuality(SPELL_DID, 0), true))
		{
			// destroy it if the spell was learned
			target->ReleaseFromAnyWeenieParent();

			BinaryWriter removeInventoryObjectMessage;
			removeInventoryObjectMessage.Write<DWORD>(0x24);
			removeInventoryObjectMessage.Write<DWORD>(target->GetID());
			_weenie->SendNetMessage(&removeInventoryObjectMessage, PRIVATE_MSG, FALSE, FALSE);

			target->MarkForDestroy();
		}
	}

	_weenie->DoForcedStopCompletely();

	Done();
}
