
#include "StdAfx.h"
#include "TownCrier.h"
#include "ChatMsgs.h"
#include "World.h"
#include "Player.h"
#include "SpellCastingManager.h"
#include "Config.h"

CTownCrier::CTownCrier()
{
	m_Qualities.SetInt(ITEM_USEABLE_INT, USEABLE_REMOTE);
	m_Qualities.SetFloat(USE_RADIUS_FLOAT, 3.0);
}

CTownCrier::~CTownCrier()
{
}

std::string CTownCrier::GetNewsText(bool paid)
{
	/*
	switch (Random::GenInt(0, 4))
	{
	case 0:
		return "Creatures have begun to inhabit the landscape. Be careful!";
	case 1:
		return "Item may now be appraised and creature attributes may now be assessed.";
	case 2:
		return "My fellow Town Criers have remembered how to speak.";
	default:
		return "Rejoice! Have you heard? The people of Ispar have returned!";
	}
	*/

	std::vector<std::string> phrases;
	phrases.push_back("Welcome to GamesDeadLol!");

	/*
	phrases.push_back("Creatures have begun to inhabit the landscape.");
	phrases.push_back("Many items may now be appraised and creature attributes may now be assessed.");
	phrases.push_back("Vendors now sell a limited number of items for play testing.");
	phrases.push_back("Players temporarily have limitless pyreal for purchasing vendor items.");
	phrases.push_back("Creature kills now award XP based upon their difficulty.");
	phrases.push_back("Players may now level off experience earned.");
	phrases.push_back("Town criers can talk now!");
	phrases.push_back("Fixed spell casting animations not showing properly.");
	phrases.push_back("Signs and other decorative items are now spawned properly throughout the world.");
	phrases.push_back("Drop and pickup animations are now back to normal.");
	phrases.push_back("Added sound effects on various actions.");
	phrases.push_back("A lot of core systems are being overhauled in preparation of character saving; expect bugs until then.");
	*/

	return phrases[Random::GenInt(0, (DWORD)(phrases.size() - 1))];
}

int CTownCrier::DoUseResponse(CWeenieObject *player)
{
	if (!IsCompletelyIdle())
	{
		return WERROR_ACTIONS_LOCKED;
	}

	if ((m_LastUsed + 5.0) < Timer::cur_time)
	{
		m_LastUsedBy = player->GetID();
		m_LastUsed = Timer::cur_time;

		MovementParameters params;
		TurnToObject(player->GetID(), &params);
	}

	return WERROR_NONE;
}

int CTownCrier::Use(CPlayerWeenie *pOther)
{
	CGenericUseEvent *useEvent = new CGenericUseEvent();
	useEvent->_target_id = GetID();
	useEvent->_do_use_emote = false;
	useEvent->_do_use_message = false;
	pOther->ExecuteUseEvent(useEvent);

	return WERROR_NONE;
}

void CTownCrier::HandleMoveToDone(DWORD error)
{
	CWeenieObject::HandleMoveToDone(error);
	
	if (CWeenieObject *pOther = g_pWorld->FindObject(m_LastUsedBy))
	{
		pOther->SendNetMessage(DirectChat(GetNewsText(false).c_str(), GetName().c_str(), GetID(), pOther->GetID(), LTT_SPEECH_DIRECT), PRIVATE_MSG, TRUE);
		
		if (g_pConfig->TownCrierBuffs())
		{
			MakeSpellcastingManager();

			m_SpellcastingManager->CastSpellInstant(pOther->GetID(), ArmorOther7_SpellID);
			m_SpellcastingManager->CastSpellInstant(pOther->GetID(), BladeProtectionOther7_SpellID);
			m_SpellcastingManager->CastSpellInstant(pOther->GetID(), PiercingProtectionOther7_SpellID);
			m_SpellcastingManager->CastSpellInstant(pOther->GetID(), BludgeonProtectionOther7_SpellID);
			m_SpellcastingManager->CastSpellInstant(pOther->GetID(), FireProtectionOther7_SpellID);
			m_SpellcastingManager->CastSpellInstant(pOther->GetID(), ColdProtectionOther7_SpellID);
			m_SpellcastingManager->CastSpellInstant(pOther->GetID(), AcidProtectionOther7_SpellID);
			m_SpellcastingManager->CastSpellInstant(pOther->GetID(), LightningProtectionOther7_SpellID);

			m_SpellcastingManager->CastSpellInstant(pOther->GetID(), StrengthOther7_SpellID);
			m_SpellcastingManager->CastSpellInstant(pOther->GetID(), EnduranceOther7_SpellID);
			m_SpellcastingManager->CastSpellInstant(pOther->GetID(), CoordinationOther7_SpellID);
			m_SpellcastingManager->CastSpellInstant(pOther->GetID(), QuicknessOther7_SpellID);
			m_SpellcastingManager->CastSpellInstant(pOther->GetID(), FocusOther7_SpellID);
			m_SpellcastingManager->CastSpellInstant(pOther->GetID(), WillPowerOther7_SpellID);

			m_SpellcastingManager->CastSpellInstant(pOther->GetID(), CreatureEnchantmentMasteryOther7_SpellID);
			m_SpellcastingManager->CastSpellInstant(pOther->GetID(), LifeMagicMasteryOther7_SpellID);
			m_SpellcastingManager->CastSpellInstant(pOther->GetID(), ItemEnchantmentMasteryOther7_SpellID);
			m_SpellcastingManager->CastSpellInstant(pOther->GetID(), WarMagicMasteryOther7_SpellID);
			m_SpellcastingManager->CastSpellInstant(pOther->GetID(), ManaMasteryOther7_SpellID);

			m_SpellcastingManager->CastSpellInstant(pOther->GetID(), SprintOther7_SpellID);
			m_SpellcastingManager->CastSpellInstant(pOther->GetID(), JumpingMasteryOther7_SpellID);
			m_SpellcastingManager->CastSpellInstant(pOther->GetID(), ImpregnabilityOther7_SpellID);
			m_SpellcastingManager->CastSpellInstant(pOther->GetID(), InvulnerabilityOther7_SpellID);
			m_SpellcastingManager->CastSpellInstant(pOther->GetID(), MagicResistanceOther7_SpellID);
			m_SpellcastingManager->CastSpellInstant(pOther->GetID(), FealtyOther7_SpellID);
			m_SpellcastingManager->CastSpellInstant(pOther->GetID(), LeadershipMasteryOther7_SpellID);

			m_SpellcastingManager->CastSpellInstant(pOther->GetID(), SwordMasteryOther7_SpellID);
			m_SpellcastingManager->CastSpellInstant(pOther->GetID(), DaggerMasteryOther7_SpellID);
			m_SpellcastingManager->CastSpellInstant(pOther->GetID(), UnarmedCombatMasteryOther7_SpellID);
			m_SpellcastingManager->CastSpellInstant(pOther->GetID(), BowMasteryOther7_SpellID);

			pOther->MakeSpellcastingManager();			
			pOther->m_SpellcastingManager->CastSpellInstant(pOther->GetID(), BloodDrinker7_SpellID);
			pOther->m_SpellcastingManager->CastSpellInstant(pOther->GetID(), Heartseeker7_SpellID);
			pOther->m_SpellcastingManager->CastSpellInstant(pOther->GetID(), Defender7_SpellID);
			pOther->m_SpellcastingManager->CastSpellInstant(pOther->GetID(), Swiftkiller7_SpellID);

			DoForcedMotion(Motion_YMCA);
		}
		else
		{
			DoForcedMotion(Motion_WaveHigh);
		}
	}
}

DWORD CTownCrier::ReceiveInventoryItem(CWeenieObject *source, CWeenieObject *item, DWORD desired_slot)
{
	g_pWorld->RemoveEntity(item);
	return 0;
}


