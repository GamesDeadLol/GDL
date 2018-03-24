
#include "StdAfx.h"
#include "SkillAlterationDevice.h"
#include "UseManager.h"
#include "Player.h"

CSkillAlterationDeviceWeenie::CSkillAlterationDeviceWeenie()
{
}

CSkillAlterationDeviceWeenie::~CSkillAlterationDeviceWeenie()
{
}

int CSkillAlterationDeviceWeenie::Use(CPlayerWeenie *player)
{
	if (!player->FindContainedItem(GetID()))
	{
		player->NotifyUseDone(WERROR_NONE);
		return WERROR_NONE;
	}

	// 1 = raise
	// 2 = lower
	int alterationType = InqIntQuality(TYPE_OF_ALTERATION_INT, 0);
	STypeSkill skillToAlter = SkillTable::OldToNewSkill((STypeSkill)InqIntQuality(SKILL_TO_BE_ALTERED_INT, STypeSkill::UNDEF_SKILL));

	if (alterationType <= 0 || alterationType > 2 || skillToAlter <= 0 || skillToAlter > NUM_SKILL)
	{
		player->NotifyUseDone(WERROR_NONE);
		return WERROR_NONE;
	}

	// 1 = specialize ?

	Skill skill;
	if (!player->m_Qualities.InqSkill(skillToAlter, skill))
	{
		player->NotifyUseDone(WERROR_NONE);
		return WERROR_NONE;
	}

	int numSkillCredits = player->InqIntQuality(AVAILABLE_SKILL_CREDITS_INT, 0, TRUE);

	switch (alterationType)
	{
	case 1: // raise
		{
			switch (skill._sac)
			{
			default:
				player->SendText("You may only use this if the skill is trained.", LTT_DEFAULT);
				break;

			case TRAINED_SKILL_ADVANCEMENT_CLASS:
				{
					SkillTable *pSkillTable = SkillSystem::GetSkillTable();
					const SkillBase *pSkillBase = pSkillTable->GetSkillBase(skillToAlter);
					if (pSkillBase != NULL)
					{
						numSkillCredits += pSkillBase->_trained_cost;
						if (numSkillCredits < pSkillBase->_specialized_cost)
						{
							player->SendText(csprintf("You need %d credits to specialize this skill.", numSkillCredits), LTT_DEFAULT);							
						}
						else
						{
							numSkillCredits -= pSkillBase->_specialized_cost;
							player->m_Qualities.SetInt(AVAILABLE_SKILL_CREDITS_INT, numSkillCredits);
							player->NotifyIntStatUpdated(AVAILABLE_SKILL_CREDITS_INT);

							skill._sac = SPECIALIZED_SKILL_ADVANCEMENT_CLASS;
							skill._level_from_pp = ExperienceSystem::SkillLevelFromExperience(skill._sac, skill._pp);
							player->m_Qualities.SetSkill(skillToAlter, skill);
							player->NotifySkillStatUpdated(skillToAlter);
							player->SendText(csprintf("You are now specialized in %s!", pSkillBase->_name.c_str()), LTT_ADVANCEMENT);
							player->EmitSound(Sound_RaiseTrait, 1.0, true);

							DecrementStackOrStructureNum();
						}
					}
					else
					{
						player->SendText("Cannot raise or lower this skill.", LTT_DEFAULT);
					}
					break;
				}
			}

			break;
		}
	case 2: // lower
		{
			switch (skill._sac)
			{
			default:
				player->SendText("You may only use this if the skill is trained or specialized.", LTT_DEFAULT);
				break;

			case SPECIALIZED_SKILL_ADVANCEMENT_CLASS:
				{
					SkillTable *pSkillTable = SkillSystem::GetSkillTable();
					const SkillBase *pSkillBase = pSkillTable->GetSkillBase(skillToAlter);
					if (pSkillBase != NULL)
					{
						numSkillCredits += pSkillBase->_specialized_cost;
						player->m_Qualities.SetInt(AVAILABLE_SKILL_CREDITS_INT, numSkillCredits);
						player->NotifyIntStatUpdated(AVAILABLE_SKILL_CREDITS_INT);

						DWORD64 xpToAward = 0;

						if (pSkillBase->_trained_cost > 0)
						{
							skill._sac = UNTRAINED_SKILL_ADVANCEMENT_CLASS;
							xpToAward = skill._pp;
							skill._pp = 0;
						}
						else
						{
							skill._sac = TRAINED_SKILL_ADVANCEMENT_CLASS;
						}

						skill._level_from_pp = ExperienceSystem::SkillLevelFromExperience(skill._sac, skill._pp);
						player->m_Qualities.SetSkill(skillToAlter, skill);
						player->NotifySkillStatUpdated(skillToAlter);

						if (xpToAward > 0)
						{
							player->m_Qualities.SetInt64(AVAILABLE_EXPERIENCE_INT64, player->InqInt64Quality(AVAILABLE_EXPERIENCE_INT64, 0) + xpToAward);
							player->NotifyInt64StatUpdated(AVAILABLE_EXPERIENCE_INT64);
						}

						player->SendText(csprintf("You are no longer specialized in %s!", pSkillBase->_name.c_str()), LTT_ADVANCEMENT);
						player->EmitSound(Sound_RaiseTrait, 1.0, true);

						DecrementStackOrStructureNum();
					}
					else
					{
						player->SendText("Cannot raise or lower this skill.", LTT_DEFAULT);
					}
					break;
				}

			case TRAINED_SKILL_ADVANCEMENT_CLASS:
				{
					SkillTable *pSkillTable = SkillSystem::GetSkillTable();
					const SkillBase *pSkillBase = pSkillTable->GetSkillBase(skillToAlter);
					if (pSkillBase != NULL)
					{
						if (pSkillBase->_trained_cost > 0)
						{
							numSkillCredits += pSkillBase->_trained_cost;
							player->m_Qualities.SetInt(AVAILABLE_SKILL_CREDITS_INT, numSkillCredits);
							player->NotifyIntStatUpdated(AVAILABLE_SKILL_CREDITS_INT);

							DWORD64 xpToAward = skill._pp;
							skill._pp = 0;

							skill._sac = UNTRAINED_SKILL_ADVANCEMENT_CLASS;
							skill._level_from_pp = ExperienceSystem::SkillLevelFromExperience(skill._sac, skill._pp);
							player->m_Qualities.SetSkill(skillToAlter, skill);
							player->NotifySkillStatUpdated(skillToAlter);

							if (xpToAward > 0)
							{
								player->m_Qualities.SetInt64(AVAILABLE_EXPERIENCE_INT64, player->InqInt64Quality(AVAILABLE_EXPERIENCE_INT64, 0) + xpToAward);
								player->NotifyInt64StatUpdated(AVAILABLE_EXPERIENCE_INT64);
							}

							player->SendText(csprintf("You are no longer trained in %s!", pSkillBase->_name.c_str()), LTT_ADVANCEMENT);
							player->EmitSound(Sound_RaiseTrait, 1.0, true);

							DecrementStackOrStructureNum();
						}
						else
						{
							player->SendText(csprintf("You cannot untrain %s!", pSkillBase->_name.c_str()), LTT_DEFAULT);
						}
					}
					else
					{
						player->SendText("Cannot raise or lower this skill.", LTT_DEFAULT);
					}
					break;
				}
			}

			break;
		}
	}

	player->NotifyUseDone(WERROR_NONE);
	return WERROR_NONE;
}
