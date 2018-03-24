
#include "StdAfx.h"
#include "Lockpick.h"
#include "UseManager.h"
#include "Player.h"

CLockpickWeenie::CLockpickWeenie()
{
}

CLockpickWeenie::~CLockpickWeenie()
{
}

int CLockpickWeenie::UseWith(CPlayerWeenie *player, CWeenieObject *with)
{
	CGenericUseEvent *useEvent = new CGenericUseEvent;
	useEvent->_target_id = with->GetID();
	useEvent->_tool_id = GetID();
	useEvent->_max_use_distance = 1.0;
	player->ExecuteUseEvent(useEvent);

	return WERROR_NONE;
}

int CLockpickWeenie::DoUseWithResponse(CWeenieObject *player, CWeenieObject *with)
{
	if (CPlayerWeenie *player_weenie = player->AsPlayer())
	{
		if (!with->IsLocked())
		{
			player->SendText("Already unlocked. Re-locking via lockpicks is not implemented yet.", LTT_DEFAULT);
			return WERROR_CHEST_ALREADY_UNLOCKED;
		}
		else
		{
			int resistLockpick = 0;
			if (with->m_Qualities.InqInt(RESIST_LOCKPICK_INT, resistLockpick))
			{
				SKILL_ADVANCEMENT_CLASS sac;
				if (!player->m_Qualities.InqSkillAdvancementClass(LOCKPICK_SKILL, sac) || (sac != TRAINED_SKILL_ADVANCEMENT_CLASS && sac != SPECIALIZED_SKILL_ADVANCEMENT_CLASS))
				{
					player->SendText("You must be trained in Lockpicking.", LTT_DEFAULT);
					return WERROR_DONT_KNOW_LOCKPICKING;
				}
				
				DWORD skillLevel = 0;
				if (player->InqSkill(LOCKPICK_SKILL, skillLevel, FALSE))
				{
					DecrementStackOrStructureNum();

					std::string lockpickText;

					if (GenericSkillCheck(skillLevel + InqIntQuality(LOCKPICK_MOD_INT, 0), resistLockpick))
					{
						lockpickText += "You have successfully picked the lock!  It is now unlocked.\n";

						with->SetLocked(FALSE);
						with->EmitSound(Sound_LockSuccess, 1.0f);
					}
					else
					{
						lockpickText += "You have failed to pick the lock.  It is still locked. ";
						with->EmitSound(Sound_PicklockFail, 1.0f);
					}

					int numUsesLeft = GetStructureNum();
					if (numUsesLeft > 0)
					{
						lockpickText += csprintf(" Your lockpicks have %d uses left.", numUsesLeft);
					}
					else if (numUsesLeft == 0)
					{
						lockpickText += " Your lockpicks are used up.";
					}

					player->SendText(lockpickText.c_str(), LTT_DEFAULT);
				}
				else
				{
					player->SendText("Your Lockpicking skill is too low for that.", LTT_DEFAULT);
				}
			}
		}
	}

	return WERROR_NONE;
}